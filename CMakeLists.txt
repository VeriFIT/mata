cmake_minimum_required(VERSION 2.8.2)

# 3rd party modules
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
# Enable fancy colours
set(CMAKE_COLOR_MAKEFILE ON)
# Enable verbose output for 'make'
# set(CMAKE_VERBOSE_MAKEFILE ON)

##############################################################################
#                             OUTPUT CUSTOMIZATION
##############################################################################

# Build with Clang? -- currently does nothing, initialize with
#   $ CXX=clang++ make debug
# to enable Clang
option(USE_CLANG "build with clang" OFF)
# option(USE_CLANG "build with clang" ON)

##############################################################################
#                               COMPILER FUN
##############################################################################

# enable C++
enable_language(CXX)

# get the git revision
include(GetGitRevisionDescription)
git_describe(GIT_DESCRIBE)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

message("-- Default C++ compiler: ${CMAKE_CXX_COMPILER}")

##############################################################################
#                                 PROJECT
##############################################################################

project(vata-ng)

# Include CTest so that sophisticated testing can be done now
include(CTest)
enable_testing()

if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    setup_target_for_coverage(${PROJECT_NAME}_coverage tests coverage)
endif()

##############################################################################
#                                SUB-MODULES
##############################################################################

# Process subdirectories
add_subdirectory(src)
