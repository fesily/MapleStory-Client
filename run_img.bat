@echo off
rem ---------------------------------------------------------------------------
rem Runs the client against a loose .img data folder (USE_IMG build).
rem
rem   run_img.bat [data folder]
rem
rem Default data folder is the BeiDou client data. The script assembles a run
rem directory (exe + dlls + fonts + Icon.png) and starts the client from there,
rem so no files have to be copied around by hand.
rem
rem The client takes its configuration from the environment, so no Settings file
rem is written any more: every setting can be passed as MAPLESTORY_<SETTING>,
rem for example MAPLESTORY_UILAYOUT=classic or MAPLESTORY_SERVERIP=10.0.0.1.
rem The script only fills in the data folder (first parameter) and the defaults
rem below; the short names WIDTH, HEIGHT and UILAYOUT still work.
rem ---------------------------------------------------------------------------
setlocal EnableExtensions
set "REPO=%~dp0"
set "RUN=%REPO%run_img"
set "BUILD=%REPO%x64\Debug"

rem The first parameter wins over the default data folder. For the other
rem settings, a name already set as MAPLESTORY_* in the environment wins over
rem the short name, exactly like the environment wins over the settings file.
if not "%~1"=="" set "MAPLESTORY_DATAPATH=%~1"
if not defined MAPLESTORY_DATAPATH set "MAPLESTORY_DATAPATH=%REPO%data"

if not defined MAPLESTORY_WIDTH if defined WIDTH set "MAPLESTORY_WIDTH=%WIDTH%"
if not defined MAPLESTORY_WIDTH set "MAPLESTORY_WIDTH=1366"

if not defined MAPLESTORY_HEIGHT if defined HEIGHT set "MAPLESTORY_HEIGHT=%HEIGHT%"
if not defined MAPLESTORY_HEIGHT set "MAPLESTORY_HEIGHT=768"

if not defined MAPLESTORY_UILAYOUT if defined UILAYOUT set "MAPLESTORY_UILAYOUT=%UILAYOUT%"

if not exist "%BUILD%\MapleStory.exe" (
	echo [ERROR] Build the client first:
	echo         msbuild MapleStory.vcxproj -p:Configuration=Debug -p:Platform=x64
	exit /b 1
)

if not exist "%MAPLESTORY_DATAPATH%\" (
	echo [ERROR] .img data folder not found: %MAPLESTORY_DATAPATH%
	echo         usage: run_img.bat "C:\path\to\data"
	exit /b 1
)

if not exist "%RUN%\" mkdir "%RUN%"

copy /y "%BUILD%\MapleStory.exe" "%RUN%" >nul
if exist "%BUILD%\*.dll" copy /y "%BUILD%\*.dll" "%RUN%" >nul
copy /y "%REPO%Icon.png" "%RUN%" >nul

if not exist "%RUN%\fonts\" (
	echo Copying fonts...
	xcopy /y /e /i /q "%REPO%fonts" "%RUN%\fonts" >nul
)

rem Left over from when the script configured the client through the file
if exist "%RUN%\Settings" del "%RUN%\Settings"

echo Data folder: %MAPLESTORY_DATAPATH%
echo Resolution:  %MAPLESTORY_WIDTH%x%MAPLESTORY_HEIGHT%
if defined MAPLESTORY_UILAYOUT echo UI layout:   %MAPLESTORY_UILAYOUT%

pushd "%RUN%"
MapleStory.exe
popd
