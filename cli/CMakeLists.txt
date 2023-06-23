set(CMAKE_COLOR_MAKEFILE ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(mata-cli)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

# MATA headers
include_directories(../include)

# generate the file with version names
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.cc.in
   ${CMAKE_CURRENT_BINARY_DIR}/version.cc @ONLY)

add_executable(mata-code
	interpreter.cc
	mata-code.cc
  ${CMAKE_CURRENT_BINARY_DIR}/version.cc
)

target_link_libraries(mata-code libmata)

# Add common compile warnings.
target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${COMMON_WARNINGS}>")
target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options(libmata PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_WARNINGS}>")
	target_compile_options(libmata PRIVATE "$<$<CONFIG:RELEASE>:${CLANG_WARNINGS}>")
endif()

target_compile_options(libmata PRIVATE "${COMMON_COMPILER_FLAGS}")
