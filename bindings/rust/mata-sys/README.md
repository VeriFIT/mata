# mata-sys

[![crates.io](https://img.shields.io/crates/v/mata-sys.svg)](https://crates.io/crates/mata-sys)
[![docs.rs](https://img.shields.io/docsrs/mata-sys)](https://docs.rs/mata-sys)

Native build of [Mata](https://github.com/VeriFIT/mata), the finite automata library of the VeriFIT group, and a [cxx](https://cxx.rs) bridge to its NFA API and its experimental transducer (NFT) API. Use the `mata` crate for a safe, idiomatic API; this crate only builds and links `libmata` and declares the bridge.

## What it contains

- `cxx/shim.hh`, `cxx/shim.cc`: one flat function per operation, in namespace `mata_sys`. The shim turns Mata's templates, overloads and string parameter maps into signatures cxx can bridge, and it checks the preconditions that Mata checks only with debug assertions. Read-only queries take `mata::Automaton`, the base class of NFAs and NFTs, so both share them.
- `cxx/nft.hh`, `cxx/nft.cc`: the same for transducers (see [Transducers](#transducers)).
- `cxx/common.hh`, `cxx/common.cc`: helpers of both, including `.mata` parsing. Parsing serializes mintermization (used for `@NFA-bits` input), because CUDD swaps a process-wide out-of-memory handler around its allocations.
- `src/lib.rs`: the bridge `mata_sys::ffi`; `src/nft.rs`: the bridge `mata_sys::nft::ffi`.
- `build.rs`: builds Mata with CMake, compiles the shim with `-std=c++23`, and links both.

## Building

Requirements: CMake 3.23 or newer and a C++23 compiler: GCC 14 or newer, or Clang 18 or newer (on macOS, Homebrew's LLVM; Apple's Clang is too old). Tested on 64-bit Linux and on WebAssembly ([below](#webassembly)).

| Variable                | Effect                                                                                                                                             |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `MATA_DIR`              | Use an installed Mata (`include/` and `lib/libmata.a` or `lib64/libmata.a`) instead of building one.                                               |
| `MATA_SOURCE_DIR`       | Build this Mata source tree instead.                                                                                                               |
| `MATA_CMAKE_BUILD_TYPE` | CMake build type of Mata, `Release` by default. `Debug` turns on Mata's assertions (the shim follows).                                             |
| `MATA_VERSION`          | The version `LIBRARY_VERSION` reports, for builds whose source has no git tags or `VERSION` file. By default, the version of Mata's CMake package. |

The build script refuses a Mata that the bridge cannot use safely:

- States must be 32-bit (`MATA_STATE_BITS=32`, the default since pull request #939, recorded in the generated `mata/config.hh`): the bridge passes states as `u32`.
- The headers must be free of the static buffers that every automaton shared before pull request #868. The `mata` crate's types are `Send` and `Sync`, which is sound only without them.

The build script exports `DEP_MATA_ROOT` and `DEP_MATA_INCLUDE` to the build scripts of dependent crates, and Mata's headers to their cxx bridges, so a crate can add its own C++ code on top of the same `libmata`:

```rust,ignore
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("my-crate/cxx/my-algorithm.hh");
        #[namespace = "mata::nfa"]
        type Nfa = mata_sys::ffi::Nfa;
        fn my_algorithm(nfa: &Nfa) -> usize;
    }
}
```

`libmata.a` contains Mata's copies of CUDD 3.0.0, whose C functions (`Cudd_Init`, …) have no prefix, and of RE2, in namespace `re2`. A binary that also links another CUDD (such as `cudd-sys`) or another RE2 gets duplicate symbols.

### WebAssembly

The crates build for `wasm32-unknown-emscripten`, the Rust target that links C++ code through Emscripten:

1. Install the target (`rustup target add wasm32-unknown-emscripten`) and the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html), and put its `emcc` on `PATH` (`source emsdk_env.sh`). Rust's CI builds the target's standard library with the Emscripten release that was current at the time, and the Rust documentation asks for a matching one, so use a recent release. Tested: Emscripten 6.0.13 with Rust 1.99.1.
2. Build with `--target wasm32-unknown-emscripten`. The build script then compiles Mata and the shim with `-fwasm-exceptions` (Mata reports errors with C++ exceptions, and the target uses WebAssembly exceptions), passes Emscripten's CMake toolchain file unless `CMAKE_TOOLCHAIN_FILE` is set, and links Emscripten's C++ runtime.
3. Let the final program's heap grow, which a library cannot ask for: under `[target.wasm32-unknown-emscripten]` in `.cargo/config.toml`, set `rustflags = ["-C", "link-arg=-sALLOW_MEMORY_GROWTH=1"]`. Otherwise the heap stays at 16 MiB.

The output is a `.js` file with a `.wasm` file next to it, for Node or a browser. Programs run on one thread (Rust's standard library for Emscripten cannot spawn threads) and have at most 4 GiB of memory. Emscripten programs see a virtual environment, not the variables of the shell.

Other WebAssembly targets do not work: `wasm32-unknown-unknown` (wasm-bindgen) has no C++ standard library, and on the WASI targets, CUDD and Mata's SAT reduction use process functions (`fork`, `posix_spawn`) that WASI does not have.

## Transducers

Mata's transducer (NFT) interface is experimental and moving fast, so expect breaking changes. The shim rejects transducers that are not well formed, and transducers have at most `MAX_LEVELS` (1024) levels. Not bound: `reduce()`, `complement()` and composition on several levels at once.

## License

MIT, the license of Mata. `libmata.a` also contains Mata's copies of CUDD and RE2, which are under BSD-3-Clause, so the crate's license is `MIT AND BSD-3-Clause`.
