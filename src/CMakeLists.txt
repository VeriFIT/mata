# uncomment for profiling
#add_definitions(-pg)
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")


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

target_include_directories(libmata PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(libmata PUBLIC cudd)
target_link_libraries(libmata PRIVATE re2 simlib)

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
		COMMAND ${CMAKE_SOURCE_DIR}/scripts/merge_static_libraries.sh libcombined.a ${CMAKE_BINARY_DIR}/src/libmata.a
		${CMAKE_BINARY_DIR}/3rdparty/simlib/libsimlib.a ${CMAKE_BINARY_DIR}/3rdparty/re2/libre2.a
		${CMAKE_BINARY_DIR}/3rdparty/cudd/libcudd.a 2>&1
		COMMAND mv ${CMAKE_BINARY_DIR}/src/libcombined.a ${CMAKE_BINARY_DIR}/src/libmata.a 2>&1
)
