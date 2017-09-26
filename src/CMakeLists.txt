cmake_minimum_required(VERSION 2.8.2)

set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(libvata2)

# Export compile commands to be used with YouCompleteMe
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Flags
set(cxx_compiler_flags
  -pedantic-errors
  -Wextra
  -Wall
  -Wfloat-equal
  -fdiagnostics-show-option
  -march=native
  -std=c++14
  -Wctor-dtor-privacy
  -Weffc++
  -fPIC
  -fno-strict-aliasing
  -Woverloaded-virtual
  -Wold-style-cast
)

foreach(flag ${cxx_compiler_flags})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach()

# if DEBUG, also test coverage
if(CMAKE_BUILD_TYPE MATCHES "Coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

# uncomment for profiling
#add_definitions(-pg)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

include_directories(../include)

add_library(libvata2 STATIC
	nfa.cc
	nfa-dispatch.cc
	nfa-universal.cc
	parser.cc
	vm.cc
	vm-dispatch.cc
)

set_target_properties(libvata2 PROPERTIES
  OUTPUT_NAME vata2
  CLEAN_DIRECT_OUTPUT 1
)

add_executable(tests
	tests-main.cc
	tests-nfa.cc
	tests-nfa-dispatch.cc
	tests-parser.cc
	tests-vm-dispatch.cc
)

target_link_libraries(tests libvata2)

add_test(
	NAME catch_tests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tests
)

# problems with valgrind under appleOS
if(NOT APPLE)
	add_test(
		NAME "catch_tests_valgrind"
		COMMAND ${VALGRIND_PROGRAM}
			--tool=memcheck
			--leak-check=yes
			--show-reachable=yes
			--error-exitcode=1
			${CMAKE_CURRENT_BINARY_DIR}/tests
	)
endif()
