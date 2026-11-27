file (GLOB_RECURSE MATA_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_executable (tests ${MATA_TEST_SOURCES})

target_link_libraries (tests PRIVATE libmata Catch2::Catch2WithMain)

# The tests read the example fixtures from the source tree, so they run from any build or working directory.
target_compile_definitions (tests PRIVATE MATA_RMC_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples/rmc")

# Add Mata's compile warnings.
mata_add_warnings (tests)

# The unit tests are the default PGO training workload, so they follow the same optimization
#  settings as the library.
mata_add_optimizations (tests)

catch_discover_tests (tests)

# The Delta backend is fixed per process, so each one gets a process of its own, whatever the build default is.
foreach (backend std pool)
	add_test (NAME "delta-allocator-${backend}" COMMAND tests "[delta-allocator]")
	set_tests_properties ("delta-allocator-${backend}" PROPERTIES ENVIRONMENT "MATA_DELTA_ALLOCATOR=${backend}")
endforeach ()
add_test (NAME delta-allocator-invalid COMMAND tests "mata::utils::DeltaAllocator returns aligned*")
set_tests_properties (
	delta-allocator-invalid PROPERTIES ENVIRONMENT "MATA_DELTA_ALLOCATOR=bogus" PASS_REGULAR_EXPRESSION
									   "MATA_DELTA_ALLOCATOR must be 'std' or 'pool'; got 'bogus'"
)
