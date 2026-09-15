cmake_minimum_required(VERSION 3.21)

if(WIN32)
  if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    message(FATAL_ERROR
      "The Visual Studio generator cannot be used.\n"
      "Use: cmake --preset windows\n"
      "or:  cmake -G Ninja -S . -B build")
  endif()

  if(NOT CMAKE_C_COMPILER)
    find_program(LLVM_CLANG_CL clang-cl
      HINTS "$ENV{LLVM_ROOT}/bin"
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "C:/llvm/bin")
    if(LLVM_CLANG_CL)
      set(CMAKE_C_COMPILER "${LLVM_CLANG_CL}")
      message(STATUS "Using clang-cl: ${LLVM_CLANG_CL}")
    else()
      message(FATAL_ERROR
        "clang-cl.exe not found.\n"
        "Install LLVM (https://llvm.org) or pass -DLLVM_ROOT=<install dir>.")
    endif()
  endif()
endif()

project(mtdetect LANGUAGES C ASM_MASM)

option(MTDETECT_UNITY_BUILD "Batch sources into unity files for faster code" ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_LIBRARIES "")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(MTDETECT_WDK_VERSION "10.0.28000.0" CACHE STRING "WDK version with km headers")
set(MTDETECT_WDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "Windows Kits root")

set(MTDETECT_WDK_KM_INC "${MTDETECT_WDK_ROOT}/Include/${MTDETECT_WDK_VERSION}/km")
set(MTDETECT_WDK_SHARED_INC "${MTDETECT_WDK_ROOT}/Include/${MTDETECT_WDK_VERSION}/shared")
set(MTDETECT_WDK_KM_LIB "${MTDETECT_WDK_ROOT}/Lib/${MTDETECT_WDK_VERSION}/km/x64")

if(NOT EXISTS "${MTDETECT_WDK_KM_INC}/ntddk.h")
  message(FATAL_ERROR
    "WDK km headers not found at ${MTDETECT_WDK_KM_INC}/ntddk.h.\n"
    "Install the Windows WDK for ${MTDETECT_WDK_VERSION} or pass -DMTDETECT_WDK_VERSION=<version>.")
endif()

if(NOT EXISTS "${MTDETECT_WDK_KM_LIB}/ntoskrnl.lib")
  message(FATAL_ERROR
    "WDK km libs not found at ${MTDETECT_WDK_KM_LIB}/ntoskrnl.lib.\n"
    "Install the Windows WDK for ${MTDETECT_WDK_VERSION}.")
endif()

file(GLOB_RECURSE MTDETECT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

file(GLOB_RECURSE MTDETECT_ASM CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.asm")

file(GLOB_RECURSE MTDETECT_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

file(GLOB_RECURSE MTTEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c")

file(GLOB_RECURSE MTTEST_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/test/*.h")

add_library(mtdetect MODULE ${MTDETECT_SOURCES} ${MTDETECT_ASM} ${MTDETECT_HEADERS})
add_library(mttest MODULE ${MTTEST_SOURCES} ${MTTEST_HEADERS})

if(MTDETECT_UNITY_BUILD)
  set_target_properties(mtdetect PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 50)
  set_target_properties(mttest PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 50)
endif()

foreach(target mtdetect mttest)
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME ${target}
    SUFFIX ".sys"
    PREFIX "")

  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MTDETECT_WDK_KM_INC}
    ${MTDETECT_WDK_SHARED_INC})

  target_compile_definitions(${target} PRIVATE
    _AMD64_
    AMD64
    WIN64
    _KERNEL_MODE
    POOL_NX_OPTIN=1)

  target_compile_options(${target} PRIVATE
    /std:c17 /W4 /O2 /Ot /Oi /DNDEBUG
    /kernel /GS /GR- /EHs-c- /GF /Gy /Gw
    /DWIN32_LEAN_AND_MEAN)

  target_link_directories(${target} PRIVATE
    ${MTDETECT_WDK_KM_LIB})

  target_link_options(${target} PRIVATE
    /DRIVER:WDM /SUBSYSTEM:NATIVE /ENTRY:DriverEntry
    /NODEFAULTLIB /OPT:REF /OPT:ICF /INCREMENTAL:NO /ALIGN:0x1000)

  target_link_libraries(${target} PRIVATE
    ntoskrnl hal aux_klib BufferOverflowK)
endforeach()
