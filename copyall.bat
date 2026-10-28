@echo off
rem Collects the release DLLs (MvTools2, DePan, DePanEstimate) into out\<platform>\<toolset>\
rem
rem Before running: batch build these configurations from the IDE (Build / Batch Build):
rem   Visual Studio 2026, mvtools.slnx:  Release, Rel_Clang (Win32, x64), IntelCpp (x64)
rem   Visual Studio 2022, mvtools.sln:   Release_v141_xp (Win32, x64) - VS 2026 has no v141_xp toolset
rem Windows on ARM64: build_arm64_mingw.bat writes out\ARM64\llvm-mingw itself;
rem   "copyall.bat arm64" runs it at the end (needs llvm-mingw and Ninja, see the batch file).
rem
rem Each copied file is listed with its date: check that none is left over from an older build.
setlocal
cd /d "%~dp0"
set MISSING=0

for %%P in (Sources\mvtools2 DePan\DePan DePanEstimate\DePanEstimate) do (
  for /f "tokens=1,2 delims=\" %%a in ("%%P") do (
    call :copy %%a x64 Release %%b x64\MSVC
    call :copy %%a x64 Release_v141_xp %%b x64\MSVC_XP
    call :copy %%a x64 Rel_Clang %%b x64\ClangCL
    call :copy %%a x64 IntelCpp %%b x64\IntelCpp
    call :copy %%a Win32 Release %%b Win32\MSVC
    call :copy %%a Win32 Release_v141_xp %%b Win32\MSVC_XP
    call :copy %%a Win32 Rel_Clang %%b Win32\ClangCL
  )
)

if /i "%~1"=="arm64" (
  echo.
  echo Windows on ARM64 build...
  call build_arm64_mingw.bat || set /a MISSING+=1
)

echo.
if %MISSING%==0 (echo All files copied.) else (echo WARNING: %MISSING% file^(s^) missing or failed, see above.)
endlocal & exit /b %MISSING%

rem :copy <project dir> <platform> <configuration> <dll name> <out subdir>
:copy
set "SRC=%~1\%~2\%~3\%~4.dll"
if not exist "%SRC%" (
  echo   MISSING  %SRC%
  set /a MISSING+=1
  goto :eof
)
if not exist "out\%~5" mkdir "out\%~5"
copy /y "%SRC%" "out\%~5\" >nul || (set /a MISSING+=1 & goto :eof)
for %%F in ("%SRC%") do echo   %%~tF  %SRC% -^> out\%~5
goto :eof
