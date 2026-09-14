# Templating the transition relation — plan and handoff

Status: phases 0 and 1 are done and committed (`32d61032`). **Next: phase 1.5**, the delta-access
harness and the depth-2 baseline (`key_arity` 1, today's delta), which has to land before phase 2
templates the posts. Phases 2–5
are not started.

⚠️ This file is **local only** — do not commit changes to it. `git update-index --skip-worktree
Plan.md` keeps it out of `git status` and out of accidental commits.

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

**What it is not:** this does *not* make the NFA/NFT algorithms level-generic. The depth-2 view is
used in a few hundred places (`SymbolPost`/`StatePost`) plus ~100 `.targets`. (The once-quoted 260
and 103 do not reproduce — a grep today gives 394 and 94 — so treat them as an order of magnitude,
and use the exact per-commit check in Phase 2 instead.) Those stay
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

### Phase 1 — `Automaton` is templated

`template <DeltaLike D> class AutomatonBase`, with `using Automaton = AutomatonBase<Delta>;`. Not
one call site outside `core/` moved: every reference already went through the name `Automaton`, and
a plain alias keeps working as a base-class name, in a mem-initialiser, in `Automaton::operator=`,
and as an explicit template argument in `std::static_pointer_cast<mata::Automaton, ...>`.

`State`, `Target` and `key_arity` are read off `D`. `StateSet` and `StateRenaming` became **member**
aliases spelled over `State`, so a divergent state type cannot leave the base's own signatures
behind; both are the same type as `mata::StateSet` / `mata::StateRenaming` at `D = Delta`. There is
deliberately **no** `Key` alias — see §3.7; the count `key_arity` is enough, and `Key<I>` waits for
Phase 2.

Two entities had to leave `src/core/automaton.cc`'s anonymous namespace, because a template
instantiated in another TU cannot use internal-linkage entities: `reachable_states()` became the
private member `reachable_states_()`, and `TarjanNodeData` a private nested struct.

`Limits::max_state` in `distances_from_initial()` became `std::numeric_limits<State>::max()`.
`Limits` is tied to `mata::State`; the same value today, correct under a divergent one.

**T1.2 done in the same commit, not after.** It cannot be deferred: the bodies had to move from
`src/core/automaton.cc` into `core/automaton.tpp` (a third party instantiating over its own relation
needs them), which is precisely what makes `extern template` load-bearing. `src/core/automaton.cc`
is now one line. Verified by `nm`: `AutomatonBase<Delta>::get_useful_states` is defined in
`core/automaton.cc.o` and undefined in the 8 other TUs that reference it.

**T1.3 done.** `AutomatonWithRuns` now reads `std::vector<typename A::State>&`.

**Codegen (invariant 4) measured, not assumed.** `src/core/automaton.cc` at `-O2 -S`, folding
`.cold`/`.part` clones into their parents: every function present both before and after is
**identical or smaller** — the shared subtotal falls 9007 → 8521 instructions. The TU nevertheless
grows 8706 → 9380, entirely from 8 symbols an explicit instantiation is *required* to emit and
which were previously inlined away or internal: `reachable_states_` (+375, and its one caller
correspondingly −330), the three constructors (+546, previously implicitly inline), `TarjanNodeData`'s
constructor (+81), one `for_each_target` lambda instantiation (+158, and `find_accepting_path_`
correspondingly −129), `is_state` (+8). Those out-of-line copies are the point: they are what the
other 8 TUs link against instead of emitting their own.

**`DeltaLike` was not the whole contract; it is now.** See §3.11.

---

## 3. Decisions already settled

Do not re-open these without new information; each cost real discussion.

### 3.1 Nesting, not a parameter pack

`Delta<Post<PostEntry<Symbol, Targets>>>`, **not** `DeltaT<Target, Keys...>`.

A pack fixes the container implementation at every level (always an `OrdVector`). Nesting lets each
post be a different class — a hash post, a bitmap post for a dense key space. Worth the verbosity.

**The generic classes are `posts::Post` and `posts::PostEntry`, not `StatePost`/`SymbolPost`.**
A relation is `Delta -> Post -> Post -> ... -> Targets`, one `Post` per key, and only the outermost
one is the post *of a state*. `mata::StatePost` and `mata::SymbolPost` are **aliases** for the
depth-2 instantiation, exactly like `mata::Delta` and `mata::Transition`.

This was got wrong once and is worth stating plainly, because the wrong version compiles and passes
every test. Templating `StatePost`/`SymbolPost` *in place* and reusing them at every level gives the
right structure with lying names: a depth-3 chain then spells
`StatePost<SymbolPost<Symbol, StatePost<SymbolPost<...>>>>` — a "state post" nested inside a "symbol
post", which is nonsense on its face and was only noticed once it was drawn as a diagram. The fix
was a rename, not a redesign: `Post<E>` was always just `OrdVector<E>`, an ordered map from
`E::Key` to `E::Nested`, with nothing state-specific in it, and `PostEntry` was always just a key
plus what is under it. Nothing outside `core/` moved, because the aliases absorb it.

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

**Two counts, and the conversion between them.** *Structure depth* is how many lookups it takes to
get from `Delta` to a target set, counting the source-state index as the first. `key_arity` is how
many of those lookups are keys — always one fewer. Today's delta is `delta[q][a] -> targets`:
**structure depth 2, `key_arity` 1.**

| structure depth | `key_arity` | the lookup | example |
|---|---|---|---|
| 2 | 1 | `delta[q][a]` | today's NFA and NFT |
| 3 | 2 | `delta[q][a][b]` | a two-tape relation |
| 4 | 3 | `delta[q][a][b][c]` | the cap |

**Decided:** the cap is **structure depth 4**, which is **`key_arity <= 3`**.

⚠️ A *third* convention is loose in the older prose and means neither of these: `nfa.hh` says
*three-level*, `delta.hh` says *four-level*, both counting **containers** (`Delta`'s vector,
`StatePost`, `SymbolPost`, `StateSet`). Phase 5 deletes that wording. Where this document says
"depth-2" unqualified it means today's delta — structure depth 2, `key_arity` 1.

### 3.3b Depth is real, up to structure depth 4, and today's delta wins every tie

`key_arity >= 2` is **not** speculative: relations up to **structure depth 4**, i.e. `key_arity <= 3`,
are in scope. This settles what was open question 3, and it makes Phase 3 mandatory rather than
optional.

Two consequences, and the second is the one that constrains the design:

1. `DeltaLike` currently carries `requires D::key_arity == 1`, which is honest today — `reverted()`
   binds exactly one key per move — and is the single line Phase 3 relaxes, to `<= 3`.
2. **There is probably no single access pattern that is fast both shallow (depth `<= 4`, arity
   `<= 3`) and deep (deeper than that).** So do not look for one. The shallow path is the one that
   must be optimal; a deeper relation, if it ever arrives, gets its own strategy behind the same
   interface. That is why T3.4's `GenericallyWalkable` cap exists at all, and why it sits where it
   does.

And the hard constraint that follows from both: **`key_arity == 1` — today's delta, structure depth
2 — must not regress.** Not "should not". It is what every NFA and NFT in the tree runs on, and a
generalisation that costs it anything is not worth having. This is invariant 4, and Phase 1.5 says
how it is measured. The belief that shallow and deep want different access patterns is a *belief*;
it is to be settled by measurement, not by argument, which is the other reason the harness comes
first.

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
using Automaton2 = AutomatonBase<Delta<Post<PostEntry<Symbol, StateTargets<State>>>>>;
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

**Done in Phase 4, as `ReservedKeys<K, Epsilon, MaxOrdinary>`** — not `KeyLevel`, since §3.3 rules
that word out. The descriptor and the module constant are tied by a `static_assert` in each module's
`delta.hh` rather than one becoming the other, precisely because of the paragraph above; Phase 4's
notes have the detail.

### 3.9 `Automaton` stays a base class

It is never named as a type outside inheritance plumbing — no variable, parameter or return type
anywhere. But it is not dissolvable into free functions: it *owns* `delta`, `initial`, `final`.
It is a **data-owning mixin**, templated on the Delta because it stores one, `deducing this`
members, no virtuals ever.

### 3.13 Key vocabulary belongs to the key type, not to `Delta`

Raised by a concrete case: someone keying transitions by **intervals** rather than symbols.

The first answer — keep `get_used_symbols()` on `Delta` as a forwarder to a generic
`get_used_keys<0>()` — does not survive it. With `Interval{lo, hi}` keys, "the used symbols" is not
the level-0 keys under another name; it is the *union of their expansions*. A different computation,
so a rename is wrong even where the name is right. It only looked adequate because `Symbol` keys make
the expansion an identity.

The fix follows §3.6 exactly: as `TargetTraits::state_of` answers "which state does this target
denote", a `KeyTraits<K>` answers "which symbols does this key admit" —

```cpp
template <> struct KeyTraits<Symbol>   { /* for_each_symbol: fn(k);  admits: k == s */ };
template <> struct KeyTraits<Interval> { /* for_each_symbol: lo..hi; admits: lo <= s && s <= hi */ };
```

so the member keeps its name and becomes *correct* for both, constrained on
`KeyDenotesSymbols<Key<0>>` so a weight-keyed relation never sees it.

**Done in Phase 4** — but *not* folded into one descriptor with epsilon, as this section originally
proposed. They cannot be: which symbols a key admits is a property of the key *type* (so, a traits
specialised on it), while where the ordinary keys stop is a property of the *relation* (so, a
template argument) — NFA and NFT share the key type and differ only in the second. Two mechanisms
meeting on the same entry. Phase 4's notes have the argument.

**Not urgent, and cheap because the hard part is already done.** T2.2 and T2.3 stopped the posts
storing a `Symbol` and made them store `Entry::Key`; that was the structural change. `KeyTraits` is a
specialisable traits template, so it is purely additive. Had the posts kept `Symbol symbol` baked in,
intervals would have meant redoing T2.2 and T2.3.

**Where a third party needs their own post, and where they do not.** The dividing line is the
container, not the key. Interval keys over a sorted vector need no new post — the storage is
unchanged, and the ordering (`operator<=>`) and lookup (`admits`) are key-type concerns. An interval
*tree*, a hash post, or a bitmap post for a dense key space do need their own post class, and that is
exactly what §3.1 chose nesting over a parameter pack to allow. The contract for doing so is
`PostLike`, and `tests/core/concepts.cc` proves it is satisfiable from outside `mata`.

### 3.10 Two audiences, two mechanisms

- **Users of `Nfa`/`Nft`** are protected by `Nfa`/`Nft` being **non-template classes** with
  **non-template forwarding wrappers** over every templated base member. A wrong argument then
  produces ordinary overload resolution, not an instantiation dump.
- **Implementers of new automata** are protected by **concepts**. Constrain the primary template
  parameter so the failure is reported at instantiation, not inside.

Concepts do nothing for the first audience; wrappers do nothing for the second.

---

### 3.11 `DeltaLike` is one whole contract, not a set of opt-in capabilities

`DeltaLike` was under-specified: it listed what the algorithms use to *read* a relation, while
`AutomatonBase` also wrote to one. An exhaustive audit of every `delta.` and `D::` use in
`core/automaton.{hh,tpp}` found four requirements the concept never asked for:

| used by | needed | resolution |
|---|---|---|
| `reverted()` → `get_terminating_states()`, `distances_to_final()` | `add(source, key, target)` | into `DeltaLike` |
| `trim_impl()` → `trim()` | `defragment(is_staying, renaming)` | into `DeltaLike` |
| `is_identical()` | `delta == delta` | into `DeltaLike` |
| `AutomatonBase(size_t, …)` | `D(size_t)` | into `DeltaLike` |

**Tried first and reverted: splitting the write side into opt-in concepts** (`RevertibleDeltaLike`,
`TrimmableDeltaLike`) that constrained only the members needing them, leaving `DeltaLike` a
read-only floor. It is the §4/T3.4 pattern, and applying it here was wrong for three reasons:

1. It contradicts **§3.10**, which is already settled: *constrain the primary template parameter so
   the failure is reported at instantiation, not inside.* With the split, `AutomatonBase<D>`
   succeeded and the complaint arrived at the first `trim()` or `get_terminating_states()` call —
   and those calls are usually made from inside somebody else's template, which is the diagnostic
   §3.10 exists to prevent. Whole, the failure lands on the line that *names* the type:
   `error: template constraint failure for 'template<class D> requires DeltaLike<D> class
   mata::AutomatonBase'`, followed by the offending expression.
2. **It bought nothing.** No relation, in the tree or planned, can read but not write. The one that
   demonstrated the split was written to demonstrate the split. §7 q3 says the same thing about
   `key_arity >= 2`: do not build for a consumer that does not exist.
3. It is **more surface to keep right.** Every future member that writes needs the correct clause,
   and forgetting one is invisible — the same weakness as invariant 3.

Two things carried over from the attempt and are worth keeping:

**`key_arity == 1` is now stated in `DeltaLike`.** `reverted()` binds exactly one key per move and
hands that one key back to `add`, so `AutomatonBase` is depth-2 only *today* regardless of what the
concept says. Saying so turns a mid-instantiation failure into a rejection at the point of use, and
gives Phase 3 one obvious line to relax (T3.5, to `<= 3`). This is the one place where folding the write
side in costs something: `DeltaLike` can no longer be satisfied by a deeper relation, which §1's
fourth bullet eventually wants. That is a Phase 3 problem either way, because `add`'s signature
cannot be spelled arity-generically until `Key<I>` exists.

**Prefer deleting a requirement to documenting it — but not at any price.** The `D(size_t)`
requirement was first *removed*, by having `AutomatonBase(size_t, …)` default-construct and call
`allocate()`. That was the wrong trade: it turned a construction into a construct-then-mutate, it
needed `delta{}` in the initialiser list to keep `-Weffc++` quiet, and its postcondition rested on
what `allocate()` happens to do from zero states — which `DeltaLike` does not specify, since it
requires only that the call is well-formed. Requiring the constructor costs implementers one line
and lets a relation presize in one step if it can.

## 4. Phases

Each phase is independently mergeable and leaves the tree green.

### Phase 1 — template `Automaton` — **DONE**, see §2

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

⚠️ The Cython half of that gate was **not** run for Phase 1: this machine has no Cython for the
`python3` the bindings Makefile invokes, and `.venv` has Cython but no `setuptools`. What *was*
verified is the C++ half — a hand-written TU reproducing exactly what Cython emits for the four
`mata::Automaton` uses (`nfa.pxd:133` members, `wrap_delta`, and both
`static_pointer_cast<CAutomaton, …>` upcasts) compiles clean at `-Wall -Wextra`. Install Cython and
setuptools before Phase 2 and re-run the real thing.

### Phase 1.5 — the delta-access harness, and the depth-2 baseline

Throughout: **depth 2 = `key_arity` 1 = today's delta** (§3.3).

**Comes before Phase 2, not before Phase 3.** Phase 2 templates the posts, which can move codegen on
its own; if the first baseline is taken just before Phase 3, a Phase 2 regression is already baked in
and unattributable. Baseline at `32d61032` — Phase 1 complete, nothing templated below `Delta` yet.

#### What the corpus actually looks like

Measured first, rather than guessed, with `tests-integration/src/delta-shape-stats.cc` over all 66
loadable automata in `tests-integration/automata` (18 731 states, 65 321 transitions, mintermized —
the inputs are `@NFA-bits`, so the post shapes only exist after that):

| | min | p50 | p90 | p99 | max | mean |
|---|---|---|---|---|---|---|
| states per automaton | 2 | 128 | 512 | 2048 | 4096 | 331 |
| posts per source | 0 | **2** | 4 | 11 | 32 | 2.61 |
| targets per post | 1 | **1** | 2 | 4 | 128 | 1.41 |

Sharper still, from the histograms: **57.5%** of states have exactly 2 posts and 91.4% have at most
4; **81.0%** of posts have exactly 1 target and 95.6% have at most 2; **6.3%** of states have no
outgoing transitions at all; and there are **3.49 transitions per state**.

This is not what the plan had been assuming, and it changes what the refactor has to protect:

1. **Setup cost dominates, not iteration.** Any fixed per-`state_post` cost is amortised over about
   three targets. T3.2 replaces the hand-written cursor with a `std::array<std::pair<It,It>,
   key_arity>` and an unrolled seek — which *is* a fixed setup cost. This is the concrete reason
   T3.2 is the risky step, not just a hunch: there is almost nothing to amortise it over.
2. **The binary search over keys is close to irrelevant.** `lower_bound` over 2 elements is not a
   search. The original worry about "the branchy part", and the appeal of a hash or bitmap post,
   have little to bite on at these shapes.
3. **The inner contiguous run is one element, 81% of the time.** `load_targets()` plus `seek()` do
   more work than the iteration they set up.
4. **Everything is cache-resident.** A 4096-state automaton is the corpus maximum. Nothing here
   measures memory latency, so a change that trades instructions for locality will not show up.

⚠️ **Caveat, and it matters:** these 66 automata are the sample committed to the repository, chosen
to be small enough to live there. The real `nfa-bench` corpus is external and is not necessarily
like this. So the grid below covers the measured regime — where the library must be *fastest* —
**and** larger sizes as stress points, so a regression on big automata is still caught.

#### The harness

The existing `bench-*` targets are end-to-end (parse a `.mata` file, run an operation) and are far
too coarse to see an access-pattern change. What is needed is a microbenchmark over **synthetic**
deltas, sweeping the four dimensions that decide which access pattern wins:

| dimension | why it matters |
|---|---|
| number of states | whether `state_posts_` fits in cache; the outer indexed lookup |
| posts per source | the binary search over keys — the branchy part, and the part a hash or bitmap post would change |
| targets per post | the inner contiguous run; how much of the cost amortises over one seek |
| access pattern | sequential full walk / random `state_post(q)` / single-key lookup / resumable cursor mid-traversal |

The last dimension is the one that matters most and is easiest to get wrong: `for_each_successor`,
`for_each_move`, `successor_cursor` and a direct `state_post(q)[k]` lookup have different costs and
different sensitivities, and Tarjan's walk uses the *cursor* — the one shape that has to store and
resume a position, and the one T3.2 rewrites into a `std::array` of iterator pairs.

Deliverables, both written and building clean: `tests-integration/src/delta-shape-stats.cc` (the
corpus measurement above) and `tests-integration/src/bench-delta-access.cc` (the harness). That
directory globs `src/*.cc` and makes one executable per file linked against `libmata`, so no CMake
edit is needed — but the glob runs at **configure** time, so a new file needs `cmake -B <dir> -S .`
re-run before `--target` can find it.

The grid, off the measured percentiles:

- **states**: 128 (corpus median), 4096 (corpus max), 65536, 262144 (stress, beyond the sample)
- **posts per source**: 1, 2 (the mode, 57.5%), 4 (91st pct), 32 (corpus max), 256 (stress)
- **targets per post**: 1 (81%), 2 (95.6%), 128 (tail max)
- plus a **`realistic`** shape per size, drawing posts and targets from the measured histograms
  rather than uniformly. **This is the row that gates** — the uniform cells exist to separate the
  dimensions, not to represent anything.

Five access patterns, because they have different costs and different sensitivities:
`sweep_successors`, `sweep_moves`, `sweep_cursor`, `random_post` (shuffled order, so the outer
index is exercised rather than prefetched) and `point_lookup`.

Method: fixed RNG seed so two runs build byte-identical relations; the process pins itself to one
core; 3 warm-up passes; a **deterministic** iteration count derived from the shape (a fixed work
target of 20M transition-visits), best of 11 repetitions, `spread = (max - min) / min` reported per
row.

#### Establishing what the harness can actually detect — do this before trusting it

A gate that cannot see the regression it guards against is worse than no gate, because it grants
false confidence. So the first thing measured was not the library but the harness: repeated runs of
the **same binary**, compared row by row. Three defects came out of that, in order of how much they
mattered.

**1. The iteration count was auto-scaled by a calibration loop.** Two runs settled on different
counts and were therefore not measuring the same work. `point_lookup` — the cheapest pattern per
state, so the one needing the most iterations — reached **100% cross-run noise** on the same build.
Replaced with a count derived deterministically from the shape (fixed 20M transition-visit target).

**2. No CPU pinning.** Migration mid-measurement moves the numbers by more than anything this
benchmark exists to detect. Now pinned with `sched_setaffinity`.

Those two together took overall p99 noise from **94% to 13.9%**.

**3. A whole run can come out globally slow.** Runs 3-7 agreed within 1%; run 8 was **11% slower on
98% of rows** with no code change. That is drift — thermal, frequency governor, another process —
and it means *comparing baseline runs against candidate runs taken later is unsafe*: uncorrected, it
produces 275 false positives.

The fix exploits the difference in shape: drift moves every row by the same factor, whereas a real
regression moves the rows that touch the changed code. So `compare-bench.py` now computes the median
candidate/baseline ratio, reports it separately as a **global shift**, and judges rows on the
drift-corrected delta. If the global shift itself exceeds the threshold it says so and refuses to
guess, because a uniform regression and drift are indistinguishable from one pair of runs — the
answer then is to re-run baseline and candidate **interleaved** (A B A B A B).

⚠️ A hypothesis worth recording as *wrong*: the whole-shape uniform shifts looked like address-layout
luck, so ASLR was disabled with `setarch -R`. Noise appeared to get six times worse — but that was
entirely run 8 being globally slow, and ASLR had nothing to do with it. Do not bother disabling it.

#### What the gate is

With deterministic iterations, pinning, drift correction, and min-of-2 runs per side:

| rows | median noise | max noise | use |
|---|---|---|---|
| **`shape=realistic`** (20 rows) | **0.9%** | **3.2%** | **the gate, at 5%** |
| `shape=uniform` (255 rows) | 0.9% | 28.2% | diagnostic only |

So: **gate on the `realistic` rows at 5%.** The uniform cells exist to separate the dimensions and a
few of them are pathological (`point_lookup` at `states=4096 posts=32 targets=128` swung 39% between
two runs of one binary); treat a uniform-row move under ~15% as noise and look at it by hand rather
than failing on it.

#### The `key_arity` 1 baseline at `32d61032`

`ns_per_state`, minimum over four runs (which agreed within 0.4%), `realistic` shape:

| states | trans/state | sweep_successors | sweep_moves | sweep_cursor | random_post | point_lookup |
|---|---|---|---|---|---|---|
| 128 | 4.09 | 3.36 | 4.03 | 4.06 | 3.78 | 4.92 |
| 4096 | 3.75 | 8.02 | 11.33 | 9.56 | 6.65 | 10.48 |
| 65536 | 3.68 | 31.45 | 32.99 | 29.73 | 38.81 | 49.06 |
| 262144 | 3.66 | 34.99 | 36.49 | 32.63 | 55.64 | 72.24 |

The full 275-row baseline, and the harness scripts, are outside the repository (this baseline is
local by intent): see the session scratchpad, `BASELINE-32d61032.tsv` and `compare-bench.py`.

Note `sweep_cursor` is *not* slower than `sweep_successors` at scale — it is the fastest of the three
walks at 65536 and 262144 states. T3.2 has to preserve that, and with 3.66 transitions per state it
has very little to amortise a fixed setup cost over.

Usage, per commit inside Phase 2 and Phase 3:

```sh
# Two runs of the candidate, then compare against the recorded baseline.
b=<scratchpad>; for r in 1 2; do $BUILD/tests-integration/bench-delta-access > $b/cand$r.tsv; done
python3 $b/compare-bench.py --baseline $b/BASELINE-32d61032.tsv --candidate $b/cand1.tsv $b/cand2.tsv
# `--realistic-only` on the binary gives just the 20 gating rows, for a faster inner loop.
```

### Phase 2 — template the posts, bottom-up

**Naming, settled.** The templates go in a nested namespace and are aliased back into `mata::`:

```cpp
namespace mata::posts {
    template <typename State>       class StateTargets { ... };
    template <typename K, typename N, typename R> class PostEntry { ... };
    template <typename Entry>       class Post       { ... };
    template <typename P>           class Delta      { ... };
}
namespace mata {
    using StateTargets = posts::StateTargets<State>;
    using SymbolPost   = posts::PostEntry<Symbol, StateTargets>;
    using StatePost    = posts::Post<SymbolPost>;
    using Delta        = posts::Delta<StatePost>;
}
```

This is **not** cosmetic, and the generic names carry their own weight (§3.1): the classes are named
for what they are, so a depth-3 chain reads `Post<PostEntry<K, Post<PostEntry<K, …>>>>` rather than
nesting a "state post" inside a "symbol post". The depth-2 names live only on the aliases, where they
are accurate — an NFA has exactly one post, and it *is* the post of a state.

Naming the generic classes differently also removes a problem the earlier draft of this section had
to work around: a class template and an alias of the same name cannot coexist in one scope, so
`using SymbolPost = SymbolPost<...>` was ill-formed and the nested namespace was load-bearing for
that reason alone. It is no longer — `posts::PostEntry` and `mata::SymbolPost` are different names —
but the namespace stays, because it keeps the generic vocabulary out of the way of the depth-2
vocabulary and reads like the existing `mata::nfa`/`mata::nft` seams.

Note `Post` is parameterised on its **entry**, not on the post nested inside it — §3.1's
`Delta<Post<PostEntry<Symbol, Targets>>>` is the authority. One post class per level, each free to be
a different implementation (hash post, bitmap post for a dense key space).

**Status: T2.1, T2.2 and T2.3 are done** (uncommitted). `posts::StateTargets<State>`,
`posts::PostEntry<Key, Nested>` and `posts::Post<Entry>`, with `core/delta.tpp` created and
`extern template` wired for the first two. Debug 254/254, `debug-werror` zero warnings, bindings
99/99, nothing outside `core/` touched. **T2.4 and T2.5 are blocked — see below.**

⚠️ **T2.4 is not blocked by the nesting — it is blocked by the two classes that sit *outside* it.**
The stack itself works exactly as §3.1 describes and T2.1-T2.3 built it:

```cpp
posts::Delta<posts::Post<posts::PostEntry<Symbol, posts::StateTargets<State>>>>
```

each level parameterised on the one below, down to the targets, with `mata::Delta`, `StatePost`,
`SymbolPost` and `StateSet` as the aliases naming that stack. `Delta<P>` is straightforward. What
stops T2.4 landing on its own is that two of its members return types with no `P` in them:

  - `successor_cursor()` returns `SuccessorCursor`, which is **not part of the chain**: it reaches
    into two levels at once, holding two `StatePost::const_iterator`s *and* a `const State*` pair
    from `SymbolPost::target_span()`. Leave it concrete and `Delta<P>` is instantiable only at
    `P = StatePost`, which defeats the step. It has to follow the chain as well:

    ```cpp
    template <typename P> class SuccessorCursor;        // primary: static_assert(false) past arity 3
    template <> class SuccessorCursor<Post<...>>;       // arity 1 -- today's flat code, unchanged
    ```

    parameterised on the **whole** chain `P`, one specialisation per arity. Note it must *not* be a
    per-post nested type composed down the levels (`Post<E>::Cursor` delegating to
    `SymbolPost`'s): tidier, but that is exactly the composed form measured 24.4% and 18.6% slower
    than flat at arity 2 and 3 (T3.2). A flat cursor has to see the whole chain at once. Templating
    it *is* T3.2, so T3.2 lands inside T2.4 rather than after it.
  - `Transitions::const_iterator` yields `Transition`, a fixed `(State, Symbol, State)` triple, and
    holds a `StateSet::const_iterator`. It needs the same treatment as `Move` did in T2.3.
  - `epsilon_symbol_posts(state, Symbol epsilon = EPSILON)` plus `get_used_symbols` ×6,
    `get_max_symbol()` and `add_symbols_to(OnTheFlyAlphabet&)` are all *symbol*-specific, which §3.7
    says `Delta` must not be. **Decided: leave them as they are for T2.4**, keyed on `Key`, with a
    TODO pointing at Phase 4. They are only instantiated when called, and every caller is in-tree
    with `Symbol` keys, so deferring is exactly the status quo rather than a regression — and Phase 4
    already owns key-level traits, which is the right home. See §3.13 for why a rename is not enough.

Revised order: **T2.4 + T2.5 + T3.2 as one step**, after templating `Transition` alongside it. Doing
the cursor first in isolation is not possible either — its arity-2 and arity-3 specialisations need a
templated post chain to instantiate over, which is what T2.4 provides. The two are mutually
dependent and have to be written together.

One commit each:

| | new | notes |
|---|---|---|
| T2.1 | `posts::StateTargets<State>` | the leaf. `utils::OrdVector` has `value_type` and `is_sorted()` but none of `Target`/`key_arity`/`sorted_by_target`, which is exactly why a wrapper is needed |
| T2.2 | `posts::PostEntry<Key, Nested, Reserved>` | |
| T2.3 | `posts::Post<Entry>` | |
| T2.4 | `posts::Delta<Post>` | |
| T2.5 | `src/core/delta.cc` → `core/delta.tpp` + `extern template` | **do not skip; see below** |

T2.1 also lets `StatePost::key_arity` be computed (`Nested::key_arity + 1`) rather than hardcoded
(`delta.hh:166`), enables the `static_assert(TargetSetLike<...>)` that Phase 0 left as a TODO
(`delta.hh:873`), and is the hook a payload target hangs off — §1's second goal.

**T2.5 is the big one and the original table omitted it.** Templating `Delta` means its 778 lines of
member definitions in `src/core/delta.cc` must move into a header, exactly as `automaton.cc` did in
Phase 1, with `extern template class posts::Delta<StatePost>;` and one explicit instantiation. This
is where §6's compile-time warning actually bites: `Delta` is ~87 member functions, and without
`extern template` all of them land in every TU. Phase 1 established the pattern and the `nm` check
that proves it works — reuse both. It is larger than Phase 1's move.

**Verify per commit, exactly.** The old formulation ("the 260 depth-2 sites compile untouched") cannot
be reproduced — a grep over `include` + `src` today gives 394 and 94, so the numbers were measured
some other way. Use a check that is exact and automatic instead:

```sh
# Must print nothing. If it names a call site, the alias is wrong -- fix the alias, not the site.
git diff --name-only HEAD~1 | grep -vE '^(include/mata/core/|src/core/|tests/|docs/|Plan\.md)'
```

Plus the Phase 1.5 baseline, per commit: depth-2 must not regress.

### Phase 3 — generalise the walks — **DONE** (T3.1, T3.2, T3.3, T3.5; T3.4 dropped)

257 tests, zero-warning `-Werror`, bindings 99/99, nothing outside `core/` touched, and
performance-neutral: interleaved against the pre-Phase-3 build, worst row +1.6%, global shift +0.1%.

**The finding that matters, and it contradicts Phase 1.5.** T3.1 replaced the hand-unrolled
`for_each_target`/`for_each_move` with the recursive `walk_targets`/`walk_moves`. The depth prototype
had measured that recursion at **0.99-1.01x** of hand-written loops at every arity, which is why the
plan called it free. On the real relation it cost **+18-19% on `for_each_successor` and +19-20% on
`for_each_move`**, consistently at 4096, 65 536 and 262 144 states.

The prototype's posts were plain structs over `std::vector`; the real ones wrap `utils::OrdVector`
behind private inheritance with virtual `begin()`/`end()`. A prototype cannot see that, and the
lesson generalises: **a prototype measures the shape of an algorithm, not the cost of the types it
will actually run on.** Every Phase 1.5 conclusion drawn from `bench-depth-prototype` inherits that
caveat, including the arity-2/3 cursor numbers that justified T3.2.

Fixed the same way T3.2 handles it: `if constexpr (key_arity == 1)` keeps the shipping loops verbatim
for the depth everything actually runs on, and the recursion serves the arities that have no
hand-written form. So the pattern for the whole phase is: **specialise the arity in use, stay generic
beyond it.**

Two smaller regressions caught while writing T3.1, neither visible to the benchmark suite:
`has_target` lost its short-circuit when expressed as a full walk that sets a flag (it feeds
`is_successor` -> `has_self_loop` -> `is_acyclic`, once per SCC), and `num_of_moves` went from
O(entries) to O(targets). Both now have dedicated recursive helpers, `any_target` and
`count_targets`.

**T3.3's `operator==` had a trap.** Comparing posts with `operator==` looks obviously right and is
silently wrong: an entry's `operator==` compares **only its key**, deliberately, because the post is
an ordered map that `OrdVector` searches by that key. So post-wise comparison ignores every target
difference. The existing `Delta::operator==()` tests caught it immediately. `posts_equal()` now
recurses into `nested()` explicitly, with the reason recorded at the definition.

**T3.5 is complete.** `DeltaLike` is relaxed from `key_arity == 1` to `<= 3`, and `AutomatonBase`
instantiates and works over arity-2 and arity-3 relations — tested, not assumed. `reverted()` is
generic: it writes through `insert_target`, which takes the *target* first so that the keys can be a
trailing pack, and reads a move apart with `std::forward_as_tuple` + `std::index_sequence`.

> **Correction (Phase 5).** This section used to end by saying `reverted()` was the one piece left,
> blocked on three things. Two of them had already been removed by the time it was written —
> `PostEntryLike` requires mutable `nested()`, and `insert_target` is declarable — and Phase 3 did
> generalise `reverted()`. The third, that "routing the arguments through a trailing pack loses the
> implicit conversions `delta.add(0, 0, 1)` relies on", is *true and was the real obstacle*, and
> Phase 5 worked out what it actually implies; see there. The stale claim then propagated into a
> `@note` on `reverted()` and into a comment in `tests/core/cursor.cc`, both of which described a
> restriction that no longer existed. Prose goes stale silently (§6), including this document's.

**Contracts widened, deliberately** (§3.11's "one whole contract" reasoning): `TargetSetLike` and
`PostLike` now require `push_back`, because trimming and renumbering rebuild a post by appending
rather than filtering in place — one requirement on an implementer instead of the two that
`erase_if` + `rename` would have been. `tests/core/concepts.cc` caught the widening the moment it
landed, which is what that test is for.

**Tests added.** `tests/core/cursor.cc`: every cursor specialisation cross-checked against
`walk_targets`, with holes at every level, empty target runs, single targets so all levels exhaust at
once, and a suspend/resume path because that is the cursor's only reason to exist. Validated by
mutation — three deliberate carry bugs (outer advance without re-descent, dropped middle carry,
empty runs not skipped) were each caught. Plus `AutomatonBase` over arity-2 and arity-3 relations,
explicitly instantiated so a member that quietly needs more than the concept asks for cannot hide.

#### Original plan (for reference)

- **T3.1** recursive `walk()` with `if constexpr (Post::key_arity == 0)`, replacing the hardcoded
  nesting in `for_each_target` / `for_each_move`.
- **T3.2** `SuccessorCursor` → **a flat struct of iterator pairs with hand-written carry, one
  explicit specialisation per arity, 1 through 3.** The plan's original intent —
  `std::array<std::pair<It,It>, key_arity>` with an unrolled seek — was right; only the spelling was
  wrong. It cannot be an *array*, because every nesting level has a different iterator type, so it has
  to be a struct with distinct members per level. Written that way it is the fastest of six forms
  measured, and the same size as the alternatives (32/48/64 bytes at arity 1/2/3).

  Six forms built and swept. Ratios against hand-written nested loops at the same shape; noise 1.1%
  median, 7.5% worst.

  | form | arity 1 | arity 2 | arity 3 | verdict |
  |---|---|---|---|---|
  | **flat, hand-written carry** | **0.827x** | **0.852x** | **0.790x** | **ship this** |
  | composed, one struct per level | 0.837x | 1.126x | 0.970x | degrades with fanout |
  | range-yielding | 1.110x | 1.129x | 1.395x | rejected |
  | `std::array` of indices | 5.404x | — | — | rejected outright |
  | CSR layout | 0.412x | — | — | out of scope, see `csr.md` |

  (Denser fanout: 4 keys, 2 targets per level. At the sparse corpus-like fanout — 2 keys, 1 target —
  flat and composed are within 1% of each other, i.e. indistinguishable. The gap only opens when each
  level has real branching, which is what a depth-3 or depth-4 relation *is*, so the sparse
  measurement alone is misleading and was what made composition look adequate.)

  **Arity 1 needs no new code:** the existing `SuccessorCursor::const_iterator` *is* the flat form,
  and it stays byte-identical. That satisfies invariant 4 in the strongest sense — the `key_arity == 1`
  path is not "within tolerance", it is unchanged. Arity 2 and 3 are roughly 40 and 55 new lines each,
  and they are fiddly: the carry has to scan-and-descend at each level, and getting it wrong yields
  silently skipped targets rather than a compile error. Cross-check every specialisation against the
  recursive walk, as `bench-depth-prototype.cc` does.

  **This is the honest justification for the cap.** Not "deeper gets slow" — measured, it does not:
  the recursive *walk* is flat at every arity to 9, and the composed cursor plateaus at ~1.27x rather
  than diverging. The cap is at 3 because **each extra arity costs a hand-written specialisation**, and
  three is where that stops paying.

  **Past arity 3 it is a hard compile error, not a fallback.** A silent fallback to the composed cursor
  is exactly the kind of surprise this plan exists to prevent: the relation would keep working and
  quietly lose 18-24%, discoverable only by benchmarking. So the primary `FlatCursor` template carries
  `static_assert(false, ...)` — well-formed in an uninstantiated template since C++23 (CWG2518), which
  this project already requires — firing only if something instantiates arity 4 or more, with a message
  naming the cap and pointing here.

  ⚠️ Still **untested**: keeping a compact index position while *suspended* and materialising a flat
  cursor on resume, so a descent is paid per resume instead of per target. Storing indices and
  dereferencing through them is dead (5.4x), but that is not the same design. It matters only because
  Tarjan keeps one cursor per state — 32 B x `num_of_states` today, 64 B at arity 3. Every benchmark
  here does uninterrupted sweeps and cannot see the trade. Needs a harness that suspends at a
  Tarjan-like rate.

- **T3.3** `Transitions::const_iterator`, `num_of_transitions`, `get_used_symbols*`, `defragment`,
  `operator==`.
- ~~**T3.4** `GenericallyWalkable`~~ **— dropped.** The idea was to constrain the *walks* rather than
  the class, so a deeper relation stayed usable and only the generic walks refused. It does not work
  here, and the reason is worth recording: the cap belongs to the **cursor**, and
  `mata::AutomatonBase` needs the cursor unconditionally — Tarjan's SCC walk drives
  `get_useful_states()`, `is_acyclic()`, `is_lang_empty()` and `trim()` through `successor_cursor()`.
  A relation past the cap would satisfy a walk-only concept and then fail somewhere inside Tarjan,
  which is precisely the diagnostic §3.10 exists to prevent, and the same mistake §3.11 already
  corrected once. So the cap goes in `DeltaLike` itself, where it is reported on the line that names
  the type.
- **T3.5** relax `DeltaLike`'s `requires D::key_arity == 1` to **`<= 3`** — the whole cap, in one
  place — and generalise `reverted()`,
  which is the member that put it there: its lambda binds exactly one key per move. This is the last
  step, not the first — the constraint is accurate until the walks and `reverted()` are generic, and
  relaxing it early replaces a clean rejection with a failure inside an instantiation.

**`core/delta.hh:334` is right, for a reason it does not give.** It justifies keeping the cursor
depth-aware with "composing a cursor out of per-post cursors is measurably slower, and `Delta` is the
one place entitled to know its own representation." Measured, composition *is* slower — but not at
depth 2, where it is within 1% at sparse fanout and ~3.6% corpus-weighted. The real gap appears at
arity 2 and 3 with branching at each level: **24.4% and 18.6%**. So the comment's conclusion holds and
its stated scope is too narrow: the flat form is worth hand-writing not because depth 2 needs it, but
because arity 2 and 3 do. Reword it to say that, and drop the appeal to entitlement — the reason is
measurable, not architectural.

**Gate, in this order:**

1. **The Phase 1.5 harness, depth-2, every dimension.** This is invariant 4 and it is a hard stop, not a
   tolerance to be negotiated: `key_arity == 1` must not regress. T3.2 is where it is most likely to
   break, since a `std::array<std::pair<It,It>, key_arity>` with an unrolled seek has to compile down
   to what the hand-written depth-2 cursor does today.
2. `bench-automata-inclusion` and `bench-bool-comb-intersect` against `4c453a1a`, for the end-to-end
   view the microbenchmarks cannot give.
3. Cheap pre-check, useful but weak — it caught nothing in Phase 1 beyond mangled names: compile
   `src/core/automaton.cc` at `-O2 -S` before and after and diff, normalising label numbers
   (`.LFB`, `.LFE`, `.LLSDA*`, `.LFSB`) and assertion line numbers first, or the diff is all noise.

### Phase 4 — key vocabulary — **DONE** (T4.1, T4.2, §3.13)

Landed in `core/concepts.hh` (the vocabulary), `core/delta.hh` + `core/delta.tpp` (the members),
`nfa/delta.hh` + `nft/delta.hh` (the seam), `tests/core/keys.cc` (new) and one line of
`tests/core/concepts.cc`. 260 tests, zero warnings under `-Werror`, layout unchanged.

- **T4.1 — done, named `ReservedKeys<K, Epsilon, MaxOrdinary>`, not `KeyLevel`.** Renamed because
  §3.3 spent a section establishing that "level" is unusable in this codebase (it is an NFT's tape
  level *and* two different structural counts), and a new core type called `KeyLevel` walks straight
  back into it.

  Carried as a third, **defaulted** parameter of `posts::PostEntry<K, N, R>` — on the entry,
  because the entry is where the key type is named, so at a higher `key_arity` each level takes its
  own convention from its own entry instead of sharing one. Propagated up as `Reserved` through
  `StatePost` and `Delta`. All five defaults that had baked in core's constant now read the
  descriptor: `moves_epsilons`, `moves_symbols`, and both `epsilon_symbol_posts` overloads (the plan
  said four; `moves_symbols` had a second, separate use of `EPSILON` in its guard body).

- **T4.2 — done, named `ReservedKeysAtTail<P>`, and `PostLike` is *defined in terms of it*.** Not
  asserted at the members that walk backwards, which was the first instinct: a `static_assert` in a
  member body is exactly the "failure at first call, from inside somebody else's template" that
  §3.10 and §3.11 exist to avoid. Folding it into `PostLike` checks it where a post is *named*, at
  the cost of every post implementer supplying one line — which `tests/core/concepts.cc` now proves
  is satisfiable from outside `mata` (`TinyPost` declares it on the post, not on the entry, so
  mata's entry-carries-it arrangement is shown to be a convention and not the contract).

  `PostLike` lost its own `sorted_by_key` requirement to the new concept rather than duplicating it:
  sortedness and the reserved tail are two unrelated-looking requirements that are really one
  invariant, and separating them is what let the invariant go unstated for so long.

- **§3.13 `KeyTraits<K>` — done.** `get_used_symbols` ×6, `get_max_symbol` and `add_symbols_to` now
  take the union of the keys' *expansions* rather than the keys, and return the **symbol** type
  rather than the key type. `tests/core/keys.cc` keys a relation by `Interval{lo, hi}` and checks 3
  keys yield 6 symbols; at `Key = Symbol` the expansion is the identity and nothing changes.

#### `nfa::EPSILON` does **not** become `Delta::Reserved::epsilon` — the plan contradicted itself

The old closing line of this phase ("Then `nfa::EPSILON` becomes `Delta::post<0>::epsilon` behind
the existing seam") is unsafe, and §3.8 four hundred lines earlier already says why: the module
`EPSILON` is re-exported by **using-declaration**, and redefining it as its own `constexpr` object
breaks any TU that opens both `mata` and a module namespace (8 errors in
`src/applications/strings/replace.cc`, measured then). Making it `Delta::Reserved::epsilon` is that
same redefinition wearing a different value.

Landed instead: the module constant stays a using-declaration, and each module's `delta.hh` carries
`static_assert(Delta::Reserved::epsilon == EPSILON)`. That buys the single source of truth the line
was after — give a relation a different tail without updating the constant and it is a compile
error, not a silent disagreement between explicit and defaulted arguments — without the ambiguity.
There is a matching assert in `core/delta.hh` for `mata::EPSILON`.

#### The two halves of the key vocabulary cannot be one descriptor

§3.13 says epsilon "is a reserved-*key* convention, not a symbol fact, and the two belong in one
descriptor". They do not, and NFA and NFT are the proof:

- **which symbols a key admits** is a property of the key *type* → a traits specialised on `K`
  (`KeyTraits<K>`), because `Interval` and `Symbol` are different types and answer differently;
- **where the ordinary keys stop** is a property of the *relation* → a template argument
  (`ReservedKeys`), because NFA and NFT key by the same `Symbol` and still differ. A traits
  specialised on the key type has exactly one answer per key type, so it cannot express this.

So they ship as two mechanisms that meet on the same entry. Everything §3.13 says about the *work*
being cheap held up — T2.2/T2.3 having stopped the posts storing a `Symbol` is what made this purely
additive.

#### `KeyDenotesSymbols` needs the members to be lazy, and that dictates their shape

`KeyTraits` deliberately has **no primary definition**, so a key that denotes no symbols gets no
answer rather than a wrong one. That makes the obvious spelling fail:

```cpp
// Hard error when Delta is instantiated over a weight key -- not an unavailable member.
utils::OrdVector<typename KeyTraits<Key>::SymbolType> get_used_symbols() const
    requires KeyDenotesSymbols<Key>;
```

A member's declared type is formed when the *class* is instantiated, before any constraint on it is
looked at. Checked in isolation: the relation then fails to instantiate at all. The two shapes that
work are a deduced (`auto`) return type and a defaulted **member** template parameter; the second
shipped, because a library header returning `auto` from `get_used_symbols` is worse to read than one
extra template line. `mata::SymbolKeyOf<K, Key>` bundles the guard, its `std::same_as` half pinning
the parameter back so it cannot be supplied by hand.

#### Two things measured on the way

- **`epsilon_symbol_posts` got 62% smaller**: 157 → 59 instructions at `-O2`. Nothing to do with
  epsilons — the old body searched with `find(Entry(epsilon))`, constructing and destroying a whole
  `SymbolPost` temporary to look up a key; it now calls `find(epsilon)`, which is what `PostLike`
  requires anyway. Same semantics (`SymbolPost`'s comparison operators only look at the key).
- **The symbol members are no longer emitted once.** As member templates, `extern template class
  posts::Delta<StatePost>` no longer suppresses them, so each calling TU emits its own copy (6 call
  sites). Measured on `src/core/delta.cc` at `-O2 -S`: 287 → 229 functions, **0 new**, and every one
  of the 237 shared functions identical in instruction count. The 58 that left are the symbol
  members plus the `OrdVector<Symbol>` / `std::set` / `std::vector<bool>` tail only they
  instantiated — `delta.o` no longer even links against `OnTheFlyAlphabet`.
- **`KeyTraits::for_each_symbol` costs nothing.** `get_used_symbols_vec` at `-O2`: 247 → 252
  instructions, **same 19 calls and same 33 jumps**, the difference being six `movq` for one `movl`,
  i.e. register scheduling. No lost inline. (The two were compiled in different TUs, which accounts
  for that much on its own.)

#### The gate

| check | result |
|---|---|
| `ctest` (debug) | **260/260** in 3.18 s — 257 before, plus three new cases in `tests/core/keys.cc` |
| assertions | `2008649`, of which the **`2008616` pre-existing ones are unchanged** — no behaviour moved |
| `-Werror` (`MATA_WERROR=ON`) | rc 0, **zero** warnings |
| Python bindings | `build_ext` rc 0, **99/99** pytest |
| Doxygen XML | 145 files, **0 invalid** (after `rm -rf docs/xml`) |
| Sphinx | rc 0; new concepts added to `docs/src/core.rst` and confirmed rendering |
| layering greps (§8) | all clean |
| files outside `core/` | exactly the two module seams, **additions only** (16 and 29 lines, 0 deletions) |
| `src/core/automaton.cc` at `-O2 -S` | **117 functions and 7755 instructions, in both** — the structural hot path is byte-for-byte unchanged |
| layout | `sizeof` of `SymbolPost` 40 / `StatePost` 32 / `Delta` 24 / `StateSet` 32, asserted at HEAD *and* now — the third template parameter costs nothing |

No delta-access benchmark run, deliberately: `automaton.cc` being instruction-for-instruction
identical is a stronger statement than the benchmark could make, and `for_each_target`,
`for_each_move` and the cursor were not touched at all.

**Both halves of T4.1 were mutation-tested**, because a test written against a seam that no shipping
relation exercises is exactly the kind that passes for the wrong reason (see Phase 3's
`RevertsTransitions` assert, §6):

- reverting the five defaults to `EPSILON` → **7 assertions in 4 sections** fail. Worth noting that
  the revert does not even *compile* once a non-symbol-keyed relation is in the test file, since
  `mata::EPSILON` will not convert to its key — so the weight-keyed relation from §3.13's half of
  this phase turns out to guard T4.1's half too, and the mutation had to be run with it removed.
- taking the O(1) `back()` path unconditionally (`if constexpr (true)`) → the "epsilon is not the
  greatest key" section fails on its own. That mutation is invisible to every pre-existing test,
  because `mata::Delta`'s epsilon *is* the greatest key.

#### Left for whoever owns NFT's semantics

`DONT_CARE == EPSILON - 1` is still **not** reserved: `nft`'s descriptor is the default, so
`moves_symbols()` iterates over `DONT_CARE` as an ordinary symbol, exactly as before. Whether it
should is a semantics question (a `DONT_CARE` is a wildcard *symbol*, not an epsilon) and this phase
deliberately changed no behaviour anywhere — `2008616` assertions before, the same `2008616` from
the pre-existing tests after. `include/mata/nft/delta.hh` carries the one-line recipe for answering
it, and the `static_assert` there forces the module constant to keep up when someone does.

### Phase 5 — ergonomics and docs — **DONE**

Landed in `core/concepts.hh`, `core/delta.hh`, `core/delta.tpp`, `core/automaton.hh`, the two module
seams, `nfa/nfa.hh`, `docs/src/core.rst`, `tests/core/levels.cc` (new) and small edits to
`tests/core/keys.cc` and `tests/core/cursor.cc`. **262 tests**, zero warnings under `-Werror`,
bindings 99/99, docs valid, and `src/core/automaton.cc` still **byte-identical** at `-O2`.

- **`Key<I>`, `Reserved<I>`, `PostAt<I>`, `TargetSet`, `KeyPath` on `Delta`.** The singular
  `Delta::Key` and `Delta::Reserved` are gone. A *post* keeps its singular `Key` — it has exactly
  one, so there is nothing to disambiguate — and the asymmetry is deliberate: a relation spans every
  level, and there a singular name silently meant the outermost one.
- **`PostChain` / `RelationOf`.** `RelationOf<Symbol, StateSet>` is asserted `std::same_as`
  `mata::Delta`, which is the strongest available statement about a chain builder: not that it
  produces *a* working relation but that it reproduces the shipping one exactly. A position may hold
  a `ReservedKeysLike` descriptor instead of a key, so giving one level a different reserved tail no
  longer means hand-spelling the whole stack — which is what NFT's `DONT_CARE` recipe needed, and
  that recipe is now one line.
- **Docs.** The `@page nfa` paragraph and the `Delta` class comment now count *keys*; §arity in
  `core/concepts.hh` no longer cites them as wrong. `std::OrdVector<State>` in `nfa.hh` was wrong
  twice (namespace, and `posts::StateTargets` since T2.1) and is gone. The new concepts and the
  level-indexed names are in `docs/src/core.rst` and confirmed rendering.

#### The defect this phase actually found: `DeltaLike` promised `add`

`DeltaLike` required `{ d.add(s, k, s) }`, and that was wrong twice over:

1. **Nothing in `AutomatonBase` calls it.** Its only mention was a `@note` on `reverted()` claiming
   the opposite — `reverted()` writes through `insert_target` and `mutable_state_post`. §3.11's own
   rule ("anything added to `AutomatonBase` which is not covered here is visible as a gap") cuts both
   ways, and nobody had checked the other direction.
2. **`add(source, key, target)` names exactly one key.** Its *declaration* is well-formed at any
   arity, so `DeltaLike<Delta2>` held, the call matched, and it then failed several instantiations
   deep inside `SymbolPost::insert`. Reproduced before fixing. That is precisely the diagnostic
   §3.10 exists to avoid, promised by the concept meant to prevent it.

`DeltaLike` now requires neither `add` nor a key type at all — the latter because §3.7 has always
said `Automaton` needs no key, and requiring one would also have forced a *level* on the contract.

Ten members that *write* one key, or hand back a `(source, key, target)` triple with room for one,
are now constrained `requires(P::key_arity == 1)`: both `add` overloads plus the `Transition` one,
`remove` ×2, `contains` ×2, `transitions()`, `get_transitions_to()`, `get_transitions_between()`.
Above arity 1 they are **absent**, not broken, which is the pattern T3.5 chose for the same reason.
`Transitions` also carries a `static_assert` naming `for_each_move()` as the generic alternative,
because an explicit class instantiation reaches nested classes that no constrained member guards.

#### `get_successors()` is generic — both overloads, and this took a correction

It used to return `Nested`, which above arity 1 is *the post one level down* rather than the targets
at the bottom. The state-only overload was generalised to walk and collect. The **keyed** overload I
first constrained to arity 1 instead, reasoning that it returns a reference into the relation and
that collecting would mean copying on a hot path.

That was the wrong call, and the reasoning had the question backwards. "Which states can I reach over
this key" means the same thing however many keys are left below it, so the answer is always a set of
targets; reaching one level *down* is a different question, and `find()` and `for_each_move()`
already answer it. The performance objection was real but is not a reason to constrain — it is a
reason for the **return type to follow the arity**:

```cpp
using Successors = std::conditional_t<key_arity == 1, const TargetSet&, TargetSet>;
```

At arity 1 the targets under one key are already contiguous, so it stays a reference — load-bearing,
because `Nfa::post()` (`nfa.hh:450`) passes it straight out as `const StateSet&` and a by-value
result there would dangle. Above arity 1 they sit under every remaining key and must be gathered, so
a fresh set. One `if constexpr`, two honest return types, no member missing anywhere.

**Free at arity 1**, all four forms instruction-identical at `-O2` against pre-Phase-4:
`StatePost::get_successors()` 55 → 55, `StatePost::get_successors(Key)` 145 → 145,
`Delta::get_successors(State)` 61 → 61, `Delta::get_successors(State, Key)` 154 → 154. The
`tests/core/levels.cc` check for the arity-1 case compares **addresses**, not values — equality would
pass just as well against a copy, which is exactly what must not happen.

#### `add_target`, and what the pack problem really was

The recorded blocker — "routing the arguments through a trailing pack loses the implicit conversions
`delta.add(0, 0, 1)` relies on" — is real, and worth stating precisely because two plausible fixes do
not work:

- a deduced pack (`const Keys&... keys`) carries the caller's `int` down and converts it per level
  inside `insert_target`: a narrowing **and** a sign-conversion warning each, which `-Werror`
  rejects. Measured.
- deducing the *indices* instead and declaring the parameters `Key<Is>...` moves the conversion to
  the right place but still fails: the literal has already become an `int` *parameter* by then, so it
  is no longer the in-range constant expression that keeps `add(0, 0, 1)` quiet. Also measured.

What works is making the key path **one declared value**: `KeyPath` is `std::tuple<Key<0>, …>`, built
over an index sequence, and `add_target(source, target, {1, 4})` converts the braced arguments at the
parameter — exactly where `add()` converts them. So the phase ships `add_target`, which presizes and
then calls `insert_target`; `add` keeps its argument order and its 260-odd call sites.

#### Two things to know, neither a Phase 5 decision

- **A trailing `requires`-clause changes a member's mangled name.** The ten constrained members all
  have new symbols (the Itanium ABI encodes the constraint), so a prebuilt `libmata.a` and new
  headers will not link. Not new damage — Phase 4 gave `SymbolPost` a third template parameter, which
  changed every mangled name in the post stack — but worth knowing once. `c++filt` does not demangle
  the constraint encoding, which makes assembly diffs read as though the members had vanished.
- **`distances_from_initial()` returns `num_of_states() + 1` entries.** Pre-existing, unrelated to
  key arity (the shipping arity-1 relation does the same, checked), and no test noticed. Left alone:
  it is a public member with a lot of tests standing on it, and changing it is not this phase's
  business. Recorded here because `tests/core/levels.cc` had to index rather than compare whole, and
  the next person to write such a test deserves to know why.

#### The gate

| check | result |
|---|---|
| `ctest` | **262/262** — 260 before, plus two new cases in `tests/core/levels.cc` |
| assertions | `2008688`; the `2008649` from before all still pass, so no behaviour moved |
| `-Werror` | rc 0, **zero** warnings |
| bindings | `build_ext` rc 0, **99/99** pytest |
| docs | 154 Doxygen XML files, **0 invalid**; Sphinx rc 0; new names render |
| layering greps (§8) | all clean |
| files outside `core/` | the two module seams, plus `nfa/nfa.hh` for the `@page` prose (a doc comment, not a call site) |
| `src/core/automaton.cc` at `-O2 -S` | **117 functions, 7755 instructions, in both** — identical to pre-Phase-4 |
| `src/core/delta.cc` | every shared function identical except Phase 4's `epsilon_symbol_posts`; the only new code is `add_target` and one `insert_target` instantiation |
| `get_successors` ×4 | instruction-identical at arity 1 (55/145/61/154, before and after) |

**Mutation-tested**, since most of what this phase adds is checked by `static_assert` and a
`requires`-probe that returns `false` is easy to write wrongly:

- un-constraining `Delta::add` (its state before this phase) → `tests/core/levels.cc:137` fails to
  compile, which is the assertion that an arity-2 relation must *not* offer it.
- dropping the presizing from `add_target` → the presize section fails at runtime (`4 == 8`), and
  notably does **not** crash: `mutable_state_post` grows for the source on its own, so only the
  *target* goes unsized. A test that merely wrote and read back would have missed it.

### Separate track — configurable `State`

Not part of the above. One typedef in `core/types.hh` behind a CMake option, landable any time
after Phase 1.

🚩 **Blocker:** `bindings/python/libmata/nfa/nfa.pxd:31` declares `ctypedef uintptr_t State`.
Change `State` to `uint32_t` and the bindings silently disagree with the C++ ABI — no compile
error, wrong behaviour at runtime. Resolve before enabling the option.

---

## 5. Invariants

1. `Nfa` and `Nft` remain **non-template classes**.
2. The seams keep resolving; no call site outside `core/` is ever edited. Checked exactly, per
   commit, by the `git diff --name-only` command in §4/Phase 2 — not by counting sites, which does
   not reproduce.
3. Every templated base member has a non-template forwarding wrapper in **both** leaves.
   Currently 4 such members: `is_lang_empty`, `is_identical`, `trim`, `trim_impl` (protected).
4. **`key_arity == 1` does not regress** — today's delta, structure depth 2; see §3.3. A hard gate,
   measured by the Phase 1.5 harness across all four dimensions, not an aspiration. It is what every
   NFA and NFT in the tree runs on.
5. `DeltaLike` stays the *complete* contract: everything `AutomatonBase` needs is in it, and no
   member carries a constraint of its own.

Invariant 5 **is** enforced, and cheaply: `tests/core/concepts.cc` explicitly instantiates
`AutomatonBase<TinyDelta>` over a read-only relation defined outside `mata`. Ordinary use
instantiates one member at a time, so a hidden requirement in an unused member stays invisible; an
explicit instantiation instantiates every member whose constraints are satisfied and so fails if
any of them needs more than the concept asks. That is what caught the `D(size_t)` requirement.
`TinyDelta` is also a second implementation of the post protocol, which matters because a concept
satisfied by exactly one type says very little.

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

**A stale build directory reports green tests that never ran.** `build/` here was left configured
by `make debug-lib` (`BUILD_TESTING=OFF`, `MATA_BUILD_EXAMPLES=OFF`), and CMake caches that. `make
debug` then rebuilds only `libmata` while `ctest --test-dir build` happily runs a **days-old**
`build/tests/tests`, printing a perfect 253/253 that proves nothing. Tell-tale: the run takes ~0.3 s
instead of ~3 s, and `cmake --build build --target help` lists no `tests` target. Check
`grep BUILD_TESTING build/CMakeCache.txt`, or just use a throwaway `BUILD_DIR`.

**An angle bracket in a doc comment can take down the whole doc build.** `@c Key<I>` in
`core/delta.hh` (added by Phase 0) made Doxygen read `<I>` as the HTML italics tag and emit
`<emphasis>` closed out of order inside `<computeroutput>`, producing **invalid XML**; breathe then
died with `mismatched tag` and Sphinx exited 2. The `docs` CMake target only runs Doxygen, so
`make docs` still returned 0 and the breakage sat unnoticed. Only single-letter HTML tag names bite
(`<I>`, `<A>`, `<B>`); `std::vector<uint8_t>` and `numeric_limits<T>` are fine. Write `` `Key<I>` ``
in backticks. Worth checking after any doc edit:
```sh
cmake --build <dir> --target docs
python3 -c "import glob,xml.etree.ElementTree as E; [E.parse(f) for f in glob.glob('docs/xml/*.xml')]"
cd docs && sphinx-build -b html --conf-dir . src /tmp/sx   # must exit 0
```

**Doxygen never deletes from `docs/xml/`.** Removing an entity leaves its XML behind, and
`doxygenconcept`/`doxygenclass` will still happily render it, so a page can document something that
no longer exists. `rm -rf docs/xml` before a doc run that is meant to prove something.

**Prose goes stale silently.** Three doc claims became false *within Phase 0*. `static_assert`
catches code drifting from the concepts; nothing catches comments. Prefer asserting a fact over
describing it.

**Check publication before deleting public API.** `mata/automaton.hh` had never been in a stable
release (only `-dev` tags), so its compat forwarder was deleted. `nft/delta.hh` is in every release
back to `v1.32.30`, so it stayed as a shim. Same test, opposite answers:
`git cat-file -e v1.32.44:<path>`.

**A constrained member's *type* is still formed when the class is instantiated.** A
`requires`-clause on a member decides whether it can be *called*; it does not stop the declaration
being instantiated. So a return type naming a traits with no primary definition
(`KeyTraits<Key>::SymbolType`) makes the whole class fail to instantiate rather than merely lack the
member — the opposite of the intent. Use a deduced return type or a defaulted **member** template
parameter (Phase 4 shipped the latter, `mata::SymbolKeyOf`). Cheap to check in isolation before
committing to a shape; a 20-line file settled it.

**GCC 15 does not SFINAE explicit template arguments inside a `requires`-expression.**
`requires { d.template f<X>(); }` where the constraint on `f` fails is reported as a **hard error**
("no matching function for call"), not as an unsatisfied requirement evaluating to `false`.
Reproduced in isolation, so `!requires { ... }` cannot be used to assert that an explicitly-argued
call is rejected. Argument-deduced calls SFINAE correctly, so the usual
`concept Has… = requires(const D d) { d.f(); }` probes are unaffected.

**Turning a member into a member template silently un-does `extern template`.** An explicit class
instantiation does not instantiate member templates, so members converted for laziness (Phase 4's
symbol members) stop being emitted once in `src/core/delta.cc` and start being emitted in every
calling TU. Harmless here — measured 287 → 229 functions in that TU, 0 new, all 237 shared ones
identical — but it is a per-TU compile-time cost that no build error will point at, and it changes
which libraries an object file needs (`delta.o` stopped referencing `OnTheFlyAlphabet`).

**A trailing `requires`-clause is part of a function's mangled name.** Constraining ten existing
`Delta` members in Phase 5 gave every one of them a new symbol, so a prebuilt `libmata.a` plus new
headers does not link. `c++filt` (GCC 15) does not demangle the constraint encoding either, so in an
assembly diff the constrained members read as *removed* and a row of raw `_ZN…Qeqsr…` strings reads
as *added*. Both are the same function.

**A doc comment can be wrong for years without anyone noticing.** `Delta::get_max_symbol()` was
documented as "the maximum **non-epsilon** used symbol" and has never excluded epsilons — and must
not, because NFT's simulation mints fresh symbols at `max + 1` and would collide with real epsilon
transitions if it did. Corrected to describe the code. Same lesson as "prose goes stale silently",
except this one was never true to begin with, so no drift caused it: prefer asserting a fact.

---

## 7. Open questions

1. ~~**Does the "max 4" cap mean `key_arity <= 4` or `<= 2`?**~~ **Answered:** neither — the "4" was
   *structure depth*, so the cap is `key_arity <= 3`. §3.3 has the conversion table; the question was
   mis-framed because it offered two counts and the intended one was a third.
2. **May `Nfa` and `Nft` diverge?** If yes, `to_nfa_move()` (`nft.hh:1371-1381`) stops being an O(1)
   move of the Delta and becomes a full rebuild — on the hot path of every transducer inclusion
   check. Recommendation: make divergence *expressible*, keep both on one instantiation until
   something concrete forces otherwise.
3. ~~**Is `key_arity >= 2` real or speculative?**~~ **Answered:** real, up to `key_arity` 3
   (structure depth 4). See §3.3b.
   Phase 3 is therefore mandatory, and is still the largest and riskiest — hence Phase 1.5 landing first.
4. **Is exact-match keying an assumption worth keeping?** `StatePost::find(Key)` and
   `get_successors(Key)` search for a key *equal* to the one given. An interval-keyed relation almost
   never wants that — it wants "which post admits symbol *s*", a `lower_bound` on `lo` followed by a
   containment check. §3.1 grants each post freedom of *container* ("a hash post, a bitmap post for a
   dense key space"); intervals show **lookup is a second axis** the plan does not cover, and a
   container choice does not supply it. Additive, so it blocks nothing: `find()` stays as it is and a
   containment lookup would sit alongside. But until it exists, an interval user has to write their
   own post for what should be a traits concern. See §3.13.
   **Half-answered by Phase 4:** `KeyTraits<K>::admits(key, symbol)` now exists and is part of
   `KeyDenotesSymbols`, so the *predicate* is a traits concern already. What is still missing is a
   lookup that uses it — nothing in `StatePost` calls `admits`, and finding the right entry with it
   needs the search too, not just the test.
5. **Should a structural-only automaton be able to ask "is there an accepting path"?** Today it
   cannot: `is_lang_empty` requires `get_word_for_path` to instantiate, and the two searches are
   private. Deliberate — add a protected accessor when something concrete needs it.

---

## 8. Verification recipe

```sh
# Use a build dir you know has tests enabled -- see the stale-build-dir trap in §6.
make debug JOBS=8 BUILD_DIR=/tmp/bt && ctest --test-dir /tmp/bt -j8  # expect 262/262 in ~3s, not ~0.3s
make debug-werror JOBS=8 BUILD_DIR=/tmp/bw               # expect rc=0 and *zero* warnings

# Bindings. The Makefile calls bare `python3`, so the venv has to be active for it to be the
#  venv's python. `.venv` already has Cython, pytest, tabulate and networkx; setuptools is the
#  one thing missing.
uv pip install --python .venv setuptools
cd bindings/python && ../../.venv/bin/python setup.py build_ext -fi -j8   # expect rc=0
../../.venv/bin/python -m pytest tests/                                  # never yet run

# Benchmarks: Release, and profiling off, or the numbers are meaningless (Phase 1.5).
cmake -B /tmp/br -S . -DCMAKE_BUILD_TYPE=Release -DNO_PROFILING=ON && cmake --build /tmp/br -j8

# Docs. Doxygen does not clean its output, so stale entities keep rendering (§6).
rm -rf docs/xml && cmake --build /tmp/bt --target docs
python3 -c "import glob,xml.etree.ElementTree as E; [E.parse(f) for f in glob.glob('docs/xml/*.xml')]"
cd docs && sphinx-build -b html --conf-dir . src /tmp/sx   # must exit 0

# Layering. The first must print nothing; the second only the two `using Run = mata::Run;` lines.
grep -rE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]mata/(nfa|nft)/' include/mata/core src/core
grep -rE --exclude=types.hh --exclude=delta.hh 'mata::(State|Delta|SymbolPost|StatePost|EPSILON)\b' \
  include/mata/nfa include/mata/nft src/nfa src/nft

# No key type reaches AutomatonBase (§2, Phase 0). Must print nothing.
grep -nE '\b(Symbol|Word)\b' include/mata/core/automaton.hh include/mata/core/automaton.tpp \
  src/core/automaton.cc

# No call site outside core/ moved (invariant 2). Must print nothing.
git diff --name-only HEAD~1 | grep -vE '^(include/mata/core/|src/core/|tests/|docs/|Plan\.md)'
```
