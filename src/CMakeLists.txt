# create config.cc file that contains definitions of global variables (such as version, verbosity, etc.)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.cc.in" "${CMAKE_CURRENT_BINARY_DIR}/config.cc" @ONLY)



add_library(libmata STATIC
# add_library(libmata SHARED
	alphabet.cc
	"${CMAKE_CURRENT_BINARY_DIR}/config.cc"
	inter-aut.cc
	mintermization.cc
	parser.cc
	re2parser.cc
	nfa/nfa.cc
	nfa/inclusion.cc
	nfa/universal.cc
	nfa/complement.cc
	nfa/product.cc
	nfa/concatenation.cc
	strings/nfa-noodlification.cc
	strings/nfa-segmentation.cc
	strings/nfa-strings.cc
	nfa/delta.cc
	nfa/operations.cc
	nfa/builder.cc
	nfa/learning.cc

	nft/nft.cc
	nft/inclusion.cc
	nft/universal.cc
	nft/complement.cc
	nft/composition.cc
	nft/intersection.cc
	nft/concatenation.cc
	nft/operations.cc
	nft/builder.cc
	nft/strings.cc
)

# libmata needs at least c++20
target_compile_features(libmata PUBLIC cxx_std_20)

set_target_properties(libmata PROPERTIES
  OUTPUT_NAME mata
)

target_include_directories(libmata PUBLIC "${PROJECT_SOURCE_DIR}/include/")

# For the case of WASM build we need to link with pthread
if (EMSCRIPTEN)
	target_link_libraries(libmata PRIVATE pthread)
endif()

target_link_libraries(libmata PUBLIC cudd simlib)
target_link_libraries(libmata PRIVATE re2)

# Add common compile warnings.
target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
	target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
endif()
