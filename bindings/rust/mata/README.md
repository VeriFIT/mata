# mata

[![crates.io](https://img.shields.io/crates/v/mata.svg)](https://crates.io/crates/mata)
[![docs.rs](https://img.shields.io/docsrs/mata)](https://docs.rs/mata)

The crates.io and docs.rs links above are for the first published release, which is in preparation;
until then, use this crate from the repository.

Safe Rust bindings to [Mata](https://github.com/VeriFIT/mata), the finite automata library of the VeriFIT group at FIT BUT.

```rust
use mata::Nfa;

let a = Nfa::from_regex("(a|b)*abb")?;
let b = Nfa::from_regex("(a|b)*b")?;
assert!(a.is_included_in(&b));

// A word in L(b) but not in L(a).
let word = b.find_word_not_in(&a).expect("L(b) is larger");
assert!(b.accepts(&word) && !a.accepts(&word));

assert_eq!(a.minimize().num_states(), 4);
# Ok::<(), mata::Error>(())
```

## Overview

- [`Nfa`] is an automaton with states `0..num_states()` ([`State`]) and transitions labelled with [`Symbol`]s, which are plain numbers. Automata from [`Nfa::from_regex`] read bytes.
- An [`Alphabet`] gives symbols names. Automata do not keep one; operations that need it borrow it.
- Operations such as `union`, `intersection`, `determinize` and `minimize` return a new [`Nfa`]. `&`, `|` and `-` are intersection, union and difference.
- Each check has a variant that returns a counterexample, such as `is_included_in` and `find_word_not_in`.
- The `_within` methods stop at [`Limits`], for the constructions that can blow up.
- [`Nfa::parse_mata`], [`Nfa::display_with`] and [`Nfa::dot`] read and write the `.mata` and DOT formats.
- [`nft`] holds finite transducers. Mata's transducers are experimental and moving fast, so expect breaking changes.

## Building

`mata-sys` builds Mata from source, which needs CMake 3.23 or newer and GCC 14 or newer or Clang 18 or newer (on macOS, Homebrew's LLVM). The minimum supported Rust version is 1.95. See [`mata-sys`](https://docs.rs/mata-sys) for using an installed Mata or building to WebAssembly.

## Features

- `serde`: `Serialize` and `Deserialize` for [`Nfa`], [`Alphabet`], [`State`], [`Symbol`] and [`Transition`].

<!-- Explicit link targets, so that the links also work on crates.io, where this README is shown too. -->

[`Alphabet`]: https://docs.rs/mata/latest/mata/alphabet/struct.Alphabet.html
[`Limits`]: https://docs.rs/mata/latest/mata/struct.Limits.html
[`Nfa`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html
[`Nfa::display_with`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html#method.display_with
[`Nfa::dot`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html#method.dot
[`Nfa::from_regex`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html#method.from_regex
[`Nfa::parse_mata`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html#method.parse_mata
[`Nfa::reduce`]: https://docs.rs/mata/latest/mata/nfa/struct.Nfa.html#method.reduce
[`nft`]: https://docs.rs/mata/latest/mata/nft/index.html
[`State`]: https://docs.rs/mata/latest/mata/struct.State.html
[`Symbol`]: https://docs.rs/mata/latest/mata/struct.Symbol.html
[`Transition`]: https://docs.rs/mata/latest/mata/struct.Transition.html
