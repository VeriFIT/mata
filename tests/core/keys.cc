/** @file
 * @brief The key vocabulary: where a level's reserved keys start, and which keys get the symbol members.
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
 * The symbol members are the same problem one level along: they exist only for an integral key,
 *  and a relation keyed by an interval or a weight has to instantiate and work without them rather
 *  than get members that return the keys under another name. See the Plan, §3.8 and §3.13.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/relation.hh"

using namespace mata;

namespace {

/// A key that is not a symbol: ordered, so the relation works, but with no symbol members.
struct Interval {
	Symbol lo{};
	Symbol hi{};
	auto operator<=>(const Interval& other) const = default;
	bool operator==(const Interval& other) const = default;
};

/// Another non-integral key, for the same reason.
struct Weight {
	unsigned centi{};
	auto operator<=>(const Weight& other) const = default;
	bool operator==(const Weight& other) const = default;
};

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
template <typename Reserved> using DeltaOver = posts::DeltaBase<PostOver<Reserved>>;

using NarrowDelta = DeltaOver<NarrowTail>;
using WideDelta = DeltaOver<WideTail>;

using IntervalReserved = ReservedKeys<Interval, Interval{OUR_EPSILON, OUR_EPSILON},
                                      Interval{OUR_EPSILON - 1, OUR_EPSILON - 1}>;
using IntervalDelta = posts::RelationOf<IntervalReserved, StateSet>;

using WeightReserved = ReservedKeys<Weight, Weight{100}, Weight{99}>;
using WeightDelta = posts::RelationOf<WeightReserved, StateSet>;

/// An alphabet that is not @c mata::Alphabet: its own wider symbol type, and able to grow.
struct WideAlphabet {
	using Symbol = unsigned long long;
	void update_next_symbol_value(Symbol) {}
	void try_add_new_symbol(const std::string&, Symbol) {}
};

/// …and one that cannot grow, so @c ExtensibleAlphabet must reject it.
struct FixedAlphabet {
	using Symbol = mata::Symbol;
};

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

/// Does this relation offer the symbol members at all? They are guarded on the key being integral,
///  so the answer can be *no* without the relation failing to instantiate.
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

/// §3.13: the symbol members follow the key *type*. An integral key has them, anything else does
///  not, and the relation instantiates either way.
static_assert(HasSymbolMembers<Delta>);
static_assert(!HasSymbolMembers<IntervalDelta>);
static_assert(!HasSymbolMembers<WeightDelta>);

/// @name An alphabet need not be mata's
///
/// @c mata::Alphabet cannot be specialised by inheritance — its symbol type is baked into its
///  virtual signatures, so a derived class redefining the alias overrides nothing. A third party
///  therefore brings their *own* class and specialises @c mata::AlphabetTraits, and everything that
///  asks about symbols has to work against that with no @c mata::Alphabet involved.
///@{
static_assert(std::same_as<AlphabetTraits<WideAlphabet>::Symbol, unsigned long long>);
static_assert(ExtensibleAlphabet<WideAlphabet>); ///< it can grow…
static_assert(!ExtensibleAlphabet<FixedAlphabet>); ///< …and a fixed one cannot, which is not an error
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

TEST_CASE("the symbol members of an integral-keyed relation read the keys directly") {
	SECTION("the used symbols are the keys, once each, in every container") {
		Delta delta{};
		delta.add(0, 1, 1);
		delta.add(0, 4, 1);
		delta.add(1, 4, 2);
		CHECK(delta.get_used_symbols() == utils::OrdVector<Symbol>{1, 4});
		CHECK(delta.get_max_symbol() == 4);
		CHECK(delta.get_used_symbols_set() == std::set<Symbol>{1, 4});
		CHECK(utils::OrdVector<Symbol>{delta.get_used_symbols_sps()} == delta.get_used_symbols());
		const std::vector<bool> bv{delta.get_used_symbols_bv()};
		REQUIRE(bv.size() >= 5); // It starts at a fixed capacity, so only the bits are specified.
		CHECK((!bv[0] && bv[1] && !bv[2] && !bv[3] && bv[4]));
		static_assert(std::same_as<decltype(delta.get_used_symbols()), utils::OrdVector<Symbol>>);
		static_assert(std::same_as<decltype(delta.get_max_symbol()), Symbol>);
	}

	SECTION("an empty relation has no symbols") {
		const Delta delta{};
		CHECK(delta.get_used_symbols().empty());
		CHECK(delta.get_max_symbol() == 0);
	}
}

TEST_CASE("a key that is not a symbol still gets a working relation") {
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

TEST_CASE("a failed remove() names the transition, whatever the key type") {
	// The message used to be built with std::to_string, which only exists for arithmetic types, so
	// an interval-keyed relation could add and query but could not instantiate remove() at all.
	SECTION("a key std::to_string cannot print is still removable") {
		IntervalDelta delta{};
		delta.add(0, Interval{1, 3}, 1);
		delta.add(0, Interval{7, 8}, 2);

		delta.remove(0, Interval{1, 3}, 1);
		CHECK_FALSE(delta.contains(0, Interval{1, 3}, 1));
		CHECK(delta.contains(0, Interval{7, 8}, 2));
		CHECK(delta.num_of_transitions() == 1);
	}

	SECTION("a key that cannot print itself gets a placeholder, not a compile error") {
		IntervalDelta delta{};
		delta.add(0, Interval{1, 3}, 1);
		try {
			delta.remove(0, Interval{20, 20}, 1);
			CHECK(false); // Unreachable: the transition is not there.
		} catch (const std::invalid_argument& error) {
			// Interval specialises no std::formatter, so the key reads as the placeholder. Specialising
			// one would put "[20,20]" here instead, which is the whole opt-in.
			CHECK(std::string{error.what()}.find("<unprintable>") != std::string::npos);
		}
	}

	SECTION("an arithmetic key still prints its value") {
		Delta delta{};
		delta.add(0, 'a', 1);
		try {
			delta.remove(0, 'b', 1);
			CHECK(false); // Unreachable.
		} catch (const std::invalid_argument& error) {
			const std::string message{error.what()};
			CHECK(message.find("98") != std::string::npos); // 'b'
			CHECK(message.find("<unprintable>") == std::string::npos);
		}
	}
}
