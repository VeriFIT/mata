set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(libmata)

# Export compile commands to be used with YouCompleteMe
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# uncomment for profiling
#add_definitions(-pg)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

include_directories(../include)
include_directories(../3rdparty/) # this is here since it is clearer to include re2/utils than just utils
include_directories(../3rdparty/cudd/include)
include_directories(../3rdparty/re2)
include_directories(../3rdparty/simlib/include)

# create config.cc file that contains definitions of global variables
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.cc.in" "${CMAKE_CURRENT_BINARY_DIR}/config.cc" @ONLY)

add_library(libmata STATIC
# add_library(libmata SHARED
	alphabet.cc
	afa/afa.cc
	"${CMAKE_CURRENT_BINARY_DIR}/config.cc"
	inter-aut.cc
	mintermization.cc
	parser.cc
	re2parser.cc
	nfa/nfa.cc
	nfa/inclusion.cc
	nfa/universal.cc
	nfa/complement.cc
	nfa/intersection.cc
	nfa/concatenation.cc
	strings/nfa-noodlification.cc
	strings/nfa-segmentation.cc
	strings/nfa-strings.cc
	nfa/delta.cc
	nfa/operations.cc
	nfa/builder.cc
)

set_target_properties(libmata PROPERTIES
  OUTPUT_NAME mata
  CLEAN_DIRECT_OUTPUT 1
)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	target_link_libraries(libmata pthread)
endif()

add_dependencies(libmata cudd re2 simlib)
target_link_libraries(libmata simlib cudd)

# Add common compile warnings.
target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
	target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
endif()

target_compile_options(libmata PRIVATE "${COMMON_COMPILER_FLAGS}")

add_custom_command(TARGET libmata POST_BUILD
		COMMAND ${CMAKE_SOURCE_DIR}/merge_static_libraries.sh libcombined.a ${CMAKE_BINARY_DIR}/src/libmata.a
		${CMAKE_BINARY_DIR}/3rdparty/simlib/libsimlib.a ${CMAKE_BINARY_DIR}/3rdparty/re2/libre2.a
		${CMAKE_BINARY_DIR}/3rdparty/cudd/libcudd.a
		COMMAND mv ${CMAKE_BINARY_DIR}/src/libcombined.a ${CMAKE_BINARY_DIR}/src/libmata.a)
