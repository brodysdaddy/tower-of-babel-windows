@echo off
REM Builds tower.exe. Works from any terminal:
REM   - uses g++ (MinGW/MSYS2) or cl (Visual Studio) if already on PATH,
REM   - otherwise finds Visual Studio itself and sets it up.
setlocal
cd /d "%~dp0"
set OUT=tower.exe
set SRC=src\core\tower_game.cpp src\gfx\framebuffer.cpp src\app\tower_app.cpp src\windows\main_win.cpp src\windows\sound_win.cpp

where g++ >nul 2>nul
if %errorlevel%==0 goto gcc
where cl >nul 2>nul
if %errorlevel%==0 goto msvc

REM Neither on PATH: look for Visual Studio and load its build environment.
set "VSINSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VSINSTALLER%\vswhere.exe" goto nocompiler
pushd "%VSINSTALLER%"
for /f "usebackq delims=" %%i in (`.\vswhere.exe -latest -products * -property installationPath`) do set "VSPATH=%%i"
popd
if not defined VSPATH goto nocompiler
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto nocompiler
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
goto msvc

:gcc
g++ -std=c++17 -Wall -Wextra -O2 %SRC% -o %OUT% -mwindows -lgdi32 -luser32 -lwinmm
goto done

:msvc
cl /nologo /std:c++17 /EHsc /W4 /O2 %SRC% /Fe:%OUT% /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib
goto done

:nocompiler
echo No C++ compiler found. Install Visual Studio (Desktop development with C++) or MSYS2 g++.
exit /b 1

:done
set RESULT=%errorlevel%
del *.obj >nul 2>nul
if not %RESULT%==0 (
    echo BUILD FAILED. If it says "cannot open file", close any running %OUT% and try again.
    exit /b %RESULT%
)
echo Built %OUT% - double-click it or run: tower.exe
