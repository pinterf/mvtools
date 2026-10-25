# mvtools
Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family.\
Supporting Y, YUV 4:2:0, 4:2:2, 4:4:4 (also 4:1:0, 4:1:1, 4:4:0, with or without alpha) at native 8, 10, 12, 14 and 16 bit depths,
32 bit float and planar RGB in selected filters, YUY2 (converted to YV16 internally).\
Still supporting Windows XP (v141_xp build).\
Windows x86, x64 and ARM64 versions.\
From December 20, 2020: Linux port; since 2.7.48 also Linux aarch64 (ARM64) with NEON optimizations.

Files: mvtools2.dll, and the DePan plugins depan.dll, depanestimate.dll (see README_depans.txt)

## Credits: 

- Manao, Fizick, Tsp, TSchniede, SEt, Vit, Firesledge, cretindesalpes 

## Links
- Doom9 forum: https://forum.doom9.org/showthread.php?t=173356
- Avisynth wiki: http://avisynth.nl/index.php/MVTools
- Project host: https://github.com/pinterf/mvtools under mvtools-pfmod branch
- http://avisynth.nl/index.php/External_filters#64-bit_filters 
- http://avisynth.org.ru/mvtools/mvtools2.html

For more information see also documents folder.

Note:
For a more experimental feature set (new parameters, use hw devices for motion estimation, but Windows only)
have a look at DTL's fork: https://github.com/DTL2020/mvtools

## External dependencies: 

- fftw3 single precision library, used only at specific dct parameter values (MAnalyse, MRecalculate) and by DePanEstimate
  - Windows: libfftw3f-3.dll (or fftw3.dll) from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229.
    Windows on ARM64 needs an ARM64 build of the dll, e.g. from the MSYS2 package mingw-w64-clang-aarch64-fftw (not tested).
  - Linux: libfftw3f_threads.so.3, package libfftw3-single3 (deb) or fftw-devel (rpm), x86-64 and aarch64 alike.
  
- MSVC builds may require Microsoft Visual C++ Redistributables https://www.visualstudio.com/downloads/
  (the Windows ARM64 llvm-mingw build links its C++ runtime statically, it needs nothing else).
  

Others
Modification base:

- mvtools2, 2.6.0.5 64 bit version from
http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Source code:

## Build Instructions

Note:

The Windows x86/x64 MSVC and ClangCL builds use external (nasm) assembler source - where there exists.

Other builds use internal SIMD code: x86 intrinsics (MinGW, Linux x86-64; governed by defines in def.h),
NEON intrinsics on aarch64 (Windows ARM64, Linux aarch64). Every SIMD path has a C equivalent with the same output.

### Windows MSVC

- Visual Studio 2026: open `mvtools.slnx` (Release: v143, Rel_Clang: ClangCL, IntelCpp: Intel C++ Compiler 2025 ICX).
- Windows XP build (Release_v141_xp): needs **Visual Studio 2022** with the
  "MSVC v141 - VS 2017 C++ build tools" component; VS 2026 no longer offers the v141_xp toolset.
  Open `mvtools.sln` in VS 2022 for that configuration.

- Prerequisite: for asm compilation use nasm https://www.nasm.us/ 
  Visual Studio integration: https://github.com/ShiftMediaProject/VSNASM/releases

  Compiler nasm.exe can appear in e.g. `C:\Program Files\Microsoft Visual Studio\18\Community\VC\`

  You can check the files nasm.targets, nasm.props and nasm.xml in e.g. `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Microsoft\VC\v170\BuildCustomizations\`

  For the XP compatible (v141_xp) build, copy them to the v150 BuildCustomizations folder of the VS 2022 installation:
  `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Microsoft\VC\v150\BuildCustomizations\`

  **Important for the v150 copy:** recent VSNASM files use task parameters
  (`TrackFileAccess` on `GetOutOfDateItems`, and `AcceptableNonZeroExitCodes`, `StdOutEncoding`,
  `StdErrEncoding`, `UseMsbuildResourceManager` on `ParallelCustomBuild`/`CustomBuild`) that are
  not supported by the older `Microsoft.Build.CppTasks.Common.dll` (v15.0) of the v150
  toolset folder. The v170 copy is fine as-is. In the **v150 copy only**, remove those attributes
  from `nasm.targets`:
  - In both `<GetOutOfDateItems>` calls: remove `TrackFileAccess="$(TrackFileAccess)"`
  - In `<ParallelCustomBuild>`: remove `AcceptableNonZeroExitCodes`, `StdOutEncoding`, `StdErrEncoding`, `UseMsbuildResourceManager`
  - In `<CustomBuild>`: remove `AcceptableNonZeroExitCodes`, `StdOutEncoding`, `StdErrEncoding`

  Some nasm versions (e.g. the one valid around 2022.Dec.) will throw a lot of warnings on x265 assembler files. Temporarily they are silenced with -w-macro-params-legacy parameter.

* build from IDE

Note: CMake is not used for the MSVC builds (CMakeLists.txt refers to the solution); the sources also
compile with MSVC for ARM64, but the solution has no ARM64 configuration (use the llvm-mingw build below).

### Windows Visual Studio + Intel C++ Compiler ICX

- Prerequisite: Intel C++ Compiler 2025 (ICX) + Visual Studio integration

* build from IDE, configuration IntelCpp

### Windows GCC
(mingw installed by msys2, x86/x64)
From the 'build' folder under project root:

    del ..\CMakeCache.txt
    cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=on
    @rem test: cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=off
    cmake --build . --config Release  

### Windows on ARM64, cross-compiled on x64 Windows (llvm-mingw)

No ARM64 hardware is needed. Prerequisites:
- llvm-mingw, UCRT flavour (`llvm-mingw-<tag>-ucrt-x86_64.zip`) from https://github.com/mstorsjo/llvm-mingw/releases
- CMake and Ninja (e.g. `pip install ninja`) on PATH

From the project root:

    set LLVM_MINGW_ROOT=C:\path\to\llvm-mingw-<tag>-ucrt-x86_64
    build_arm64_mingw.bat

The DLLs (MvTools2, DePan, DePanEstimate) are copied to `out\ARM64\llvm-mingw`; the build directory is
`build\arm64_mingw`. The toolchain file is `cmake/toolchains/llvm-mingw-aarch64.cmake`, it can be used with
plain cmake as well (`-G Ninja -DCMAKE_TOOLCHAIN_FILE=... -DLLVM_MINGW_ROOT=...`).

### Windows on ARM64, built on an ARM64 Windows machine (MSYS2 CLANGARM64)

Not tested. The MSYS2 CLANGARM64 environment runs on Windows on ARM64 only (its tools are ARM64 programs).
In a CLANGARM64 shell:

    pacman -S mingw-w64-clang-aarch64-clang mingw-w64-clang-aarch64-cmake mingw-w64-clang-aarch64-ninja
    cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

The NEON code paths are enabled automatically for the aarch64 target. The DLLs depend on the libc++
runtime of the environment unless linked statically (add `-DCMAKE_SHARED_LINKER_FLAGS=-static` to the first cmake line).

## Linux build instructions

x86-64 and aarch64 (ARM64) alike; on aarch64 the NEON code paths are enabled automatically
(ENABLE_NEON_SIMD), on x86-64 the SSE/AVX ones (ENABLE_INTEL_SIMD). GCC and Clang
(aarch64 tested with GCC 11, 12 and Clang 15).

* Clone repo

        git clone https://github.com/pinterf/mvtools
        cd mvtools
        cmake -B build -S .
        cmake --build build

  Useful hints:        
  build after clean:

      cmake --build build --clean-first

  delete CMake cache

      rm build/CMakeCache.txt

  a C-only build (for testing):

      cmake -B build -S . -DENABLE_INTEL_SIMD=OFF -DENABLE_NEON_SIMD=OFF

* Find binaries at

        build/Sources/libmvtools2.so
        build/DePan/libdepan.so
        build/DePanEstimate/libdepanestimate.so

* Install binaries

        cd build
        sudo make install
