Depan and DepanEstimate
=======================

Motion estimation and compensation plugin for Avisynth+ and Avisynth v2.6 family.
Supporting YUY2, 4:2:0, 4:2:2, 4:4:4 at native 8, 10, 12, 14 and 16 bit depths.
Still supporting Windows XP.
x86 and x64 version

Files: depan.dll, depanestimate.dll 

Credits: 
- Fizick

Modification base:
- Depan: 1.13.1 by Fizick
- DepanEstimate 1.10 by Fizick
http://avisynth.org.ru/depan/depan.html

Change log
- Depan: 2.13.1.1 (20161218) YUY2 input fix, as in MvTools 2.7.8.22
  Previous build: Depan: 2.13.1 (20161119)
- DepanEstimate: 2.10 (20161119)

  General support of 10-16 bit formats with Avisynth Plus (r2294 or newer recommended)
  First digit of version number became 2, others left untouched, (sync'd with Fizick's version number)

- Built with Visual Studio 2015 Community Edition, v140-xp toolset

Links
  http://avisynth.org.ru/depan/depan.html

For more information see also documents folder.

External dependencies for DepanEstimate: 
- libfftw3f-3.dll (or renamed to FFT3W.DLL)
from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  
- Require Microsoft Visual C++ Redistributable 2015 Update 3
  
Source code:
https://github.com/pinterf/mvtools
under mvtools-pfmod branch
