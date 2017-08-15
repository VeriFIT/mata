cmake_minimum_required(VERSION 2.8.2)

set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(vata2-cli)

# VATA2 headers
include_directories(../include)

# generate the file with version names
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.cc.in
   ${CMAKE_CURRENT_BINARY_DIR}/version.cc @ONLY)

add_executable(vata
	vata.cc
  ${CMAKE_CURRENT_BINARY_DIR}/version.cc
)
