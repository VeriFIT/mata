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
keep that invariant. This crate holds the connectives only, not the atoms of a particular logic.

## Limits

- `exists` (and so `forall`) decides a formula without free variables right away; otherwise it stays lazy, closing
  the projection under padding with `mata_lazy::Formula::pad_closure`.
- A complement reads all 2ᵏ columns of its k tapes at each step.

## License

MIT, like Mata.
