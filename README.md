# mvtools
Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family.
Supporting YUY2, 4:2:0, 4:2:2, 4:4:4 at native 8, 10, 12, 14 and 16 bit depths.
Still supporting Windows XP.
x86 and x64 versions

File: mvtools2.dll

Credits: 
- Manao, Fizick, Tsp, TSchniede, SEt, Vit, Firesledge, cretindesalpes 

Modification base:
- mvtools2, 2.6.0.5 64 bit version from
http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Change log
- 2.7.15.22 (20170316)
  Fix: 16 bit SAD for non-AVX code path
  Misc: MDegrain1-6: add error on lsb_flag=true for non-8 bit sources
  
- 2.7.14.22 (20170206)
  Fix: MAnalyze divide=2 showed "vector clip is too small"
       (inherited from 2.6.0.5, sanity check was done but length was not filled for divideextra data)
  Fix: MFlow access violation in internal mv resizer when resizing factor was big (MCaWarpSharp3 4x supersampling case)
       (bug introduced in upstream 2.5.11.22)

- 2.7.13.22 (20170201)
  Fix: MDegrain1-6,N 10-16 bit thSCD scaling
  Fix: MVShow: tolerance scaling for 10-16 bits

- 2.7.12.22 (20170120)
  New: Faster SATD (dct=5..10) 8 bit: updated x264 function selectors, SSE2/4/AVX/AVX2
       +10% speed for a whole typical MDegrain3 process on my i7-3770
  New: Much Faster SATD (dct=5..10) 10-16 bit: SSE2/SSE4 instead of C 
       +50% speed for a whole typical MDegrain3 process (which is approx half speed of 8 bit)

- 2.7.11.22 (20170116)
  New: MDegrain6
  Mod: MDegrain1-6 SSE4 for 10-16 bit (was: C. 3-5% gain, wasn't bottleneck)

- 2.7.10.22 (20161228)
  Fix: for YV12 the debug info text chroma part was positioned at wrong place

- 2.7.9.22 (20161220)
  Apply 2.5.11.9-svp analysis speedup, mainly when chroma is involved

- 2.7.8.22 (20161218)
  Fix: YUY2 input access violation (regression after 2.7.0.22d)
       Fixed also in Depan.dll
  Fix: MDegrain: free up YUY2 planes only if not planar input (bug inherited from 2.5.11.22 MDegrain3)
       resulting in freeze at script exit

- 2.7.7.22 (20161214) - optimizing a bit
  speed: change some 8 bit SAD functions for the better
  speed: separating bottleneck 8 bit/16 bit code paths in order not to use slower 
         int64 calculations for 8 bit, where there are no integer overflow problems
  speed: more __forceinlines for helping the compiler
  info:  general speed gain of 5-15% compared to 2.7.6.22, much reduced speed gap
         compared to the "classic" YV12 8 bit mvtools2 versions

- 2.7.6.22 (20161204) - fixes and speedup
  fix: sumLumaChange underflow (used for dct=2,6,9) (regression during 16 bit support)
  fix: MeanLumaChange scale for 10-16 bits (used for dct=2,6,9)
  fix: Mask fix: 8 bit mask resizer bug in SIMD intrinsics  - Thx real.finder
       (regression on inline asm -> SIMD transition)       
  Fix: dctmode=1,2: pixel distance was not corrected for 16 bit pixel sizes
  speed: Let's help VS2015 with some __forceinline directives to recognize the truth.
  speed: Misc optimizations throughout the code (bit shifts instead of div or mul)
  speed: FFTW DCT: C code replaced with SIMD SSE2/SSE4 (FloatToBytes, BytesToFloat)
  speed: 16 bit SAD: a few optimizations in SSE2, AVX-coded SSE2 and AVX2 codepath
  VS2015 compiler: /MT -> /MD (from static to dynamic dlls - now it reallys need VS2015 redistributables)

- 2.7.5.22 (20161119)
  Milestone release:
  General support of 10-16 bit formats with Avisynth Plus (r2294 or newer recommended)
  with new MDegrain4 and MDegrain5 filters.

  Fix for MSCDetection: scene change filler pixel default value was always 0 (2.7.1.22 regression)
  MCompensate: possible bugfix bottom padding UV
  Fix SAD for 10-16 bit depths for horizontal block sizes >= 16
  Fix nSCD2 (Scene change threshold block count %) (2.7.1.22 regression)
  MBlockFPS: overlap fixes (right columns and bottom lines)
  MBlockFPS: overlap fix: missing copy buffer to output, thanks StainlessS

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

Current build as of November 19, 2016

Previous builds
- 2.7.1.22 (October 20, 2016)
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
