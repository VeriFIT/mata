## Building Mata to WASM

In order to build Mata library to WASM, you need to have `emscripten` set up, along with all of its dependencies.
You can use the system packages or consult [emsdk](https://github.com/emscripten-core/emsdk).
For building Mata to WASM then use the following steps:

1. create folder `build-wasm` for the WASM build of libmata.a
2. in `build-wasm` run `emcmake cmake -DBUILD_TYPE=Release -DMATA_BUILD_EXAMPLES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF ..`
3. build the library using `emmake make` (the WASM static library `src/libmata.a` should be created after that)

## Building the Rust binding to WASM

The Rust binding builds for the target `wasm32-unknown-emscripten`, which Rust supports at [Tier 2](https://doc.rust-lang.org/rustc/platform-support.html) (without host tools): the target is guaranteed to build and `rustup` ships its standard library, but Rust does not always run its tests on it. With `emcc` on `PATH`, build the binding and run its tests under Node:

```shell
rustup target add wasm32-unknown-emscripten
cd bindings/rust
cargo test --workspace --all-features --target wasm32-unknown-emscripten
```

See the [`mata-sys` README](bindings/rust/mata-sys/README.md#webassembly) for the details.
