//////////////////////////////////////////////////////////////////////////////////
//	Loose .img backend: WZ key stream, inflate and canvas decoding				//
//																				//
//	Canvas payloads are															//
//		[0x78 0x9C][raw deflate]              (plain)							//
//	or, for newer content, a chunked encrypted stream							//
//		[int32 chunk size][chunk ^ key stream]*                               //
//	which decrypts to the zlib form. The deflate stream is not always			//
//	terminated properly, so inflating stops once the expected pixel size is		//
//	reached (same behaviour as MapleLib/WzComparerR2).							//
//////////////////////////////////////////////////////////////////////////////////
#include "../../MapleStory.h"

#ifdef USE_IMG
#include "img_impl.hpp"

#include <cstdio>
#include <cstring>

#include <lz4.h>

#include <Windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#include "../miniz/miniz_tinfl.h"

namespace
{
	// MapleCryptoConstants.MAPLESTORY_USERKEY_DEFAULT, trimmed to the AES-256 key
	// WzKeyGenerator.GenerateWzKey() uses: key[i * 4] = user_key[i * 16]
	uint8_t const WZ_AES_KEY[32] =
	{
		0x13, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00,
		0x1B, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00
	};

	// WzAESConstant.WZ_GMSIV - the IV the data set was packed with
	uint8_t const WZ_IV[4] = { 0x4D, 0x23, 0xC7, 0x2B };

	// Key stream of the first block, measured from the data set ("Property" as the
	// header string of every image). Used to detect a wrong key or foreign data.
	uint8_t const WZ_KEY_CHECK[8] = { 0x96, 0xAE, 0x3F, 0xA4, 0x48, 0xFA, 0xDD, 0x90 };

	// Never destroyed (see img_immortal), like the other caches of the backend
	std::vector<uint8_t>& g_keystream = nl::img_immortal<std::vector<uint8_t>>();
	bool g_keystream_checked = false;
	tinfl_decompressor g_inflator;

	// Canvas failures are logged a few times, a broken image would flood the log
	size_t g_canvas_errors = 0;

	bool report_canvas_error(std::string const& message)
	{
		if (g_canvas_errors++ < 5)
			nl::img_log_error(message);

		return false;
	}

	bool is_zlib_header(uint8_t const* data)
	{
		return data[0] == 0x78
			&& (data[1] == 0x9C || data[1] == 0xDA || data[1] == 0x01 || data[1] == 0x5E);
	}

	// MapleLib WzPngProperty.DecryptListWzBlocks / WzComparerR2 ChunkedEncryptedInputStream
	std::vector<uint8_t> decrypt_chunks(std::vector<uint8_t> const& payload)
	{
		std::vector<uint8_t> out;
		out.reserve(payload.size());
		size_t offset = 0;

		while (offset + 4 <= payload.size())
		{
			int32_t chunk = 0;

			for (int i = 0; i < 4; i++)
				chunk |= static_cast<int32_t>(payload[offset + i]) << (8 * i);

			offset += 4;

			if (chunk <= 0 || static_cast<size_t>(chunk) > payload.size() - offset)
				return {};

			auto const& key = nl::img_keystream(static_cast<size_t>(chunk));

			if (key.size() < static_cast<size_t>(chunk))
				return {};

			for (int32_t i = 0; i < chunk; i++)
				out.push_back(static_cast<uint8_t>(payload[offset + i] ^ key[static_cast<size_t>(i)]));

			offset += static_cast<size_t>(chunk);
		}

		return out;
	}

	// Native (pre-conversion) size of a canvas in bytes
	size_t native_size(int32_t format, size_t pixels)
	{
		switch (format)
		{
		case 1:		// BGRA4444
		case 257:	// ARGB1555
		case 513:	// RGB565
			return pixels * 2;
		case 2:		// BGRA8888
			return pixels * 4;
		default:
			return 0;
		}
	}

	uint8_t expand_nibble(uint8_t value)
	{
		return static_cast<uint8_t>((value & 0x0F) * 17);
	}

	uint8_t expand_5(uint8_t value)
	{
		return static_cast<uint8_t>((value << 3) | (value >> 2));
	}

	uint8_t expand_6(uint8_t value)
	{
		return static_cast<uint8_t>((value << 2) | (value >> 4));
	}
}

namespace nl
{
	std::vector<uint8_t> const& img_keystream(size_t length)
	{
		size_t needed = length == 0 ? 0 : ((length + 15) / 16) * 16;

		if (g_keystream.size() >= needed || needed == 0)
			return g_keystream;

		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_KEY_HANDLE key = nullptr;

		if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0)
		{
			BCryptSetProperty(
				algorithm,
				BCRYPT_CHAINING_MODE,
				reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
				sizeof(BCRYPT_CHAIN_MODE_ECB),
				0
				);

			if (BCryptGenerateSymmetricKey(algorithm, &key, nullptr, 0, const_cast<PUCHAR>(WZ_AES_KEY), sizeof(WZ_AES_KEY), 0) >= 0)
			{
				// WzMutableKey: block 0 is the IV repeated, block n+1 is AES(block n)
				uint8_t block[16];

				if (g_keystream.empty())
				{
					for (int i = 0; i < 4; i++)
						std::memcpy(block + i * 4, WZ_IV, 4);
				}
				else
				{
					std::memcpy(block, g_keystream.data() + g_keystream.size() - 16, 16);
				}

				while (g_keystream.size() < needed)
				{
					uint8_t out[16];
					ULONG written = 0;

					if (BCryptEncrypt(key, block, 16, nullptr, nullptr, 0, out, 16, &written, 0) < 0 || written != 16)
						break;

					std::memcpy(block, out, 16);
					g_keystream.insert(g_keystream.end(), out, out + 16);
				}

				BCryptDestroyKey(key);
			}

			BCryptCloseAlgorithmProvider(algorithm, 0);
		}

		if (!g_keystream_checked && g_keystream.size() >= sizeof(WZ_KEY_CHECK))
		{
			g_keystream_checked = true;

			if (std::memcmp(g_keystream.data(), WZ_KEY_CHECK, sizeof(WZ_KEY_CHECK)) != 0)
				img_log_error("WZ key stream mismatch: the .img data was packed for another client version");
		}

		return g_keystream;
	}

	// pOut_buf_size is the remaining space; the caller must supply room for the
	// whole stream (TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF).
	std::vector<uint8_t> img_inflate(std::vector<uint8_t> const& payload, size_t expected, size_t offset)
	{
		if (expected == 0 || offset >= payload.size())
			return {};

		std::vector<uint8_t> out(expected);

		// The decompressor keeps state between calls, so every stream starts fresh
		tinfl_init(&g_inflator);

		uint8_t* out_base = out.data();
		size_t in_offset = offset;
		size_t out_offset = 0;

		while (out_offset < expected)
		{
			size_t in_bytes = payload.size() - in_offset;
			size_t out_bytes = expected - out_offset;

			int flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;

			if (in_offset < payload.size())
				flags |= TINFL_FLAG_HAS_MORE_INPUT;

			tinfl_status status = tinfl_decompress(
				&g_inflator,
				payload.data() + in_offset, &in_bytes,
				out_base, out_base + out_offset, &out_bytes,
				flags
				);

			in_offset += in_bytes;
			out_offset += out_bytes;

			if (status == TINFL_STATUS_DONE)
				break;

			if (status < 0)
			{
				// Image payloads are not always terminated properly; what matters is
				// that the expected amount of pixels came out
				if (out_offset >= expected)
					break;

				char line[256];
				std::snprintf(
					line, sizeof(line),
					"inflate failed: status=%d payload=%zu expected=%zu in=%zu out=%zu",
					static_cast<int>(status), payload.size(), expected, in_offset, out_offset
					);
				report_canvas_error(line);
				return {};
			}

			if (in_bytes == 0 && out_bytes == 0)
				break;
		}

		if (out_offset == 0)
			return {};

		return out;
	}

	std::vector<uint8_t> img_canvas_pixels(img_prop const& prop)
	{
		if (prop.width == 0 || prop.height == 0)
			return {};

		size_t pixels = static_cast<size_t>(prop.width) * prop.height;

		// A package stores its canvases as one LZ4 block of BGRA pixels, the loose
		// folder as zlib compressed pixels in one of several formats
		if (prop.codec == IMG_CODEC_LZ4)
		{
			if (!prop.blob_ptr || prop.blob_size <= 4)
				return {};

			std::vector<uint8_t> out(pixels * 4);
			int read = LZ4_decompress_fast(
				reinterpret_cast<char const*>(prop.blob_ptr) + 4,
				reinterpret_cast<char*>(out.data()),
				static_cast<int>(out.size())
				);

			if (read < 0)
			{
				report_canvas_error("Could not decompress an nx canvas");
				return {};
			}

			return out;
		}

		if (prop.payload.size() < 2)
			return {};

		size_t native = native_size(prop.fmt, pixels);

		if (native == 0)
		{
			report_canvas_error("Unsupported canvas format " + std::to_string(prop.fmt) + " in the .img data");
			return {};
		}

		std::vector<uint8_t> decrypted;

		if (!is_zlib_header(prop.payload.data()))
		{
			decrypted = decrypt_chunks(prop.payload);

			if (decrypted.size() < 2 || !is_zlib_header(decrypted.data()))
			{
				report_canvas_error("Could not decrypt a canvas payload");
				return {};
			}
		}

		std::vector<uint8_t> const& source = decrypted.empty() ? prop.payload : decrypted;
		std::vector<uint8_t> raw = img_inflate(source, native, 2);

		if (raw.size() < native)
		{
			report_canvas_error("Truncated canvas payload");
			return {};
		}

		// Everything is stored as BGRA - the atlas is uploaded with GL_BGRA
		std::vector<uint8_t> out(pixels * 4);

		switch (prop.fmt)
		{
		case 2:
			std::memcpy(out.data(), raw.data(), pixels * 4);
			break;
		case 1:
		{
			for (size_t i = 0; i < pixels; i++)
			{
				uint16_t value = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
				out[i * 4 + 0] = expand_nibble(static_cast<uint8_t>(value & 0x000F));
				out[i * 4 + 1] = expand_nibble(static_cast<uint8_t>((value & 0x00F0) >> 4));
				out[i * 4 + 2] = expand_nibble(static_cast<uint8_t>((value & 0x0F00) >> 8));
				out[i * 4 + 3] = expand_nibble(static_cast<uint8_t>((value & 0xF000) >> 12));
			}

			break;
		}
		case 257:
		{
			for (size_t i = 0; i < pixels; i++)
			{
				uint16_t value = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
				out[i * 4 + 0] = expand_5(static_cast<uint8_t>(value & 0x001F));
				out[i * 4 + 1] = expand_5(static_cast<uint8_t>((value & 0x03E0) >> 5));
				out[i * 4 + 2] = expand_5(static_cast<uint8_t>((value & 0x7C00) >> 10));
				out[i * 4 + 3] = (value & 0x8000) ? 0xFF : 0x00;
			}

			break;
		}
		case 513:
		{
			for (size_t i = 0; i < pixels; i++)
			{
				uint16_t value = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
				out[i * 4 + 0] = expand_5(static_cast<uint8_t>(value & 0x001F));
				out[i * 4 + 1] = expand_6(static_cast<uint8_t>((value & 0x07E0) >> 5));
				out[i * 4 + 2] = expand_5(static_cast<uint8_t>((value & 0xF800) >> 11));
				out[i * 4 + 3] = 0xFF;
			}

			break;
		}
		default:
			return {};
		}

		return out;
	}
}

#endif // USE_IMG
