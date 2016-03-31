# mvtools
mvtools plugin for avisynth. mvtools2.dll

Modification base:
  mvtools2 2.6.0.5 64 bit version from
  http://avisynth.nl/index.php/AviSynth%2B#AviSynth.2B_x64_plugins

Features
  2.6.0.5 x64 capable version ported under Avisynth 2.6 API
  Fixed access violation in MDepan
  Fixed access violation in x64 asm code
  Built with Visual Studio 2015 Community Edition, v140_xp toolset
  Compiler: Intel C++ 16 (because of inline 64 bit asm code)

Links
  http://avisynth.org.ru/mvtools/mvtools2.html
  http://ldesoras.free.fr/prod.html (2.6.0.5)
  http://avisynth.nl/index.php/MVTools
  http://avisynth.nl/index.php/External_filters#64-bit_filters 

For more information see also documents folder.

Current build as of March 31, 2016: 2.7.0.1

External dependencies: 
  FFT3W.DLL (libfftw3f-3.dll)
  from http://www.fftw.org/ or look at ICL builds at http://forum.doom9.org/showthread.php?t=173229
  
  May require Visual C++ Redistributable 2015 Update 1
  from https://www.microsoft.com/en-us/download/details.aspx?id=49984
  
For asm complilation use yasm (1.3)
  http://yasm.tortall.net/Download.html
  hint: vsyasm.* files should be renamed to yasm.*
