Change log
- 2.7.45 (20210608)
  - Fix: change parameter 'ml' from int to float in MBlockFPS. (Other filters with 'ml' are O.K.: MMask, MFlowInter, MFlowFPS are using float.)
  - Fix MBlockFPS html doc as well, which mentions 'thres' instead of 'ml'. Add mode 5-8 to MBlockFPS docs</li>
  - Move change log from readme to CHANGELOG.md
  - Code change/speedup: MSuper: rfilter=0 and 1
    8 bit: drop old SSE code, port to SIMD intrinsics. Add SIMD to 16 bit case. Quicker, much quicker.
    (rfilter: Hierarchical levels smoothing and reducing (halving) filter)
  - Code change/speedup: MSuper: sharp=1 for pel=2 or 4
    Bicubic resizer drop old SSE code, port to SIMD intrinsics, implement SIMD intrinsics to 16 bit case.
    No need for Bilinear.asm and Bilinear-x64.asm any more.
  - SATD: add 8 bit C versions (geee, there wasn't one) (as an alternative to the external asm)
  - SAD: add internal SIMD for 8 bit SAD (SSE4.1) (as an alternative to the external asm)
  - Overlaps: Add internal SIMD for 8 bit. (as an alternative to the external asm)
  - In def.h any existing external assembler file can be disabled. 
    The primary reason for this was to quickly test the linux port, for me this was easier than bothering with asm compilation and linking.
    For non-Windows cases all of these are disabled now.
    - USE_COPYCODE_ASM (CopyCode-a.asm). Has internal alternative. Same speed.
    - USE_OVERLAPS_ASM (Overlap-a.asm). asm implements 8 bit only. Has internal SIMD alternative. About the same speed.
    - USE_SAD_ASM (sad-a.asm) asm implements 8 bit only. Note: Internal 8 bit SIMD SAD is a bit slower that these handcrafted ones.
    - USE_SATD_ASM (Pixel-a.asm) asm implements 8 bit only. Note: SATD 8 bit has no SIMD replacement yet. 
    - USE_LUMA_ASM (Variance-a.asm) asm implements 8 bit only. Has internal alternative. 
    - USE_FDCT88INT_ASM (fdct_mmx.asm, fdct_mmx_x64.asm) 
                        Only used for 8x8 block sizes. Quick integer version instead of fftw3.
                        No internal alternative, fftw3 is used instead.
    - USE_AVSTP (do find search for avstp.dll on Windows)
  - Minor and not so minor cosmetics, mainly for GCC.
  - Add Cmake build system.
  - MvTools2: Linux/GCC port (needs sse4.1), Dewindowsification.
    fftw3: MAnalyze dct modes that require fftw3 library will search for libfftw3f_threads.so.3
    Install either libfftw3-single3 (deb) or fftw-devel (rpm) package");
    e.g. sudo apt-get update
         sudo apt-get install libfftw3-dev
  - Not done (will be done in a second phase):
    Add back some external asms for linux build. For 8 bit SAD and SATD mainly.
    Linux port is still Intel-only, though every part has C alternative by now.
  - Separate the 3 projects (mvtools2, depan, depan_estimate).
  - DePan and DePanEstimate: Linux port
  - DePanEstimate: add fft_threads variable (default 1) for fftw3 mode (experimental)
  - DepanEstimate: add MT guard around sensible fft3w functions
  - mingw build fixes
  - Add build instructions to README.md
  - experimental avx2 for MDegrain1..6 (was not worth speedwise on my i7700 - memory transfer is bottleneck)
  - experimental 32-bit float internal Overlaps result buffer for MDegrain1..6
    whether if it is any quicker/more exact than the integer-scaled arithmetic version
    (when out32=true, do not use, it is only for development/test, maybe will be removed in the future)

- 2.7.44 (20201214)
  - MAnalyze: fix motion vector generation inconsistencies across multiple runs.
    Note: when internal multithreading is used (avstp + mt=true), inconsistencies will still occur by design.

- 2.7.43 (20200602)
  
- MCompensate: fix crash for GreyScale formats when overlap is used
  
- 2.7.42 (20200522)
  - MDegrain family: limit and limitc to float, allowing more granurality for 10+ bit depth
  - Update Avisynth headers, use V8 interface frame property copy if available

- 2.7.41 (20190502)
  - Fix: regression since 2.7.35: MSuper chroma for non-planar YUY2 (Thanks to mkauf)
  - Project moved to Visual Studio 2019, v141_xp and v142 toolset

- 2.7.40 (20190212)
  - Fix: MFlowInter possible crash with specific parameter and colorspace settings.
  - Fix: MFlowInter possible overflow at 16 bit clips (artifacts)
  - MFlow, MFlowInter, MFlowFPS, MFlowBlur, MBlockFPS: support all 8-16 and 32 bit float Y, YUV (4:2:0, 4:2:2, 4:4:4) and planar RGB color spaces.
    Input clip format is independent from the ones used for getting motion vectors.

- 2.7.39 (20190102)
  - MSuper: fix 16 bits, pel=2, sharp=2, which caused bottom-section artifacts for MDegrain using 8 bit vector origin and 16 bit real clip
  - MDegrain1-6,N: Enhanced:\
       Input clip (and super) format now is fully independent from vector clip's base format (subsampling had to be the same before)
       E.g. make motion vector clips from a YV12 source and apply them on a 8-32 bit 4:4:4 input

- 2.7.38 (20181209)
  - MCompensate: Fix regression in latest v37: overlap=0 crash - ouch, sorry
  - MAnalyze DCT: FFTW float: quicker postprocess of DCT'd blocks, now is correct for non power-of-2 block sizes\
  e.g. 12 or 24 (no effect on 8x8 for which fast integer DCT is used)
  - MAnalyze DCT: more consistent handling of post DCT internal normalization for non-square block sizes.

- 2.7.37 (20181128)
  - MCompensate: limit thSAD, thSAD2, thSCD1 to valid range 0-(8x8x255)\
  (e.g. given thSAD = 100000 will go back to 16320)
  - Fix: MCompensate: use int64 to avoid effective thSAD and thSAD2 overflow typically happen at bigger block sizes or large thSAD parameter value.
  - MCompensate: SSE2 (8bit) and SSE4 (10-16 bit) overlap result calculation
  - Changed: SAD 8x8, 8x4, 4x4, 4x8 to use SSE2 instead of MMX registers
  - Fix: MDegrain if overlap<>0: missing rounder in rightmost 8 pixels for non-mod8 width 8 bit clips
  - Fix: MSuper artifacts at 10-32 bits and nPel==4
  - New: MCompensate 32 bit float and planar RGB support (by using motion vectors made from 8-16 bit YUV clip).\ 
       Input and super clip can be of a different format than the one used for motion vector creation.\
       (Similar to MDegrain1-6)

- 2.7.36 (20181120)
  - Fix: Allow overlap operation when there are only two blocks in either horizontal or vertical direction\
  (was: division by 0 crash)
  - Fix: Fallback to overlap=0 mode when block count is only 1 in either h or v direction (was: undefined behaviour)
       The cases above occured for small frame sizes, when frame size and overlap values resulted in less than 3 blocks in a direction.
  - Misc: update html docs with overlap drawing and others.

- 2.7.35 (20181113)
  - MFlowXXX: Slight speed gain by putting the out-of-frame vector check to resizer
  - Fix: MAnalyze: Fix a possible internal overflow on larger blocksizes with small overlap\
  e.g. BlkSize=32, OverlapX=0, OverlapY=4
  - MSuper: Planar RGB support. Now MSuper supports Y and YUV and planar RGB 8-32 bits, and YUY2\
          note: different filters may support only a limited set of super clip formats.
  - MFlowFPS: Planar RGB support: generate vectors in YUV, use RGB input and super clip
  - MFlowFPS: less memory for 4:4:4 and greyscale
  - MFlowFPS: a bit faster 4:4:4, much faster greyscale
  - MFlowFPS: support different bit depth for clip and the vectors (use vectors from 8 bit analysis for a 16 bit clip)

- 2.7.34 (20181108)
  
- MFlowInter: Use less memory, eliminates ten full-frame internal buffers
  
- 2.7.33 (20181021)
  
- MFlowXX: Fix random access violation caused by enlarged vectors pointing on out-of-frame positions
  
- 2.7.32 (20181018)
  - MAnalyze: Enhance mt mode report for Avisynth+: MT_SERIALIZED instead of MT_MULTI_INSTANCE when temporal=true or using output file.
  - MAnalyze: fix a possible internal overflow on larger blocksizes and lambda combinations.\
  e.g. truemotion=true with blksize=32

- 2.7.31 (20180409)
  - Fix: MFlow: SC detection after having the mv clip. Fixed in 2.5.11.22 but was missed during 2.6.0.5 merge.
  - Fix: MFlow: crash in 16bit 4:2:0, mode=1
  - Fix: MDegrain, out16=true: Green bottom lines when overlap blocks are not covering the full vertical area

- 2.7.30 (20180405)
  - Fix: crash in MFlowInter (and possibly other MFlow...). v2.7.29 revelead this additional bug (which was not even 100% reproducible), 
  this fix is basically the 2nd part of the solution.

- 2.7.29 (20180403)
  - Fix: MFlowInter (and possibly other MFlow...) crash with specific combination of analyze parameters\
  (e.g. blkSize=16,overlapv=4,divide=1)
    Bug existed since at least 2.5.11.22

- 2.7.28 (20180323)
  
- Fix: in MDegrain1-6/N allow Y8 input for out16 parameter
  
- 2.7.27 (20180318)
  - Fix: MDepan: use zerow parameter. The parameter had no effect probably since it had been introduced. (veins1)
  - MDepan: report MT mode for Avisynth+. MT_MULTI_INSTANCE, except for logfile writing output mode when it reports MT_SERIALIZED.
    (other filters already have proper registration, MDepan was missed)

- 2.7.26 (20180314)
  - New: MDegrain1-6 and N: new parameter bool "out16" = false. If set, 8 bit input results in native 16bit output\
  (like lsb=true hack but this is native).
  Faster than lsb=true by up to 12% (i7-7700)
  - Faster: special 10 bit SAD functions instead of the generic 10-16bit one.\
   Depending on the block size, 4-17% gain for a typical MDegrain1-6 session

- 2.7.25 (20180227)
  - Fix: x64: not-cleared mmx state in MSuper assembly code would cause crash later,\
  e.g. in x264 encoding, depending on following filters.
  - Fix: MSCDetection SC value parameter name to Ysc from Yth (must be an ancient typo),\
  docs are OK, but the fix is mentioned in docs
  - MSuper: import 8 bit sse2 interpolators from mvtools-vs. Extend them for 10-16bits (faster super clip)\
          Some filters are still todo.
  - MSuper: support 32bit float clips, which can be used later by MDegrains (but not for MAnalyse)
  - MDegrains: allow degraining clip with different bit depth from vectors. Clip and Super must be the same bit depth
  - MDegrains: consistently use limit and limitC, 255 do nothing, otherwise scale 0-254 value to the current bit-depth range
  - Overlaps: more correct internal rounding for 8 bits:\
            old: `pixel = Sum( (tmp + 256) >> 6) >> 5`\
            new: `pixel = (Sum( (tmp + 32) >> 6) + 16) >> 5`
  - Overlaps: round for 16bits\
            old: `pixel = Sum(tmp) >> 11`\
            new: `pixel = (Sum(tmp) + 1024) >> 11`
  - Overlaps: 32bit float (but still use the original 11 bit window constants)
  - Project: change from yasm to nasm.

- 2.7.24 (20171205)
  - Fix: MFlowBlur: possible access violation crash when nPel>1</li>
  - New: MScaleVect parameter 'bits'. e.g. Analyze 8 bit clips, use their vectors for 16 bits
  - Move project to VS2017

- 2.7.23 (20171012)
  - Fix: MScaleVect wrong rounding of scaled motion vectors with negative components.\
  e.g. proper scaling (-1,-2) to (-2,-4) instead of (-1,-3)

- 2.7.22 (20170830)
  - Misc: Stop using version suffix .22
  - Fix: [DCT 8x8@8bit] garbage on x64: internal assembly code did not save xmm6/xmm7
  - Fix: [DCT 8x8@8bit] safe multithreading for integer DCT (8x8 block size, 8 bit video):\
  assembly had a single working buffer.
  - Fix: [MDegrain] did not release input motion vector clips in destructor, possible hang at script closing.\
  Bug since 2.7.1.22 (introducing MDegrain4/5)       
  - Mod: fftw conversion constant of `sqrt(2)/2` is more accurate (was: 0.707), 16 bit formats may benefit (by feisty2)
  - Fix: SSE4 assembly instructions in x64, broke on non-SSE4 processors

- 2.7.21.22 (20170629)
  
- Fix: [MDegrainN] fix chroma plane processing
  
- 2.7.20.22 (20170526) - hotfix
  
- Fix: [MMask] greyscale input resulted in AV when filter exiting
  
- 2.7.19.22 (20170525)
  - New: [MMask] Support any planar input video formats e.g. greyscale, Planar RGB.\
               Input clip can even be of different bit depth or format from vector's original format\
               For kind==5 where U and V is filled, the greyscale option is not allowed
  - Mod: [MMask] Faster: request source frame only for kind=5.
  - Fix: [MxxxxFPS,MMask]: MakeVectorOcclusionMaskTime garbage in bottom blocks **(30 hrs of debugging)**
  - Fix: [MMask] bottom padding garbage for padded frame dimension
  - Fix: [MMask] proper 10+ bits scene change values (for default: 1023, 4095, 16383, 65535. Was: 65535)\
       Parameter is still in 8-bit range  
  - Fix: [MRecalculate] prevent overflow during thSAD scaling in 16 bits or large block sizes (32, 48...)
  - Fix: [DepanEstimate] Sometimes giving wrong motion instead of scene change detection
  - Fix: [MAnalyze] Possible overflow in MAnalyze 8 bit, block size 48x48 and above.\
       Overflow-safe predictor recalc for big block sizes
  - New: [General] Add block size 12x3 for SAD, allow 6x24\
       List of available block sizes
       ```
         64x64, 64x48, 64x32, 64x16
         48x64, 48x48, 48x24, 48x12
         32x64, 32x32, 32x24, 32x16, 32x8
         24x48, 24x24, 24x32, 24x12, 24x6
         16x64, 16x32, 16x16, 16x12, 16x8, 16x4, 16x2
         12x48, 12x24, 12x16, 12x12, 12x6, 12x3
         8x32, 8x16, 8x8, 8x4, 8x2, 8x1
         6x24, 6x12, 6x6, 6x3
         4x8, 4x4, 4x2
         3x6, 3x3
         2x4, 2x2
       ```
  - Mod: [Internal] Reorganized 10-16 bit SAD simd intrinsics, faster 8-12% for BlkSize 12-32

- 2.7.18.22 (20170512)
  - Fix: 10-16 bit: DCT buffer possible overflow

  - Fix: DCT is fast again for non 8x8 blocksizes. Regression since 2.7.5.22.

  - New: Chroma SAD is now always half of luma SAD, regardless of video format\
       Without this: YV24's luma:chroma SAD ratio is 4:8 instead of 4:2 (of YV12)

  - New: MAnalyze, MRecalculate new parameter: "scaleCSAD" integer, default 0\
       Fine tune chroma SAD weight relative to luma SAD.
       ScaleCSAD values for luma:chroma SAD ratio
     ```
       -2: 4:0.5
       -1: 4:1
        0: 4:2 (default, same as the native ratio for YV12)
        1: 4:4
        2: 4:8
     ```
  - New: Block sizes 64, 48, 24, 12, 6
       MAnalyze/MRecalculate new block sizes (SATD support mod4 sizes)\
       List of available block sizes
       ```
         64x64, 64x48, 64x32, 64x16
         48x64, 48x48, 48x24, 48x12
         32x64, 32x32, 32x24, 32x16, 32x8
         24x48, 24x24, 24x32, 24x12, 24x6
         16x64, 16x32, 16x16, 16x12, 16x8, 16x4, 16x2
         12x48, 12x24, 12x16, 12x12, 12x6
         8x32, 8x16, 8x8, 8x4, 8x2, 8x1
         6x24, 6x12, 6x6, 6x3
         4x8, 4x4, 4x2
         3x6, 3x3
         2x4, 2x2
       ```
  - Note: some smaller block sizes can only be available in 4:4:4 formats,\
  due to block size division (chroma subsampling)

  - New: All block sizes are supported in MDegrain1-6, MDegrainN, and MScaleVect

  - New: Changed to 2017 version of asm files for 8 bit SAD/SATD functions from x265 project.\
       Added not implemented asm code for 12, 24, 48 sizes\
       For some block sizes AVX2 and SSE4 is supported (AVX2 if reported under AviSynth+)
       e.g. BlkSize 32 is faster now.

  - New: MMask SAD Mask to give identical weights for other-than-YV12 formats, e.g. for YV24

- 2.7.17.22 (20170426)
  - Fix: Regression in 2.7.16.22: MDegrain right pixel artifacts on non-modulo 16 widths
  - Misc: MMask, mode SADMask output is normalized further by video subsampling\
  (YV16/YV24 has larger SAD value due to bigger chroma part that classic YV12)

- 2.7.16.22 (20170423)
  - Fix: MMask 10-16 bits
  - Fix: MRecalculate 14-16 bits passed nSCD1=999999 internally which caused overflow (scene change problems later)
       Fix is done by clamping SCD1 to 8*8*(255-0) (maximum value of sum of SADs on a 8x8 block)
  - Misc: MDegrainX 8 bits: internal 16 bit buffer to 8 bits: SSE2

- 2.7.15.22 (20170316)
  - Fix: 16 bit SAD for non-AVX code path
  - Misc: MDegrain1-6: add error on lsb_flag=true for non-8 bit sources
  
- 2.7.14.22 (20170206)
  - Fix: MAnalyze divide=2 showed "vector clip is too small"
       (inherited from 2.6.0.5, sanity check was done but length was not filled for divideextra data)
  - Fix: MFlow access violation in internal mv resizer when resizing factor was big (MCaWarpSharp3 4x supersampling case)
       (bug introduced in upstream 2.5.11.22)

- 2.7.13.22 (20170201)
  - Fix: MDegrain1-6,N 10-16 bit thSCD scaling
  - Fix: MVShow: tolerance scaling for 10-16 bits

- 2.7.12.22 (20170120)
  - New: Faster SATD (dct=5..10) 8 bit: updated x264 function selectors, SSE2/4/AVX/AVX2\
       +10% speed for a whole typical MDegrain3 process on my i7-3770
  - New: Much Faster SATD (dct=5..10) 10-16 bit: SSE2/SSE4 instead of C\
       +50% speed for a whole typical MDegrain3 process (which is approx half speed of 8 bit)

- 2.7.11.22 (20170116)
  - New: MDegrain6
  - Mod: MDegrain1-6 SSE4 for 10-16 bit (was: C. 3-5% gain, wasn't bottleneck)

- 2.7.10.22 (20161228)
  
- Fix: for YV12 the debug info text chroma part was positioned at wrong place
  
- 2.7.9.22 (20161220)
  
- Apply 2.5.11.9-svp analysis speedup, mainly when chroma is involved
  
- 2.7.8.22 (20161218)
  - Fix: YUY2 input access violation (regression after 2.7.0.22d)
       Fixed also in Depan.dll
  - Fix: MDegrain: free up YUY2 planes only if not planar input (bug inherited from 2.5.11.22 MDegrain3)\
       resulting in freeze at script exit

- 2.7.7.22 (20161214) - optimizing a bit
  - speed: change some 8 bit SAD functions for the better
  - speed: separating bottleneck 8 bit/16 bit code paths in order not to use slower 
         int64 calculations for 8 bit,\
         where there are no integer overflow problems
  - speed: more __forceinlines for helping the compiler
  - info:  general speed gain of 5-15% compared to 2.7.6.22, much reduced speed gap\
         compared to the "classic" YV12 8 bit mvtools2 versions

- 2.7.6.22 (20161204) - fixes and speedup
  - Fix: sumLumaChange underflow (used for dct=2,6,9) (regression during 16 bit support)
  - Fix: MeanLumaChange scale for 10-16 bits (used for dct=2,6,9)
  - Fix: Mask fix: 8 bit mask resizer bug in SIMD intrinsics  - Thx real.finder\
       (regression on inline asm -> SIMD transition)       
  - Fix: dctmode=1,2: pixel distance was not corrected for 16 bit pixel sizes
  - speed: Let's help VS2015 with some __forceinline directives to recognize the truth.
  - speed: Misc optimizations throughout the code (bit shifts instead of div or mul)
  - speed: FFTW DCT: C code replaced with SIMD SSE2/SSE4 (FloatToBytes, BytesToFloat)
  - speed: 16 bit SAD: a few optimizations in SSE2, AVX-coded SSE2 and AVX2 codepath
  - VS2015 compiler: /MT -> /MD (from static to dynamic dlls - now it reallys need VS2015 redistributables)

- 2.7.5.22 (20161119)
  - Milestone release:
  General support of 10-16 bit formats with Avisynth Plus (r2294 or newer recommended)
  with new MDegrain4 and MDegrain5 filters.

  - Fix for MSCDetection: scene change filler pixel default value was always 0 (2.7.1.22 regression)
  - MCompensate: possible bugfix bottom padding UV
  Fix SAD for 10-16 bit depths for horizontal block sizes >= 16
  Fix nSCD2 (Scene change threshold block count %) (2.7.1.22 regression)
  - MBlockFPS: overlap fixes (right columns and bottom lines)
  - MBlockFPS: overlap fix: missing copy buffer to output, thanks StainlessS

- 2.7.1.22 (20161020)
  
  - Some additional fixes for YV24
  - New: MDegrain4, MDegrain5
  - Experimental native 10-16 bit support (MSuper, MAnalyze, MDegrain1-5, MDegrainN)
  including 16 bit SATD\
  (slow C) and SSE2 optimized regular SAD for 8+
  (for 10+ bits you need at least Avisynth+ r2290), 
  - Inline assembly rewritten to intrinsics -> 64 bit build is possible in VS2015
(External assembly untouched)
  
  - Note:
    Built with VS2015, this build is 3-8% slower than previous ICC build.\
  Don't use it if you stuck at YV12 or YUY2 clips and this speed loss makes you unhappy
  
- 2.7.0.22d 
  - Some additional fixes for YV24
  - Allow greyscale input (Y8)
  - dct modes >= 5 now use SATD again (so far it was in dead code, contrary to 2.5.13.1 remarks)
  - fftw 3.3.5 support (changed function names, 3.3.4 still OK), see http://www.fftw.org/download.html
  - XP support
  - compiled to SSE2 with optional SSE4.2 paths
  (not new but don't forget: first search for libfftw3f-3.dll, then fftw3.dll)

- 2.7.0.22c: (August 04, 2016)

  - test build to support YV16 and YV24, from now there is no need for YUY2 planar hack in scripts. Use ConvertToYV16() and work with it.

- 2.7.0.22 (April 29, 2016)

  - Synchronized to Fizick's 2.5.11.22 version
    e.g. fixed: greenish garbage in MFlowInter
  - Dropped tclip support in MFlow, MFlowInter (could not resolve with 2.5.11.22)
  - 2.6.0.5 x64 capable version ported under Avisynth 2.6 API
  - Fixed access violation in MDepan
  - Fixed access violation in x64 asm code
  - Built with Visual Studio 2015 Community Edition, v140 toolset
  - Compiler: Intel C++ (because of inline 64 bit asm code)

  - version number ending "22" hints to Fizick's 2.5.11.22 version

- 2.7.0.1 (March 31, 2016)

  - Version schema changed: skipped 2.6.x.x to leave numbering space to the previous authors