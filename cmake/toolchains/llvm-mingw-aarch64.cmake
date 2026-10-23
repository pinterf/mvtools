# ---------------------------------------------------------------------------
#  CMake toolchain: Windows on ARM64 (aarch64) via llvm-mingw
#
#  llvm-mingw (https://github.com/mstorsjo/llvm-mingw) is an x86_64-hosted
#  toolchain targeting aarch64-w64-mingw32: no ARM64 hardware is needed.
#  The UCRT flavour ("-ucrt-" release) is expected, the CRT of Windows 10/11.
#  libc++ and libunwind are linked statically: the plugin DLLs depend only
#  on system DLLs and the UCRT.
#
#  Usage (see build_arm64_mingw.bat):
#    cmake -S . -B build/arm64_mingw -G Ninja -DCMAKE_BUILD_TYPE=Release ^
#          -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-aarch64.cmake ^
#          -DLLVM_MINGW_ROOT=C:/avsplus_build_deps/llvm-mingw-20261020-ucrt-x86_64
# ---------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT LLVM_MINGW_ROOT AND DEFINED ENV{LLVM_MINGW_ROOT})
  set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}")
endif()
set(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" CACHE PATH "Root directory of the llvm-mingw installation")

# the try_compile projects re-read this file without the cache of the outer project
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW_ROOT)

set(_bin "${LLVM_MINGW_ROOT}/bin")
if(NOT EXISTS "${_bin}/aarch64-w64-mingw32-clang++.exe")
  message(FATAL_ERROR "llvm-mingw not found: set LLVM_MINGW_ROOT (now '${LLVM_MINGW_ROOT}')")
endif()

set(CMAKE_C_COMPILER   "${_bin}/aarch64-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER "${_bin}/aarch64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER  "${_bin}/llvm-rc.exe")
set(CMAKE_AR           "${_bin}/llvm-ar.exe"     CACHE FILEPATH "")
set(CMAKE_RANLIB       "${_bin}/llvm-ranlib.exe" CACHE FILEPATH "")
set(CMAKE_C_COMPILER_TARGET   aarch64-w64-mingw32)
set(CMAKE_CXX_COMPILER_TARGET aarch64-w64-mingw32)

set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
