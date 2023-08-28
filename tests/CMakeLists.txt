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
		nfa/delta.cc
		nfa/nfa.cc
		nfa/builder.cc
		nfa/nfa-concatenation.cc
		nfa/nfa-intersection.cc
		nfa/nfa-profiling.cc
		nfa/nfa-plumbing.cc
		strings/nfa-noodlification.cc
		strings/nfa-segmentation.cc
		strings/nfa-string-solving.cc
)

target_link_libraries(tests PRIVATE libmata Catch2::Catch2)

# Add common compile warnings.
target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(tests PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
    target_compile_options(tests PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
endif()

catch_discover_tests(tests)
