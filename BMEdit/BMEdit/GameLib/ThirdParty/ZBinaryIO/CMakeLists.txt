cmake_minimum_required(VERSION 3.5)
project(ZBinaryReader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "/MT") 
	set(CMAKE_CXX_FLAGS_DEBUG "/MT")
endif()

add_subdirectory(include)

option(ZBINARYREADER_BUILD_TESTS "Build tests" ON)
if(ZBINARYREADER_BUILD_TESTS)

  add_library(coverage_config INTERFACE)

  option(ZBINARYREADER_CODE_COVERAGE "Enable coverage reporting" OFF)
  if(ZBINARYREADER_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(coverage_config INTERFACE
      -O0        # no optimization
      -g         # generate debug info
      --coverage # sets all required flags
    )
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
      target_link_options(coverage_config INTERFACE --coverage)
    else()
      target_link_libraries(coverage_config INTERFACE --coverage)
    endif()
  endif()

  enable_testing()

	add_subdirectory(test)
	add_subdirectory(extern/googletest)
endif()