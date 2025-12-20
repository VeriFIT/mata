# create config.cc file that contains definitions of global variables (such as version, verbosity, etc.)
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/config.cc.in" "${CMAKE_CURRENT_BINARY_DIR}/config.cc" @ONLY)

# create glob all source files with .cc and .hh extensions
file (GLOB_RECURSE MATA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# add_library(libmata SHARED
add_library (libmata STATIC
	${MATA_SOURCES}
	"${CMAKE_CURRENT_BINARY_DIR}/config.cc"
)

# libmata needs at least c++20
target_compile_features (libmata PUBLIC cxx_std_20)

set_target_properties (libmata PROPERTIES
	OUTPUT_NAME mata
)

target_include_directories (libmata PUBLIC "${PROJECT_SOURCE_DIR}/include/")

# For the case of WASM build we need to link with pthread
if (EMSCRIPTEN)
	target_link_libraries (libmata PRIVATE pthread)
endif ()

target_link_libraries (libmata PUBLIC cudd simlib)
target_link_libraries (libmata PRIVATE re2)

# Add common compile warnings.
target_compile_options (libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
target_compile_options (libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options (libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
	target_compile_options (libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
endif ()
