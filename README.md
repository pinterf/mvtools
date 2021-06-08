# mvtools
Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family.\
Supporting YUY2, 4:2:0, 4:2:2, 4:4:4 at native 8, 10, 12, 14 and 16 bit depths, 32bit float in selected filters.\
Still supporting Windows XP.
x86 and x64 versions
From December 20, 2020: Linux port

File: mvtools2.dll

## Credits: 

- Manao, Fizick, Tsp, TSchniede, SEt, Vit, Firesledge, cretindesalpes 

## Links

- http://avisynth.org.ru/mvtools/mvtools2.html
- http://avisynth.nl/index.php/External_filters#64-bit_filters 
- Avisynth wiki: http://avisynth.nl/index.php/MVTools
- Project host: https://github.com/pinterf/mvtools under mvtools-pfmod branch

For more information see also documents folder.

## External dependencies: 

- libfftw3f-3.dll (or renamed to FFT3W.DLL)
from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  It is used only at specific dct parameter values
  
- May require Microsoft Visual C++ Redistributables https://www.visualstudio.com/downloads/
  

Others
Modification base:

- mvtools2, 2.6.0.5 64 bit version from
http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Source code:

## Build Instructrions

Note:

Windows MSVC builds are using external assembler source - if there exists.

Other builds are using internal SIMD code, governed by defines in def.h

### Windows MSVC

- Prequisite: for asm compilation use nasm https://www.nasm.us/ 
  Visual Studio integration: https://github.com/ShiftMediaProject/VSNASM/releases

* build from IDE

## Windows GCC
(mingw installed by msys2)
From the 'build' folder under project root:

    del ..\CMakeCache.txt
    cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=on
    @rem test: cmake .. -G "MinGW Makefiles" -DENABLE_INTEL_SIMD:bool=off
    cmake --build . --config Release  

## Linux build instructions

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

* Find binaries at

        build/mvtools/libmvtools2.so
        build/depan/libdepan.so
        build/depanestimate/libdepanestimate.so

* Install binaries

        cd build
        sudo make install
