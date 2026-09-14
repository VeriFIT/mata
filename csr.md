# A CSR post layout for `mata::Delta`

A finding parked for later, from the Phase 1.5 access benchmarking. It is not part of the templating
plan and does not block it. Written to be picked up cold whenever `Delta` gets optimised on its own.

**Summary:** laying each state's targets end to end in one block, with an offset per symbol, reads
**2.6× faster** than hand-written loops over today's layout (**5.7×** on corpus-shaped data) and also
**writes cheaper**. The blocker is not performance — it is that `.targets` is an `OrdVector` at ~94
call sites.

---

## 1. What is slow today, and why

`StatePost` is an `OrdVector<SymbolPost>`, and every `SymbolPost` owns its own `StateSet` — itself an
`OrdVector<State>`, so a `std::vector`. A state with *k* symbols is therefore:

- one vector of `SymbolPost` objects, plus
- *k* separately heap-allocated target vectors, scattered wherever the allocator put them.

The corpus makes this close to worst case. Measured over `tests-integration/automata`, **81% of posts
hold exactly one target** and 95.6% hold at most two. So the common case is *an entire 24-byte vector
header plus a heap allocation to store a single 8-byte state*, and walking a state's *k* symbols costs
*k* cache misses on unrelated lines.

That, not the accessor design, is where the cost is. The evidence: `for_each_successor` is 0.99× of
hand-written loops over this layout — the abstraction is free — while changing the *layout* under the
same accessor is worth 2.6×.

## 2. The layout

Per state, three flat vectors instead of *1 + k*:

```cpp
struct CsrPost {
    std::vector<Symbol>   symbols;  // sorted, as StatePost's keys are today
    std::vector<uint32_t> offsets;  // symbols.size() + 1 boundaries
    std::vector<State>    targets;  // every target of the state, grouped by symbol
};
```

Targets for `symbols[i]` are `targets[offsets[i] .. offsets[i+1])`. Symbols stay sorted, so a lookup is
the same binary search followed by an offset pair.

What this buys:

- **Successor walk** is one flat contiguous loop over `targets`. There are no post boundaries at all.
- **Cursor** collapses to a single pointer. `operator++` is `++p`; pausing stores one pointer, which is
  strictly *less* state than the current cursor carries (two `StatePost` iterators plus two `State`
  pointers).
- **Allocations** drop from *1 + k* per state to 3, regardless of *k*.

## 3. Measured — reads

`tests-integration/src/bench-delta-matrix.cc`. 1 235 shapes: 6 state counts (2²–2¹⁵) × symbols per
state {1..10, 16, 32, 64, 128, 256} × targets per symbol {same}. Every ratio is against **hand-written
nested `for` loops** over today's `StatePost`/`SymbolPost` — the code a user writes instead of using an
accessor. Cells with ≥1 000 transitions, folded over two runs.

| implementation | median | p10 | p90 |
|---|---|---|---|
| `for_each_successor` | 0.991× | 0.890 | 1.032 |
| `for_each_move` | 0.995× | 0.927 | 1.023 |
| `successor_cursor` | 1.178× | 0.777 | 2.056 |
| `csr_successors` | **0.399×** | 0.186 | 0.778 |
| `csr_moves` | 0.827× | 0.341 | 1.005 |
| `csr_cursor` | **0.387×** | 0.159 | 0.717 |

The win is largest exactly where the corpus lives, because that is where today's layout wastes most:

| targets/symbol | share of real posts | `successor_cursor` | `csr_cursor` |
|---|---|---|---|
| 1 | 81.0% | 0.966× | **0.178×** |
| 2 | 14.6% | 0.716× | **0.173×** |
| 3 | 3.2% | 0.899× | 0.245× |
| 16 | 0.006% | 1.412× | 0.459× |
| 256 | — | 1.760× | 0.734× |

Weighted by how often each band occurs: `csr_cursor` **0.175×**, against 0.934× for today's cursor.
Against today's cursor specifically, CSR is 3.0–5.6× faster at every state count from 4 to 32 768.

`csr_moves` (0.827×) gains least, because carrying the symbol means walking `symbols`/`offsets`
alongside `targets` rather than one flat range.

## 4. Measured — writes

This was expected to be the catch, and is not. `tests-integration/src/bench-delta-build.cc`, one
transition at a time, against `Delta::add`:

| symbols/state | csr incremental | csr bulk |
|---|---|---|
| 1 | 1.04× | 0.87× |
| 2 | 0.86× | 0.57× |
| 4 | 0.74× | 0.53× |
| 16 | 0.48× | 0.36× |
| 64 | 0.40× | 0.25× |

Median **0.81×** incremental, 0.53× bulk; worst cell 1.20×.

CSR *should* be worse here — a target lands mid-block, so the tail shifts and every later offset moves.
It wins anyway because today's insert is worse still: adding a new symbol inserts into
`OrdVector<SymbolPost>`, which shifts `SymbolPost` objects that each **contain a `std::vector`**. Moving
those costs far more than shifting flat `uint32_t`/`State` arrays. Only the single-symbol case, where
there is nothing to shift either way, is a wash.

So today's layout pays for its indirection on both sides.

## 5. What actually blocks it

None of this is a performance question, and no benchmark settles it.

1. **`.targets` is an `OrdVector<State>`, used ~94 times.** In CSR the natural return is a
   `std::span<const State>`. A span is not an `OrdVector`: no `insert`, no `find`, no set operations.
   Either those sites change — breaking the "no call site outside `core/` is ever edited" invariant — or
   a proxy has to expose an `OrdVector`-shaped read-only view over a span.
2. **`mutable_state_post()` hands out a `StatePost&`** for in-place mutation. There is no `StatePost`
   object in a CSR layout to hand out; it would have to become a proxy that writes back.
3. **`SymbolPost` is a public value type**, constructed directly in tests and builders
   (`SymbolPost{symbol, StateSet{...}}`). It would survive only as a view.
4. **`OrdVector<SymbolPost>` semantics leak** — `StatePost` inherits `insert`, `erase`, `find`,
   `push_back`, `back`, `filter`. Each needs a CSR equivalent or a documented removal.
5. **Iterator invalidation changes shape.** Today, mutating one `SymbolPost`'s targets does not disturb
   another's. In CSR any insert can reallocate the whole state's block and shift every later offset, so
   anything holding a target pointer across a write breaks. `SuccessorCursor` already stores raw
   `const State*`; that assumption needs re-auditing.

## 6. Suggested order, if this is ever picked up

1. Write the proxy first and prove it against the existing test suite with **today's** storage behind
   it. If `.targets` cannot be made to work through a view, stop — nothing later matters.
2. Then swap the storage. Phase 2 of the templating plan is what makes this cheap: once posts are
   template parameters, a CSR post is an alternative instantiation rather than a rewrite, and both can
   be benchmarked side by side against the same `AutomatonBase`.
3. Keep `csr_moves` in mind as the weak case, and check `defragment`, `remove` and product/determinise
   construction, none of which were measured here.

## 7. Reproducing

```sh
cmake -B build-rel -S . -DCMAKE_BUILD_TYPE=Release -DNO_PROFILING=ON
cmake --build build-rel --target bench-delta-matrix bench-delta-build --parallel -j8
build-rel/tests-integration/bench-delta-matrix > reads.tsv    # ~2 min
build-rel/tests-integration/bench-delta-build  > writes.tsv   # ~4 s
```

Both pin themselves to one core, use deterministic iteration counts, and **interleave** repetitions
round-robin across implementations. That last point matters: measured sequentially, a CPU clock change
mid-config moved one ratio from 1.00× to 1.14× with no code change at all.

⚠️ Ratio noise between two runs is 1.4% median but **22.8% at p90**, so no single cell is a fact. The
conclusions above rest on medians over thousands of cells and on effect sizes of 2.6–5.7×, which are far
larger than that. Individual cells need the ≥1 000 transition filter before they mean anything.

The CSR side is measured as a **mirror** built from a real `Delta`, not as a replacement for it — so the
read numbers are honest about layout and say nothing about integration.
