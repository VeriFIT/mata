# Templating the transition relation — plan and handoff

Status as of `c913712d`. Phase 0 is done and committed; phases 1–5 are not started.

This document is written to be read cold. It records what was decided, **why**, and which
alternatives were tried and rejected — so the settled questions are not re-opened, and the open
ones are visible.

---

## 1. Goal, and what it is not

Make the transition relation parameterisable so that:

- a third party can define their own `Delta` and inherit every structural operation unchanged;
- the leaf payload of a transition can be something other than a bare state;
- the state type is configurable for benchmarking;
- deeper relations (more than one key between source and target) are expressible.

**What it is not:** this does *not* make the NFA/NFT algorithms level-generic. Measured, the
depth-2 view is used in 260 places (`SymbolPost`/`StatePost`) plus 103 `.targets`. Those stay
depth-2, correctly — an NFA *is* state × symbol → states. The deliverable is that
`mata::Automaton`'s contract works for a relation someone else defines.

Corollary worth internalising: `mata::Automaton` is the **only** consumer of the depth-agnostic
API. `for_each_successor` / `for_each_move` / `successor_cursor` appear nowhere outside
`src/core/`. Everything else goes through the concrete depth-2 view.

---

## 2. What is already done

### Commits `057a62ee` … `4c453a1a` — the `core` extraction (C1–C4)

The dependency cycle `automaton.hh → nfa/delta.hh → nfa/types.hh` with `nfa/nfa.hh → automaton.hh`
is gone. Layout is now:

```
include/mata/core/{types,concepts,delta,automaton}.hh, automaton.tpp
src/core/{delta,automaton}.cc
```

`mata::nfa` and `mata::nft` re-export from `mata::` through **seams** in their `types.hh` and
`delta.hh`. Those shims are permanent, not cruft: they are what lets a module redefine a type for
itself later by editing one line, with no call-site churn.

Invariants established (worth re-checking after every phase):

```sh
# core must not reach into an automaton module
grep -rE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]mata/(nfa|nft)/' include/mata/core src/core

# modules must not bypass their own seam
grep -rE --exclude=types.hh --exclude=delta.hh \
  'mata::(State|StateSet|Run|StateRenaming|ParameterMap|Limits|EPSILON|Delta|StatePost|SymbolPost)\b' \
  include/mata/nfa include/mata/nft src/nfa src/nft
```

`nft → nfa` is down to **33 references**, all legitimate (`nfa::Nfa` ×25, `nfa::builder` ×4, plus
`revert`, `encode_word`, `are_equivalent`, `algorithms`). Zero type-level references remain.

### Commit `651244e1` — docs

`docs/src/core.rst` added, `automaton.rst` folded into it. Inherited members now render on both the
NFA and NFT pages (see §6, "Doxygen/Sphinx").

### Commit `c913712d` — Phase 0

**`include/mata/core/concepts.hh`** (new). `TargetTraits`, `WalkableRange`, `TargetSetLike`,
`PostEntryLike`, `PostLike`, `AutomatonWithRuns`, `DeltaLike`.

**Post protocol** on the existing concrete classes — `Key`, `Nested`, `Entry`, `Target`,
`key_arity`, `key()`, `nested()`, `is_sorted()`, `sorted_by_key`. `symbol` and `targets` stay
public, so the 260 depth-2 sites are untouched.

**Three assertions** at the bottom of `delta.hh`, all passing:

```cpp
static_assert(PostEntryLike<SymbolPost>);
static_assert(PostLike<StatePost>);
static_assert(DeltaLike<Delta>);
```

**`state_of` routing.** Every place that received a target and used it as a state now projects
through `Delta::state_of()` — reachability, distances, `reverted`, `find_accepting_path_`, both
Tarjan dereferences. Identity today; correct under a payload target.

**`Automaton` has zero symbol dependency.** `grep -E '\b(Symbol|Word)\b'` over
`core/automaton.{hh,tpp}` and `src/core/automaton.cc` returns nothing.

**Emptiness restructured.** `has_no_accepting_path` is gone; `is_lang_empty` dispatches directly
between the two private searches:

```cpp
template <AutomatonWithRuns Self>
bool is_lang_empty(this const Self& self, typename Self::Run* cex = nullptr) {
    if (cex == nullptr) { return self.has_no_accepting_path_scc_(); }
    if (!self.find_accepting_path_(cex->path)) { return true; }
    cex->word = self.get_word_for_path(*cex).first.word;
    return false;
}
```

`is_lang_empty_scc()` was deleted from both leaves — it was an exact alias with zero callers.

---

## 3. Decisions already settled

Do not re-open these without new information; each cost real discussion.

### 3.1 Nesting, not a parameter pack

`Delta<StatePost<SymbolPost<Symbol, Targets>>>`, **not** `DeltaT<Target, Keys...>`.

A pack fixes the container implementation at every level (always an `OrdVector`). Nesting lets each
post be a different class — a hash post, a bitmap post for a dense key space. Worth the verbosity.

### 3.2 No alternation

An earlier draft had container and keyed-level alternating. Wrong. The **post is the ordered map**;
`StatePost` already *is* `Symbol → targets`. `SymbolPost` is its *entry*, an implementation detail
of the post that holds it. One key adds one post:

```
Delta -> Post<Symbol, Post<Symbol, ... Targets>>
```

### 3.3 The word "level" is unusable

`mata::Level` already exists (`alphabet.hh:20`), and `mata::nft::Levels` is the per-state tape-level
vector. Use **post** for the nesting (matching `StatePost`/`SymbolPost`/`state_post()`) and
**`key_arity`** for the count.

⚠️ **Unresolved:** the "max 4" cap was never restated in these terms. Existing docs use "level" for
two different counts — `nfa.hh` says *three-level*, `delta.hh` says *four-level*, both counting
containers rather than keys. Decide whether the cap means **4 keys** (`key_arity <= 4`) or a
4-deep structure (`key_arity <= 2`) before Phase 3.

### 3.4 `AutomatonBase` takes one parameter

```cpp
template <DeltaLike D> class AutomatonBase {
    using State = typename D::State;
    using Target = typename D::Target;
    D delta;
    utils::SparseSet<State> initial, final;
};
```

Do **not** pass `State`/`Symbol` in parallel with the Delta — that permits disagreement. Derive
them. Ergonomics come from alias templates that *build* a consistent stack:

```cpp
template <typename State, typename Symbol>
using Automaton2 = AutomatonBase<Delta<StatePost<SymbolPost<Symbol, StateTargets<State>>>>>;
```

### 3.5 `Target` and `State` are different types

`Target` is the leaf payload; `State` is the index — what `state_post(q)` takes, what
`num_of_states()` counts, what `initial`/`final` hold. They coincide today and stop coinciding the
moment a payload exists.

### 3.6 `state_of` lives in `TargetTraits`, not on a post

Which state a target denotes is a property of the **target type**. Putting it on a post conflated
two jobs and would have needed forwarding through every post as depth grew.

```cpp
template <> struct mata::TargetTraits<MyPayload> {
    using State = mata::State;
    static State state_of(const MyPayload& t) { return t.state; }
};
```

Only `Target` propagates up the nesting (one alias per post, no logic). **Verified free**: at `-O2`
the assembly of `src/core/automaton.cc` is byte-identical with and without the `state_of` calls
(9030 instructions either way). At `-O0` it costs 34 instructions in a 33k TU; `[[gnu::always_inline]]`
does not help and was rejected.

### 3.7 `Automaton` needs no `Symbol`

It transports keys (in `reverted()`, via `const auto&`) but never inspects, compares or stores one.
`Delta` should expose `Key<I>`, never `Symbol` — at key_arity 2 there is no "the symbol".

### 3.8 Epsilon belongs to a post, not a namespace

NFT proves it: it does not have a *different* epsilon, it reserves a *wider tail*
(`DONT_CARE = EPSILON - 1`). A `KeyLevel<K, Eps>` descriptor carrying `epsilon` and `max_ordinary`
makes `Delta`'s four defaulted parameters resolve per-instantiation.

`EPSILON` is currently re-exported by **using-declaration**, not redefinition. This is load-bearing:
`constexpr Symbol EPSILON{mata::EPSILON}` creates a *distinct object* and breaks any TU with both
`using namespace mata` and `using namespace mata::nft` (8 errors in
`src/applications/strings/replace.cc`). Do not "tidy" it back.

### 3.9 `Automaton` stays a base class

It is never named as a type outside inheritance plumbing — no variable, parameter or return type
anywhere. But it is not dissolvable into free functions: it *owns* `delta`, `initial`, `final`.
It is a **data-owning mixin**, templated on the Delta because it stores one, `deducing this`
members, no virtuals ever.

### 3.10 Two audiences, two mechanisms

- **Users of `Nfa`/`Nft`** are protected by `Nfa`/`Nft` being **non-template classes** with
  **non-template forwarding wrappers** over every templated base member. A wrong argument then
  produces ordinary overload resolution, not an instantiation dump.
- **Implementers of new automata** are protected by **concepts**. Constrain the primary template
  parameter so the failure is reported at instantiation, not inside.

Concepts do nothing for the first audience; wrappers do nothing for the second.

---

## 4. Phases

Each phase is independently mergeable and leaves the tree green.

### Phase 1 — template `Automaton`

- **T1.1** `template <DeltaLike D> class AutomatonBase`, deriving `State`/`Target`/`Key<I>`/`key_arity`.
  `using Automaton = AutomatonBase<Delta>;` keeps `mata::Automaton` concrete for `nfa.pxd:133`.
  Edits: the 9 `utils::SparseSet<State>` sites. C3 already left every type spelled as the bare token
  a parameter binds, so the body needs no changes.
- **T1.2** `extern template class AutomatonBase<Delta>;` plus explicit instantiation in a `.cc`.
  Do this **now**, not later — see §6.
- **T1.3** `AutomatonWithRuns` currently hardcodes `mata::State`. Change to
  `std::same_as<std::vector<typename A::State>&>` once `AutomatonBase` exposes `State`, or it
  silently stops constraining a divergent state type.

Gate: bindings. `CAutomaton` and `CNfa(CAutomaton)` must still resolve.

### Phase 2 — template the posts, bottom-up

One commit each, alias preserved so nothing downstream moves:

| | new | alias kept |
|---|---|---|
| T2.1 | `StateTargets<State>` | — |
| T2.2 | `SymbolPost<Key, Nested>` | `using SymbolPost = SymbolPost<Symbol, StateTargets<State>>` |
| T2.3 | `StatePost<Nested>` | likewise |
| T2.4 | `Delta<StatePost>` | likewise |

T2.1 also lets `StatePost::key_arity` be computed (`Nested::key_arity + 1`) rather than hardcoded,
and enables the `static_assert(TargetSetLike<...>)` that Phase 0 left as a TODO in `delta.hh`.

Verify per commit: the 260 depth-2 sites compile **untouched**. If one needs editing, the alias is
wrong — fix the alias, not the call site.

### Phase 3 — generalise the walks

- **T3.1** recursive `walk()` with `if constexpr (Post::key_arity == 0)`, replacing the hardcoded
  nesting in `for_each_target` / `for_each_move`.
- **T3.2** `SuccessorCursor` → `std::array<std::pair<It,It>, key_arity>` with a compile-time
  unrolled seek. **This is the one place a depth-2 regression is plausible**, and where >4 gets slow.
- **T3.3** `Transitions::const_iterator`, `num_of_transitions`, `get_used_symbols*`, `defragment`,
  `operator==`.
- **T3.4** `GenericallyWalkable = DeltaLike<D> && D::key_arity <= N`, constraining the *walks*, not
  the class. A deeper `Delta` must stay constructible with `add`/`contains`/`state_post`; only the
  generic walks refuse, with a message naming the alternative.

**Gate:** `bench-automata-inclusion` and `bench-bool-comb-intersect` against `4c453a1a`. Agree the
tolerance before starting. A cheaper pre-check that caught nothing so far: compile
`src/core/automaton.cc` at `-O2 -S` before and after and diff the assembly.

### Phase 4 — post traits

- **T4.1** `KeyLevel<K, Eps>` carrying `epsilon` and `max_ordinary`. Rebind the four defaulted
  parameters (`moves_epsilons`, `moves_symbols`, both `epsilon_symbol_posts`) so
  `nft_aut.delta.epsilon_symbol_posts(q)` resolves per-instantiation instead of picking up core's
  constant. **This is a silent-wrong-answer bug** if divergent epsilons land before this.
- **T4.2** Concept asserting reserved keys sort contiguously at the tail. `src/core/delta.cc:401`
  (`first_epsilon_it`) walks backwards relying on it; a wrong traits would not fail to compile, it
  would return wrong iterators.

Then `nfa::EPSILON` becomes `Delta::post<0>::epsilon` behind the existing seam.

### Phase 5 — ergonomics and docs

Alias templates, `Delta::Key<I>`, and the deferred doc work: rewrite the `@page nfa` prose at
`nfa.hh:19-58` (8 references to a "three-level data structure") and `delta.hh:466`
("four-level hierarchical structure").

### Separate track — configurable `State`

Not part of the above. One typedef in `core/types.hh` behind a CMake option, landable any time
after Phase 1.

🚩 **Blocker:** `bindings/python/libmata/nfa/nfa.pxd:31` declares `ctypedef uintptr_t State`.
Change `State` to `uint32_t` and the bindings silently disagree with the C++ ABI — no compile
error, wrong behaviour at runtime. Resolve before enabling the option.

---

## 5. Invariants

1. `Nfa` and `Nft` remain **non-template classes**.
2. The seams keep resolving; the 260 depth-2 sites and 103 `.targets` uses are never edited.
3. Every templated base member has a non-template forwarding wrapper in **both** leaves.
   Currently 4 such members: `is_lang_empty`, `is_identical`, `trim`, `trim_impl` (protected).
4. Depth-2 codegen does not regress.

⚠️ Invariant 3 is **unenforced**. It works by name hiding — `Nft::trim` hides `Automaton::trim` —
and nothing catches a leaf that forgets. `automaton.hh:288` already relies on a subtle version:
a leaf redeclaring `trim` hides it by name, but `trim_impl` stays reachable unqualified *because*
only `trim` was redeclared. Templating adds more such members. Consider a lint.

---

## 6. Traps found the hard way

**Bindings break silently.** The `.pxd` files name `mata::nfa::Delta`, `mata::nfa::StatePost`,
`mata::nfa::SymbolPost`, `mata::Automaton` as **string literals**. They fail at Cython codegen or at
runtime, never at C++ compile time. Rebuild them every phase:
`cd bindings/python && python3 setup.py build_ext -fi -j8`.
Their *tests* additionally need `tabulate` and `networkx` (never run during this work).

**Compile time.** `Delta` is ~87 member functions across 299 header lines plus 780 lines currently
compiled once in `delta.cc`. Templated, all of it lands in every TU. `extern template` in Phase 1,
not bolted on later.

**Doxygen/Sphinx.**
- Sphinx (9.1, current) **cannot parse the C++23 explicit object parameter**. Worked around with
  `PREDEFINED = BUILDING_DOX this=` in `Doxyfile.in`, which is safe only because
  `INLINE_SOURCES = NO`. More `deducing this` members will need this to keep holding.
- `doxygenfile` does **not** render inherited members; `doxygenclass` with `:members:` does. That is
  why `nfa.rst`/`nft.rst` use `doxygenclass` for the class and a sectioned `doxygenfile` for the
  free functions.
- A refactor emptying a doc page produces **no error** — `SPHINXOPTS` has no `-W`, and ~3100 of the
  ~3147 warnings come from the `breathe-apidoc` page trees. C2 and C4 silently gutted the Delta
  sections and it was only caught by inspection.

**Prose goes stale silently.** Three doc claims became false *within Phase 0*. `static_assert`
catches code drifting from the concepts; nothing catches comments. Prefer asserting a fact over
describing it.

**Check publication before deleting public API.** `mata/automaton.hh` had never been in a stable
release (only `-dev` tags), so its compat forwarder was deleted. `nft/delta.hh` is in every release
back to `v1.32.30`, so it stayed as a shim. Same test, opposite answers:
`git cat-file -e v1.32.44:<path>`.

---

## 7. Open questions

1. **Does the "max 4" cap mean `key_arity <= 4` or `<= 2`?** See §3.3. Blocks T3.4.
2. **May `Nfa` and `Nft` diverge?** If yes, `to_nfa_move()` (`nft.hh:1371-1381`) stops being an O(1)
   move of the Delta and becomes a full rebuild — on the hot path of every transducer inclusion
   check. Recommendation: make divergence *expressible*, keep both on one instantiation until
   something concrete forces otherwise.
3. **Is `key_arity >= 2` real or speculative?** No in-tree consumer. Phase 3 is the largest and
   riskiest; phases 1, 2 and 4 deliver the extensibility, the traits and the benchmarking without it.
4. **Should a structural-only automaton be able to ask "is there an accepting path"?** Today it
   cannot: `is_lang_empty` requires `get_word_for_path` to instantiate, and the two searches are
   private. Deliberate — add a protected accessor when something concrete needs it.

---

## 8. Verification recipe

```sh
make debug JOBS=8 && ctest --test-dir build -j8          # expect 253/253
cd bindings/python && python3 setup.py build_ext -fi -j8 # expect rc=0
make debug-werror JOBS=8 BUILD_DIR=/tmp/bw               # expect no error:
# layering — both must print nothing
grep -rE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]mata/(nfa|nft)/' include/mata/core src/core
grep -rE --exclude=types.hh --exclude=delta.hh 'mata::(State|Delta|SymbolPost|StatePost|EPSILON)\b' \
  include/mata/nfa include/mata/nft src/nfa src/nft
```
