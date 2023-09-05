cmake_minimum_required(VERSION 3.15.0)
project(
    libMATA
    #VERSION ???? TODO: set the version automatically during merging
    DESCRIPTION "A fast and simple automata library"
    LANGUAGES C CXX) # C is needed for CUDD

# 3rd party modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(MATA_WERROR "Warnings should be handled as errors" OFF)
option(MATA_ENABLE_COVERAGE "Build with coverage compiler flags" OFF)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Store compile commands into 'build/compile_commands.json', which are needed, beside others, for linters.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    option(MATA_BUILD_EXAMPLES "Build Mata examples" ON)

    message("-- Default C++ compiler: ${CMAKE_CXX_COMPILER}")

    include(FetchContent)

    ### CCACHE ###
    # TODO: Should we use CCACHE? Why it might be useful: https://stackoverflow.com/questions/10136761/when-is-the-case-to-use-ccache
    # Why we use the following code: https://stackoverflow.com/questions/64592095/cmake-ccache-rule-launch-compile-or-cmake-lang-compiler-launcher
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()

    ### Doxygen - voluntary ###
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

    ### Testing ###
    include(CTest) # also enables testing, if it was not disabled from command line

    if (BUILD_TESTING)
        # Catch2 library for writing tests
        FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.9 # TODO: maybe we should move to v3
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
        include(Catch)
        # Adds Catch2::Catch2
    endif()

endif()

# -fPIC, needed for python binding
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Common warnings compiler flags.
set(MATA_COMMON_WARNINGS
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
        -Wconversion
        -Wsign-conversion
        -Wreturn-type
        )
# Clang-specific warnings compiler flags.
set(MATA_CLANG_WARNINGS
        -Winconsistent-missing-override
        -Wgnu-anonymous-struct
        -Wnested-anon-types
        -Wno-system-headers
        )

# Set whether to treat warnings as errors.
if (MATA_WERROR)
    message("-- Treating warnings in mata as errors")
    set(MATA_COMMON_WARNINGS "${MATA_COMMON_WARNINGS}" -Werror)
endif ()

# if DEBUG, also test coverage
if(MATA_ENABLE_COVERAGE)
    if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
        message(WARNING "Code coverage results with an optimised (non-Debug) build may be misleading")
    endif() 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage")
endif()

# Get git hash (used in src/)
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)


##### SUBDIRECTORIES #####
add_subdirectory(src)
add_subdirectory(3rdparty/re2 EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simlib EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cudd EXCLUDE_FROM_ALL)

if(MATA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests only if Mata is the main project and we enabled testing
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) AND BUILD_TESTING)
    add_subdirectory(tests)
    add_subdirectory(tests-integration)
endif()


##### INSTALLING AND UNINSTALLING #####
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
# Add it only if we are building mata as the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
