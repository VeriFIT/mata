set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(libmata)

# Export compile commands to be used with YouCompleteMe
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Flags
set(cxx_compiler_flags
  -Wextra
  -Wall
  -Wfloat-equal
  -fms-extensions
  -fdiagnostics-show-option
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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# using regular Clang or AppleClang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic-errors")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wgnu-anonymous-struct")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnested-anon-types")
endif()

# if DEBUG, also test coverage
if(CMAKE_BUILD_TYPE MATCHES "Coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

# uncomment for profiling
#add_definitions(-pg)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

include_directories(../include)
include_directories(../3rdparty/) # this is here since it is clearer to include re2/utils than just utils
include_directories(../3rdparty/re2)
include_directories(../3rdparty/simlib/include)

add_library(libmata STATIC
# add_library(libmata SHARED
	afa/afa.cc
	parser.cc
	re2parser.cc
	nfa/nfa.cc
	nfa/nfa-incl.cc
	nfa/nfa-universal.cc
	nfa/nfa-complement.cc
	rra/rrt.cc
)

set_target_properties(libmata PROPERTIES
  OUTPUT_NAME mata
  CLEAN_DIRECT_OUTPUT 1
)

add_executable(tests
	tests-main.cc
	tests-parser.cc
	tests-parser-dispatch.cc
	tests-re2parser.cc
	tests-vm.cc
	tests-vm-dispatch.cc
	afa/tests-afa.cc
	nfa/tests-nfa.cc
	nfa/tests-nfa-dispatch.cc
	rra/tests-rrt.cc
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	target_link_libraries(libmata pthread)
endif()

target_link_libraries(libmata simlib)
target_link_libraries(tests libmata re2 simlib)
include(Catch)
catch_discover_tests(tests)
add_custom_command(
	TARGET tests POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/unit-test-data
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/unit-test-data/ ${CMAKE_BINARY_DIR}/src/unit-test-data/
)

if(FALSE)

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
else()    # for appleOS
	add_test(
		NAME "catch_tests_valgrind"
		COMMAND ${VALGRIND_PROGRAM}
			--tool=memcheck
			--leak-check=yes
			--show-reachable=yes
			--error-exitcode=1
			--suppressions=${CMAKE_SOURCE_DIR}/darwin17-7.suppressions
			--gen-suppressions=all
			${CMAKE_CURRENT_BINARY_DIR}/tests
	)
endif()

endif()
