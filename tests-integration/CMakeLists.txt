# Instrument the integration benchmarks for gcov. Off by default: the previous unconditional
#  `-fprofile-arcs` was never paired with `-ftest-coverage`, so no `.gcno` files were produced and
#  the `.gcda` files every run writes at exit were useless to gcov and only littered the build tree.
option (MATA_BENCH_PROFILING "Instrument the integration benchmarks for gcov" OFF)

file (GLOB JOBS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/jobs/*.yaml.in")
file (GLOB INPUTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/inputs/*.input.in")

# Generate into the build directory. These files embed `CMAKE_CURRENT_BINARY_DIR`, so writing them
#  into the source tree made every configured build directory overwrite the other's job files: after
#  configuring a Debug build, `make test-performance` on the Release build measured the Debug
#  binaries and said nothing about it. It also made configuring from a read-only source tree fail.
foreach (JOB ${JOBS})
	get_filename_component (TARGET_JOB ${JOB} NAME_WE)
	configure_file ("${JOB}" "${CMAKE_CURRENT_BINARY_DIR}/jobs/${TARGET_JOB}.yaml" @ONLY)
	list (APPEND STALE_IN_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/jobs/${TARGET_JOB}.yaml")
endforeach ()
foreach (INPUT ${INPUTS})
	get_filename_component (TARGET_INPUT ${INPUT} NAME_WE)
	configure_file ("${INPUT}" "${CMAKE_CURRENT_BINARY_DIR}/inputs/${TARGET_INPUT}.input" @ONLY)
	list (APPEND STALE_IN_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/inputs/${TARGET_INPUT}.input")
endforeach ()

configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh.in" "${CMAKE_CURRENT_BINARY_DIR}/src/utils/config.hh" @ONLY
)
list (APPEND STALE_IN_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh")

# A working tree configured before the files above moved into the build directory still carries the
#  in-source copies. They used to be covered by `.gitignore`; they no longer are, so without this
#  they would show up as untracked files, and the stale `config.hh` would still be the one found by
#  a build that predates the include-path change. Only generated files are removed here; their
#  `*.in` templates are what the repository tracks.
file (REMOVE ${STALE_IN_SOURCE_FILES})

file (GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)

list (FILTER SOURCES EXCLUDE REGEX ".*/tests-integration/src/utils/.*")
foreach (SOURCE ${SOURCES})
	get_filename_component (TARGET ${SOURCE} NAME_WE)

	get_filename_component (TARGET_PATH ${SOURCE} DIRECTORY)
	file (RELATIVE_PATH TARGET_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/" "${TARGET_PATH}")

	add_executable (${TARGET} ${SOURCE} src/utils/utils.cc)
	# `CMAKE_RUNTIME_OUTPUT_DIRECTORY` is read when a target is created, so setting it after
	#  `add_executable()` gave every target the directory computed for the previous source file of
	#  the sorted glob, and the last target none at all.
	set_target_properties (${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_PATH}")
	target_link_libraries (${TARGET} libmata)
	# For the generated `utils/config.hh`.
	target_include_directories (${TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")

	if (MATA_BENCH_PROFILING)
		target_compile_options (${TARGET} PRIVATE -g --coverage)
		target_link_options (${TARGET} PRIVATE --coverage)
	endif ()

	# Add Mata's compile warnings.
	mata_add_warnings (${TARGET})

	# The benchmarks measure the library, so they must be built with the same optimization settings.
	mata_add_optimizations (${TARGET})
endforeach ()
