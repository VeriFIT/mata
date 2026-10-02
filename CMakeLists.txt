cmake_minimum_required (VERSION 3.15.0)

# The source of truth for the version is the `VERSION` file that CI (and the Python sdist) writes.
#  Outside a release checkout, fall back to the latest git tag, and to 0.0.0 when there is neither.
#  Tags look like `v1.32.45-dev.25`, while `project(VERSION)` accepts numeric components only, so
#  keep just `<major>.<minor>.<patch>`.
set (MATA_VERSION_RAW "")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
	file (READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MATA_VERSION_RAW)
else ()
	find_program (MATA_GIT_EXECUTABLE git)
	if (MATA_GIT_EXECUTABLE)
		execute_process (
			COMMAND "${MATA_GIT_EXECUTABLE}" describe --tags --abbrev=0
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			OUTPUT_VARIABLE MATA_VERSION_RAW
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
	endif ()
endif ()
if (MATA_VERSION_RAW MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
	set (MATA_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else ()
	set (MATA_VERSION "0.0.0")
endif ()

project (
	libMATA
	VERSION ${MATA_VERSION}
	DESCRIPTION "A fast and simple automata library"
	LANGUAGES C CXX) # C is needed for CUDD

if (EXISTS "${PROJECT_SOURCE_DIR}/VERSION")
	set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/VERSION")
endif ()

# Without this, a plain `cmake -B build && cmake --build build` compiles libmata with no
#  optimisation and with asserts enabled. The Makefile always passes a type, so this only affects
#  generic CMake workflows; `add_subdirectory()` consumers keep their own type.
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	message (STATUS "No build type selected, defaulting to Release")
	set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

# Mata uses C++23. Setting CMAKE_CXX_STANDARD to 23 is not enough. The compiler has to actually
# implement the feature. Fail here with a readable message instead of letting it surface during compilation.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
	message (FATAL_ERROR
		"Mata requires GCC >= 14 (found ${CMAKE_CXX_COMPILER_VERSION}). "
		"Install a newer compiler and select it, e.g. `CXX=g++-14 cmake ...`.")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18)
	message (FATAL_ERROR
		"Mata requires Clang >= 18 (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}) for C++23. "
		"Note AppleClang version numbers differ from upstream LLVM.")
endif ()

# 3rd party modules
set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option (MATA_WERROR "Warnings should be handled as errors" OFF)
option (MATA_ENABLE_COVERAGE "Build with coverage compiler flags" OFF)

# Registering the install rules unconditionally means a `FetchContent`/`add_subdirectory()` consumer
#  installs libmata.a and the Mata, CUDD and simlib headers along with its own artefacts.
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	set (MATA_INSTALL_DEFAULT ON)
else ()
	set (MATA_INSTALL_DEFAULT OFF)
endif ()
option (MATA_INSTALL "Generate install rules for Mata" ${MATA_INSTALL_DEFAULT})

# Only do these if this is the main project, and not if it is included through add_subdirectory
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	# Store compile commands into 'build/compile_commands.json', which are needed, beside others, for linters.
	set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

	option (MATA_BUILD_EXAMPLES "Build Mata examples" ON)

	message ("-- Default C++ compiler: ${CMAKE_CXX_COMPILER}")

	include (FetchContent)

	### CCACHE ###
	# TODO: Should we use CCACHE? Why it might be useful: https://stackoverflow.com/questions/10136761/when-is-the-case-to-use-ccache
	# Why we use the following code: https://stackoverflow.com/questions/64592095/cmake-ccache-rule-launch-compile-or-cmake-lang-compiler-launcher
	find_program (CCACHE_PROGRAM ccache)
	if (CCACHE_PROGRAM)
		set (CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
	endif ()

	### Doxygen - voluntary ###
	find_package (Doxygen)
	if (DOXYGEN_FOUND)
		configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
			${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY
		)
		add_custom_target (docs
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating API documentation with Doxygen" VERBATIM
		)
	else ()
		message (WARNING "Doxygen not found. Documentation will not be built")
	endif ()

	### Testing ###
	include (CTest) # also enables testing, if it was not disabled from command line

	if (BUILD_TESTING)
		# Catch2 library for writing tests
		if (FETCHCONTENT_FULLY_DISCONNECTED)
			find_package (Catch2 REQUIRED)
		else ()
			fetchcontent_declare (
				Catch2
				GIT_REPOSITORY https://github.com/catchorg/Catch2.git
				GIT_TAG v3.16.0
			)
			fetchcontent_makeavailable (Catch2)
			list (APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
			get_target_property (catch2_includes Catch2 INTERFACE_INCLUDE_DIRECTORIES)
			target_include_directories (Catch2 SYSTEM INTERFACE ${catch2_includes})
		endif ()
		include (Catch) # Adds Catch2::Catch2WithMain and Catch2::Catch2
	endif ()

endif ()

# -fPIC, needed for python binding
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# Common warnings compiler flags.
set (MATA_COMMON_WARNINGS
	-Wextra
	-Wall
	-Wfloat-equal
	-Wctor-dtor-privacy
	-Weffc++
	-Woverloaded-virtual
	-Wold-style-cast
	-Wunused-parameter
	-Wsign-compare
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-Wreturn-type
)
# Clang-specific warnings compiler flags.
set (MATA_CLANG_WARNINGS
	-Winconsistent-missing-override
	-Wgnu-anonymous-struct
	-Wnested-anon-types
	-Wno-system-headers
)

# Set whether to treat warnings as errors.
if (MATA_WERROR)
	message ("-- Treating warnings in mata as errors")
	list (APPEND MATA_COMMON_WARNINGS -Werror)
endif ()

# Attach Mata's warning flags to a target in every build configuration.
# Do not guard these with `$<CONFIG:DEBUG>`/`$<CONFIG:RELEASE>`: that silently drops the warnings
#  (and `-Werror`) from `RelWithDebInfo`, `MinSizeRel` and from builds with no configuration set.
function (mata_add_warnings TARGET)
	target_compile_options (${TARGET} PRIVATE
		${MATA_COMMON_WARNINGS}
		"$<$<CXX_COMPILER_ID:Clang,AppleClang>:${MATA_CLANG_WARNINGS}>"
	)
endfunction ()

# if DEBUG, also test coverage
if (MATA_ENABLE_COVERAGE)
	if (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
		message (WARNING "Code coverage results with an optimised (non-Debug) build may be misleading")
	endif ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage")
endif ()

# Get git hash (used in src/)
include (GetGitRevisionDescription)
get_git_head_revision (GIT_REFSPEC GIT_SHA1)


##### SUBDIRECTORIES #####
add_subdirectory (src)
add_subdirectory (3rdparty/re2 EXCLUDE_FROM_ALL)
add_subdirectory (3rdparty/simlib EXCLUDE_FROM_ALL)
add_subdirectory (3rdparty/cudd EXCLUDE_FROM_ALL)

if ((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) AND MATA_BUILD_EXAMPLES)
	message ("-- Building examples")
	add_subdirectory (examples)
endif ()

# Build tests only if Mata is the main project and we enabled testing
if ((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) AND BUILD_TESTING)
	message ("-- Building tests")
	add_subdirectory (tests)
	add_subdirectory (tests-integration)
endif ()


##### INSTALLING AND UNINSTALLING #####
if (MATA_INSTALL)
	include (GNUInstallDirs)
	include (CMakePackageConfigHelpers)

	install (
		TARGETS libmata cudd_headers simlib_headers
		EXPORT mataTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
	# TODO: should headers be installed in some nicer way? there is something called FILE_SET in cmake, but I do not feel it will make it better
	install (
		DIRECTORY
			${PROJECT_SOURCE_DIR}/include/
			${PROJECT_SOURCE_DIR}/3rdparty/cudd/include/ # TODO remove this after we remove PUBLIC dependency on CUDD
			${PROJECT_SOURCE_DIR}/3rdparty/simlib/include/ # TODO remove this after we remove PUBLIC dependency on simlib
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)
	install (
		EXPORT mataTargets
		NAMESPACE mata::
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mata
	)

	configure_package_config_file (cmake/mataConfig.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/mataConfig.cmake"
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mata
	)
	# Without this file `find_package(mata 1 REQUIRED)` reports the installed package as
	#  "version: unknown" and rejects it.
	write_basic_package_version_file (
		"${CMAKE_CURRENT_BINARY_DIR}/mataConfigVersion.cmake"
		COMPATIBILITY SameMajorVersion
	)
	install (
		FILES
			"${CMAKE_CURRENT_BINARY_DIR}/mataConfig.cmake"
			"${CMAKE_CURRENT_BINARY_DIR}/mataConfigVersion.cmake"
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mata
	)
endif ()


# Uninstall target from https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
# Add it only if we are building mata as the main project
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT TARGET uninstall)
	configure_file (
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
		IMMEDIATE @ONLY)

	add_custom_target (uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif ()
