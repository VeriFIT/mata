file (GLOB_RECURSE MATA_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_executable (tests ${MATA_TEST_SOURCES})

target_link_libraries (tests PRIVATE libmata Catch2::Catch2WithMain)

# Add common compile warnings.
target_compile_options (tests PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
target_compile_options (tests PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
    target_compile_options (tests PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
    target_compile_options (tests PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
endif ()

catch_discover_tests (tests)
