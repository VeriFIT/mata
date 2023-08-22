set(CMAKE_CXX_STANDARD 20)
set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# Export compile commands to be used with YouCompleteMe
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Run cmake with `-CNO_PROFILING` to turn off the profiling options
if(NOT NO_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs")
endif()

include_directories(../include)
include_directories(../3rdparty/) # this is here since it is clearer to include re2/utils than just utils
include_directories(../3rdparty/cudd/include)
include_directories(../3rdparty/re2)
include_directories(../3rdparty/simlib/include)

file(GLOB JOBS "${CMAKE_CURRENT_SOURCE_DIR}/jobs/*.cc.in")
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
    target_link_libraries(${TARGET} libmata re2 simlib cudd)

    # Add common compile warnings.
    target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
    target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

    # Optionally, also add Clang-specific warnings.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Using regular Clang or AppleClang.
        target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
        target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
    endif()

    target_compile_options(${TARGET} PRIVATE "${COMMON_COMPILER_FLAGS}")
endforeach()