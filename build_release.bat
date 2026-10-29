@echo off
setlocal EnableDelayedExpansion

rem ============================================================
rem  MvTools2 release build (MvTools2, DePan, DePanEstimate)
rem
rem  Builds the release variants and assembles them, with the
rem  documentation, into a release folder and a 7-Zip archive:
rem    Win32\ClangCL, x64\ClangCL   Rel_Clang, Visual Studio 2026 (mvtools.slnx)
rem    Win32\MSVC_XP, x64\MSVC_XP   Release_v141_xp, Visual Studio 2022 (mvtools.sln):
rem                                 VS 2026 has no v141_xp toolset
rem    ARM64\llvm-mingw             Windows on ARM64, cross-compiled with llvm-mingw
rem                                 (build_arm64_mingw.bat, needs Ninja)
rem
rem  Switches, 1 = build (default), 0 = skip; set them before the run,
rem  e.g. "set BUILD_XP=0 && build_release.bat":
rem    BUILD_CLANG  BUILD_XP  BUILD_ARM64
rem  A missing VS 2022 or llvm-mingw skips that variant with a warning,
rem  unless its switch was set explicitly.
rem
rem  Detected, can be overridden from the environment:
rem    VER         FileVersion of Sources\mvtools.rc
rem    BUILD_DATE  today, YYYYMMDD
rem  Output: %DEST_ROOT%\<VER>\ and %DEST_ROOT%\mvtools-<VER>-with-depans<BUILD_DATE>.7z
rem          (DEST_ROOT default: build\release)
rem
rem  The Visual Studio builds are Rebuilds into the usual project output
rem  folders (<project>\<platform>\<configuration>), the files are copied
rem  from there.
rem ============================================================

set "SRC=%~dp0"
set "SRC=%SRC:~0,-1%"
if not defined DEST_ROOT set "DEST_ROOT=%SRC%\build\release"
if not defined LLVM_MINGW_ROOT set "LLVM_MINGW_ROOT=C:\avsplus_build_deps\llvm-mingw-20261020-ucrt-x86_64"
set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"

set "_OVR_XP=%BUILD_XP%"
set "_OVR_ARM64=%BUILD_ARM64%"
if not defined BUILD_CLANG set "BUILD_CLANG=1"
if not defined BUILD_XP    set "BUILD_XP=1"
if not defined BUILD_ARM64 set "BUILD_ARM64=1"

rem ============================================================
rem  DETECTION
rem ============================================================

rem  Version: the line VALUE "FileVersion", "2.7.48" of mvtools.rc
if not defined VER (
  for /f tokens^=4^ delims^=^" %%A in ('findstr /c:"FileVersion" "%SRC%\Sources\mvtools.rc"') do set "VER=%%A"
)
if not defined VER (
  echo ERROR: no FileVersion found in Sources\mvtools.rc
  goto :fatal_exit
)
if not defined BUILD_DATE (
  for /f %%D in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "BUILD_DATE=%%D"
)

rem  MSBuild of VS 2026 (18.x) and VS 2022 (17.x); for VS 2022 the first
rem  installation with the v141_xp toolset and the NASM integration in the
rem  v150 BuildCustomizations folder (the one v141_xp uses, see README.md).
rem  vswhere writes into a temp file: its version range argument has a comma
rem  and a parenthesis, which a for /f command line does not take unescaped.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: vswhere.exe not found: !VSWHERE!
  goto :fatal_exit
)
set "VSW_TMP=%TEMP%\mvtools_build_release_vswhere.txt"
set "MSBUILD_2026="
set "MSBUILD_2022="
"%VSWHERE%" -version [18.0,19.0) -products * -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe > "%VSW_TMP%" 2>nul
set /p MSBUILD_2026=<"%VSW_TMP%"
"%VSWHERE%" -version [17.0,18.0) -products * -requires Microsoft.VisualStudio.Component.WinXP -property installationPath > "%VSW_TMP%" 2>nul
for /f "usebackq delims=" %%I in ("%VSW_TMP%") do (
  if not defined MSBUILD_2022 if exist "%%I\MSBuild\Microsoft\VC\v150\BuildCustomizations\nasm.targets" set "MSBUILD_2022=%%I\MSBuild\Current\Bin\MSBuild.exe"
)
del "%VSW_TMP%" 2>nul

if "%BUILD_CLANG%"=="1" if not defined MSBUILD_2026 (
  echo ERROR: Visual Studio 2026 MSBuild not found, needed for the ClangCL builds ^(BUILD_CLANG=0 skips them^)
  goto :fatal_exit
)
if "%BUILD_XP%"=="1" if not defined MSBUILD_2022 (
  if defined _OVR_XP (
    echo ERROR: BUILD_XP was set, but no Visual Studio 2022 with v141_xp and NASM was found
    goto :fatal_exit
  )
  echo WARNING: no Visual Studio 2022 with v141_xp and NASM found, skipping the MSVC_XP builds
  set "BUILD_XP=0"
)
if "%BUILD_ARM64%"=="1" if not exist "%LLVM_MINGW_ROOT%\bin\aarch64-w64-mingw32-clang++.exe" (
  if defined _OVR_ARM64 (
    echo ERROR: BUILD_ARM64 was set, but llvm-mingw was not found at: !LLVM_MINGW_ROOT!
    goto :fatal_exit
  )
  echo WARNING: llvm-mingw not found at !LLVM_MINGW_ROOT!, skipping the ARM64 build
  set "BUILD_ARM64=0"
)

set "DEST=%DEST_ROOT%\%VER%"
set "ARCHIVE=%DEST_ROOT%\mvtools-%VER%-with-depans%BUILD_DATE%.7z"

set TOTAL_BUILDS=0
if "%BUILD_CLANG%"=="1" set /a TOTAL_BUILDS+=2
if "%BUILD_XP%"=="1"    set /a TOTAL_BUILDS+=2
if "%BUILD_ARM64%"=="1" set /a TOTAL_BUILDS+=1
set BUILDNUM=0

echo.
echo ========================================================
echo  MvTools2 release : %VER%  (%BUILD_DATE%)
echo  Destination      : %DEST%
echo  Archive          : %ARCHIVE%
echo  ClangCL=%BUILD_CLANG%  MSVC_XP=%BUILD_XP%  ARM64=%BUILD_ARM64%
echo  VS 2026 MSBuild  : !MSBUILD_2026!
echo  VS 2022 MSBuild  : !MSBUILD_2022!
echo ========================================================
echo.

if exist "%DEST%" (
  echo WARNING: the destination exists and will be deleted: !DEST!
  echo          Press Ctrl+C to abort, any key to continue.
  pause > nul
  rmdir /s /q "%DEST%"
)
mkdir "%DEST%" || goto :fatal_exit

rem ============================================================
rem  BUILDS
rem ============================================================

if not "%BUILD_CLANG%"=="1" goto :skip_clang
call :vs_build "%MSBUILD_2026%" mvtools.slnx Rel_Clang Win32 Win32\ClangCL || goto :fatal_exit
call :vs_build "%MSBUILD_2026%" mvtools.slnx Rel_Clang x64 x64\ClangCL || goto :fatal_exit
:skip_clang

if not "%BUILD_XP%"=="1" goto :skip_xp
call :vs_build "%MSBUILD_2022%" mvtools.sln Release_v141_xp Win32 Win32\MSVC_XP || goto :fatal_exit
call :vs_build "%MSBUILD_2022%" mvtools.sln Release_v141_xp x64 x64\MSVC_XP || goto :fatal_exit
:skip_xp

if not "%BUILD_ARM64%"=="1" goto :skip_arm64
set /a BUILDNUM+=1
echo.
echo [BUILD %BUILDNUM%/%TOTAL_BUILDS%] ARM64 llvm-mingw
call "%SRC%\build_arm64_mingw.bat" || goto :fatal_exit
set "A64_OUT=%SRC%\out\ARM64\llvm-mingw"
mkdir "%DEST%\ARM64\llvm-mingw"
call :copy_file "%A64_OUT%\MvTools2.dll" "%DEST%\ARM64\llvm-mingw\mvtools2.dll" || goto :fatal_exit
call :copy_file "%A64_OUT%\DePan.dll" "%DEST%\ARM64\llvm-mingw\DePan.dll" || goto :fatal_exit
call :copy_file "%A64_OUT%\DePanEstimate.dll" "%DEST%\ARM64\llvm-mingw\DePanEstimate.dll" || goto :fatal_exit
:skip_arm64

rem ============================================================
rem  DOCUMENTATION
rem ============================================================
echo.
echo [DOCS] Copying documentation...
for %%F in (CHANGELOG.md README.md README_depans.txt distrib\Note.txt) do (
  call :copy_file "%SRC%\%%F" "%DEST%\%%~nxF" || goto :fatal_exit
)
for %%D in (Documentation Doc_Depan Doc_DepanEstimate) do (
  xcopy /e /i /y /q "%SRC%\%%D" "%DEST%\%%D" > nul || goto :fatal_exit
)

rem ============================================================
rem  ARCHIVE (the content of the release folder, without the folder itself)
rem ============================================================
echo.
if not exist "%SEVENZIP%" (
  echo WARNING: 7-Zip not found at !SEVENZIP!, no archive created
  goto :done
)
echo [ARCHIVE] %ARCHIVE%
if exist "%ARCHIVE%" del "%ARCHIVE%"
"%SEVENZIP%" a -mx=9 "%ARCHIVE%" "%DEST%\*" > nul
if errorlevel 1 (
  echo ERROR: 7-Zip failed
  goto :fatal_exit
)

:done
echo.
echo ========================================================
echo  RELEASE BUILD COMPLETE: %DEST%
echo ========================================================
dir /s /b "%DEST%\*.dll"
endlocal
exit /b 0

rem  "exit /b 1" inside a parenthesised block does not reliably set the exit
rem  code, so the error paths jump here.
:fatal_exit
echo.
echo RELEASE BUILD FAILED
endlocal
exit /b 1


rem ------------------------------------------------------------
rem  :vs_build <msbuild exe> <solution> <configuration> <platform> <dest subdir>
rem  Rebuilds the solution, then copies the three DLLs.
rem ------------------------------------------------------------
:vs_build
set /a BUILDNUM+=1
echo.
echo [BUILD %BUILDNUM%/%TOTAL_BUILDS%] %~3 %~4
"%~1" "%SRC%\%~2" -t:Rebuild -p:Configuration=%~3 -p:Platform=%~4 -m -nologo -v:minimal -nodeReuse:false
if errorlevel 1 (
  echo ERROR: build failed: %~3 %~4
  exit /b 1
)
mkdir "%DEST%\%~5"
call :copy_file "%SRC%\Sources\%~4\%~3\mvtools2.dll" "%DEST%\%~5\mvtools2.dll" || exit /b 1
call :copy_file "%SRC%\DePan\%~4\%~3\DePan.dll" "%DEST%\%~5\DePan.dll" || exit /b 1
call :copy_file "%SRC%\DePanEstimate\%~4\%~3\DePanEstimate.dll" "%DEST%\%~5\DePanEstimate.dll" || exit /b 1
exit /b 0

rem ------------------------------------------------------------
rem  :copy_file <source> <destination file>
rem ------------------------------------------------------------
:copy_file
if not exist "%~1" (
  echo ERROR: missing: %~1
  exit /b 1
)
copy /y "%~1" "%~2" > nul || exit /b 1
for %%F in ("%~1") do echo   %%~tF  %%~nxF -^> %~2
exit /b 0
