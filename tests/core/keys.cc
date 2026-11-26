/** @file
 * @brief The key vocabulary: where a key position's reserved keys start, and how a relation reaches its key members.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"
#include "mata/relation.hh"
#include "mata/utils/ord-vector.hh"

using namespace mata;

namespace {

// A key that is not a symbol.
struct Interval {
	Symbol lo{};
	Symbol hi{};
	auto operator<=>(const Interval& other) const = default;
	bool operator==(const Interval& other) const = default;
	// For Catch2's failure messages.
	friend std::ostream& operator<<(std::ostream& os, const Interval& i) {
		return os << '[' << i.lo << ',' << i.hi << ']';
	}
};

// A key that is not a symbol and can be printed.
struct Weight {
	unsigned centi{};
	auto operator<=>(const Weight& other) const = default;
	bool operator==(const Weight& other) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Weight& w) { return os << w.centi << "c"; }
};

} // namespace.

// Weight is printable, so it can name alphabet symbols; Interval is not.
template <> struct std::formatter<Weight> {
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	template <typename Ctx> auto format(const Weight& w, Ctx& ctx) const {
		return std::format_to(ctx.out(), "{}c", w.centi);
	}
};

namespace {

// Far from mata::EPSILON, so a default using mata::EPSILON instead of the descriptor's cannot pass.
constexpr Symbol OUR_EPSILON{100};

// Only epsilon is reserved, as in an NFA.
using NarrowTail = ReservedKeys<Symbol, OUR_EPSILON>;
// A wider reserved tail, as for an NFT's DONT_CARE: 90..99 are neither ordinary symbols nor epsilons.
using WideTail = ReservedKeys<Symbol, OUR_EPSILON, OUR_EPSILON - 11>;

// Not PostChain: PostChain would take the invalid descriptor below for a key type.
template <typename Reserved> using PostOver = posts::Post<posts::PostEntry<Symbol, StateSet>, Reserved>;
template <typename Reserved> using DeltaOver = posts::DeltaBase<PostOver<Reserved>>;

using NarrowDelta = DeltaOver<NarrowTail>;
using WideDelta = DeltaOver<WideTail>;

using IntervalReserved =
	ReservedKeys<Interval, Interval{OUR_EPSILON, OUR_EPSILON}, Interval{OUR_EPSILON - 1, OUR_EPSILON - 1}>;
using IntervalDelta = posts::RelationOf<IntervalReserved, StateSet>;

using WeightReserved = ReservedKeys<Weight, Weight{100}, Weight{99}>;
using WeightDelta = posts::RelationOf<WeightReserved, StateSet>;

// An alphabet that is not mata::Alphabet, with a wider symbol type.
struct WideAlphabet {
	using Symbol = unsigned long long;
	void update_next_symbol_value(Symbol) {}
	void try_add_new_symbol(const std::string&, Symbol) {}
};

struct FixedAlphabet {
	using Symbol = mata::Symbol;
};

struct IntervalAlphabet {
	using Symbol = Interval;
	void update_next_symbol_value(Symbol) {}
	void try_add_new_symbol(const std::string&, Symbol) {}
};

struct WeightAlphabet {
	using Symbol = Weight;
	std::vector<std::pair<std::string, Weight>> received{};
	void update_next_symbol_value(Symbol) {}
	void try_add_new_symbol(const std::string& name, const Symbol symbol) { received.emplace_back(name, symbol); }
};

struct RecordingAlphabet {
	using Symbol = mata::Symbol;
	std::vector<Symbol> received{};
	void update_next_symbol_value(Symbol) {}
	void try_add_new_symbol(const std::string&, const Symbol symbol) { received.push_back(symbol); }
};

// Re-exports the protected key members. A using-declaration compiles even where the member's constraint fails.
template <typename D> struct Exposing : D {
	using D::add_keys_to;
	using D::D;
	using D::get_max_key;
	using D::get_used_keys;
	using D::get_used_keys_bv;
	using D::get_used_keys_chv;
	using D::get_used_keys_set;
	using D::get_used_keys_sps;
	using D::get_used_keys_vec;
};

// Ordinary keys run past the reserved ones. Not a ReservedKeys, whose static_assert would reject it.
struct InvertedTail {
	using Key = Symbol;
	static constexpr Symbol min_epsilon{100};
	static constexpr Symbol max_ordinary{200};
	static constexpr bool epsilon_is_greatest{false};
};

// Only mata::Delta has the symbol-named members.
template <typename D>
concept HasSymbolMembers = requires(const D d) {
	d.get_used_symbols();
	d.get_max_symbol();
};
// Any ordered key has these.
template <typename D>
concept HasKeyMembers = requires(const D d) {
	d.get_used_keys();
	d.get_used_keys_set();
	d.get_max_key();
};
// Only an unsigned integral key has these.
template <typename D>
concept HasIndexedKeyMembers = requires(const D d) {
	d.get_used_keys_sps();
	d.get_used_keys_bv();
	d.get_used_keys_chv();
};
template <typename D, typename A>
concept CanAddKeysTo = requires(const D d, A& a) { d.add_keys_to(a); };
template <typename D, typename A>
concept CanAddSymbolsTo = requires(const D d, A& a) { d.add_symbols_to(a); };

template <typename Moves> std::vector<Symbol> keys_of(const Moves& moves) {
	std::vector<Symbol> keys{};
	for (const auto& move : moves) { keys.push_back(move.symbol); }
	return keys;
}

} // namespace.

static_assert(posts::ReservedKeysLike<NarrowTail>);
static_assert(posts::ReservedKeysLike<WideTail>);
static_assert(posts::SortedWithReservedKeys<PostOver<NarrowTail>>);
static_assert(posts::PostLike<PostOver<WideTail>>);

static_assert(!posts::ReservedKeysLike<InvertedTail>);
static_assert(!posts::SortedWithReservedKeys<PostOver<InvertedTail>>);
static_assert(!posts::PostLike<PostOver<InvertedTail>>);

static_assert(!HasKeyMembers<IntervalDelta>); // protected
static_assert(HasKeyMembers<Exposing<IntervalDelta>>);
static_assert(HasKeyMembers<Exposing<WeightDelta>>);
static_assert(HasKeyMembers<Exposing<NarrowDelta>>);
static_assert(HasIndexedKeyMembers<Exposing<NarrowDelta>>);
static_assert(!HasIndexedKeyMembers<Exposing<IntervalDelta>>);
static_assert(!HasIndexedKeyMembers<Exposing<WeightDelta>>);
static_assert(HasSymbolMembers<Delta>);
static_assert(!HasSymbolMembers<Exposing<IntervalDelta>>);
static_assert(
	std::same_as<decltype(std::declval<const Exposing<IntervalDelta>&>().get_used_keys()), utils::OrdVector<Interval>>
);
static_assert(
	std::same_as<decltype(std::declval<const Exposing<IntervalDelta>&>().get_max_key()), std::optional<Interval>>
);

// A relation feeds an alphabet only if its keys are the alphabet's symbols and are printable.
static_assert(Printable<Symbol> && Printable<Weight> && !Printable<Interval>);
static_assert(CanAddKeysTo<Exposing<WeightDelta>, WeightAlphabet>);
static_assert(!CanAddKeysTo<Exposing<IntervalDelta>, IntervalAlphabet>); // right type, not printable
static_assert(!CanAddKeysTo<Exposing<IntervalDelta>, WideAlphabet>);
static_assert(!CanAddKeysTo<Exposing<WeightDelta>, WideAlphabet>);
static_assert(!CanAddKeysTo<Exposing<NarrowDelta>, WideAlphabet>); // would truncate
static_assert(!CanAddSymbolsTo<Delta, WideAlphabet>);

static_assert(std::same_as<WideAlphabet::Symbol, unsigned long long>);
static_assert(ExtensibleAlphabet<WideAlphabet>);
static_assert(!ExtensibleAlphabet<FixedAlphabet>);

TEST_CASE("mata::ReservedKeys - the defaults follow the relation, not core's constant") {
	SECTION("epsilon_symbol_posts() defaults to the relation's own epsilon") {
		NarrowDelta delta{};
		// The descriptor's epsilon and mata::EPSILON; the default must find the first.
		delta.add(0, OUR_EPSILON, 1);
		delta.add(0, EPSILON, 2);
		delta.add(0, 7, 3);

		const auto epsilon_post{delta.epsilon_symbol_posts(0)};
		REQUIRE(epsilon_post != delta.state_post(0).end());
		CHECK(epsilon_post->symbol == OUR_EPSILON);
		CHECK(epsilon_post->symbol != EPSILON);

		const auto core_epsilon_post{delta.epsilon_symbol_posts(0, EPSILON)};
		REQUIRE(core_epsilon_post != delta.state_post(0).end());
		CHECK(core_epsilon_post->symbol == EPSILON);

		// The lookup is by exact key.
		CHECK(delta.epsilon_symbol_posts(0, OUR_EPSILON + 1) == delta.state_post(0).end());
	}

	SECTION("the O(1) back() path is only taken when nothing can sort above epsilon") {
		static_assert(Delta::Reserved<0>::epsilon_is_greatest);
		// Here keys sort above epsilon, so the last entry is not the epsilon one.
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

		// Ordinary keys are those up to max_ordinary, 99.
		CHECK(keys_of(state_post.moves_symbols()) == std::vector<Symbol>{3, 98});
		CHECK(keys_of(state_post.moves_epsilons()) == std::vector<Symbol>{OUR_EPSILON, OUR_EPSILON + 5, EPSILON});
	}

	SECTION("a wider reserved tail belongs to neither range") {
		WideDelta delta{};
		for (const Symbol symbol : {Symbol{3}, Symbol{89}, Symbol{95}, OUR_EPSILON}) { delta.add(0, symbol, 1); }
		const auto& state_post{delta.state_post(0)};
		static_assert(WideTail::max_ordinary == 89);
		CHECK(keys_of(state_post.moves_symbols()) == std::vector<Symbol>{3, 89});
		CHECK(keys_of(state_post.moves_epsilons()) == std::vector<Symbol>{OUR_EPSILON});
	}

	SECTION("moves_symbols() refuses a reserved key as its upper bound") {
		WideDelta delta{};
		delta.add(0, 3, 1);
		const auto& state_post{delta.state_post(0)};
		CHECK_NOTHROW(state_post.moves_symbols(WideTail::max_ordinary));
		// Anything in the reserved band, not only epsilon.
		CHECK_THROWS(state_post.moves_symbols(WideTail::max_ordinary + 1));
		CHECK_THROWS(state_post.moves_symbols(OUR_EPSILON));
		CHECK_THROWS(state_post.moves_symbols(EPSILON));
	}

	SECTION("mata::Delta is unchanged: its descriptor says what it always did") {
		static_assert(Delta::Reserved<0>::min_epsilon == EPSILON);
		static_assert(Delta::Reserved<0>::max_ordinary == EPSILON - 1);
	}
}

TEST_CASE("mata::Delta's used-key queries do not carry state between calls") {
	Delta first{};
	first.add(0, 1, 1);
	Delta second{};
	second.add(0, 2, 1);

	(void) first.get_used_symbols();
	(void) first.get_used_symbols_vec();
	(void) first.get_used_symbols_set();
	(void) first.get_used_symbols_sps();
	(void) first.get_used_symbols_bv();

	CHECK(second.get_used_symbols() == utils::OrdVector<Symbol>{2});
	CHECK(second.get_used_symbols_vec() == utils::OrdVector<Symbol>{2});
	CHECK(second.get_used_symbols_set() == std::set<Symbol>{2});
	CHECK(utils::OrdVector<Symbol>{second.get_used_symbols_sps()} == utils::OrdVector<Symbol>{2});
	const std::vector<bool> bv{second.get_used_symbols_bv()};
	REQUIRE(bv.size() >= 3);
	CHECK_FALSE(bv[0]);
	CHECK_FALSE(bv[1]);
	CHECK(bv[2]);
}

TEST_CASE("mata::Delta's symbol members are the key members under the NFA's names") {
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
		CHECK_FALSE(bv[0]);
		CHECK(bv[1]);
		CHECK_FALSE(bv[2]);
		CHECK_FALSE(bv[3]);
		CHECK(bv[4]);
		static_assert(std::same_as<decltype(delta.get_used_symbols()), utils::OrdVector<Symbol>>);
		static_assert(std::same_as<decltype(delta.get_max_symbol()), Symbol>);
	}

	SECTION("the used symbols stay exact past the 64 distinct ones kept sorted incrementally") {
		// 64 distinct symbols fill the incremental accumulation, so the next new one switches it to a final sort.
		Delta delta{};
		std::set<Symbol> expected{};
		for (Symbol symbol{10}; symbol < 10 + 2 * 64; symbol += 2) {
			delta.add(0, symbol, 0);
			expected.insert(symbol);
		}
		// At the switch, a new symbol below the collected ones, then a duplicate, another new one and a new greatest.
		for (const Symbol symbol : {Symbol{11}, Symbol{12}, Symbol{13}, Symbol{500}}) {
			delta.add(1, symbol, 0);
			expected.insert(symbol);
		}
		REQUIRE(expected.size() == 64 + 3);
		const utils::OrdVector<Symbol> expected_symbols(expected);
		CHECK(delta.get_used_symbols() == expected_symbols);
		CHECK(delta.get_used_symbols_set() == expected);
		CHECK(utils::OrdVector<Symbol>{delta.get_used_symbols_sps()} == expected_symbols);
	}

	SECTION("an empty relation has no symbols, and its maximum is still zero") {
		const Delta delta{};
		CHECK(delta.get_used_symbols().empty());
		CHECK(delta.get_max_symbol() == 0);
	}
}

TEST_CASE("an interval-keyed relation reports the intervals it uses") {
	Exposing<IntervalDelta> delta{};
	delta.add(0, Interval{1, 3}, 1);
	delta.add(0, Interval{7, 8}, 2);
	delta.add(1, Interval{3, 4}, 2);

	SECTION("as keys, in the key's own order") {
		CHECK(delta.get_used_keys() == utils::OrdVector<Interval>{{1, 3}, {3, 4}, {7, 8}});
		CHECK(delta.get_used_keys_set() == std::set<Interval>{{1, 3}, {3, 4}, {7, 8}});
		CHECK(delta.get_max_key() == Interval{7, 8});
	}

	SECTION("an empty relation has no greatest key, rather than an invented one") {
		const Exposing<IntervalDelta> empty{};
		CHECK(empty.get_used_keys().empty());
		CHECK_FALSE(empty.get_max_key().has_value());
	}
}

TEST_CASE("a relation feeds an alphabet with its keys, named by their printed form") {
	Exposing<WeightDelta> delta{};
	delta.add(0, Weight{50}, 1);
	delta.add(0, Weight{75}, 2);
	delta.add(1, Weight{50}, 2);

	WeightAlphabet alphabet{};
	delta.add_keys_to(alphabet);
	// Once per distinct key, in key order: a key used by many transitions is converted and named once.
	REQUIRE(alphabet.received.size() == 2);
	CHECK(alphabet.received[0] == std::pair<std::string, Weight>{"50c", Weight{50}});
	CHECK(alphabet.received[1] == std::pair<std::string, Weight>{"75c", Weight{75}});
}

TEST_CASE("a relation feeds an alphabet with its ordinary keys only") {
	Exposing<WideDelta> delta{};
	delta.add(0, WideTail::max_ordinary, 1);
	delta.add(0, WideTail::max_ordinary + 1, 1); // Reserved, but not an epsilon.
	delta.add(0, OUR_EPSILON, 1);

	RecordingAlphabet alphabet{};
	delta.add_keys_to(alphabet);
	CHECK(alphabet.received == std::vector<Symbol>{WideTail::max_ordinary});
}

TEST_CASE("a key that is not a symbol still gets a working relation") {
	Exposing<WeightDelta> delta{};
	delta.add(0, Weight{50}, 1);
	delta.add(0, Weight{75}, 2);
	delta.add(1, Weight{50}, 2);

	CHECK(delta.num_of_transitions() == 3);
	CHECK(delta.get_used_keys() == utils::OrdVector<Weight>{Weight{50}, Weight{75}});
	CHECK(delta.get_max_key() == Weight{75});
	std::vector<State> successors{};
	delta.for_each_successor(0, [&successors](const State target) { successors.push_back(target); });
	CHECK(successors == std::vector<State>{1, 2});

	size_t epsilon_moves{0};
	for ([[maybe_unused]] const auto& move : delta.state_post(0).moves_epsilons()) { ++epsilon_moves; }
	CHECK(epsilon_moves == 0);
}

TEST_CASE("a failed remove() names the transition, whatever the key type") {
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
			// Interval has no std::formatter.
			CHECK(std::string{error.what()}.find("<unprintable>") != std::string::npos);
		}
	}

	SECTION("an arithmetic key still prints its value") {
		Delta delta{};
		delta.add(0, 'a', 1);
		try {
			// The generic relation throws; the shipped mata::Delta::remove() reports a missing transition by false.
			static_cast<Delta::Base&>(delta).remove(0, 'b', 1);
			CHECK(false); // Unreachable.
		} catch (const std::invalid_argument& error) {
			const std::string message{error.what()};
			CHECK(message.find("98") != std::string::npos); // 'b'
			CHECK(message.find("<unprintable>") == std::string::npos);
		}
	}
}
