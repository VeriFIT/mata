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

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-single-param-jobs.yaml.in" "${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-single-param-jobs.yaml" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-double-param-jobs.yaml.in" "${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-double-param-jobs.yaml" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-test-jobs.yaml.in" "${CMAKE_CURRENT_SOURCE_DIR}/jobs/corr-test-jobs.yaml" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inputs/single-automata.input.in" "${CMAKE_CURRENT_SOURCE_DIR}/inputs/single-automata.input" @ONLY)
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