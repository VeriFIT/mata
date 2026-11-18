# `config.cc` embeds the hash of the git HEAD, which changes with every commit and every checkout.
#  Generating it with `configure_file()` here made `${GIT_DIR}/HEAD` a configure dependency of every
#  build directory, so the next build after any such change re-ran the whole configure step before
#  compiling anything. The script below runs as a build step and rewrites `config.cc` only when the
#  hash really changed, so a new HEAD recompiles one object instead.
set (MATA_CONFIG_CC "${CMAKE_CURRENT_BINARY_DIR}/config.cc")
add_custom_target (mata-config-cc
	BYPRODUCTS "${MATA_CONFIG_CC}"
	COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
		"-DCONFIG_IN=${CMAKE_CURRENT_SOURCE_DIR}/config.cc.in"
		"-DCONFIG_OUT=${MATA_CONFIG_CC}"
		-P "${PROJECT_SOURCE_DIR}/cmake/GetGitSha.cmake"
	COMMENT "Checking the git revision embedded in libmata"
	VERBATIM
)
set_source_files_properties ("${MATA_CONFIG_CC}" PROPERTIES GENERATED TRUE)

# create glob all source files with .cc and .hh extensions
file (GLOB_RECURSE MATA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file (GLOB_RECURSE MATA_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.hh"
	  "${PROJECT_SOURCE_DIR}/include/*.tpp"
)

# add_library(libmata SHARED
add_library (
	libmata STATIC
	${MATA_SOURCES}
	"${MATA_CONFIG_CC}"
	# we add files from the libraries here, so we create one static library libmata.a with everything and so that we can export only libmata
	$<TARGET_OBJECTS:cudd>
	$<TARGET_OBJECTS:simlib>
	$<TARGET_OBJECTS:re2>
)

# Same spelling for `add_subdirectory()`/`FetchContent` consumers and for `find_package(mata)` ones.
add_library (mata::libmata ALIAS libmata)

# The build step above has to run before any object of `libmata` is compiled: `config.cc` is one of
#  its sources.
add_dependencies (libmata mata-config-cc)

target_compile_features (libmata PUBLIC cxx_std_23)

set_target_properties (libmata PROPERTIES OUTPUT_NAME mata)

# Just to be safe, RE2 might need it, for WASM it is also needed
find_package (Threads REQUIRED)
target_link_libraries (libmata PRIVATE Threads::Threads)

target_link_libraries (libmata PUBLIC cudd_headers simlib_headers)

if (MATA_USE_MIMALLOC)
	target_link_libraries (libmata PRIVATE mimalloc)
endif ()

if (MATA_DELTA_ALLOCATOR STREQUAL "pool")
	set_source_files_properties (
		"${CMAKE_CURRENT_SOURCE_DIR}/delta-allocator.cc" PROPERTIES COMPILE_DEFINITIONS
																	MATA_DELTA_ALLOCATOR_DEFAULT_POOL
	)
endif ()

target_include_directories (
	libmata
	PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
		   $<INSTALL_INTERFACE:include>
	# we need to add include files from the libraries
	PRIVATE $<TARGET_PROPERTY:cudd,INTERFACE_INCLUDE_DIRECTORIES>
			$<TARGET_PROPERTY:simlib,INTERFACE_INCLUDE_DIRECTORIES>
			$<TARGET_PROPERTY:re2,INTERFACE_INCLUDE_DIRECTORIES>
)

# The installed headers: the sources, and the generated `mata/config.hh`.
target_sources (
	libmata
	PUBLIC FILE_SET
		   HEADERS
		   BASE_DIRS
		   ${PROJECT_SOURCE_DIR}/include
		   ${PROJECT_BINARY_DIR}/include
		   FILES
		   ${MATA_HEADERS}
		   ${PROJECT_BINARY_DIR}/include/mata/config.hh
)

# Add Mata's compile warnings.
mata_add_warnings (libmata)

# Add the opt-in optimization settings (IPO, `-march=native`).
mata_add_optimizations (libmata)

if (MATA_EXPENSIVE_ASSERTS)
	target_compile_definitions (libmata PUBLIC MATA_EXPENSIVE_ASSERTS)
endif ()
