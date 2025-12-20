# Run cmake with `-CNO_PROFILING` to turn off the profiling options
if (NOT NO_PROFILING)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs")
endif ()

file (GLOB JOBS "${CMAKE_CURRENT_SOURCE_DIR}/jobs/*.yaml.in")
file (GLOB INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/inputs/*.input.in")
foreach (JOB ${JOBS})
	get_filename_component (TARGET_JOB ${JOB} NAME_WE)
	configure_file ("${JOB}" "${CMAKE_CURRENT_SOURCE_DIR}/jobs/${TARGET_JOB}.yaml" @ONLY)
endforeach ()
foreach (INPUT ${INPUTS})
	get_filename_component (TARGET_INPUT ${INPUT} NAME_WE)
	configure_file ("${INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/inputs/${TARGET_INPUT}.input" @ONLY)
endforeach ()

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/inputs/double-automata.input.in" "${CMAKE_CURRENT_SOURCE_DIR}/inputs/double-automata.input" @ONLY)

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh" @ONLY)

file (GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)

list (FILTER SOURCES EXCLUDE REGEX ".*/tests-integration/src/utils/.*")
foreach (SOURCE ${SOURCES})
	get_filename_component (TARGET ${SOURCE} NAME_WE)

	get_filename_component (TARGET_PATH ${SOURCE} DIRECTORY)
	file (RELATIVE_PATH TARGET_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/" "${TARGET_PATH}")

	add_executable (${TARGET} ${SOURCE} src/utils/utils.cc)
	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_PATH})
	target_link_libraries (${TARGET} libmata)

	# Add common compile warnings.
	target_compile_options (${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
	target_compile_options (${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

	# Optionally, also add Clang-specific warnings.
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
		target_compile_options (${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
		target_compile_options (${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
	endif ()
endforeach ()
