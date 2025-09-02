add_executable(tests
		ord-vector.cc
		sparse-set.cc
		synchronized-iterator.cc
		two-dimensional-map.cc
		alphabet.cc
		parser.cc
		re2parser.cc
		mintermization.cc
		nfa/delta.cc
		nfa/nfa.cc
		nfa/builder.cc
		nfa/nfa-concatenation.cc
		nfa/nfa-product.cc
		nfa/nfa-profiling.cc
		nfa/nfa-plumbing.cc
		nft/delta.cc
		nft/nft.cc
		nft/builder.cc
		nft/nft-composition.cc
		nft/nft-concatenation.cc
		nft/nft-intersection.cc
		nft/nft-profiling.cc
		nft/nft-plumbing.cc
		strings/noodlification.cc
		strings/segmentation.cc
		strings/string-solving.cc
		strings/replace.cc
)

target_link_libraries(tests PRIVATE libmata Catch2::Catch2WithMain)

# Add common compile warnings.
target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
    target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
endif()

catch_discover_tests(tests)
