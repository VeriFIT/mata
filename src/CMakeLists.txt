# create config.cc file that contains definitions of global variables (such as version, verbosity, etc.)
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/config.cc.in" "${CMAKE_CURRENT_BINARY_DIR}/config.cc" @ONLY)

# create glob all source files with .cc and .hh extensions
file (GLOB_RECURSE MATA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# add_library(libmata SHARED
add_library (libmata STATIC
	${MATA_SOURCES}
	"${CMAKE_CURRENT_BINARY_DIR}/config.cc"
	# we add files from the libraries here, so we create one static library libmata.a with everything and so that we can export only libmata
	$<TARGET_OBJECTS:cudd>
	$<TARGET_OBJECTS:simlib>
	$<TARGET_OBJECTS:re2>
)

# libmata needs at least c++20
target_compile_features (libmata PUBLIC cxx_std_20)

set_target_properties (libmata PROPERTIES
	OUTPUT_NAME mata
)

# Just to be safe, RE2 might need it, for WASM it is also needed
find_package(Threads REQUIRED)
target_link_libraries(libmata PRIVATE Threads::Threads)

target_link_libraries(libmata PUBLIC cudd_headers simlib_headers)

target_include_directories (libmata 
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>

	# we need to add include files from the libraries
	PRIVATE
		$<TARGET_PROPERTY:cudd,INTERFACE_INCLUDE_DIRECTORIES>
		$<TARGET_PROPERTY:simlib,INTERFACE_INCLUDE_DIRECTORIES>
		$<TARGET_PROPERTY:re2,INTERFACE_INCLUDE_DIRECTORIES>
)

# Add common compile warnings.
target_compile_options (libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_COMMON_WARNINGS}>")
target_compile_options (libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_COMMON_WARNINGS}>")

# Optionally, also add Clang-specific warnings.
if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") # Using regular Clang or AppleClang.
	target_compile_options (libmata PRIVATE "$<$<CONFIG:DEBUG>:${MATA_CLANG_WARNINGS}>")
	target_compile_options (libmata PRIVATE "$<$<CONFIG:RELEASE>:${MATA_CLANG_WARNINGS}>")
endif ()
