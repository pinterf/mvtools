Depan and DepanEstimate
=======================

Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family.
Supporting YUY2, 4:2:0, 4:2:2, 4:4:4, planar RGB* at native 8-16 bit depths
Still supporting Windows XP.
x86 and x64 version
*in DepanStabilize

Files: depan.dll, depanestimate.dll 

Credits: 
- Fizick

Modification base:
- Depan: 1.13.1 by Fizick
- DepanEstimate 1.10 by Fizick
http://avisynth.org.ru/depan/depan.html

Change log (modifications by pinterf: https://github.com/pinterf/):

- (20261018)
  Depan 2.16
  - Support Y with alpha (YA) and YUVA, all subsamplings incl. 4:1:0, 4:1:1, 4:4:0.
    DePanScenes copies the alpha from the source.
  - DePan, DePanInterleave, DePanStabilize: the alpha plane marks the new empty borders (#67, #43).
    Alpha is moved like luma, without mirror and blur: the borders get alpha 0, the rest keeps the moved source
    alpha (DePanStabilize: areas filled from prev, next frames count as picture). Works with zoom and rotation too.
    New parameter alphainvert (default false): the borders get the maximum alpha value instead. E.g. with
    AddAlphaPlane(0) before DePan the alpha plane is a mask of the borders, usable to fill them later.
    Formats with alpha: YUVA, YA (DePanStabilize: planar RGBA too).
  - 32 bit float input is rejected with an error (was not supported).
  - Fix: DePanStabilize on YUY2 produced garbage. Regression since 2.13.1.0 (mvtools 2.7.5.22): the packed
    YUY2 frame was processed as one plane, the YUY2 <-> planar conversion of the original DePan was lost.
  - Fix: DePanScenes on planar formats marked only the top-left part of the planes (half width and height,
    a quarter of the width for 4:1:1, 4:1:0 chroma), the rest was uninitialized, so AverageLuma-style scene change
    checks could read garbage. Regression since 2.13.1.0 (mvtools 2.7.5.22). YUY2 was not affected.
  - Fix: info=true at 10-16 bits: possible crash (access violation), or text shading at the wrong place.
    The chroma part of the text was drawn at double row offset, with 4:4:4 it could write past the frame buffer.
    Characters not fitting into the frame are skipped.
  - Fix: info=true on Y (greyscale) clips: "GetPlaneWidthSubsampling not available on Y or YA pixel type" error.
  - Fix: subpixel=2 (bicubic, default) with pure shifts (no zoom, rotation): the 16 interpolation weights summed
    to 2046..2051 instead of 2048 (truncated), so areas were made slightly darker or brighter, depending on the
    subpixel position. Visible from 10 bits, e.g. 16 bit mid grey was off by -32..+48. 8 bit: rarely +-1.
  - Fix: DePanStabilize with prev, next (border fill from neighbor frames): the remaining empty border parts,
    covered by none of the used frames, were not written: they showed garbage (usually picture leftovers
    from earlier frames). Now they are filled like the border without prev, next: luma (RGB: all planes) 0,
    chroma neutral.
  DepanEstimate 2.13
  - Support Y with alpha (YA) and YUVA (was: "input must be YUV or YUY2"). Alpha is copied when info or show is set.
  - 32 bit float input is rejected with an error.
  - Fix: info=true at 10-16 bits: crash (4:4:4) or misplaced text; text position was outside the right edge.
  - Fix: zoom window and info position used the row size in bytes instead of pixels at 10-16 bits.
  - (internal cosmetics): clear the unused part of the motion data clip (luma 0, chroma neutral, alpha opaque).
    Was uninitialized memory outside the data area: harmless for DePan, but the output differed from run to run,
    and DePanScenes copied it into its not marked planes. Deterministic output helps testing (frame hashes,
    comparing the output of different versions).

- (20260421)
  Depan 2.15
  - Update Avisynth headers to V12 interface
  - Declare MT mode via SetCacheHints(CACHE_GET_MTMODE) for all four filters:
    DePan, DePanInterleave, DePanScenes: MT_MULTI_INSTANCE (all per-instance state is independent).
    DePanStabilize: MT_SERIALIZED when debuglog= is active (file writes in GetFrame) or when
      any of vdx=/vdy=/vzoom=/vrot= are set (uses env->SetVar(), writing shared global script
      variables); MT_MULTI_INSTANCE otherwise.
  DepanEstimate 2.12
  - Update Avisynth headers to V12 interface (AcquireGlobalLock/ReleaseGlobalLock support)
  - Thread-safe FFTW plan creation/destruction using GlobalLockGuard:
    On Avisynth+ V12+ uses the shared global named lock "fftw", coordinating with
    other plugins (e.g. dfttest) that use the same FFTW DLL.
    Falls back to a local static mutex on older Avisynth versions.
  - Declare MT mode via SetCacheHints(CACHE_GET_MTMODE):
    MT_SERIALIZED when file logging (log=/extlog=) is active (concurrent clones
    would open and write the same file path, corrupting output).
    MT_MULTI_INSTANCE otherwise (all per-instance state is independent).

- (20240503)
  - Moved to Visual Studio 2022, v141_xp and v143 toolset, Intel Compiler ICX 2024.1 and ICL build support
  Depan 2.14
    - Fix: "DepanScenes" plane parameter for YUY2 clips did not work
  DepanEstimate 2.11
    - Throw an error if memory allocation fails

- (20200522)
  Depan 2.13.1.6 
  DepanEstimate 2.10.0.4
  - Update Avisynth headers, use V8 interface frame property copy if available

- (20200430)
  Depan 2.13.1.5: 
  fix regression: DepanInterleave colorspace check allowed no planar, only YUY2
  Depan: Fix an issue when Depan would search for inputlog file even when there was no inputlog filename provided
  Depan: Fix crash on YUY2 input on internal YV16 conversion
- (20190502)
  Moved to Visual Studio 2019 Community Edition, v141_xp and v142 toolset
  DepanEstimate 2.10.0.3
  Depan 2.13.1.4
  (20190311)
  - DepanStabilize planar RGB 8-16 bits support: Generate motion vectors with mvtools MDepan, use it for DepanStabilize. Input clip can be planar RGB.
  - add missing roundings to some internal interpolation calculations
  - update documentation from Fizick's 2016 version
- (20170525)
  Fix: DepanEstimate 2.10.0.2 : did not recognize Scene changes, giving valid motion vectors instead
  Depan: no change, rebuild with current headers 2.13.1.3 (20170525)
- Fix: DepanStabilize: removed too strict checking for large motion vectors received from depan
       that resulted in like scene change
- (20161228)
  Fix: for YV12 the debug info text chroma part was positioned at wrong place
  Depan: 2.13.1.2 
  DepanEstimate: 2.10.0.1
- (20161218)
  Depan: 2.13.1.1: YUY2 input fix, as in MvTools 2.7.8.22
  Previous build: Depan: 2.13.1 (20161119)
- (20161119)
  DepanEstimate: 2.10 
  - General support of 10-16 bit formats with Avisynth Plus (r2294 or newer recommended)
  - First digit of version number became 2 (Fizick's 1.x.x), others left untouched, (sync'd with Fizick's version number)
  - Built with Visual Studio 2015 Community Edition, v140-xp toolset

For more information see also documents folder.

External dependencies for DepanEstimate: 
- libfftw3f-3.dll (or renamed to FFT3W.DLL)
from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  
- Requires Microsoft Visual C++ Redistributable
  
Source code:
https://github.com/pinterf/mvtools
under mvtools-pfmod branch
