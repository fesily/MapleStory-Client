//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Text.h"

#include "../Constants.h"
#include "../Error.h"

#include "../Util/QuadTree.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef USE_NX
#include <nlnx/bitmap.hpp>
#endif

namespace ms
{
	// Graphics engine which uses OpenGL
	class GraphicsGL : public Singleton<GraphicsGL>
	{
	public:
		GraphicsGL();

		// Initialize all resources
		Error init();
		// Re-initialize after changing screen modes
		void reinit();
		// Establish the state the renderer draws with: the shader program and its
		// uniforms, the vertex attributes, the texture and the blend mode. Whatever
		// else draws (the debug windows do) changes state of its own, so the
		// renderer sets its own up instead of assuming it survived.
		void resetstate();

		// Clear all bitmaps if most of the space is used up
		void clear();

		// Add a bitmap to the available resources
		void addbitmap(const nl::bitmap& bmp);
		// Draw the bitmap with the given parameters
		void draw(const nl::bitmap& bmp, const Rectangle<int16_t>& rect, const Range<int16_t>& vertical, const Range<int16_t>& horizontal, const Color& color, float angle);

		// Create a layout for the text with the parameters specified
		Text::Layout createlayout(const std::string& text, Text::Font font, Text::Alignment alignment, Color::Name color, int16_t maxwidth, bool formatted, int16_t line_adj);
		// Draw a text with the given parameters
		void drawtext(const DrawArgument& args, const Range<int16_t>& vertical, const std::string& text, const Text::Layout& layout, Text::Font font, Color::Name color, Text::Background back);

		// Draw a rectangle filled with the specified color
		void drawrectangle(int16_t x, int16_t y, int16_t width, int16_t height, float red, float green, float blue, float alpha);
		// Fill the screen with the specified color
		void drawscreenfill(float red, float green, float blue, float alpha);

		// Lock the current scene
		void lock();
		// Unlock the scene
		void unlock();

		// Draw the buffer contents with the specified scene opacity
		void flush(float opacity);
		// Clear the buffer contents
		void clearscene();

	private:
		void clearinternal();
		bool addfont(const char* name, const char* cjkname, Text::Font id, FT_UInt width, FT_UInt height);

		struct Offset
		{
			GLshort left;
			GLshort right;
			GLshort top;
			GLshort bottom;

			Offset(GLshort x, GLshort y, GLshort width, GLshort height)
			{
				left = x;
				right = x + width;
				top = y;
				bottom = y + height;
			}

			Offset()
			{
				left = 0;
				right = 0;
				top = 0;
				bottom = 0;
			}
		};

		// Add a bitmap to the available resources
		const Offset& getoffset(const nl::bitmap& bmp);

		// A texture holding a single bitmap which is too large for the atlas; they are
		// tracked per canvas so that they can be deleted again when memory runs short
		struct DirectTexture
		{
			GLuint texture;
			GLshort width;
			GLshort height;
			size_t bytes;
			size_t lastframe;

			DirectTexture(GLuint tex, GLshort w, GLshort h, size_t size, size_t frame)
			{
				texture = tex;
				width = w;
				height = h;
				bytes = size;
				lastframe = frame;
			}
		};

		// Upload a bitmap to a texture of its own, or find the one it is already in
		GLuint gettexture(const nl::bitmap& bmp);
		// Find the texture with the given OpenGL name
		DirectTexture* findtexture(GLuint name);
		// Delete the least recently used textures until they fit the budget again
		void evicttextures();

		struct Leftover
		{
			GLshort left;
			GLshort right;
			GLshort top;
			GLshort bottom;

			Leftover(GLshort x, GLshort y, GLshort width, GLshort height)
			{
				left = x;
				right = x + width;
				top = y;
				bottom = y + height;
			}

			Leftover()
			{
				left = 0;
				right = 0;
				top = 0;
				bottom = 0;
			}

			GLshort width() const
			{
				return right - left;
			}

			GLshort height() const
			{
				return bottom - top;
			}
		};

		struct Quad
		{
			struct Vertex
			{
				// Local Space Position
				GLshort localcoord_x;
				GLshort localcoord_y;

				// Texture Coordinates
				GLshort texcoord_x;
				GLshort texcoord_y;

				Color color;
			};

			static const size_t LENGTH = 4;
			Vertex vertices[LENGTH];

			// The texture the quad is drawn from, 0 is the atlas. It stays out of the
			// vertex layout: quads sharing one texture are drawn together by flush()
			GLuint texture;

			Quad(GLshort left, GLshort right, GLshort top, GLshort bottom, const Offset& offset, const Color& color, GLfloat rotation, GLuint tex = 0)
			{
				vertices[0] = { left, top, offset.left, offset.top, color };
				vertices[1] = { left, bottom, offset.left, offset.bottom, color };
				vertices[2] = { right, bottom, offset.right, offset.bottom, color };
				vertices[3] = { right, top, offset.right, offset.top, color };

				texture = tex;

				if (rotation != 0.0f)
				{
					GLfloat cos = std::cos(rotation);
					GLfloat sin = std::sin(rotation);
					GLshort center_x = (left + right) / 2;
					GLshort center_y = (top + bottom) / 2;

					for (size_t i = 0; i < LENGTH; i++)
					{
						GLshort vertice_x = vertices[i].localcoord_x - center_x;
						GLshort vertice_y = vertices[i].localcoord_y - center_y;
						GLfloat rounded_x = std::roundf(vertice_x * cos - vertice_y * sin);
						GLfloat rounded_y = std::roundf(vertice_x * sin + vertice_y * cos);
						vertices[i].localcoord_x = static_cast<GLshort>(rounded_x + center_x);
						vertices[i].localcoord_y = static_cast<GLshort>(rounded_y + center_y);
					}
				}
			}
		};

		struct Font
		{
			struct Char
			{
				GLshort ax;
				GLshort ay;
				GLshort bw;
				GLshort bh;
				GLshort bl;
				GLshort bt;
				Offset offset;
			};

			GLshort width;
			GLshort height;
			// Text origin offset and line pitch. Frozen per font instead of derived
			// from the loaded face: a face with taller ink would otherwise move the
			// origin and the pitch of every text (see FONT_LINESPACES).
			GLshort linespace_;
			// Glyphs are loaded from the font on first use and cached per Unicode
			// codepoint (see GraphicsGL::getchar).
			std::unordered_map<uint32_t, Char> chars;

			Font(GLshort w, GLshort h, GLshort ls)
			{
				width = w;
				height = h;
				linespace_ = ls;
			}

			Font()
			{
				width = 0;
				height = 0;
				linespace_ = 0;
			}

			int16_t linespace() const
			{
				return linespace_;
			}
		};

	public:
		// Look up the glyph of a codepoint. Glyphs the font provides are loaded into
		// the atlas on first use, codepoints the font does not provide return a blank
		// glyph. Callers which lay a text out themselves (FormatText) measure with
		// this, the layout builder reads it from the same place.
		const Font::Char& getchar(Text::Font id, uint32_t codepoint);
		// The line pitch of a font (the frozen table the layout builder uses as well)
		int16_t linespace(Text::Font font) const;

	private:
		class LayoutBuilder
		{
		public:
			LayoutBuilder(GraphicsGL& graphics, Text::Font id, Text::Alignment alignment, Color::Name color, int16_t maxwidth, bool formatted, int16_t line_adj);

			size_t add(const std::string& text, size_t prev, size_t first, size_t last);
			Text::Layout finish(size_t first, size_t last);

		private:
			void add_word(size_t first, size_t last, Text::Font font, Color::Name color);
			void add_line();

			GraphicsGL& graphics;
			const Font& font;

			Text::Font fontid;
			Text::Font baseid;
			Text::Alignment alignment;
			Color::Name color;
			int16_t maxwidth;
			bool formatted;

			int16_t ax;
			int16_t ay;

			std::vector<Text::Layout::Line> lines;
			std::vector<Text::Layout::Word> words;
			std::vector<int16_t> advances;
			int16_t width;
			int16_t endy;
			int16_t line_adj;
		};

		int16_t VWIDTH;
		int16_t VHEIGHT;
		Rectangle<int16_t> SCREEN;

		static const GLshort ATLASW = 8192;
		static const GLshort ATLASH = 8192;
		static const GLshort MINLOSIZE = 32;
		// Rows of the atlas reserved for glyphs which are loaded on demand
		static const GLshort GLYPHBANDHEIGHT = 1024;

		// A bitmap which is larger than this in either dimension is not packed into the
		// atlas: a canvas of that size takes up a large part of it, and every rebuild of
		// the atlas would upload it again
		static const GLshort DIRECTMAXSIZE = 256;

		// Budget of the textures holding those bitmaps, in bytes and in number; the ones
		// which have not been drawn for the longest time are deleted once it is exceeded
		static const size_t DIRECTMAXBYTES = 192 * 1024 * 1024;
		static const size_t DIRECTMAXCOUNT = 160;

		// Number of frames between two statistics logs
		static const size_t STATSINTERVAL = 300;

		bool locked;

		std::vector<Quad> quads;

		// The vertices of the quads, packed for the buffer: a quad carries its texture
		// next to its vertices, which the attribute pointers (stride of one vertex)
		// would not skip, so the quads are copied out without it when they are drawn
		std::vector<Quad::Vertex> vertexdata;
		GLuint VBO;
		GLuint atlas;

		GLint shaderProgram;
		GLint attribute_coord;
		GLint attribute_color;
		GLint uniform_texture;
		GLint uniform_atlassize;
		GLint uniform_screensize;
		GLint uniform_yoffset;
		GLint uniform_fontregion;

		std::unordered_map<size_t, Offset> offsets;
		Offset nulloffset;

		std::map<size_t, DirectTexture> directtextures;
		size_t directbytes;

		size_t framecount;
		size_t atlasuploads;
		size_t directuploads;
		size_t directevictions;

		QuadTree<size_t, Leftover> leftovers;
		size_t rlid;
		size_t wasted;
		Point<GLshort> border;
		Range<GLshort> yrange;

		FT_Library ftlibrary;
		FT_Face faces[Text::Font::NUM_FONTS];
		// Faces for the characters the main fonts do not provide, one per font size
		FT_Face cjkfaces[Text::Font::NUM_FONTS];
		Font fonts[Text::Font::NUM_FONTS];
		Point<GLshort> fontborder;
		GLshort fontymax;

		// Area of the atlas below the fixed ASCII glyph strips which is used for
		// glyphs loaded on demand, and the cursor into it.
		Point<GLshort> glyphborder;
		GLshort glyphrowheight;
		GLshort glyphbandbottom;
		bool glyphspacefull;
	};
}