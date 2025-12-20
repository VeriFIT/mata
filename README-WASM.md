## Building Mata to WASM

In order to build Mata library to WASM, you need to have `emscripten` set up, along with all of its dependencies.
You can use the system packages or consult [emsdk](https://github.com/emscripten-core/emsdk).
For building Mata to WASM then use the following steps:

1. create folder `build-wasm` for the WASM build of libmata.a
2. in `build-wasm` run `emcmake cmake -DBUILD_TYPE=Release -DMATA_BUILD_EXAMPLES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF ..`
3. build the library using `emmake make` (the WASM static library `src/libmata.a` should be created after that)
