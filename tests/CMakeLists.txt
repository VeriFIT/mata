file (GLOB_RECURSE MATA_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_executable (tests ${MATA_TEST_SOURCES})

target_link_libraries (tests PRIVATE libmata Catch2::Catch2WithMain)

# Add Mata's compile warnings.
mata_add_warnings (tests)

catch_discover_tests (tests)
