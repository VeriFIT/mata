cmake_minimum_required(VERSION 3.15.0)
project(
    libMATA
    #VERSION ???? TODO: set the version automatically during merging
    DESCRIPTION "A fast and simple automata library"
    LANGUAGES C CXX)

# 3rd party modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# -fPIC, needed for python binding
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
    # include(CodeCoverage)
    # setup_target_for_coverage(${PROJECT_NAME}_coverage tests coverage)
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

include(CTest) # also enables testing
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.9 # TODO: maybe we should move to v3
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
include(Catch)

##############################################################################
#                                SUB-MODULES
##############################################################################

# Process subdirectories
add_subdirectory(src)
add_subdirectory(3rdparty/re2 EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simlib EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cudd EXCLUDE_FROM_ALL)

option(MATA_BUILD_EXAMPLES "MATA_BUILD_EXAMPLES" ON)
if(MATA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests only if Mata is the main project (can be overriden by MATA_BUILD_TESTING)
# and we enabled testing
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MATA_BUILD_TESTING) AND BUILD_TESTING)
    add_subdirectory(tests)
    add_subdirectory(tests-integration)
endif()

install(TARGETS libmata
        ARCHIVE DESTINATION lib)
# TODO: should headers be installed in some nicer way? there is something called FILE_SET in cmake, but I do not feel it will make it better
get_target_property(LIBMATA_PUBLIC_INCLUDES libmata INTERFACE_INCLUDE_DIRECTORIES)
install(DIRECTORY "${LIBMATA_PUBLIC_INCLUDES}" TYPE INCLUDE)
# TODO remove this after we remove PUBLIC dependency on CUDD
get_target_property(CUDD_PUBLIC_INCLUDES cudd INTERFACE_INCLUDE_DIRECTORIES)
install(DIRECTORY "${CUDD_PUBLIC_INCLUDES}" TYPE INCLUDE)
# TODO remove this after we remove PUBLIC dependency on simlib
get_target_property(SIMLIB_PUBLIC_INCLUDES simlib INTERFACE_INCLUDE_DIRECTORIES)
install(DIRECTORY "${SIMLIB_PUBLIC_INCLUDES}" TYPE INCLUDE)

# Uninstall target from https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
