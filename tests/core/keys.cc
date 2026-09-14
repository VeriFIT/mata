/** @file
 * @brief The key vocabulary: where a level's reserved keys start, and which symbols a key admits.
 *
 * Both halves are here because both are **silent** when wrong, and in the same way: they decide
 *  what a *default argument* means and what a *member returns*, so a mistake in either produces a
 *  plausible answer rather than a diagnostic.
 *
 * Before @c mata::ReservedKeys, `moves_epsilons()`, `moves_symbols()` and
 *  `epsilon_symbol_posts()` defaulted to whichever `EPSILON` was in scope in
 *  @c mata/core/delta.hh — core's constant, always, for every instantiation. Nothing was wrong
 *  while every module shared one epsilon, and nothing would have gone wrong loudly once one did
 *  not: an NFT reserving a wider tail would simply have got core's threshold back, and
 *  `moves_symbols()` would have iterated over its reserved keys as if they were ordinary symbols.
 *  So the tests below are all built on relations whose descriptor **disagrees** with
 *  @c mata::EPSILON, which is the only way to tell a per-instantiation default from a baked-in one.
 *
 * @c mata::KeyTraits is the same problem one level along: with an interval key, "the symbols used
 *  on the transitions" is not the set of keys under another name, and a version that returned the
 *  keys would still compile and still look like a set of symbols. See the Plan, §3.8 and §3.13.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <limits>
#include <set>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"

using namespace mata;

namespace {

/// A key admitting a *range* of symbols, so that the expansion is not the identity. @see §3.13.
struct Interval {
	Symbol lo{};
	Symbol hi{};
	auto operator<=>(const Interval& other) const = default;
	bool operator==(const Interval& other) const = default;
};

/// A key admitting no symbols at all: nothing to expand, so the symbol members must not exist.
struct Weight {
	unsigned centi{};
	auto operator<=>(const Weight& other) const = default;
	bool operator==(const Weight& other) const = default;
};

} // namespace.

template <> struct mata::KeyTraits<Interval> {
	using SymbolType = Symbol;
	template <typename Fn> static void for_each_symbol(const Interval& key, Fn&& fn) {
		for (SymbolType s{key.lo}; s <= key.hi; ++s) { fn(s); }
	}
	static bool admits(const Interval& key, const SymbolType symbol) {
		return key.lo <= symbol && symbol <= key.hi;
	}
};

namespace {

/// The threshold this file's relations use. Deliberately nowhere near @c mata::EPSILON, so that a
///  default resolving to core's constant instead of the descriptor's cannot pass by coincidence.
constexpr Symbol OUR_EPSILON{100};

/// Epsilon alone is reserved, as for an NFA — but at 100 rather than at the top of the key range.
using NarrowTail = ReservedKeys<Symbol, OUR_EPSILON>;
/// A *wider* reserved tail, as an NFT's `DONT_CARE` would need: 90..99 are neither ordinary symbols
///  nor epsilons. This is the shape §3.8 says a traits keyed on the key type could not express,
///  because the key type is @c Symbol either way.
using WideTail = ReservedKeys<Symbol, OUR_EPSILON, OUR_EPSILON - 11>;

/// Spelled by hand rather than with @c posts::PostChain, because the negative tests below
///  instantiate it over a *deliberately invalid* descriptor. @c PostChain dispatches on
///  @c ReservedKeysLike, so an invalid descriptor falls through to its key-type branch and is taken
///  for a key — the builder is right to refuse it, which is exactly why the witness needs the long
///  form.
template <typename Reserved>
using PostOver = posts::Post<posts::PostEntry<Symbol, StateSet, Reserved>>;
template <typename Reserved> using DeltaOver = posts::Delta<PostOver<Reserved>>;

using NarrowDelta = DeltaOver<NarrowTail>;
using WideDelta = DeltaOver<WideTail>;

using IntervalReserved = ReservedKeys<Interval, Interval{OUR_EPSILON, OUR_EPSILON},
                                      Interval{OUR_EPSILON - 1, OUR_EPSILON - 1}>;
using IntervalDelta = posts::RelationOf<IntervalReserved, StateSet>;

using WeightReserved = ReservedKeys<Weight, Weight{100}, Weight{99}>;
using WeightDelta = posts::RelationOf<WeightReserved, StateSet>;

/// A descriptor with the two thresholds the wrong way round: it says the ordinary keys run *past*
///  where the reserved ones start. Hand-rolled rather than an instantiation of @c ReservedKeys,
///  because @c ReservedKeys carries its own @c static_assert and would refuse to exist at all —
///  which is the right behaviour, and also why the concept needs a separate witness to be tested
///  against.
struct InvertedTail {
	using Key = Symbol;
	static constexpr Symbol epsilon{100};
	static constexpr Symbol max_ordinary{200};
	static constexpr bool epsilon_is_greatest{false};
};

/// Does this relation offer the symbol members at all? The point of @c mata::SymbolKeyOf is that
///  the answer can be *no* without the relation failing to instantiate.
template <typename D>
concept HasSymbolMembers = requires(const D d) {
	d.get_used_symbols();
	d.get_max_symbol();
};

/// Collect the keys a @c Moves range yields, so the epsilon/symbol split can be compared directly.
template <typename Moves> std::vector<Symbol> keys_of(const Moves& moves) {
	std::vector<Symbol> keys{};
	for (const auto& move : moves) { keys.push_back(move.symbol); }
	return keys;
}

} // namespace.

/// @name Contract checks
///@{
static_assert(ReservedKeysLike<NarrowTail>);
static_assert(ReservedKeysLike<WideTail>);
static_assert(ReservedKeysAtTail<PostOver<NarrowTail>>);
static_assert(PostLike<PostOver<WideTail>>);

/// T4.2: a descriptor whose reserved keys are *not* the tail is rejected where the post is named,
///  rather than making @c first_epsilon_it() walk back to the wrong place.
static_assert(!ReservedKeysLike<InvertedTail>);
static_assert(!ReservedKeysAtTail<PostOver<InvertedTail>>);
static_assert(!PostLike<PostOver<InvertedTail>>);

/// §3.13: an interval admits many symbols, a weight admits none, and an integral key admits itself.
static_assert(KeyDenotesSymbols<Symbol>);
static_assert(KeyDenotesSymbols<Interval>);
static_assert(!KeyDenotesSymbols<Weight>);

/// The guard leaves the relation instantiable either way, and only the members conditional.
static_assert(HasSymbolMembers<Delta>);
static_assert(HasSymbolMembers<IntervalDelta>);
static_assert(!HasSymbolMembers<WeightDelta>);

/// @name The relation and the alphabet must agree on what a symbol is
///
/// @c mata::SymbolTypeAgrees is asserted at both module seams, where it currently cannot fail: the
///  relation's keys and @c mata::Alphabet are both @c mata::Symbol by construction. A check that
///  cannot fail proves nothing on its own, so the cases below pin down what it would *catch* —
///  otherwise the concept could be `true` unconditionally and every seam assertion would still pass.
///@{
static_assert(SymbolTypeAgrees<Delta::Key<0>, Alphabet>); ///< the shipping relation
static_assert(SymbolTypeAgrees<Interval, Alphabet>); ///< an interval denotes ordinary Symbols

/// A key whose symbols are *wider* than the alphabet's. This is the mismatch the seam assertions
///  exist to catch: `translate_symb()` would hand back an @c Alphabet::Symbol that converts
///  implicitly to this key's symbol type, truncating at whichever values do not fit.
static_assert(!std::same_as<unsigned long long, Alphabet::Symbol>);
static_assert(!SymbolTypeAgrees<unsigned long long, Alphabet>);

/// …and vacuously true where there is no relationship to get wrong: a weight denotes no symbols, so
///  it has no alphabet to disagree with. Without this branch the concept would reject every
///  relation that simply has nothing to say about symbols.
static_assert(!KeyDenotesSymbols<Weight> && SymbolTypeAgrees<Weight, Alphabet>);
///@}

/// The @c std::same_as half of @c mata::SymbolKeyOf pins the member's own parameter back to the
///  relation's key, so `d.get_used_symbols<Symbol>()` on a weight-keyed relation is rejected too.
///  Not asserted here: GCC 15 reports a call with *explicit* template arguments inside a
///  requires-expression as a hard error rather than as an unsatisfied requirement, so the check
///  cannot be written as `!requires { ... }` — it fails the build instead of evaluating to false.
///  Reproduced in isolation; nothing to do with this concept.
///@}

TEST_CASE("mata::ReservedKeys — the defaults follow the relation, not core's constant") {
	SECTION("epsilon_symbol_posts() defaults to the relation's own epsilon") {
		NarrowDelta delta{};
		// Both a key the descriptor calls epsilon and the one core does. A default that had baked in
		//  mata::EPSILON would find the second; the descriptor says the first.
		delta.add(0, OUR_EPSILON, 1);
		delta.add(0, EPSILON, 2);
		delta.add(0, 7, 3);

		const auto epsilon_post{delta.epsilon_symbol_posts(0)};
		REQUIRE(epsilon_post != delta.state_post(0).end());
		CHECK(epsilon_post->symbol == OUR_EPSILON);
		CHECK(epsilon_post->symbol != EPSILON);

		// The explicit form still addresses exactly the key asked for.
		const auto core_epsilon_post{delta.epsilon_symbol_posts(0, EPSILON)};
		REQUIRE(core_epsilon_post != delta.state_post(0).end());
		CHECK(core_epsilon_post->symbol == EPSILON);

		// A threshold no key carries is absent, not the nearest one above: the lookup is by exact
		//  key, which is what the member has always documented.
		CHECK(delta.epsilon_symbol_posts(0, OUR_EPSILON + 1) == delta.state_post(0).end());
	}

	SECTION("the O(1) back() path is only taken when nothing can sort above epsilon") {
		// mata::Delta's epsilon is the greatest key there is, so an epsilon entry can only be the
		//  last one and the answer is a look at the back.
		static_assert(Delta::Reserved<0>::epsilon_is_greatest);
		// Ours is not, so entries can and do sort above it, and the back is the wrong entry. Taking
		//  the fast path here would answer "no epsilons" for a relation that has one.
		static_assert(!NarrowDelta::Reserved<0>::epsilon_is_greatest);

		NarrowDelta delta{};
		delta.add(0, OUR_EPSILON, 1);
		delta.add(0, OUR_EPSILON + 500, 2); // Sorts after the epsilon.
		REQUIRE(delta.state_post(0).back().symbol > OUR_EPSILON);

		const auto epsilon_post{delta.epsilon_symbol_posts(0)};
		REQUIRE(epsilon_post != delta.state_post(0).end());
		CHECK(epsilon_post->symbol == OUR_EPSILON);
	}

	SECTION("moves_epsilons() and moves_symbols() split at the descriptor's thresholds") {
		NarrowDelta delta{};
		for (const Symbol symbol : {Symbol{3}, Symbol{98}, OUR_EPSILON, Symbol{OUR_EPSILON + 5}, EPSILON}) {
			delta.add(0, symbol, 1);
		}
		const auto& state_post{delta.state_post(0)};

		// Ordinary: everything at or below max_ordinary, which is 99 — so 98 but not 100.
		CHECK(keys_of(state_post.moves_symbols()) == std::vector<Symbol>{3, 98});
		// Reserved: everything from epsilon up. A default that had baked in mata::EPSILON would
		//  have yielded only the last of these.
		CHECK(keys_of(state_post.moves_epsilons())
		      == std::vector<Symbol>{OUR_EPSILON, OUR_EPSILON + 5, EPSILON});
	}

	SECTION("a wider reserved tail belongs to neither range") {
		WideDelta delta{};
		for (const Symbol symbol : {Symbol{3}, Symbol{89}, Symbol{95}, OUR_EPSILON}) { delta.add(0, symbol, 1); }
		const auto& state_post{delta.state_post(0)};

		// 90..99 are reserved but are not epsilons — an NFT's DONT_CARE band. They are excluded from
		//  the ordinary symbols without being counted as epsilons, which is the whole reason
		//  max_ordinary is a separate parameter rather than epsilon minus one.
		static_assert(WideTail::max_ordinary == 89);
		CHECK(keys_of(state_post.moves_symbols()) == std::vector<Symbol>{3, 89});
		CHECK(keys_of(state_post.moves_epsilons()) == std::vector<Symbol>{OUR_EPSILON});
	}

	SECTION("moves_symbols() refuses a reserved key as its upper bound") {
		WideDelta delta{};
		delta.add(0, 3, 1);
		const auto& state_post{delta.state_post(0)};
		CHECK_NOTHROW(state_post.moves_symbols(WideTail::max_ordinary));
		// Not just epsilon itself: anything in the reserved band, since the range would otherwise
		//  have to run past where the ordinary keys stop.
		CHECK_THROWS(state_post.moves_symbols(WideTail::max_ordinary + 1));
		CHECK_THROWS(state_post.moves_symbols(OUR_EPSILON));
		CHECK_THROWS(state_post.moves_symbols(EPSILON));
	}

	SECTION("mata::Delta is unchanged: its descriptor says what it always did") {
		static_assert(Delta::Reserved<0>::epsilon == EPSILON);
		static_assert(Delta::Reserved<0>::max_ordinary == EPSILON - 1);
	}
}

TEST_CASE("mata::KeyTraits — the used symbols are the union of the keys' expansions") {
	SECTION("an integral key admits exactly itself") {
		Delta delta{};
		delta.add(0, 1, 1);
		delta.add(0, 4, 1);
		delta.add(1, 4, 2);
		CHECK(delta.get_used_symbols() == utils::OrdVector<Symbol>{1, 4});
		CHECK(delta.get_max_symbol() == 4);
	}

	SECTION("an interval key admits a range, and that is what is used") {
		IntervalDelta delta{};
		delta.add(0, Interval{1, 3}, 1);
		delta.add(0, Interval{7, 8}, 2);
		delta.add(1, Interval{3, 4}, 2); // Overlaps the first: the union, not the concatenation.

		// Three keys, six symbols. Returning the keys under another name — which is what a rename
		//  of this member would have done — would have given three of something else entirely.
		CHECK(delta.get_used_symbols() == utils::OrdVector<Symbol>{1, 2, 3, 4, 7, 8});
		CHECK(delta.get_max_symbol() == 8);

		// The return type is the *symbol* type, not the key type. Nothing else would make the
		//  comparison above even compile, which is the point.
		static_assert(std::same_as<decltype(delta.get_used_symbols()), utils::OrdVector<Symbol>>);
		static_assert(std::same_as<decltype(delta.get_max_symbol()), Symbol>);

		CHECK(delta.get_used_symbols_set() == std::set<Symbol>{1, 2, 3, 4, 7, 8});
		CHECK(utils::OrdVector<Symbol>{delta.get_used_symbols_sps()} == delta.get_used_symbols());
	}

	SECTION("admits() answers containment, not equality") {
		CHECK(KeyTraits<Interval>::admits(Interval{2, 5}, 2));
		CHECK(KeyTraits<Interval>::admits(Interval{2, 5}, 5));
		CHECK_FALSE(KeyTraits<Interval>::admits(Interval{2, 5}, 6));
		CHECK(KeyTraits<Symbol>::admits(3, 3));
		CHECK_FALSE(KeyTraits<Symbol>::admits(3, 4));
	}

	SECTION("an empty relation has no symbols") {
		const IntervalDelta delta{};
		CHECK(delta.get_used_symbols().empty());
		CHECK(delta.get_max_symbol() == 0);
	}
}

TEST_CASE("a key that denotes no symbols still gets a working relation") {
	// The relation instantiates, stores transitions and walks them. Only the members that would
	// have had to invent an answer are missing — see the static_asserts above.
	WeightDelta delta{};
	delta.add(0, Weight{50}, 1);
	delta.add(0, Weight{75}, 2);
	delta.add(1, Weight{50}, 2);

	CHECK(delta.num_of_transitions() == 3);
	std::vector<State> successors{};
	delta.for_each_successor(0, [&successors](const State target) { successors.push_back(target); });
	CHECK(successors == std::vector<State>{1, 2});

	// Its epsilon members work too: a reserved-key convention is meaningful for any ordered key,
	// even where a *symbol* is not.
	size_t epsilon_moves{0};
	for ([[maybe_unused]] const auto& move : delta.state_post(0).moves_epsilons()) { ++epsilon_moves; }
	CHECK(epsilon_moves == 0);
}
