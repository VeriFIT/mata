file (GLOB_RECURSE MATA_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# Every header below, relative to include/mata, has to compile on its own.
#  A header that leans on another being included first fails here
#  rather than at a user who includes them in a different order.
foreach (
	header IN
	ITEMS core/traits
		  core/concepts
		  core/transition
		  core/visits
		  core/moves
		  core/post
		  core/cursor
		  core/delta
		  core/tarjan
		  core/automaton
		  relation
		  synchronized-symbol-post-iterator
		  types
		  utils/arg-of
)
	set (header_check "${CMAKE_CURRENT_BINARY_DIR}/header-check/${header}.cc")
	# Rewritten whenever its content changes, so an existing build directory follows edits of this list.
	file (CONFIGURE OUTPUT "${header_check}" CONTENT "#include \"mata/${header}.hh\"\n")
	list (APPEND MATA_TEST_SOURCES "${header_check}")
endforeach ()

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
