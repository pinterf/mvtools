@echo off
rem Windows on ARM64 (aarch64) build of MvTools2, DePan and DePanEstimate:
rem cross-compiled on x64 Windows with llvm-mingw (UCRT flavour), CMake and Ninja.
rem Output: out\ARM64\llvm-mingw\ (next to the copyall.bat folders of the MSVC builds)
rem
rem   LLVM_MINGW_ROOT  llvm-mingw installation (https://github.com/mstorsjo/llvm-mingw/releases)
rem   BUILD_DIR        build directory (default: build\arm64_mingw, removed first)
setlocal
if not defined LLVM_MINGW_ROOT set "LLVM_MINGW_ROOT=C:\avsplus_build_deps\llvm-mingw-20261020-ucrt-x86_64"
if not defined BUILD_DIR set "BUILD_DIR=%~dp0build\arm64_mingw"
set "OUT=%~dp0out\ARM64\llvm-mingw"

if not exist "%LLVM_MINGW_ROOT%\bin\aarch64-w64-mingw32-clang++.exe" (
  echo ERROR: llvm-mingw not found at "%LLVM_MINGW_ROOT%", set LLVM_MINGW_ROOT
  exit /b 1
)
where ninja >nul 2>nul || ( echo ERROR: ninja.exe not found on PATH ^(e.g. pip install ninja^) & exit /b 1 )

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%~dp0cmake\toolchains\llvm-mingw-aarch64.cmake" ^
  -DLLVM_MINGW_ROOT="%LLVM_MINGW_ROOT%"
if errorlevel 1 ( echo ERROR: CMake configure failed & exit /b 1 )
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 ( echo ERROR: build failed & exit /b 1 )

if not exist "%OUT%" mkdir "%OUT%"
copy /y "%BUILD_DIR%\Sources\MvTools2.dll" "%OUT%\" >nul
copy /y "%BUILD_DIR%\DePan\DePan.dll" "%OUT%\" >nul
copy /y "%BUILD_DIR%\DePanEstimate\DePanEstimate.dll" "%OUT%\" >nul
echo Done: %OUT%
rem DCT modes and DePanEstimate need an ARM64 libfftw3f-3.dll at runtime, it is not part of this build
endlocal
