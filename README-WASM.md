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

## The playground

The [playground](https://verifit.github.io/mata/playground/) on the website runs Mata compiled to WASM.
Its interface to JavaScript is in [`bindings/wasm`](bindings/wasm/), and its page is in
[`site/playground`](site/playground/).
Its Script mode runs [Mata Script](docs/src/script-language.md) with the interpreter of the `mata` command, the
`mata-cli` crate built with its feature `web` for the target `wasm32-unknown-emscripten` (see
[above](#building-the-rust-binding-to-wasm)).
With `emcmake` on `PATH` and Rust's standard library for that target, build both, test the builds under Node,
download [Viz.js](https://github.com/mdaines/viz-js) (which draws the automata), and serve the website at
<http://localhost:8000> (with the documentation once `just docs` has built it):

```shell
just site playground
just site serve
```

Without `just`, build and test the playground with:

```shell
emcmake cmake -S bindings/wasm -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --parallel
node bindings/wasm/smoke-test.mjs build-wasm/mata-playground.js
cd bindings/rust
cargo build --release --locked --target wasm32-unknown-emscripten \
  --package mata-cli --features web --bin mata-playground-script --target-dir ../../build/wasm-script
cd ../..
node bindings/wasm/script-smoke-test.mjs build/wasm-script/wasm32-unknown-emscripten/release/mata-playground-script.js
```

The script smoke test also runs every script example of the playground. The Script mode passes the time limit chosen
on the page to the interpreter, which then stops the script with an error before its next top-level form, function
call, or loop round, keeping what the script printed; the worker is terminated a moment after the limit only when a
single library operation runs past it. The browser's stack is smaller than that of the `mata` command, so a script
that nests a few hundred function calls deep fails in the playground.

The workflow [`website.yml`](.github/workflows/website.yml) builds the website with the documentation and deploys it
to GitHub Pages when `master` is released.
