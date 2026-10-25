@echo off
rem Build the Windows version (x64) with Visual Studio 2022 or newer and CMake.
rem   scripts\build_windows.bat [path\to\original\game] [path\to\hd.pack]
rem The game folder must contain Data\data.pack and Data\ru.pack (and ideally farm.wrp.exe
rem for tutorial tips). Result: dist\FarmFrenzy\FarmFrenzy.exe with Data\ next to it.
setlocal
cd /d "%~dp0\.."

set GAME_DIR=%~1
set HD_PACK=%~2
set BUILD=build-windows

cmake -S . -B %BUILD% -A x64 -DFF_VENDORED_SDL=ON "-DFF_GAME_DIR=%GAME_DIR%" "-DFF_HD_PACK=%HD_PACK%" "-DFF_ICON=%FF_ICON%"
if errorlevel 1 exit /b 1
cmake --build %BUILD% --config Release --parallel
if errorlevel 1 exit /b 1

if exist dist\FarmFrenzy rmdir /s /q dist\FarmFrenzy
mkdir dist\FarmFrenzy
copy /y %BUILD%\Release\FarmFrenzy.exe dist\FarmFrenzy\ >nul
if exist %BUILD%\Release\Data xcopy /e /i /y %BUILD%\Release\Data dist\FarmFrenzy\Data >nul
echo Built: dist\FarmFrenzy\FarmFrenzy.exe
