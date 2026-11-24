// Tests for the tuple-oriented Nft::post(...) family and the Nft::is_in_lang(...) /
// Nft::is_prefix_in_lang(...) methods built on top of them.
//
// These methods interpret an NFT as a k-tape machine (k = num_of_levels): a single
// "transducer step" walks a path of zero-level -> level 1 -> ... -> level k-1 -> zero-level
// states, reading one symbol per level from the corresponding tape (word). An EPSILON
// transition on a level consumes nothing on that tape, a DONT_CARE transition matches any
// single real input symbol (but not epsilon), and a jump transition (target skipping levels)
// reads the same symbol on every skipped level (JumpMode::RepeatSymbol) or the symbol on the
// first level and DONT_CARE on the rest (JumpMode::AppendDontCares).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <string>

#include "mata/nft/nft.hh"

using namespace mata;
using namespace mata::nft;
using mata::Symbol;
using mata::Word;

namespace {

// Readable symbol names. Note: nft::EPSILON is the max symbol and nft::DONT_CARE == EPSILON - 1.
constexpr Symbol a{0};
constexpr Symbol b{1};
constexpr Symbol c{2};
constexpr Symbol d{3};
constexpr Symbol x{10};
constexpr Symbol y{11};

// A 2-level NFT accepting exactly the relation { ("ab", "c") }.
// Path: 0 --a(lvl0)--> 2 --c(lvl1)--> 3 --b(lvl0)--> 4 --eps(lvl1)--> 1(final).
Nft make_ab_c() {
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State qf{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(qf);
	n.insert_word_by_levels(q0, {Word{a, b}, Word{c}}, qf);
	return n;
}

// A plain vector-tuple search over (state, reading-head positions) configurations, written directly from the
// documented tape semantics. It keeps no rank encoding, arena or visited bitmap, and serves as the reference
// strategy for the randomized differential checks of the optimized Nft::post() below.
struct ReferencePost {
	StateSet result{};
	StateSet visited{};
	// Whether a discover of a final zero-level state would halt the search of a match_prefix
	// is_in_lang_by_levels(), and whether a discover of one at the very end of every word would halt a full
	// membership query. The end additionally requires every projected-out word to be empty, like post() does.
	bool hits_final{false};
	bool hits_final_at_end{false};
	// Every reached configuration: a state with the reading-head positions of all levels.
	std::set<std::pair<State, std::vector<size_t>>> configurations{};
};

ReferencePost reference_post(
	const Nft& nft,
	const StateSet& states,
	const std::vector<Word>& words,
	const BoolVector& use_level,
	const bool epsilon_closure_after,
	const JumpMode jump_mode
) {
	ReferencePost out{};
	if (nft.delta.empty()) {
		// The only reachable configurations are the initial states at the start, so the input is fully consumed iff
		// every word is empty. They halt a prefix membership query when one of them is final, a full one when the
		// input is also consumed.
		const bool all_input_consumed{std::ranges::all_of(words, [](const Word& word) { return word.empty(); })};
		out.hits_final = std::ranges::any_of(states, [&](const State state) { return nft.final.contains(state); });
		out.hits_final_at_end = out.hits_final && all_input_consumed;
		if (all_input_consumed) { out.result = states; }
		out.visited = states;
		for (const State state : states) { out.configurations.emplace(state, std::vector<size_t>(words.size(), 0)); }
		return out;
	}
	const size_t num_levels{words.size()};
	std::vector<size_t> ends(num_levels);
	for (size_t level{0}; level < num_levels; ++level) { ends[level] = words[level].size(); }

	auto symbols_match = [](const Symbol first, const Symbol second) {
		return (first == EPSILON || second == EPSILON) ? (first == EPSILON && second == EPSILON)
													   : (first == second || first == DONT_CARE || second == DONT_CARE);
	};

	std::set<std::pair<State, std::vector<size_t>>>& seen{out.configurations};
	std::vector<std::pair<State, std::vector<size_t>>> worklist{};
	auto discover = [&](const State target, const std::vector<size_t>& positions) {
		if (nft.levels[target] == 0 && nft.final.contains(target)) {
			out.hits_final = true;
			bool at_end{true};
			for (size_t level{0}; level < num_levels && at_end; ++level) {
				at_end = use_level[level] ? positions[level] == ends[level] : ends[level] == 0;
			}
			if (at_end) { out.hits_final_at_end = true; }
		}
		if (seen.insert({target, positions}).second) { worklist.emplace_back(target, positions); }
	};
	const std::vector<size_t> zero_positions(num_levels, 0);
	for (const State state : states) { discover(state, zero_positions); }

	while (!worklist.empty()) {
		const auto [current, positions]{std::move(worklist.back())};
		worklist.pop_back();
		const Level current_level{nft.levels[current]};

		if (current_level == 0) {
			out.visited.insert(current);
			// A projected-out level with a word can never be consumed; the other levels must be at their ends.
			bool at_end{true};
			for (size_t level{0}; level < num_levels && at_end; ++level) {
				at_end = use_level[level] ? positions[level] == ends[level] : ends[level] == 0;
			}
			if (at_end) {
				out.result.insert(current);
				if (!epsilon_closure_after) { continue; }
			}
		}

		for (const SymbolPost& symbol_post : nft.delta[current]) {
			for (const State target : symbol_post.targets) {
				const Level target_level{nft.levels[target]};
				const size_t stop_level{target_level == 0 ? num_levels : target_level};
				std::vector<size_t> next_positions{positions};
				bool consumed{true};
				for (size_t level = current_level; level < stop_level; ++level) {
					if (!use_level[level]) { continue; }
					const Symbol expected = (level == current_level || jump_mode != JumpMode::AppendDontCares)
												? symbol_post.symbol
												: DONT_CARE;
					const Word& word{words[level]};
					if (positions[level] >= word.size() || !symbols_match(expected, word[positions[level]])) {
						consumed = false;
						break;
					}
					next_positions[level] = positions[level] + 1;
				}
				if (consumed) { discover(target, next_positions); }
				if (symbol_post.symbol == EPSILON) { discover(target, positions); }
			}
		}
	}
	return out;
}

size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

// A random level-respecting NFT over @p num_of_symbols symbols, with jumps, DONT_CARE and EPSILON transitions.
// A jump from a state on level l targets a state on level (l + d) % num_of_levels for a d in [1, num_of_levels],
// keeping the transition well-formed: a whole cycle is only jumped from level 0, so the span is never empty.
Nft random_post_nft(
	std::mt19937& rng, const size_t num_of_states_per_level, const size_t num_of_symbols, const size_t num_of_levels
) {
	Nft nft{Nft::with_levels(num_of_levels)};
	std::vector<std::vector<State>> states_by_level(num_of_levels);
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			states_by_level[level].push_back(nft.add_state_with_level(static_cast<Level>(level)));
		}
	}
	for (const size_t initial : {size_t{0}, draw(rng, num_of_states_per_level)}) {
		nft.initial.insert(states_by_level[0][initial]);
	}
	for (const State state : states_by_level[0]) {
		if (draw(rng, 3) == 0) { nft.final.insert(state); }
	}
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (const State source : states_by_level[level]) {
			for (size_t transitions{0}, limit{draw(rng, 3)}; transitions < limit; ++transitions) {
				size_t distance{};
				do {
					distance = 1 + draw(rng, num_of_levels);
				} while ((level + distance) % num_of_levels != 0 && (level + distance) % num_of_levels <= level);
				const State target{
					states_by_level[(level + distance) % num_of_levels][draw(rng, num_of_states_per_level)]
				};
				Symbol symbol{static_cast<Symbol>(draw(rng, num_of_symbols))};
				if (draw(rng, 4) == 0) { symbol = DONT_CARE; }
				if (draw(rng, 5) == 0) { symbol = EPSILON; }
				nft.delta.add(source, symbol, target);
			}
		}
	}
	return nft;
}

std::vector<Word> random_level_words(std::mt19937& rng, const size_t num_of_levels, const size_t max_length) {
	std::vector<Word> words(num_of_levels);
	for (Word& word : words) {
		for (size_t length{draw(rng, max_length + 1)}; word.size() < length;) {
			word.push_back(static_cast<Symbol>(draw(rng, 3)));
		}
	}
	return words;
}

// A random level mask: all levels, one level, two levels, a scattered subset, or none.
BoolVector random_use_level(std::mt19937& rng, const size_t num_of_levels) {
	BoolVector use_level(num_of_levels, false);
	switch (draw(rng, 5)) {
		case 0:
			std::ranges::fill(use_level, true);
			break;
		case 1:
			use_level[draw(rng, num_of_levels)] = true;
			break;
		case 2: {
			size_t remaining{num_of_levels >= 2 ? size_t{2} : size_t{1}};
			for (size_t level{0}; level < num_of_levels && remaining > 0; ++level) {
				if (draw(rng, num_of_levels - level) < remaining) {
					use_level[level] = true;
					--remaining;
				}
			}
			break;
		}
		case 3:
			for (size_t level{0}; level < num_of_levels; ++level) { use_level[level] = draw(rng, 2) == 0; }
			break;
		default:
			break; // No levels at all.
	}
	return use_level;
}

// A BoolVector as text, for the failure dump.
std::string describe(const BoolVector& bits) {
	std::string description{"["};
	for (const bool bit : bits) { description += bit ? "1" : "0"; }
	return description + "]";
}

// Checks the public early-exit predicate of post() against the reference search. Never firing, it is called with
// exactly the reached configurations, a projected-out level being at position zero. Firing at a chosen one of
// them, it halts the search there.
void check_early_exit_config(
	const Nft& nft,
	const StateSet& starts,
	const std::vector<Word>& words,
	const BoolVector& use_level,
	const JumpMode jump_mode
) {
	const ReferencePost reference{reference_post(nft, starts, words, use_level, true, jump_mode)};
	std::set<std::pair<State, std::vector<size_t>>> configurations{};
	const auto record = [&](const State state, const std::vector<size_t>& positions) {
		configurations.emplace(state, positions);
		return false;
	};
	CHECK(nft.post(starts, words, use_level, nullptr, true, jump_mode, record) == reference.result);
	CHECK(configurations == reference.configurations);

	// Stop at a configuration that has read the most input.
	const auto chosen{std::ranges::max_element(reference.configurations, {}, [](const auto& configuration) {
		return std::accumulate(configuration.second.begin(), configuration.second.end(), size_t{0});
	})};
	REQUIRE(chosen != reference.configurations.end());
	bool halted{false};
	bool called_after_halt{false};
	const auto stop_at_chosen = [&](const State state, const std::vector<size_t>& positions) {
		called_after_halt = called_after_halt || halted;
		halted = halted || (state == chosen->first && positions == chosen->second);
		return halted;
	};
	nft.post(starts, words, use_level, nullptr, true, jump_mode, stop_at_chosen);
	CHECK(halted);
	CHECK_FALSE(called_after_halt);
}

} // namespace

TEST_CASE("mata::nft::Nft::post(symbol, level) — basic matching and other-level indifference") {
	// 0 --a(lvl0)--> 1 --c(lvl1)--> 2(final).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q2);
	n.delta.add(q0, a, q1);
	n.delta.add(q1, c, q2);

	SECTION("match on level 0, indifferent to level 1") {
		// 'a' matches on level 0; the level-1 symbol 'c' is walked over to the next zero-level state.
		CHECK(n.post(StateSet{q0}, a, 0) == StateSet{q2});
		CHECK(n.post(q0, a, 0) == StateSet{q2}); // single-state overload
	}

	SECTION("no match on level 0") { CHECK(n.post(StateSet{q0}, b, 0) == StateSet{}); }

	SECTION("match on level 1, indifferent to level 0") {
		// The level-0 transition 'a' is walked over; 'c' must match on level 1.
		CHECK(n.post(StateSet{q0}, c, 1) == StateSet{q2});
		CHECK(n.post(StateSet{q0}, d, 1) == StateSet{});
	}

	SECTION("empty NFT (empty delta)") {
		Nft empty{Nft::with_levels(2)};
		const State s{empty.add_state_with_level(0)};
		CHECK(empty.post(StateSet{s}, a, 0) == StateSet{});
		// With an epsilon closure requested and EPSILON symbol, the source state stays reachable.
		CHECK(empty.post(StateSet{s}, EPSILON, 0, EpsilonClosureOpt::Before) == StateSet{s});
	}
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — DONT_CARE") {
	// 0 --DONT_CARE(lvl0)--> 1 --c(lvl1)--> 2(final).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q2);
	n.delta.add(q0, DONT_CARE, q1);
	n.delta.add(q1, c, q2);

	// DONT_CARE transition is matched by any real symbol.
	CHECK(n.post(StateSet{q0}, a, 0) == StateSet{q2});
	CHECK(n.post(StateSet{q0}, b, 0) == StateSet{q2});
	// ... but a DONT_CARE transition is not matched by an EPSILON query.
	CHECK(n.post(StateSet{q0}, EPSILON, 0) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — epsilon closure") {
	// 0 --eps--> 1 (both zero-level), 1 --a(lvl0)--> 2 --c(lvl1)--> 3(final).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(0)};
	const State q2{n.add_state_with_level(1)};
	const State q3{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q3);
	n.delta.add(q0, EPSILON, q1);
	n.delta.add(q1, a, q2);
	n.delta.add(q2, c, q3);

	SECTION("NONE: the epsilon move is not taken") {
		CHECK(n.post(StateSet{q0}, a, 0, EpsilonClosureOpt::None) == StateSet{});
	}

	SECTION("BEFORE: epsilon closure then the symbol step") {
		CHECK(n.post(StateSet{q0}, a, 0, EpsilonClosureOpt::Before) == StateSet{q3});
	}

	SECTION("EPSILON query: closure includes the source, NONE is a single epsilon step") {
		CHECK(n.post(StateSet{q0}, EPSILON, 0, EpsilonClosureOpt::Before) == StateSet{q0, q1});
		CHECK(n.post(StateSet{q0}, EPSILON, 0, EpsilonClosureOpt::None) == StateSet{q1});
	}

	SECTION("AFTER: symbol step then epsilon closure") {
		// 0 --a(lvl0)--> 1 --c(lvl1)--> 2(zero-level), and 2 --eps--> 3(final zero-level).
		Nft m{Nft::with_levels(2)};
		const State s0{m.add_state_with_level(0)};
		const State s1{m.add_state_with_level(1)};
		const State s2{m.add_state_with_level(0)};
		const State s3{m.add_state_with_level(0)};
		m.initial.insert(s0);
		m.final.insert(s3);
		m.delta.add(s0, a, s1);
		m.delta.add(s1, c, s2);
		m.delta.add(s2, EPSILON, s3);
		CHECK(m.post(StateSet{s0}, a, 0, EpsilonClosureOpt::None) == StateSet{s2});
		CHECK(m.post(StateSet{s0}, a, 0, EpsilonClosureOpt::After) == StateSet{s2, s3});
	}
}

TEST_CASE("mata::nft::Nft::post(symbol, level) — jump transitions crossing the level") {
	SECTION("whole-cycle jump (level 0 -> level 0)") {
		// 0 --a(jump lvl0->lvl0)--> 1(final): consumes 'a' on level 0 and 'a' on level 1.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State q1{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q1);
		n.delta.add(q0, a, q1);

		// Query for a symbol on level 1 which the jump crosses.
		CHECK(n.post(StateSet{q0}, a, 1, EpsilonClosureOpt::None, JumpMode::RepeatSymbol) == StateSet{q1});
		CHECK(n.post(StateSet{q0}, b, 1, EpsilonClosureOpt::None, JumpMode::RepeatSymbol) == StateSet{});
		// AppendDontCares: the crossed level is a DONT_CARE, matched by any real symbol.
		CHECK(n.post(StateSet{q0}, b, 1, EpsilonClosureOpt::None, JumpMode::AppendDontCares) == StateSet{q1});
	}

	SECTION("3-level, not jumping over the queried level") {
		// 0 --a(lvl0)--> 1(lvl1) --b(lvl1)--> 2(lvl2): query on level 2 walks levels 0 and 1.
		Nft n{Nft::with_levels(3)};
		const State q0{n.add_state_with_level(0)};
		const State q1{n.add_state_with_level(1)};
		const State q2{n.add_state_with_level(2)};
		const State q3{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q3);
		n.delta.add(q0, a, q1);
		n.delta.add(q1, b, q2);
		n.delta.add(q2, c, q3);
		CHECK(n.post(StateSet{q0}, c, 2) == StateSet{q3});
		CHECK(n.post(StateSet{q0}, d, 2) == StateSet{});
	}
}

TEST_CASE("mata::nft::Nft::post(words) — exact tuple matching") {
	Nft n{make_ab_c()};
	const State q0{*n.initial.begin()};
	const State qf{*n.final.begin()};

	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a, b}, Word{c}}) == StateSet{qf});
	CHECK(n.post(q0, std::vector<Word>{Word{a, b}, Word{c}}) == StateSet{qf}); // single-state overload
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a, b}, Word{d}}) == StateSet{});
	// ("a","c") is a proper prefix: it reaches an intermediate zero-level state but not the final one.
	CHECK_FALSE(n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{c}}).contains(qf));
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a, b}, Word{c, d}}) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(words) — epsilon track consumes nothing") {
	// Relation ("a", ""): 0 --a(lvl0)--> 1 --eps(lvl1)--> 2(final).
	// Regression: an EPSILON transition must never advance a (possibly already exhausted) tape head.
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q2);
	n.delta.add(q0, a, q1);
	n.delta.add(q1, EPSILON, q2);

	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{}}) == StateSet{q2});
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{c}}) == StateSet{});
	// Empty words consume nothing, so only the source zero-level state is reached.
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{}, Word{}}) == StateSet{q0});
}

TEST_CASE("mata::nft::Nft::post(words) — DONT_CARE track") {
	// Relation (x, "c") for any single symbol x: 0 --DONT_CARE(lvl0)--> 1 --c(lvl1)--> 2(final).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q2);
	n.delta.add(q0, DONT_CARE, q1);
	n.delta.add(q1, c, q2);

	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{c}}) == StateSet{q2});
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{b}, Word{c}}) == StateSet{q2});
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{d}}) == StateSet{});
	// DONT_CARE requires a real input symbol; it does not match an empty (epsilon) tape.
	CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{}, Word{c}}) == StateSet{});
}

TEST_CASE("mata::nft::Nft::post(words) — jump transition and jump modes") {
	// 0 --a(jump lvl0->lvl0)--> 1(final).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q1);
	n.delta.add(q0, a, q1);

	SECTION("RepeatSymbol: same symbol on every crossed level") {
		CHECK(
			n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{a}}, nullptr, true, JumpMode::RepeatSymbol) ==
			StateSet{q1}
		);
		CHECK(
			n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{b}}, nullptr, true, JumpMode::RepeatSymbol) ==
			StateSet{}
		);
	}
	SECTION("AppendDontCares: DONT_CARE on the crossed levels") {
		CHECK(
			n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{a}}, nullptr, true, JumpMode::AppendDontCares) ==
			StateSet{q1}
		);
		CHECK(
			n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{b}}, nullptr, true, JumpMode::AppendDontCares) ==
			StateSet{q1}
		);
		// The appended DONT_CARE still needs a real symbol on level 1.
		CHECK(
			n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{}}, nullptr, true, JumpMode::AppendDontCares) ==
			StateSet{}
		);
	}
}

TEST_CASE("mata::nft::Nft::post(words) — projection (use_level bitmask and word_levels)") {
	Nft n{make_ab_c()}; // relation ("ab", "c")
	const State q0{*n.initial.begin()};
	const State qf{*n.final.begin()};

	SECTION("word_levels projection ignores the unspecified level") {
		// Project onto level 0: level 1 ("c") is ignored, so "ab" is enough to reach the final state.
		CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a, b}}, std::vector<Level>{0}).contains(qf));
		// Project onto level 1: level 0 ("ab") is ignored.
		CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{c}}, std::vector<Level>{1}).contains(qf));
		CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{d}}, std::vector<Level>{1}) == StateSet{});
	}

	SECTION("use_level bitmask overload matches word_levels projection") {
		BoolVector use_level_0{true, false};
		CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{a, b}, Word{}}, use_level_0).contains(qf));
		BoolVector use_level_1{false, true};
		CHECK(n.post(StateSet{q0}, std::vector<Word>{Word{}, Word{c}}, use_level_1).contains(qf));
	}
}

TEST_CASE("mata::nft::Nft::post(words) — empty delta, empty words, and visited states") {
	SECTION("empty delta: all-empty words keep the source states, otherwise empty") {
		Nft n{Nft::with_levels(2)};
		const State s{n.add_state_with_level(0)};
		CHECK(n.post(StateSet{s}, std::vector<Word>{Word{}, Word{}}) == StateSet{s});
		CHECK(n.post(StateSet{s}, std::vector<Word>{Word{a}, Word{}}) == StateSet{});
	}

	SECTION("visited_zero_level_states is populated") {
		// Relation ("a","x") then ("b","y") chained through zero-level state qmid.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State qmid{n.add_state_with_level(0)};
		const State qend{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(qend);
		n.insert_word_by_levels(q0, {Word{a}, Word{x}}, qmid);
		n.insert_word_by_levels(qmid, {Word{b}, Word{y}}, qend);

		StateSet visited{};
		const StateSet targets{n.post(StateSet{q0}, std::vector<Word>{Word{a, b}, Word{x, y}}, &visited)};
		CHECK(targets == StateSet{qend});
		// Both the intermediate and final zero-level states are visited while consuming the tuple.
		CHECK(visited.contains(q0));
		CHECK(visited.contains(qmid));
		CHECK(visited.contains(qend));
	}
}

TEST_CASE("mata::nft::Nft::post(words) — epsilon_closure_after flag") {
	// Consuming ("a","c") reaches zero-level state q2, and q2 --eps--> q3 (final zero-level).
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(0)};
	const State q3{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q3);
	n.delta.add(q0, a, q1);
	n.delta.add(q1, c, q2);
	n.delta.add(q2, EPSILON, q3);

	// With the closure (default) the trailing epsilon move to q3 is followed.
	CHECK(
		n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{c}}, nullptr, /*epsilon_closure_after=*/true) ==
		StateSet{q2, q3}
	);
	// Without it, exploration stops at the first zero-level state reached at the end position.
	CHECK(
		n.post(StateSet{q0}, std::vector<Word>{Word{a}, Word{c}}, nullptr, /*epsilon_closure_after=*/false) ==
		StateSet{q2}
	);
	// is_in_lang_by_levels always closes after, so it still reaches the final state.
	CHECK(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{c}}));
}

// Exercises the post()-based, short-circuiting membership path taken by is_in_lang_by_levels() under
// JumpMode::AppendDontCares (the RepeatSymbol default uses the dedicated worklist algorithm instead).
TEST_CASE("mata::nft::Nft::is_in_lang_by_levels — post-based JumpMode::AppendDontCares path") {
	SECTION("a DONT_CARE-padded jump accepts where RepeatSymbol would not") {
		// A single jump q0 --a--> qf: both endpoints are level 0, so it spans (reads) both levels.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State qf{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(qf);
		n.delta.add(q0, a, qf);

		const std::vector<Word> a_a{Word{a}, Word{a}};
		const std::vector<Word> a_b{Word{a}, Word{b}};
		// Level 0 reads 'a'; level 1 reads 'a' under RepeatSymbol but DONT_CARE (any symbol) under AppendDontCares.
		CHECK(n.is_in_lang_by_levels(a_a, false, JumpMode::RepeatSymbol));
		CHECK(n.is_in_lang_by_levels(a_a, false, JumpMode::AppendDontCares));
		CHECK_FALSE(n.is_in_lang_by_levels(a_b, false, JumpMode::RepeatSymbol));
		CHECK(n.is_in_lang_by_levels(a_b, false, JumpMode::AppendDontCares)); // DONT_CARE matches 'b'.
		// The first (non-padded) level must still match exactly.
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{Word{b}, Word{a}}, false, JumpMode::AppendDontCares));
	}

	SECTION("short-circuit still follows a trailing epsilon to a final zero-level state") {
		// ("a","c") lands on the non-final zero-level q2, which epsilon-moves to the final zero-level q3.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State q1{n.add_state_with_level(1)};
		const State q2{n.add_state_with_level(0)};
		const State q3{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q3);
		n.delta.add(q0, a, q1);
		n.delta.add(q1, c, q2);
		n.delta.add(q2, EPSILON, q3);

		CHECK(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{c}}, false, JumpMode::AppendDontCares));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{d}}, false, JumpMode::AppendDontCares));
	}

	SECTION("prefix membership short-circuits on a final state before all input is consumed") {
		// ("a","c") reaches the final zero-level q2; consuming further ("ab","cd") ends in the non-final q4.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State q1{n.add_state_with_level(1)};
		const State q2{n.add_state_with_level(0)};
		const State q3{n.add_state_with_level(1)};
		const State q4{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q2);
		n.delta.add(q0, a, q1);
		n.delta.add(q1, c, q2);
		n.delta.add(q2, b, q3);
		n.delta.add(q3, d, q4);

		const std::vector<Word> ab_cd{Word{a, b}, Word{c, d}};
		// Full membership: ("ab","cd") ends in the non-final q4, only ("a","c") is accepted outright.
		CHECK_FALSE(n.is_in_lang_by_levels(ab_cd, false, JumpMode::AppendDontCares));
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{c}}, false, JumpMode::AppendDontCares));
		// Prefix membership accepts because ("a","c") is an accepted prefix of ("ab","cd").
		CHECK(n.is_in_lang_prefix_by_levels(ab_cd, JumpMode::AppendDontCares));
	}

	SECTION("empty delta: an initial final state accepts the empty prefix (regression guard)") {
		// No transitions => empty delta; q0 is both initial and final, so the language is { ("","") }.
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q0);

		const std::vector<Word> empty_tuple{Word{}, Word{}};
		const std::vector<Word> a_b{Word{a}, Word{b}};
		CHECK(n.is_in_lang_by_levels(empty_tuple, false, JumpMode::AppendDontCares));
		CHECK_FALSE(n.is_in_lang_by_levels(a_b, false, JumpMode::AppendDontCares));
		// The empty prefix is always accepted by an initial final state, even with input still unconsumed.
		CHECK(n.is_in_lang_prefix_by_levels(a_b, JumpMode::AppendDontCares));
	}
}

// The dedicated RepeatSymbol worklist is an internal helper (anonymous namespace in operations.cc), reachable only
// through Nft::is_in_lang_by_levels() under JumpMode::RepeatSymbol (its default), which is exercised here directly.
TEST_CASE("mata::nft::Nft::is_in_lang_by_levels — JumpMode::RepeatSymbol dedicated worklist") {
	SECTION("level-by-level transitions") {
		const Nft n{make_ab_c()}; // Relation { ("ab", "c") }.
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{Word{a, b}, Word{c}}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{Word{a, b}, Word{d}}));
		CHECK_FALSE(
			n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{c}})
		); // Reaches a non-final zero-level state.
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{Word{}, Word{}}));
	}

	SECTION("a jump transition, where RepeatSymbol reads the same symbol on every spanned level") {
		Nft n{Nft::with_levels(2)};
		const State q0{n.add_state_with_level(0)};
		const State qf{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(qf);
		n.delta.add(q0, a, qf); // Jump q0 --a--> qf spanning both levels: both must read 'a'.

		CHECK(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{a}}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{Word{a}, Word{b}}));
	}
}

TEST_CASE("mata::nft::Nft::is_in_lang_by_levels — long words on three levels around the 32-bit position rank") {
	// One zero-level state with a jump loop that reads 'a' on all three levels at once. With words of length n there
	// are (n + 1)^3 reading-head tuples: 1601^3 still fits in 32 bits, 1701^3 does not.
	Nft n{Nft::with_levels(3)};
	const State q0{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(q0);
	n.delta.add(q0, a, q0);

	for (const size_t length : {size_t{1'600}, size_t{1'700}}) {
		const Word word(length, a);
		const Word shorter(length - 1, a);
		Word last_differs{word};
		last_differs.back() = b;
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{word, word, word}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, word, shorter}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, last_differs, word}));
	}
}

TEST_CASE("mata::nft::Nft::is_in_lang_by_levels — a search over most reading-head positions") {
	// q0 advances either reading head alone, so the search reaches nearly every pair of positions in q0 before it
	// takes the final step, which reads 'c' on both levels at once.
	Nft n{Nft::with_levels(2)};
	const State q0{n.add_state_with_level(0)};
	const State qf{n.add_state_with_level(0)};
	n.initial.insert(q0);
	n.final.insert(qf);
	n.insert_word_by_levels(q0, {Word{DONT_CARE}, Word{}}, q0);
	n.insert_word_by_levels(q0, {Word{}, Word{DONT_CARE}}, q0);
	n.insert_word_by_levels(q0, {Word{c}, Word{c}}, qf);

	Word ends_with_c(99, b);
	ends_with_c.push_back(c);
	const Word only_b(100, b);
	for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{ends_with_c, ends_with_c}, false, jump_mode));
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{only_b, ends_with_c}, false, jump_mode) == false);
		CHECK(n.read_word_by_levels(std::vector<Word>{ends_with_c, ends_with_c}, jump_mode) == StateSet{q0, qf});
		CHECK(n.read_word_by_levels(std::vector<Word>{ends_with_c, only_b}, jump_mode) == StateSet{q0});
	}
}

TEST_CASE("mata::nft::Nft::post(words) — differential against a plain vector-tuple search") {
	// Small randomized NFTs with jumps, DONT_CARE and EPSILON transitions, checked on small words under every
	// level-mask shape (which selects the no-level, one-level, two-level and generic-rank paths of the position
	// encoding), both jump modes and both epsilon-closure variants of post().
	for (unsigned seed{0}; seed < 120; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_levels{1 + draw(rng, 5)};
		const Nft nft{random_post_nft(rng, 1 + draw(rng, 3), 1 + draw(rng, 2), num_of_levels)};
		const std::vector<Word> words{random_level_words(rng, num_of_levels, 4)};
		const BoolVector use_level{random_use_level(rng, num_of_levels)};
		const StateSet starts{nft.initial.begin(), nft.initial.end()};
		INFO("seed " << seed << ", levels " << num_of_levels << ", use_level " << describe(use_level));
		for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
			const ReferencePost reference{reference_post(nft, starts, words, use_level, true, jump_mode)};
			StateSet visited{};
			CHECK(nft.post(starts, words, use_level, &visited, true, jump_mode) == reference.result);
			CHECK(visited == reference.visited);
			CHECK(
				nft.post(starts, words, use_level, nullptr, false, jump_mode) ==
				reference_post(nft, starts, words, use_level, false, jump_mode).result
			);
			// Membership and prefix membership ask for a final zero-level state, at any reading-head position or
			// only with all input consumed.
			CHECK(
				nft.is_in_lang_by_levels(words, true, jump_mode, true) ==
				reference_post(nft, starts, words, BoolVector(num_of_levels, true), true, jump_mode).hits_final
			);
			CHECK(
				nft.is_in_lang_by_levels(words, false, jump_mode, true) ==
				reference_post(nft, starts, words, BoolVector(num_of_levels, true), true, jump_mode).hits_final_at_end
			);
		}
	}
}

TEST_CASE("mata::nft::Nft::post(words) — differential on 33 levels, past the 32-bit rank") {
	// Words of length one on every level give 2^33 > 2^32 reading-head tuples: the position ids cannot be ranks,
	// so post() interns the tuples in its arena instead. The same reference strategy checks the results.
	for (unsigned seed{200}; seed < 212; ++seed) {
		std::mt19937 rng{seed};
		constexpr size_t num_of_levels{33};
		const Nft nft{random_post_nft(rng, 1 + draw(rng, 2), 2, num_of_levels)};
		std::vector<Word> words(num_of_levels, Word{a});
		for (Word& word : words) { word.back() = static_cast<Symbol>(draw(rng, 2)); }
		const StateSet starts{nft.initial.begin(), nft.initial.end()};
		INFO("seed " << seed);
		for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
			const ReferencePost reference{
				reference_post(nft, starts, words, BoolVector(num_of_levels, true), true, jump_mode)
			};
			StateSet visited{};
			CHECK(
				nft.post(starts, words, BoolVector(num_of_levels, true), &visited, true, jump_mode) == reference.result
			);
			CHECK(visited == reference.visited);
			CHECK(nft.is_in_lang_by_levels(words, true, jump_mode, true) == reference.hits_final);
			CHECK(nft.is_in_lang_by_levels(words, false, jump_mode, true) == reference.hits_final_at_end);
		}
		// Projecting out every second level leaves 17 used levels: 2^17 tuples, so the generic rank encoding
		// answers the same questions without the arena.
		BoolVector even_levels(num_of_levels, false);
		for (size_t level{0}; level < num_of_levels; level += 2) { even_levels[level] = true; }
		const ReferencePost reference{reference_post(nft, starts, words, even_levels, true, JumpMode::RepeatSymbol)};
		StateSet visited{};
		CHECK(nft.post(starts, words, even_levels, &visited, true, JumpMode::RepeatSymbol) == reference.result);
		CHECK(visited == reference.visited);
	}
}

TEST_CASE("mata::nft::Nft::post(words) — the early-exit predicate sees the positions of all levels") {
	// Noncontiguous level masks with one, two and three used levels select the three rank encodings; the predicate
	// gets back the positions of all five levels, zeros on the projected-out ones.
	const std::vector<BoolVector> masks{{0, 0, 0, 1, 0}, {1, 0, 1, 0, 0}, {0, 1, 0, 1, 1}};
	for (unsigned seed{0}; seed < 40; ++seed) {
		std::mt19937 rng{seed};
		constexpr size_t num_of_levels{5};
		const Nft nft{random_post_nft(rng, 1 + draw(rng, 3), 1 + draw(rng, 2), num_of_levels)};
		const std::vector<Word> words{random_level_words(rng, num_of_levels, 4)};
		const StateSet starts{nft.initial.begin(), nft.initial.end()};
		for (const BoolVector& use_level : masks) {
			INFO("seed " << seed << ", use_level " << describe(use_level));
			for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
				check_early_exit_config(nft, starts, words, use_level, jump_mode);
			}
		}
	}
	// 32 used levels of 33 with words of length one give 2^32 reading-head tuples, so the positions are interned;
	// the projected-out level 1 sits between used ones.
	for (unsigned seed{200}; seed < 206; ++seed) {
		std::mt19937 rng{seed};
		constexpr size_t num_of_levels{33};
		const Nft nft{random_post_nft(rng, 1 + draw(rng, 2), 2, num_of_levels)};
		std::vector<Word> words(num_of_levels, Word{a});
		for (Word& word : words) { word.back() = static_cast<Symbol>(draw(rng, 2)); }
		BoolVector use_level(num_of_levels, true);
		use_level[1] = false;
		const StateSet starts{nft.initial.begin(), nft.initial.end()};
		INFO("seed " << seed);
		for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
			check_early_exit_config(nft, starts, words, use_level, jump_mode);
		}
	}
}

TEST_CASE("mata::nft::Nft::is_in_lang_by_levels — the rank/arena boundary of a four-level jump") {
	// A whole-cycle jump loop on four levels reads the words in lockstep: length 254 gives 255^4 tuples, below
	// 2^32 (the mixed-radix rank), and length 255 gives 256^4 == 2^32 (the interned arena).
	for (const size_t length : {size_t{254}, size_t{255}}) {
		Nft n{Nft::with_levels(4)};
		const State q0{n.add_state_with_level(0)};
		n.initial.insert(q0);
		n.final.insert(q0);
		n.delta.add(q0, a, q0);

		const Word word(length, a);
		Word shorter(length - 1, a);
		Word last_differs{word};
		last_differs.back() = b;
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{word, word, word, word}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{shorter, word, word, word}));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, word, word, last_differs}));
	}
}

TEST_CASE("mata::nft::Nft::post(words) — the interned fallback over thousands of tuples") {
	// A cycle of three states, each consuming one level: words of length 1'700 give 1'701^3 > 2^32 reading-head
	// tuples, which do not fit the rank, so the positions are interned. The search visits some 5'000 of them.
	Nft n{Nft::with_levels(3)};
	const State q0{n.add_state_with_level(0)};
	const State q1{n.add_state_with_level(1)};
	const State q2{n.add_state_with_level(2)};
	n.initial.insert(q0);
	n.final.insert(q0);
	n.delta.add(q0, a, q1);
	n.delta.add(q1, a, q2);
	n.delta.add(q2, a, q0);

	const Word word(1'700, a);
	Word shorter(1'699, a);
	Word middle_differs{word};
	middle_differs[800] = b;
	Word last_differs{word};
	last_differs.back() = b;
	const StateSet starts{q0};
	for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
		CHECK(n.is_in_lang_by_levels(std::vector<Word>{word, word, word}, false, jump_mode, true));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{shorter, word, word}, false, jump_mode, true));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, middle_differs, word}, false, jump_mode, true));
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, word, last_differs}, false, jump_mode, true));
		// The intermediate levels can never be at their ends.
		CHECK_FALSE(n.is_in_lang_by_levels(std::vector<Word>{word, Word{a}, word}, false, jump_mode, true));

		const ReferencePost reference{
			reference_post(n, starts, std::vector<Word>{word, word, word}, BoolVector(3, true), true, jump_mode)
		};
		StateSet visited{};
		CHECK(
			n.post(starts, std::vector<Word>{word, word, word}, BoolVector(3, true), &visited, true, jump_mode) ==
			reference.result
		);
		CHECK(visited == reference.visited);
	}
}
