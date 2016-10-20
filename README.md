# mvtools
Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family. 

File: mvtools2.dll (both 32 and 64 bit Windows DLLs)

Credits: 
- Manao, Fizick, Tsp, TSchniede, SEt, Vit, Firesledge, cretindesalpes 

Modification base:
- mvtools2, 2.6.0.5 64 bit version from
http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Features
- 2.7.1.22 (20161020)
  Some additional fixes for YV24
  New: MDegrain4, MDegrain5
  Experimental native 10-16 bit support (MSuper, MAnalyze, MDegrain1-5, MDegrainN)
  including 16 bit SATD (slow C) and SSE2 optimized regular SAD for 8+
  (for 10+ bits you need at least Avisynth+ r2290), 
  Inline assembly rewritten to intrinsics -> 64 bit build is possible in VS2015
  (External assembly untouched)

  Note:
    Built with VS2015, this build is 3-8% slower than previous ICC build.
    Don't use it if you stuck at YV12 or YUY2 clips and this speed loss makes you unhappy
 
- 2.7.0.22d 
  Some additional fixes for YV24
  Allow greyscale input (Y8)
  dct modes >= 5 now use SATD again (so far it was in dead code, contrary to 2.5.13.1 remarks)
  fftw 3.3.5 support (changed function names, 3.3.4 still OK), see http://www.fftw.org/download.html
  XP support
  compiled to SSE2 with optional SSE4.2 paths
  (not new but don't forget: first search for libfftw3f-3.dll, then fftw3.dll)

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

Current build as of October 20, 2016
- 2.7.1.22

Previous builds
- 2.7.0.22d (August 16, 2016) 
- 2.7.0.22c (August 04, 2016)
- 2.7.0.22 (April 29, 2016): ending number 22 hints to Fizick's 2.5.11.22 version
- 2.7.0.1 (March 31, 2016): skipped 2.6.x.x to leave numbering space to the previous authors

External dependencies: 
- libfftw3f-3.dll (or renamed to FFT3W.DLL)
from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  
- May require Microsoft Visual C++ Redistributable 2015 Update 3
  
Others
- For asm compilation use yasm (1.2): http://yasm.tortall.net/Download.html

hint: vsyasm.* files should be renamed to yasm.

Source code:
https://github.com/pinterf/mvtools
under mvtools-pfmod branch
