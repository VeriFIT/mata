cmake_minimum_required(VERSION 3.15.0)
project(
    libMATA
    #VERSION ???? TODO: set the version automatically during merging
    DESCRIPTION "A fast and simple automata library"
    LANGUAGES C CXX)

# 3rd party modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


# Only do these if this is the main project, and not if it is included through add_subdirectory
# if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

    ############################# CCACHE ######################################
    # TODO: Should we use CCACHE? Why it might be useful: https://stackoverflow.com/questions/10136761/when-is-the-case-to-use-ccache
    # Why we use the following code: https://stackoverflow.com/questions/64592095/cmake-ccache-rule-launch-compile-or-cmake-lang-compiler-launcher
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()

##############################################################################
#                             OUTPUT CUSTOMIZATION
##############################################################################

# Common compiler flags.
set(COMMON_COMPILER_FLAGS
        -fms-extensions
        -fdiagnostics-show-option
        -fno-strict-aliasing
        )

# Common warnings compiler flags.
set(COMMON_WARNINGS
        -Wextra
        -Wall
        -Wfloat-equal
        -Wctor-dtor-privacy
        -Weffc++
        -Woverloaded-virtual
        -Wold-style-cast
        -Wunused-parameter
        -Wsign-compare
        -Wunused-parameter
        -Wpedantic
        #-pedantic-errors
        -Wconversion
        -Wsign-conversion
        -Wreturn-type
        )
# Clang-specific warnings compiler flags.
set(CLANG_WARNINGS
        -Winconsistent-missing-override
        -Wgnu-anonymous-struct
        -Wnested-anon-types
        -Wno-system-headers
        )

# Set whether to treat warnings as errors.
option(WERROR "WERROR" OFF)
message("-- Treating warnings as errors: ${WERROR}")
if (WERROR)
    set(COMMON_WARNINGS "${COMMON_WARNINGS}" -Werror)
endif ()

##############################################################################
#                                COVERAGE
##############################################################################

# if DEBUG, also test coverage
if(CMAKE_BUILD_TYPE MATCHES "Coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
    include(CodeCoverage)
    setup_target_for_coverage(${PROJECT_NAME}_coverage tests coverage)
endif()

##############################################################################
#                                DEPENDENCIES
##############################################################################

# find_package(Valgrind)

# Doxygen - voluntary
find_package(Doxygen)
if (DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
       ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY
    )
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
else()
    message(WARNING "Doxygen not found. Documentation will not be built")
endif()

##############################################################################
#                               COMPILER FUN
##############################################################################

# get the git revision
include(GetGitRevisionDescription)
git_describe(GIT_DESCRIBE)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

message("-- Default C++ compiler: ${CMAKE_CXX_COMPILER}")

##############################################################################
#                                 PROJECT
##############################################################################

# Include CTest so that sophisticated testing can be done now
include(CTest)
enable_testing()

##############################################################################
#                                SUB-MODULES
##############################################################################

# Process subdirectories
add_subdirectory(src)
add_subdirectory(3rdparty/re2 EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simlib EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cudd EXCLUDE_FROM_ALL)
add_subdirectory(tests)
if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/tests-integration")
    add_subdirectory(tests-integration)
endif()

install(TARGETS libmata
        ARCHIVE DESTINATION lib)
# TODO: should headers be installed in some nicer way? 
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/mata DESTINATION include FILES_MATCHING PATTERN "**/*.hh")
# TODO remove this after we remove PUBLIC dependency on CUDD
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cudd/include/mata DESTINATION include FILES_MATCHING PATTERN "*.h*")
