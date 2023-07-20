set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# Export compile commands to be used with YouCompleteMe
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# if DEBUG, also test coverage
if(CMAKE_BUILD_TYPE MATCHES "Coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

# uncomment for profiling
#add_definitions(-pg)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

include_directories(../include)
include_directories(../3rdparty/) # this is here since it is clearer to include re2/utils than just utils
include_directories(../3rdparty/cudd/include)
include_directories(../3rdparty/re2)
include_directories(../3rdparty/simlib/include)

add_executable(tests
		ord-vector.cc
		sparse-set.cc
		synchronized-iterator.cc
		main.cc
		alphabet.cc
		parser.cc
		re2parser.cc
		mintermization.cc
		afa/afa.cc
		nfa/nfa.cc
		nfa/builder.cc
		nfa/nfa-concatenation.cc
		nfa/nfa-intersection.cc
		nfa/nfa-profiling.cc
		strings/nfa-noodlification.cc
		strings/nfa-segmentation.cc
		strings/nfa-string-solving.cc
)

target_link_libraries(tests libmata re2 simlib cudd)

# Add common compile warnings.
target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
    target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
endif()

target_compile_options(tests PRIVATE "${COMMON_COMPILER_FLAGS}")

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

endif() # if(FALSE).
