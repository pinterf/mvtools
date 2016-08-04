# mvtools
Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family. 

File: mvtools2.dll (both 32 and 64 bit Windows DLLs)

Credits: 
- Manao, Fizick, Tsp, TSchniede, SEt, Vit, Firesledge, cretindesalpes 

Modification base:
- mvtools2, 2.6.0.5 64 bit version from
http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Features
- 2.7.0.22c: test build to support YV16 and YV24, 
  from now there is no need for YUY2 planar hack in scripts. Use ConvertToYV16() and work with it.

- Synchronized to Fizick's 2.5.11.22 version
  e.g. fixed: greenish garbage in MFlowInter
- Dropped tclip support in MFlow, MFlowInter (could not resolve with 2.5.11.22)
- 2.6.0.5 x64 capable version ported under Avisynth 2.6 API
- Fixed access violation in MDepan
- Fixed access violation in x64 asm code
- Built with Visual Studio 2015 Community Edition, v140 toolset
- Compiler: Intel C++ (because of inline 64 bit asm code)

Links
- http://avisynth.org.ru/mvtools/mvtools2.html
- http://avisynth.nl/index.php/External_filters#64-bit_filters 
- http://ldesoras.free.fr/prod.html (2.6.0.5)
- http://avisynth.nl/index.php/MVTools

For more information see also documents folder.

Current build as of August 04, 2016: 
- 2.7.0.22c

Previous build  
- 2.7.0.22 (April 29, 2016): ending number 22 hints to Fizick's 2.5.11.22 version
- 2.7.0.1 (March 31, 2016): skipped 2.6.x.x to leave numbering space to the previous authors

External dependencies: 
- FFT3W.DLL (libfftw3f-3.dll)
from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  
- May require Microsoft Visual C++ Redistributable 2015 Update 3
  
Others
- For asm compilation use yasm (1.2): http://yasm.tortall.net/Download.html

hint: vsyasm.* files should be renamed to yasm.

Source code:
https://github.com/pinterf/mvtools
under mvtools-pfmod branch
