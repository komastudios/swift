# CMakeLists.txt for simple-compiler example

cmake_minimum_required(VERSION 3.19.6)

project(SimpleCompilerExample C)

# Find the Swift Compiler Core library
# This assumes it's been built in the same build tree
find_library(SWIFT_COMPILER_CORE_LIB
  NAMES swiftCompilerCore
  HINTS ${CMAKE_BINARY_DIR}/lib
)

if(NOT SWIFT_COMPILER_CORE_LIB)
  message(STATUS "SwiftCompilerCore library not found, skipping example")
  return()
endif()

# Create executable
add_executable(simple-compiler main.c)

# Include directories
target_include_directories(simple-compiler PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# Link against the Swift Compiler Core library
target_link_libraries(simple-compiler PRIVATE
  SwiftCompilerCore
)

# Set C standard
set_target_properties(simple-compiler PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
)

# Installation
install(TARGETS simple-compiler
  RUNTIME DESTINATION bin/examples
)

# Add a test target
if(SWIFT_BUILD_EXAMPLES_TESTS)
  enable_testing()
  add_test(NAME simple-compiler-test
    COMMAND simple-compiler
  )
endif()
