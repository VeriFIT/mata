# The Mata playground: Mata compiled to WebAssembly for the playground page of the website (site/playground/).
#
# Build it with Emscripten:
#
#   emcmake cmake -S bindings/wasm -B build-wasm -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-wasm --parallel
#
# The build writes mata-playground.js and mata-playground.wasm to the build directory; `just site build` copies them to
#  the page.

cmake_minimum_required (VERSION 3.23.0)
project (mata-playground LANGUAGES C CXX)

if (NOT EMSCRIPTEN)
	message (FATAL_ERROR "Configure the playground with Emscripten: emcmake cmake -S bindings/wasm -B build-wasm")
endif ()

# The playground reports errors in its inputs through C++ exceptions, so all of Mata uses WebAssembly exceptions.
add_compile_options (-fwasm-exceptions)
add_link_options (-fwasm-exceptions)

# The page is served without cross-origin isolation, where threads are not available. Mata itself uses no threads, so
#  the library is built without them.
add_library (Threads::Threads INTERFACE IMPORTED)

set (
	MATA_BUILD_EXAMPLES
	OFF
	CACHE BOOL "" FORCE
)
set (
	BUILD_TESTING
	OFF
	CACHE BOOL "" FORCE
)
set (
	MATA_USE_MIMALLOC
	OFF
	CACHE BOOL "" FORCE
)
add_subdirectory (../.. mata EXCLUDE_FROM_ALL)

add_executable (mata-playground playground.cc bindings.cc)
target_link_libraries (mata-playground PRIVATE mata::libmata)
target_link_options (
	mata-playground
	PRIVATE
	-lembind
	-sMODULARIZE=1
	-sEXPORT_ES6=1
	-sEXPORT_NAME=createMataPlayground
	-sENVIRONMENT=web,worker,node
	-sALLOW_MEMORY_GROWTH=1
	-sMAXIMUM_MEMORY=2GB
	-sSTACK_SIZE=4MB
)
