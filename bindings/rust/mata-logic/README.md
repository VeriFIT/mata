# mata-logic

Decision procedures for logics whose models are encoded as words, on top of
[`mata-lazy`](../mata-lazy), the lazy emptiness checker of
[Mata](https://github.com/VeriFIT/mata).

A `Logic` builds formulas out of atoms, which are automata over the bits `ZERO` and `ONE`, one tape per variable,
with `and`, `or`, `not`, `implies`, `iff`, `exists` and `forall`, and decides them: `model` returns a satisfying
assignment and `counterexample` a falsifying one. The atoms and the `Padding` choose the logic:

- `Padding::Zero`: natural numbers or finite sets of positions, least significant bit first, padded with zeros
  (WS1S).
- `Padding::Sign`: integers in two's complement, least significant bit first, padded by repeating the sign
  (Presburger arithmetic, LIA).

An atom must accept exactly the encodings of the assignments that satisfy it, with every padding; the connectives
keep that invariant.

## WS1S

The module `ws1s` parses and decides formulas of WS1S, the weak monadic second-order logic of one successor, in a
subset of MONA's syntax:

```rust
use mata_lazy::Options;
use mata_logic::ws1s::Ws1s;

fn main() -> Result<(), mata_logic::Error> {
	let mut ws1s = Ws1s::new();
	let phi = ws1s.parse("(x = y + 1 & y = z + 1) => x = z + 2")?;
	assert!(ws1s.valid(&phi, &Options::new())?);
	let psi = ws1s.parse("x in X & 3 < x & all1 y: y in X => y <= x")?;
	let model = ws1s.model(&psi, &Options::new())?.expect("satisfiable");
	println!("{model}"); // for example `x = 4, X = {4}`
	Ok(())
}
```

- **Variables.** A name that starts with a lowercase letter is first-order (a position, a natural number), one that
  starts with an uppercase letter is second-order (a finite set of positions). Quantifiers range over all of them:
  `all1 x: ex1 y: x < y` is valid.
- **Syntax.** `ex1`, `all1`, `ex2`, `all2` (with lists `ex1 x, y:`), `~`, `&`, `|`, `=>`, `<=>` (from the tightest
  to the loosest), parentheses, `true`, `false`; atoms `t = u`, `t < u`, `t <= u` over positions `x`, `3` and
  `x + 3`, `t in X`, `X sub Y`, `X = Y`, and `empty`. A quantifier reaches as far right as possible. Free names are
  shared by all formulas of one `Ws1s`. The parser does not recurse, and the nesting depth is limited to 1000.
- **Encoding.** One tape per variable, bit `i` telling whether position `i` is in the set, padded with zero columns
  (`Padding::Zero`). A position is a singleton set: the atoms accept only singletons on their first-order tapes, and
  the first-order quantifiers and the queries conjoin `sing(x)` for their variables.
- **Answers.** `valid` and `satisfiable`; `model` and `counterexample` decode the witness into an assignment such
  as `x = 3, X = {1, 4}`.

## Limits

- `exists` (and so `forall`) decides a formula without free variables right away; otherwise it stays lazy, closing
  the projection under padding with `mata_lazy::Formula::pad_closure`.
- A complement reads all 2ᵏ columns of its k tapes at each step.

## License

MIT, like Mata.
