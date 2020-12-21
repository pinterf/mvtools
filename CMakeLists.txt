# We need CMake 3.8 at least, because we require
# CMAKE_CXX_STANDARD to be set to C++17.
# Visual Studio 2019 is supported from CMake 3.14.1
# Possible generators:
# "MinGW Makefiles": MSYS2/Mingw32 GCC 8.3 build
# "Visual Studio 15 2017" optional platform generator Win32 and x64
# "Visual Studio 16 2019" optional platform generator Win32 and x64
# "Visual Studio 16 2019" + LLVM 8.0 (clang) optional platform generator Win32 and x64
CMAKE_MINIMUM_REQUIRED( VERSION 2.8.11 )

project("MvTools2" LANGUAGES CXX)
  include(GNUInstallDirs)

# Avoid uselessly linking to unused libraries
set(CMAKE_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

# We require C++17 or higher.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

# Detect Intel processors and turn Intel SIMD on or off automatically.
message("-- Detected target processor as: ${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCHID)
if( ("${ARCHID}" STREQUAL "x86") OR
    ("${ARCHID}" STREQUAL "x64") OR
    ("${ARCHID}" STREQUAL "i686") OR
    ("${ARCHID}" STREQUAL "amd64") OR
    ("${ARCHID}" STREQUAL "x86_64") )
  set(INTEL_SIMD "ON")
else()
  set(INTEL_SIMD "OFF")
endif()

option(ENABLE_PLUGINS "Build set of default external plugins" ON)
option(ENABLE_INTEL_SIMD "Enable SIMD intrinsics for Intel processors" "${INTEL_SIMD}")

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

IF( MSVC )  # Check for Visual Studio
  # We do not allow creating Visual Studio solutions, existing .sln file contains 
  # all x86/x64 versions of MSVC and LLVM builds.
  MESSAGE(FATAL_ERROR "Please use the existing sln file both for MS VC and also for LLVM toolset in VS")
  # anyway we keep all things below
  # ** not tested **




  #1910-1919 = VS 15.0 (v141 toolset) Visual Studio 2017
  #1920      = VS 16.0 (v142 toolset) Visual Studio 2019

  IF( MSVC_VERSION VERSION_LESS 1910 )
    MESSAGE(FATAL_ERROR "Visual C++ 2017 or newer required.")
  ENDIF()

  IF(MSVC_IDE)
    message("Reported CMAKE_GENERATOR_TOOLSET is: ${CMAKE_GENERATOR_TOOLSET}")
    
      # For LLVM Clang installed separately, specify llvm or LLVM
      # Since Visual Studio 2019 v16.4, LLVM 9.0 is integrated, for this use Toolset: ClangCL
      IF(CMAKE_GENERATOR_TOOLSET STREQUAL "LLVM" OR CMAKE_GENERATOR_TOOLSET STREQUAL "llvm" OR CMAKE_GENERATOR_TOOLSET STREQUAL "ClangCL")
        if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")  # hope: always
          message("LLVM toolset was specified via -T. Compiler ID is: ${CMAKE_CXX_COMPILER_ID}; CMAKE_CXX_COMPILER_VERSION is: ${CMAKE_CXX_COMPILER_VERSION}")
          # Clang; 9.0.0
          # These are probably not supported when clang is downloaded as a ready-made binary: CLANG_VERSION_MAJOR CLANG_VERSION_MINOR CLANG_VERSION_STRING
          # string (REGEX REPLACE ".*clang version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${clang_full_version_string})
          if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0.1 )
            MESSAGE(FATAL_ERROR "Clang 7.0.1 or newer required") # as of 2019.december actually we are using 9.0
          endif()
        endif()
        set(CLANG_IN_VS "1")
      ELSEIF(CMAKE_GENERATOR_TOOLSET STREQUAL "v141_clang_c2")
         #1900 is reported
        message("v141_clang_c2 toolset was specified via -T. Reported MSVC_VERSION is: ${MSVC_VERSION}")
        message("May not work, try LLVM")
        set(CLANG_IN_VS "1")
      ENDIF()

      option(WINXP_SUPPORT "Make binaries compatible with Windows XP and Vista" OFF)
      if(WINXP_SUPPORT)
        # We want our project to also run on Windows XP
        # Not for LLVM: Clang stopped XP support in 2016
        # 1900 (VS2015) is not supported but we leave here
        IF(MSVC_VERSION VERSION_LESS 1910 )
          IF(NOT CLANG_IN_VS STREQUAL "1")
            set(CMAKE_GENERATOR_TOOLSET "v140_xp" CACHE STRING "The compiler toolset to use for Visual Studio." FORCE) # VS2015
            # https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
            message("CMAKE_GENERATOR_TOOLSET is forced to: ${CMAKE_GENERATOR_TOOLSET}")
            add_definitions("/Zc:threadSafeInit-")
          ENDIF()
        ELSE()
          IF(NOT CLANG_IN_VS STREQUAL "1")
            set(CMAKE_GENERATOR_TOOLSET "v141_xp" CACHE STRING "The compiler toolset to use for Visual Studio." FORCE) # VS2017, also choosable for VS2019
            # https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
            message("CMAKE_GENERATOR_TOOLSET is forced to: ${CMAKE_GENERATOR_TOOLSET}")
            add_definitions("/Zc:threadSafeInit-")
          ENDIF()
        ENDIF()
      endif()
  ENDIF()

  IF(CLANG_IN_VS STREQUAL "1")
      #these are unknown
      #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions")
      #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
      STRING( REPLACE "/EHsc" "/EHa" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
      STRING( REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-inconsistent-missing-override")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
  ELSE()
      # Enable C++ with SEH exceptions
      # Avoid an obnoxious 'overrriding /EHsc with /EHa' warning when
      # using something other than MSBuild
      STRING( REPLACE "/EHsc" "/EHa" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
      STRING( REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  ENDIF()
  # Prevent VC++ from complaining about not using MS-specific functions
  add_definitions("/D _CRT_SECURE_NO_WARNINGS /D _SECURE_SCL=0")
  
  # Enable CRT heap debugging - only effective in debug builds
  add_definitions("/D _CRTDBG_MAP_ALLOC")

  # Set additional optimization flags
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Oy /Ot /GS- /Oi")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy /Ot /GS- /Oi")

  # CPU_ARCH can be overridden with the corresponding values when using MSVC:
  # IA32 (disabled),
  # SSE (Pentium III and higher, 1999),
  # SSE2 (Pentium 4 and higher, 2000/2001),
  # AVX (Sandy Bridge and higher, 2011),
  # AVX2 (Haswell and higher, 2013)
  set(MSVC_CPU_ARCH "SSE2" CACHE STRING "Set MSVC architecture optimization level (default: SSE2)")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:${MSVC_CPU_ARCH}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:${MSVC_CPU_ARCH}")

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  # MSVC doesn't allow 64-bit builds to have their /arch set to SSE2 (no-op) or below
    if("${MSVC_CPU_ARCH}" MATCHES "(IA32|SSE|SSE2)")
      set(DELETE_THIS "/arch:${MSVC_CPU_ARCH}")
      message("MSVC doesn't allow x86-64 builds to define /arch:${MSVC_CPU_ARCH}. Setting will be ignored.")
      STRING( REPLACE "${DELETE_THIS}" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
      STRING( REPLACE "${DELETE_THIS}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
  endif()

  IF(CLANG_IN_VS STREQUAL "1")
      # suppress other frequent but harmless/unavoidable warnings
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-reorder")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-value")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-value")
    # allow per-function attributes like __attribute__((__target__("sse4.1")))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gcc-compat")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gcc-compat")
  ENDIF()

# Set C++17 flag
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /std:c++17")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

  # Enable standards-conformance mode for MSVC compilers that support this
  # flag (Visual C++ 2017 and later).
  if (NOT (MSVC_VERSION LESS 1910))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /permissive-")
  endif()

  if(ENABLE_INTEL_SIMD)
    add_definitions("/D INTEL_INTRINSICS")
  endif()

ELSE()

  if(ENABLE_INTEL_SIMD)
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -msse2 -DINTEL_INTRINSICS" )
  endif()
  IF(WIN32)
    SET( CMAKE_SHARED_LINKER_FLAGS "-Wl,--enable-stdcall-fixup" )
  ELSE()
    if(APPLE)
      # macOS uses Clang's linker, doesn't like --no-undefined
      SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,error" )
    else()
      if(GNUC)
        # make sure there are no undefined symbols
        SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
      endif()
    endif()
  ENDIF()
ENDIF()

#add_subdirectory("MvTools2")
add_subdirectory("Sources")
add_subdirectory("DePan")
add_subdirectory("DePanEstimate")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
