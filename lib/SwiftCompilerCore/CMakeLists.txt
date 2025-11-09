# CMakeLists.txt for SwiftCompilerCore - Embeddable Swift Compiler Library

# This library provides a C API for embedding the Swift compiler in
# developer tooling, allowing programmatic access to parsing, type checking,
# SIL generation, and LLVM IR generation.

cmake_minimum_required(VERSION 3.19.6)

# Check if we're building the embeddable library
option(SWIFT_BUILD_EMBEDDABLE_LIB "Build embeddable compiler library" ON)

if(NOT SWIFT_BUILD_EMBEDDABLE_LIB)
  return()
endif()

# Configuration options
option(SWIFT_EMBEDDABLE_STATIC "Build embeddable library as static" ON)
option(SWIFT_EMBEDDABLE_SHARED "Build embeddable library as shared" OFF)

# Source files
set(SWIFT_COMPILER_CORE_SOURCES
  CompilerAPI.cpp
  CompilerAPIBridging.cpp
  CompilerInstance.cpp
  ParsingAPI.cpp
  SILAPI.cpp
  IRAPI.cpp
)

# Headers
set(SWIFT_COMPILER_CORE_HEADERS
  ${CMAKE_SOURCE_DIR}/include/swift-c/CompilerAPI.h
  CompilerAPIInternal.h
)

# Create the library
if(SWIFT_EMBEDDABLE_STATIC)
  add_library(SwiftCompilerCore STATIC ${SWIFT_COMPILER_CORE_SOURCES})
  set_target_properties(SwiftCompilerCore PROPERTIES
    OUTPUT_NAME swiftCompilerCore
    ARCHIVE_OUTPUT_NAME swiftCompilerCore
  )
endif()

if(SWIFT_EMBEDDABLE_SHARED)
  add_library(SwiftCompilerCoreShared SHARED ${SWIFT_COMPILER_CORE_SOURCES})
  set_target_properties(SwiftCompilerCoreShared PROPERTIES
    OUTPUT_NAME swiftCompilerCore
    VERSION ${SWIFT_COMPILER_API_VERSION_MAJOR}.${SWIFT_COMPILER_API_VERSION_MINOR}.${SWIFT_COMPILER_API_VERSION_PATCH}
    SOVERSION ${SWIFT_COMPILER_API_VERSION_MAJOR}
  )
endif()

# Set the primary target
if(SWIFT_EMBEDDABLE_STATIC)
  set(SWIFT_COMPILER_CORE_TARGET SwiftCompilerCore)
else()
  set(SWIFT_COMPILER_CORE_TARGET SwiftCompilerCoreShared)
endif()

# Include directories
target_include_directories(${SWIFT_COMPILER_CORE_TARGET} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_include_directories(${SWIFT_COMPILER_CORE_TARGET} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include/swift
)

# Link against core Swift compiler libraries
target_link_libraries(${SWIFT_COMPILER_CORE_TARGET} PRIVATE
  swiftBasic
  swiftAST
  swiftParse
  swiftSema
  swiftSIL
  swiftSILGen
  swiftSILOptimizer
  swiftIRGen
  swiftOption
  swiftDemangling
)

# Also link against LLVM
target_link_libraries(${SWIFT_COMPILER_CORE_TARGET} PRIVATE
  LLVMCore
  LLVMSupport
  LLVMIRReader
)

# Compiler settings
target_compile_features(${SWIFT_COMPILER_CORE_TARGET} PRIVATE cxx_std_17)

# Export symbols for shared library
if(SWIFT_EMBEDDABLE_SHARED)
  set_target_properties(SwiftCompilerCoreShared PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
  target_compile_definitions(SwiftCompilerCoreShared PRIVATE
    SWIFT_COMPILER_API_EXPORTS
  )
endif()

# Installation
if(SWIFT_EMBEDDABLE_STATIC)
  install(TARGETS SwiftCompilerCore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
  )
endif()

if(SWIFT_EMBEDDABLE_SHARED)
  install(TARGETS SwiftCompilerCoreShared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()

# Install headers
install(FILES ${CMAKE_SOURCE_DIR}/include/swift-c/CompilerAPI.h
  DESTINATION include/swift-c
)

# Add to swift dependencies (so other targets can depend on this)
add_dependencies(${SWIFT_COMPILER_CORE_TARGET} ${LLVM_COMMON_DEPENDS})
