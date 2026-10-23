FILE(GLOB MvTools2_Sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "*.c"
  "*.cpp"
  "*.hpp"
  "*.h"
  "include/*.h"
  "include/avs/*.h"
  "conc/*.h"
  "conc/*.hpp"
)

IF(WIN32)
ELSE()
  LIST(REMOVE_ITEM MvTools2_Sources "AvstpFinder.cpp")
  LIST(REMOVE_ITEM MvTools2_Sources "AvstpFinder.h")
ENDIF()

# x86 ISA-specific files only for Intel targets, NEON files only for AArch64 targets
IF(NOT ENABLE_INTEL_SIMD)
  LIST(FILTER MvTools2_Sources EXCLUDE REGEX "_(ssse3|sse41|avx|avx2|avx512)\\.cpp$")
ENDIF()
IF(NOT ENABLE_NEON_SIMD)
  LIST(FILTER MvTools2_Sources EXCLUDE REGEX "_neon\\.(cpp|h)$")
ENDIF()


message("${MvTools2_Sources}")

IF( MSVC OR MINGW )
    # Export definitions in general are not needed on x64 and only cause warnings,
    # unfortunately we still must need a .def file for some COM functions.
    # NO C interface for this plugin
    # if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    #  LIST(APPEND MvTools2_Sources "MvTools264.def")
    # else()
    #  LIST(APPEND MvTools2_Sources "MvTools2.def")
    # endif() 
ENDIF()

IF( MSVC_IDE )
    # Ninja, unfortunately, seems to have some issues with using rc.exe
    LIST(APPEND MvTools2_Sources "MvTools2.rc")
ENDIF()
