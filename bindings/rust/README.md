# Rust bindings

Rust bindings to Mata, in two crates:

- [`mata`](mata): the safe API ([documentation](https://docs.rs/mata)).
- [`mata-sys`](mata-sys): builds Mata and declares a [cxx](https://cxx.rs) bridge to it ([documentation](https://docs.rs/mata-sys)).

The workspace also holds [`lazymat`](lazymat), lazy emptiness checking of combinations of automata and transducers, built on the `mata` crate.

```toml
[dependencies]
mata = "0.1"
```

```rust
use mata::Nfa;

let a = Nfa::from_regex("(a|b)*abb")?;
let b = Nfa::from_regex("(a|b)*b")?;
assert!(a.is_included_in(&b));
let word = b.find_word_not_in(&a).unwrap(); // in L(b) but not in L(a)
assert!(b.accepts(&word) && !a.accepts(&word));
```

The bindings need Rust 1.95 or newer, CMake 3.15 or newer, and GCC 14 or newer or Clang 18 or newer (on macOS, Homebrew's LLVM).

## Building and testing

```sh
cd bindings/rust
cargo test --workspace --all-features
```

or `just bindings rust` from the repository root. The [`mata-sys` README](mata-sys/README.md) covers building against an installed Mata and building to WebAssembly.
