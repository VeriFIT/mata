# Run cmake with `-CNO_PROFILING` to turn off the profiling options
if(NOT NO_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs")
endif()

file(GLOB JOBS "${CMAKE_CURRENT_SOURCE_DIR}/jobs/*.yaml.in")
file(GLOB INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/inputs/*.input.in")
foreach(JOB ${JOBS})
    get_filename_component(TARGET_JOB ${JOB} NAME_WE)
    configure_file("${JOB}" "${CMAKE_CURRENT_SOURCE_DIR}/jobs/${TARGET_JOB}.yaml" @ONLY)
endforeach()
foreach(INPUT ${INPUTS})
    get_filename_component(TARGET_INPUT ${INPUT} NAME_WE)
    configure_file("${INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/inputs/${TARGET_INPUT}.input" @ONLY)
endforeach()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inputs/double-automata.input.in" "${CMAKE_CURRENT_SOURCE_DIR}/inputs/double-automata.input" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.hh" @ONLY)

file(GLOB SOURCES "src/*.cc")
foreach(SOURCE ${SOURCES})
    get_filename_component(TARGET ${SOURCE} NAME_WE)

    add_executable(${TARGET}
        ${SOURCE}
        src/utils/utils.cc
    )
    target_link_libraries(${TARGET} libmata)

    # Add common compile warnings.
    target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
    target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

    # Optionally, also add Clang-specific warnings.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
        target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
        target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
    endif()
endforeach()
