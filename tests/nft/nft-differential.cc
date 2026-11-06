/* nft-differential.cc -- Seeded randomized differential tests of the NFT algorithms.
 *
 * The automata come from raw std::mt19937 output, so a seed gives the same transducer under every
 *  standard library, and failing inputs are printed in full.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/nft/nft.hh"
#include "mata/utils/ord-vector.hh"

using namespace mata::nft;
using mata::EnumAlphabet;
using mata::Symbol;
using mata::Word;
using mata::utils::OrdVector;

namespace {

size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A random level-respecting NFT over {0, ..., num_of_symbols - 1}: every transition goes from a
///  state on level l to a state on level (l + 1) mod num_of_levels. Each (state, symbol) gets zero,
///  one or two targets, so a word can reach several states at once whenever the level holds more
///  than one state. @p with_dont_care and @p with_epsilon add DONT_CARE and EPSILON transitions,
///  which the algorithms have to treat like the ordinary ones. @p with_jumps adds jump transitions, from a
///  level l to a level after l + 1 or to level 0 skipping a level, over any of these symbols.
Nft random_nft(
	std::mt19937& rng,
	const size_t num_of_states_per_level,
	const size_t num_of_symbols,
	const size_t num_of_levels,
	const bool with_dont_care = false,
	const bool with_epsilon = false,
	const bool with_jumps = false
) {
	Nft nft{Nft::with_levels(num_of_levels)};
	std::vector<std::vector<State>> states_by_level(num_of_levels);
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			states_by_level[level].push_back(nft.add_state_with_level(static_cast<Level>(level)));
		}
	}
	nft.initial.insert(states_by_level[0][draw(rng, num_of_states_per_level)]);
	for (const State state : states_by_level[0]) {
		if (draw(rng, 3) == 0) { nft.final.insert(state); }
	}
	for (size_t level{0}; level < num_of_levels; ++level) {
		const size_t next_level{(level + 1) % num_of_levels};
		for (const State source : states_by_level[level]) {
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
					nft.delta.add(source, symbol, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
				}
			}
			if (with_dont_care && draw(rng, 4) == 0) {
				nft.delta.add(source, DONT_CARE, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
			}
			if (with_epsilon && draw(rng, 5) == 0) {
				nft.delta.add(source, EPSILON, states_by_level[next_level][draw(rng, num_of_states_per_level)]);
			}
			// Up to two jumps, each ending on a level in [level + 2, num_of_levels], where num_of_levels stands for
			//  level 0.
			for (size_t jump{0},
				 jumps{with_jumps && level + 2 <= num_of_levels && draw(rng, 4) == 0 ? 1 + draw(rng, 2) : 0};
				 jump < jumps; ++jump) {
				const size_t end{level + 2 + draw(rng, num_of_levels - level - 1)};
				std::vector<Symbol> symbols{};
				for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { symbols.push_back(symbol); }
				if (with_dont_care) { symbols.push_back(DONT_CARE); }
				if (with_epsilon) { symbols.push_back(EPSILON); }
				nft.delta.add(
					source, symbols[draw(rng, symbols.size())],
					states_by_level[end % num_of_levels][draw(rng, num_of_states_per_level)]
				);
			}
		}
	}
	return nft;
}

std::shared_ptr<EnumAlphabet> alphabet_of(const size_t num_of_symbols) {
	auto alphabet{std::make_shared<EnumAlphabet>()};
	for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { alphabet->add_new_symbol(std::to_string(symbol)); }
	return alphabet;
}

/// Every word over {0, ..., num_of_symbols - 1} whose length is at most @p max_length and divisible
///  by @p num_of_levels, so that it can be read by a level-respecting NFT.
std::vector<Word> all_level_words(const size_t num_of_symbols, const size_t max_length, const size_t num_of_levels) {
	std::vector<Word> words{Word{}};
	std::vector<Word> result{Word{}};
	for (size_t length{1}; length <= max_length; ++length) {
		std::vector<Word> next{};
		for (const Word& word : words) {
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				Word extended{word};
				extended.push_back(symbol);
				next.push_back(std::move(extended));
			}
		}
		words = std::move(next);
		if (length % num_of_levels == 0) { result.insert(result.end(), words.begin(), words.end()); }
	}
	return result;
}

std::string describe(const unsigned seed, const Nft& nft) {
	std::string description{"seed " + std::to_string(seed) + "\nlevels:"};
	for (const Level level : nft.levels) { description += " " + std::to_string(level); }
	description += "\ninitial:";
	for (const State state : nft.initial) { description += " " + std::to_string(state); }
	description += "\nfinal:";
	for (const State state : nft.final) { description += " " + std::to_string(state); }
	description += "\ntransitions:\n";
	for (const Transition& transition : nft.delta.transitions()) {
		description += std::to_string(transition.source) + " " + std::to_string(transition.symbol) + " " +
					   std::to_string(transition.target) + "\n";
	}
	return description;
}

/// The tuples of words, one word per level, of @p nft with no word longer than @p max_length: a search over the
///  paths that drops EPSILON, spells DONT_CARE out as each of the symbols {0, ..., num_of_symbols - 1}, and repeats
///  the symbol of a jump on every level it covers. Exact up to the bound, since a path never shortens a word.
std::set<std::vector<Word>> bounded_tuples(const Nft& nft, const size_t num_of_symbols, const size_t max_length) {
	const size_t num_of_levels{nft.levels.num_of_levels};
	using Configuration = std::pair<State, std::vector<Word>>;
	std::set<Configuration> visited{};
	std::vector<Configuration> worklist{};
	for (const State state : nft.initial) {
		Configuration initial{state, std::vector<Word>(num_of_levels)};
		if (visited.insert(initial).second) { worklist.push_back(std::move(initial)); }
	}
	std::set<std::vector<Word>> tuples{};
	while (!worklist.empty()) {
		const Configuration configuration{std::move(worklist.back())};
		worklist.pop_back();
		const auto& [source, words] = configuration;
		if (nft.final.contains(source)) { tuples.insert(words); }
		for (const auto& symbol_post : nft.delta[source]) {
			std::vector<Symbol> symbols{};
			if (symbol_post.symbol == DONT_CARE) {
				for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) { symbols.push_back(symbol); }
			} else {
				symbols.push_back(symbol_post.symbol);
			}
			for (const State target : symbol_post.targets) {
				const size_t end_level{nft.levels[target] == 0 ? num_of_levels : nft.levels[target]};
				for (const Symbol symbol : symbols) {
					Configuration next{target, words};
					bool is_within_bound{true};
					for (size_t level{nft.levels[source]}; symbol != EPSILON && level < end_level; ++level) {
						next.second[level].push_back(symbol);
						is_within_bound = is_within_bound && next.second[level].size() <= max_length;
					}
					if (is_within_bound && visited.insert(next).second) { worklist.push_back(std::move(next)); }
				}
			}
		}
	}
	return tuples;
}

/// Whether @p nft has a transition over EPSILON.
bool has_epsilon_transition(const Nft& nft) {
	for (const Transition& transition : nft.delta.transitions()) {
		if (transition.symbol == EPSILON) { return true; }
	}
	return false;
}

} // Anonymous namespace.

TEST_CASE("mata::nft differential: complement accepts exactly the rejected words", "[differential]") {
	size_t nondeterministic_inputs{0};
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t num_of_levels{2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));
		if (!nft.is_deterministic()) { ++nondeterministic_inputs; }

		const Nft complemented{complement(nft, *alphabet)};
		for (const Word& word : all_level_words(num_of_symbols, 4, num_of_levels)) {
			// Exactly one of the transducer and its complement accepts each word of the alphabet.
			const bool accepted{nft.is_in_lang(word)};
			const bool accepted_by_complement{complemented.is_in_lang(word)};
			CHECK(accepted != accepted_by_complement);
		}
	}
	// Complementing goes through determinization, which is trivial on a deterministic input, so the
	//  corpus has to contain transducers where a word reaches several states at once.
	CHECK(nondeterministic_inputs > 0);
}

TEST_CASE("mata::nft differential: the inclusion algorithms agree", "[differential]") {
	size_t nondeterministic_inputs{0};
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		std::mt19937 other_rng{seed + 500};
		const size_t num_of_symbols{1 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft lhs{random_nft(rng, 1 + seed % 3, num_of_symbols, 2)};
		const Nft rhs{random_nft(other_rng, 1 + seed % 3, num_of_symbols, 2)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 500, rhs));
		if (!lhs.is_deterministic() || !rhs.is_deterministic()) { ++nondeterministic_inputs; }

		const bool naive{is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "naive"}})};
		const bool antichains{
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "antichains"}})
		};
		CHECK(naive == antichains);

		if (naive) {
			// Every bounded word of the included transducer is accepted by the including one.
			for (const Word& word : all_level_words(num_of_symbols, 4, 2)) {
				if (lhs.is_in_lang(word)) { CHECK(rhs.is_in_lang(word)); }
			}
		}
	}
	// The two algorithms can only differ on a transducer where a word reaches several states.
	CHECK(nondeterministic_inputs > 0);
}

TEST_CASE("mata::nft differential: the revert variants build the same transducer", "[differential]") {
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 3};
		const size_t num_of_levels{1 + seed % 3};
		const Nft nft{random_nft(rng, 1 + seed % 4, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));

		const Nft reverted{revert(nft)};
		CHECK(reverted.delta == simple_revert(nft).delta);
		CHECK(reverted.delta == fragile_revert(nft).delta);
		CHECK(reverted.delta == somewhat_simple_revert(nft).delta);
		CHECK(reverted.initial == simple_revert(nft).initial);
		CHECK(reverted.final == simple_revert(nft).final);
		CHECK(reverted.levels == fragile_revert(nft).levels);
		CHECK(reverted.levels == simple_revert(nft).levels);
		// Reverting twice gives the original transition relation back.
		CHECK(revert(reverted).delta == nft.delta);
	}
}

TEST_CASE("mata::nft differential: remove_epsilon() keeps the relation", "[differential]") {
	size_t shared_epsilon_successors{0};
	for (unsigned seed{0}; seed < 1'000; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t num_of_levels{2 + seed % 2};
		const size_t num_of_states_per_level{2 + seed % 2};
		// random_nft() gives the inner states transitions over every symbol, which no ε-run passes. Here
		//  every transition is EPSILON with probability 1/2, so states reached only by ε from several
		//  predecessors, the shape remove_epsilon() used to cut (#728), are common.
		Nft nft{Nft::with_levels(num_of_levels)};
		for (size_t level{0}; level < num_of_levels; ++level) {
			for (size_t index{0}; index < num_of_states_per_level; ++index) {
				nft.add_state_with_level(static_cast<Level>(level));
			}
		}
		nft.initial.insert(static_cast<State>(draw(rng, num_of_states_per_level)));
		for (State state{0}; state < num_of_states_per_level; ++state) {
			if (draw(rng, 2) == 0) { nft.final.insert(state); }
		}
		for (State source{0}; source < nft.num_of_states(); ++source) {
			const size_t next_level{(nft.levels[source] + 1) % num_of_levels};
			for (size_t transition{0}, transitions{1 + draw(rng, 2)}; transition < transitions; ++transition) {
				const Symbol symbol{draw(rng, 2) == 0 ? EPSILON : static_cast<Symbol>(draw(rng, num_of_symbols))};
				nft.delta.add(
					source, symbol,
					static_cast<State>(next_level * num_of_states_per_level + draw(rng, num_of_states_per_level))
				);
			}
		}
		std::vector<size_t> in_degree(nft.num_of_states(), 0);
		std::vector<bool> only_epsilon_in(nft.num_of_states(), true);
		for (const Transition& transition : nft.delta.transitions()) {
			++in_degree[transition.target];
			if (transition.symbol != EPSILON) { only_epsilon_in[transition.target] = false; }
		}
		for (State state{0}; state < nft.num_of_states(); ++state) {
			if (in_degree[state] > 1 && only_epsilon_in[state]) {
				++shared_epsilon_successors;
				break;
			}
		}
		INFO(describe(seed, nft));

		const Nft result{remove_epsilon(nft)};
		CHECK(result.levels.num_of_levels == num_of_levels);
		// Every tuple of words up to length 2, one word per level, is accepted by both or by neither.
		const std::vector<Word> words{all_level_words(num_of_symbols, 2, 1)};
		std::vector<Word> tuple(num_of_levels);
		std::vector<size_t> indices(num_of_levels, 0);
		while (true) {
			for (size_t level{0}; level < num_of_levels; ++level) { tuple[level] = words[indices[level]]; }
			CHECK(nft.is_in_lang_by_levels(tuple) == result.is_in_lang_by_levels(tuple));
			size_t level{0};
			while (level < num_of_levels && ++indices[level] == words.size()) { indices[level++] = 0; }
			if (level == num_of_levels) { break; }
		}
	}
	CHECK(shared_epsilon_successors > 0);
}

// An operand carries DONT_CARE or EPSILON, never both: General matches DONT_CARE with EPSILON (#763, item 7). For the
//  same reason, General accepts tuples of words of different lengths that DONT_CARE operands do not, so with
//  DONT_CARE only the tuples of words of the same length, read without any EPSILON, are compared.
// The default compose() reads its operands with JumpMode::RepeatSymbol and takes the fast composition when they have
//  no jump transition, EPSILON or DONT_CARE. Its result must then be the general composition, EPSILON placement
//  included, which are_equivalent() reads as a letter, for every pair of single synchronization levels. A one-level
//  left operand is the layout of Nft::apply(). Two one-level operands composed without projection are the exception:
//  General synchronizes their EPSILON self-loops into an EPSILON self-loop of the result, which on one level is the
//  empty word, so these results are compared without EPSILON.
TEST_CASE(
	"mata::nft differential: the default composition of jump-free transducers is the general one", "[differential]"
) {
	size_t num_of_nonempty{0};
	for (unsigned seed{0}; seed < 200; ++seed) {
		std::mt19937 lhs_rng{seed};
		std::mt19937 rhs_rng{seed + 5'300};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t lhs_num_of_levels{1 + seed % 3};
		const size_t rhs_num_of_levels{1 + (seed / 2) % 3};
		// The number of states varies apart from the number of levels, so one-level operands have several states.
		const Nft lhs{random_nft(lhs_rng, 1 + (seed / 3) % 4, num_of_symbols, lhs_num_of_levels)};
		const Nft rhs{random_nft(rhs_rng, 1 + seed % 3, num_of_symbols, rhs_num_of_levels)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 5'300, rhs));
		for (Level lhs_sync_level{0}; lhs_sync_level < lhs_num_of_levels; ++lhs_sync_level) {
			for (Level rhs_sync_level{0}; rhs_sync_level < rhs_num_of_levels; ++rhs_sync_level) {
				for (const bool project_out : {true, false}) {
					if (project_out && lhs_num_of_levels + rhs_num_of_levels == 2) { continue; }
					INFO("sync " << lhs_sync_level << "," << rhs_sync_level << " project_out " << project_out);
					const Nft by_default{compose(lhs, rhs, lhs_sync_level, rhs_sync_level, project_out)};
					const Nft general{compose(
						lhs, rhs, lhs_sync_level, rhs_sync_level, project_out, JumpMode::RepeatSymbol,
						CompositionMode::General
					)};
					REQUIRE(by_default.levels.num_of_levels == general.levels.num_of_levels);
					if (lhs_num_of_levels + rhs_num_of_levels == 2) {
						CHECK(are_equivalent(remove_epsilon(by_default), remove_epsilon(general)));
					} else {
						CHECK(are_equivalent(by_default, general));
					}
					if (!general.is_lang_empty()) { ++num_of_nonempty; }
				}
			}
		}
	}
	CHECK(num_of_nonempty > 0);
}

TEST_CASE("mata::nft differential: the composition modes agree on jump-free transducers", "[differential]") {
	size_t nondeterministic_inputs{0};
	size_t composed_with_epsilon{0};
	for (unsigned seed{0}; seed < 400; ++seed) {
		for (const auto& [with_dont_care, with_epsilon] :
			 {std::pair{false, false}, std::pair{true, false}, std::pair{false, true}}) {
			std::mt19937 lhs_rng{seed};
			std::mt19937 rhs_rng{seed + 700};
			const size_t num_of_symbols{1 + seed % 2};
			const Nft lhs{random_nft(lhs_rng, 1 + seed % 2, num_of_symbols, 2, with_dont_care, with_epsilon)};
			const Nft rhs{random_nft(rhs_rng, 1 + seed % 2, num_of_symbols, 2, with_dont_care, with_epsilon)};
			INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 700, rhs));
			if (!lhs.is_deterministic() || !rhs.is_deterministic()) { ++nondeterministic_inputs; }

			const Nft general{compose(lhs, rhs, 1, 0, true, JumpMode::NoJump, CompositionMode::General)};
			const Nft fast{compose(lhs, rhs, 1, 0, true, JumpMode::NoJump, CompositionMode::FastNoJump)};
			CHECK(general.levels.num_of_levels == fast.levels.num_of_levels);
			if (with_dont_care) {
				for (const Word& word : all_level_words(num_of_symbols, 4, 2)) {
					CHECK(general.is_in_lang(word) == fast.is_in_lang(word));
				}
				continue;
			}
			// The modes may place EPSILON differently, so the relations are compared as their EPSILON-erased tuples up
			//  to a bound instead of with are_equivalent().
			const std::set<std::vector<Word>> tuples{bounded_tuples(general, num_of_symbols, 3)};
			CHECK(tuples == bounded_tuples(fast, num_of_symbols, 3));
			if ((has_epsilon_transition(lhs) || has_epsilon_transition(rhs)) && !tuples.empty()) {
				++composed_with_epsilon;
			}
		}
	}
	// The two modes build the product differently only when a word reaches several states.
	CHECK(nondeterministic_inputs > 0);
	CHECK(composed_with_epsilon > 0);
}

namespace {
/// @p count of the levels {0, ..., @p num_of_levels - 1}, drawn at random, in increasing order.
OrdVector<Level> random_levels(std::mt19937& rng, const size_t num_of_levels, const size_t count) {
	std::vector<Level> levels(num_of_levels);
	for (size_t level{0}; level < num_of_levels; ++level) { levels[level] = static_cast<Level>(level); }
	std::ranges::shuffle(levels, rng);
	levels.resize(count);
	return OrdVector<Level>{levels};
}
} // namespace

// The direct general composition has to accept the relation of the pipeline it replaces, EPSILON placement included
//  (are_equivalent() reads EPSILON as a letter), under every jump mode, also with DONT_CARE, which the pipeline
//  matches with EPSILON too (#561), and with jump transitions. The EPSILON-erased tuples, which the comparison of the
//  composition modes above uses, are compared as well, with the jumps unwound in the jump mode.
TEST_CASE("mata::nft differential: the direct general composition is the pipeline one", "[differential]") {
	size_t num_of_nonempty{0};
	size_t num_of_nonempty_with_epsilon{0};
	size_t num_of_nonempty_with_dont_care{0};
	size_t num_of_nonempty_with_jumps{0};
	// With jumps kept by the pipeline in an operand reading every aligned level, which product() lets choose again.
	size_t num_of_nonempty_with_kept_jumps{0};
	size_t num_of_nonempty_multi_sync{0};
	for (unsigned seed{0}; seed < 500; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t lhs_num_of_levels{1 + draw(rng, 4)};
		const size_t rhs_num_of_levels{1 + draw(rng, 4)};
		const size_t num_of_sync_levels{1 + draw(rng, std::min({size_t{3}, lhs_num_of_levels, rhs_num_of_levels}))};
		if (lhs_num_of_levels + rhs_num_of_levels - num_of_sync_levels == 1) { continue; }
		const bool with_epsilon{seed % 4 != 0};
		const bool with_dont_care{seed % 3 == 1};
		const bool with_jumps{seed % 5 >= 3};
		const size_t lhs_num_of_states_per_level{1 + draw(rng, 3)};
		const size_t rhs_num_of_states_per_level{1 + draw(rng, 3)};
		Nft lhs{random_nft(
			rng, lhs_num_of_states_per_level, num_of_symbols, lhs_num_of_levels, with_dont_care, with_epsilon,
			with_jumps
		)};
		Nft rhs{random_nft(
			rng, rhs_num_of_states_per_level, num_of_symbols, rhs_num_of_levels, with_dont_care, with_epsilon,
			with_jumps
		)};
		const bool has_jumps{lhs.contains_jump_transitions() || rhs.contains_jump_transitions()};
		// random_nft() numbers the zero-level states first.
		lhs.initial.insert(static_cast<State>(draw(rng, lhs_num_of_states_per_level)));
		rhs.initial.insert(static_cast<State>(draw(rng, rhs_num_of_states_per_level)));
		const OrdVector<Level> lhs_sync_levels{random_levels(rng, lhs_num_of_levels, num_of_sync_levels)};
		const OrdVector<Level> rhs_sync_levels{random_levels(rng, rhs_num_of_levels, num_of_sync_levels)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed, rhs));
		for (const bool project_out : {false, true}) {
			if (project_out && lhs_num_of_levels + rhs_num_of_levels == 2 * num_of_sync_levels) { continue; }
			for (const JumpMode jump_mode : {JumpMode::NoJump, JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
				INFO(
					"sync " << lhs_sync_levels << " / " << rhs_sync_levels << " project_out " << project_out
							<< " jump mode " << static_cast<int>(jump_mode)
				);
				const std::optional<Nft> direct{algorithms::compose_general_direct(
					lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out, jump_mode
				)};
				REQUIRE(direct.has_value());
				// compose_general() dispatches to the direct composition here.
				CHECK(
					algorithms::compose_general(lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out, jump_mode)
						.is_identical(*direct)
				);
				const Nft pipeline{algorithms::compose_general_reference(
					lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out, jump_mode
				)};
				REQUIRE(direct->levels.num_of_levels == pipeline.levels.num_of_levels);
				CHECK(are_equivalent(*direct, pipeline, jump_mode));
				const OrdVector<Symbol> keep_dont_care{DONT_CARE};
				const std::set<std::vector<Word>> tuples{
					bounded_tuples(pipeline.unwind_jumps(keep_dont_care, jump_mode), num_of_symbols, 2)
				};
				CHECK(bounded_tuples(direct->unwind_jumps(keep_dont_care, jump_mode), num_of_symbols, 2) == tuples);
				if (!tuples.empty()) {
					++num_of_nonempty;
					if (with_epsilon) { ++num_of_nonempty_with_epsilon; }
					if (with_dont_care) { ++num_of_nonempty_with_dont_care; }
					if (has_jumps) { ++num_of_nonempty_with_jumps; }
					if (has_jumps && jump_mode != JumpMode::AppendDontCares &&
						(lhs_num_of_levels == num_of_sync_levels || rhs_num_of_levels == num_of_sync_levels)) {
						++num_of_nonempty_with_kept_jumps;
					}
					if (num_of_sync_levels > 1) { ++num_of_nonempty_multi_sync; }
				}
			}
		}
	}
	CHECK(num_of_nonempty > 0);
	CHECK(num_of_nonempty_with_epsilon > 0);
	CHECK(num_of_nonempty_with_dont_care > 0);
	CHECK(num_of_nonempty_with_jumps > 0);
	CHECK(num_of_nonempty_with_kept_jumps > 0);
	CHECK(num_of_nonempty_multi_sync > 0);
}

namespace {
/// Whether a transition of @p nft reads @p level after its first level, i.e., it is a jump over @p level.
bool jumps_over(const Nft& nft, const Level level) {
	for (const Transition& transition : nft.delta.transitions()) {
		const size_t source_level{nft.levels[transition.source]};
		const size_t end{nft.levels[transition.target] == 0 ? nft.levels.num_of_levels : nft.levels[transition.target]};
		if (source_level < level && level < end) { return true; }
	}
	return false;
}

/// Whether @p nft has a transition over DONT_CARE.
bool has_dont_care_transition(const Nft& nft) {
	for (const Transition& transition : nft.delta.transitions()) {
		if (transition.symbol == DONT_CARE) { return true; }
	}
	return false;
}

/// @p nft with its jumps unwound in @p jump_mode, DONT_CARE kept, but for the EPSILON transitions between two
/// zero-level
///  states under JumpMode::RepeatSymbol, which compose_fast_no_jump() reads as whole rounds of EPSILON already, and
///  places their EPSILON otherwise than the unwound chains.
Nft unwind_jumps_but_fast_epsilon(const Nft& nft, const JumpMode jump_mode) {
	std::vector<Transition> fast_epsilon_transitions{};
	Nft without_fast_epsilon{nft};
	if (jump_mode == JumpMode::RepeatSymbol && nft.levels.num_of_levels > 1) {
		for (const Transition& transition : nft.delta.transitions()) {
			if (transition.symbol == EPSILON && nft.levels[transition.source] == 0 &&
				nft.levels[transition.target] == 0) {
				fast_epsilon_transitions.push_back(transition);
			}
		}
		for (const Transition& transition : fast_epsilon_transitions) { without_fast_epsilon.delta.remove(transition); }
	}
	Nft unwound{without_fast_epsilon.unwind_jumps(OrdVector<Symbol>{DONT_CARE}, jump_mode)};
	for (const Transition& transition : fast_epsilon_transitions) { unwound.delta.add(transition); }
	return unwound;
}
} // namespace

// compose_fast() reads a jump transition level by level. Its result has no jumps, and it accepts the relation of
//  compose_fast_no_jump() on the operands with their jumps unwound in the jump mode (but for the fast EPSILON
//  transitions under RepeatSymbol), EPSILON placement included: are_equivalent() reads EPSILON as a letter. Wherever
//  the general composition reads the operands alike, which compose_fast() documents, it accepts the general relation
//  too, as EPSILON-erased bounded tuples, and as a whole without EPSILON.
TEST_CASE("mata::nft differential: the fast composition reads jump transitions", "[differential]") {
	size_t num_of_nonempty_with_jumps{0};
	size_t num_of_nonempty_with_epsilon{0};
	size_t num_of_nonempty_with_dont_care{0};
	size_t num_of_nonempty_projected{0};
	size_t num_of_nonempty_alike_with_jumps{0};
	size_t num_of_nonempty_equivalent_with_jumps{0};
	size_t num_of_differing_choosing_jump_again{0};
	size_t num_of_differing_dont_care_on_sync{0};
	for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares}) {
		size_t num_of_nonempty_in_mode{0};
		for (unsigned seed{0}; seed < 600; ++seed) {
			std::mt19937 rng{seed};
			const size_t num_of_symbols{1 + seed % 2};
			const size_t lhs_num_of_levels{1 + draw(rng, 4)};
			const size_t rhs_num_of_levels{1 + draw(rng, 4)};
			if (lhs_num_of_levels + rhs_num_of_levels == 2) { continue; }
			const bool with_epsilon{seed % 3 == 1};
			const bool with_dont_care{seed % 4 == 2};
			const size_t lhs_num_of_states_per_level{1 + draw(rng, 3)};
			const size_t rhs_num_of_states_per_level{1 + draw(rng, 3)};
			Nft lhs{random_nft(
				rng, lhs_num_of_states_per_level, num_of_symbols, lhs_num_of_levels, with_dont_care, with_epsilon, true
			)};
			Nft rhs{random_nft(
				rng, rhs_num_of_states_per_level, num_of_symbols, rhs_num_of_levels, with_dont_care, with_epsilon, true
			)};
			// random_nft() numbers the zero-level states first.
			lhs.initial.insert(static_cast<State>(draw(rng, lhs_num_of_states_per_level)));
			rhs.initial.insert(static_cast<State>(draw(rng, rhs_num_of_states_per_level)));
			const auto lhs_sync_level{static_cast<Level>(draw(rng, lhs_num_of_levels))};
			const auto rhs_sync_level{static_cast<Level>(draw(rng, rhs_num_of_levels))};
			const bool has_jumps{lhs.contains_jump_transitions() || rhs.contains_jump_transitions()};
			const bool has_epsilon{has_epsilon_transition(lhs) || has_epsilon_transition(rhs)};
			const bool has_dont_care{has_dont_care_transition(lhs) || has_dont_care_transition(rhs)};
			// What the general composition reads otherwise: DONT_CARE, which it matches with EPSILON (#561), and the
			//  jumps of an operand composed with a one-level one, which product() lets choose again.
			const bool reads_dont_care_on_sync{
				has_dont_care || (jump_mode == JumpMode::AppendDontCares &&
								  (jumps_over(lhs, lhs_sync_level) || jumps_over(rhs, rhs_sync_level)))
			};
			const bool chooses_jump_again{
				jump_mode == JumpMode::RepeatSymbol && ((lhs_num_of_levels == 1 && rhs.contains_jump_transitions()) ||
														(rhs_num_of_levels == 1 && lhs.contains_jump_transitions()))
			};
			const bool alike{!reads_dont_care_on_sync && !chooses_jump_again};
			INFO(describe(seed, lhs) + "\n---\n" + describe(seed, rhs));
			for (const bool project_out : {false, true}) {
				INFO(
					"sync " << lhs_sync_level << " / " << rhs_sync_level << " project_out " << project_out
							<< " jump mode " << static_cast<int>(jump_mode)
				);
				const Nft fast{
					algorithms::compose_fast(lhs, rhs, lhs_sync_level, rhs_sync_level, project_out, jump_mode)
				};
				CHECK(!fast.contains_jump_transitions());
				const Nft unwound{algorithms::compose_fast_no_jump(
					unwind_jumps_but_fast_epsilon(lhs, jump_mode), unwind_jumps_but_fast_epsilon(rhs, jump_mode),
					lhs_sync_level, rhs_sync_level, project_out
				)};
				REQUIRE(fast.levels.num_of_levels == unwound.levels.num_of_levels);
				CHECK(are_equivalent(fast, unwound));
				const std::set<std::vector<Word>> tuples{bounded_tuples(fast, num_of_symbols, 2)};
				CHECK(bounded_tuples(unwound, num_of_symbols, 2) == tuples);

				const Nft general{
					algorithms::compose_general(lhs, rhs, {lhs_sync_level}, {rhs_sync_level}, project_out, jump_mode)
				};
				if (alike) {
					const OrdVector<Symbol> keep_dont_care{DONT_CARE};
					CHECK(bounded_tuples(general.unwind_jumps(keep_dont_care, jump_mode), num_of_symbols, 2) == tuples);
				}
				if (alike && !has_epsilon) {
					CHECK(are_equivalent(fast, general, jump_mode));
				} else if (!has_epsilon && !has_dont_care && !are_equivalent(fast, general, jump_mode)) {
					// The relations the general composition reads otherwise do differ.
					++(chooses_jump_again ? num_of_differing_choosing_jump_again : num_of_differing_dont_care_on_sync);
				}
				if (tuples.empty()) { continue; }
				++num_of_nonempty_in_mode;
				if (has_jumps) { ++num_of_nonempty_with_jumps; }
				if (has_epsilon) { ++num_of_nonempty_with_epsilon; }
				if (has_dont_care) { ++num_of_nonempty_with_dont_care; }
				if (project_out) { ++num_of_nonempty_projected; }
				if (alike && has_jumps) { ++num_of_nonempty_alike_with_jumps; }
				if (alike && !has_epsilon && has_jumps) { ++num_of_nonempty_equivalent_with_jumps; }
			}
		}
		CHECK(num_of_nonempty_in_mode > 0);
	}
	CHECK(num_of_nonempty_with_jumps > 0);
	CHECK(num_of_nonempty_with_epsilon > 0);
	CHECK(num_of_nonempty_with_dont_care > 0);
	CHECK(num_of_nonempty_projected > 0);
	CHECK(num_of_nonempty_alike_with_jumps > 0);
	CHECK(num_of_nonempty_equivalent_with_jumps > 0);
	CHECK(num_of_differing_choosing_jump_again > 0);
	CHECK(num_of_differing_dont_care_on_sync > 0);
}

TEST_CASE("mata::nft differential: composing with an empty operand keeps the level count", "[differential]") {
	for (unsigned seed{0}; seed < 20; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, 2)};
		const Nft empty{Nft::with_levels(2)};
		INFO(describe(seed, nft));

		// An empty operand empties the language but must not lose the levels of the result.
		for (const auto& [lhs, rhs] :
			 {std::pair{std::cref(nft), std::cref(empty)}, std::pair{std::cref(empty), std::cref(nft)},
			  std::pair{std::cref(empty), std::cref(empty)}}) {
			const Nft general{compose(lhs.get(), rhs.get(), 1, 0, true, JumpMode::NoJump, CompositionMode::General)};
			const Nft fast{compose(lhs.get(), rhs.get(), 1, 0, true, JumpMode::NoJump, CompositionMode::FastNoJump)};
			CHECK(general.levels.num_of_levels == 2);
			CHECK(fast.levels.num_of_levels == 2);
			CHECK(general.is_lang_empty());
			CHECK(fast.is_lang_empty());
		}
	}
}

// The normalization of the composition result rewrites EPSILON runs and duplicate states only. EPSILON transitions
//  inside the rounds, fast EPSILON moves (zero-level to zero-level) and one-level operands with EPSILON make the
//  composer generate them.
TEST_CASE("mata::nft differential: compose() normalization keeps the relation and never grows", "[differential]") {
	size_t num_of_shrunk{0};
	for (unsigned seed{0}; seed < 120; ++seed) {
		std::mt19937 lhs_rng{seed};
		std::mt19937 rhs_rng{seed + 2'100};
		const size_t num_of_symbols{1 + seed % 2};
		const size_t num_of_states_per_level{1 + seed % 3};
		Nft lhs{random_nft(lhs_rng, num_of_states_per_level, num_of_symbols, 2, false, true)};
		const size_t rhs_num_of_levels{1 + (seed / 2) % 2};
		Nft rhs{random_nft(rhs_rng, num_of_states_per_level, num_of_symbols, rhs_num_of_levels, false, true)};
		// Fast EPSILON moves between the zero-level states, cycles included.
		for (Nft* const nft : {&lhs, &rhs}) {
			std::mt19937& rng{nft == &lhs ? lhs_rng : rhs_rng};
			for (State source{0}; source < nft->num_of_states(); ++source) {
				if (nft->levels[source] != 0 || draw(rng, 2) != 0) { continue; }
				State target{static_cast<State>(draw(rng, nft->num_of_states()))};
				while (nft->levels[target] != 0) { target = static_cast<State>(draw(rng, nft->num_of_states())); }
				if (target != source) { nft->delta.add(source, EPSILON, target); }
			}
		}
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 2'100, rhs));

		for (const bool project_out : {true, false}) {
			const Nft raw{compose(
				lhs, rhs, 1, 0, project_out, JumpMode::NoJump, CompositionMode::FastNoJump,
				CompositionNormalization::None
			)};
			const Nft normalized{compose(
				lhs, rhs, 1, 0, project_out, JumpMode::NoJump, CompositionMode::FastNoJump,
				CompositionNormalization::GeneratedOnly
			)};
			INFO("project_out " << project_out);
			REQUIRE(raw.levels.num_of_levels == normalized.levels.num_of_levels);
			CHECK(normalized.num_of_states() <= raw.num_of_states());
			CHECK(normalized.delta.num_of_transitions() <= raw.delta.num_of_transitions());
			if (normalized.num_of_states() < raw.num_of_states()) { ++num_of_shrunk; }
			for (const State state : normalized.initial) { CHECK(normalized.levels[state] == 0); }
			for (const State state : normalized.final) { CHECK(normalized.levels[state] == 0); }

			// Every tuple of words up to length 2, one word per level, is accepted by both or by neither.
			const size_t num_of_levels{raw.levels.num_of_levels};
			const std::vector<Word> words{all_level_words(num_of_symbols, 2, 1)};
			std::vector<Word> tuple(num_of_levels);
			std::vector<size_t> indices(num_of_levels, 0);
			while (true) {
				for (size_t level{0}; level < num_of_levels; ++level) { tuple[level] = words[indices[level]]; }
				CHECK(raw.is_in_lang_by_levels(tuple) == normalized.is_in_lang_by_levels(tuple));
				size_t level{0};
				while (level < num_of_levels && ++indices[level] == words.size()) { indices[level++] = 0; }
				if (level == num_of_levels) { break; }
			}
		}
	}
	CHECK(num_of_shrunk > 0);
}

// is_universal() carries the NFA antichain algorithm over unchanged: it demands a final state after
//  every single symbol, while a transducer with n levels can only be final after a multiple of n
//  symbols. On genuine multi-level transducers it therefore answers false even for the complete one
//  below, which accepts every word of the alphabet, and it contradicts the complement being empty.
//  ("naive" throws "is_universal naive algorithm is not implemented for NFTs", so the complement is
//  the only independent reference.) Expected to fail until is_universal() respects the levels.
TEST_CASE(
	"mata::nft differential: universality agrees with the complement on multi-level transducers",
	"[differential][!shouldfail]"
) {
	for (unsigned seed{0}; seed < 100; ++seed) {
		std::mt19937 rng{seed};
		const size_t num_of_symbols{1 + seed % 2};
		// Genuine multi-level transducers: two and three levels, each holding its own states.
		const size_t num_of_levels{2 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		const Nft nft{random_nft(rng, 1 + seed % 3, num_of_symbols, num_of_levels)};
		INFO(describe(seed, nft));

		const bool universal{nft.is_universal(*alphabet, {{"algorithm", "antichains"}})};
		CHECK(universal == complement(nft, *alphabet).is_lang_empty());

		// A universal transducer accepts every word over the alphabet, so a rejected bounded word
		//  refutes universality.
		for (const Word& word : all_level_words(num_of_symbols, 2 * num_of_levels, num_of_levels)) {
			if (!nft.is_in_lang(word)) {
				CHECK(!universal);
				break;
			}
		}
	}

	// A complete two-level transducer whose level-0 state is final accepts every word of the
	//  alphabet; the bounded check confirms it, is_universal() does not.
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	Nft universal{Nft::with_levels(2, 2)};
	universal.levels[1] = 1;
	universal.initial.insert(0);
	universal.final.insert(0);
	for (Symbol symbol{0}; symbol < 2; ++symbol) {
		universal.delta.add(0, symbol, 1);
		universal.delta.add(1, symbol, 0);
	}
	for (const Word& word : all_level_words(2, 4, 2)) { CHECK(universal.is_in_lang(word)); }
	CHECK(complement(universal, *alphabet).is_lang_empty());
	CHECK(universal.is_universal(*alphabet, {{"algorithm", "antichains"}}));
}

// complement() builds a one-state automaton with no transitions when the input has no initial state,
//  so the complement of the empty language accepts only the empty word instead of every word. An
//  input with an initial state but no transitions is complemented correctly, so the gap is the
//  missing initial state. Expected to fail until complement() handles it.
TEST_CASE(
	"mata::nft differential: complement of a transducer without initial states is universal",
	"[differential][!shouldfail]"
) {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	const Nft empty{Nft::with_levels(2)};
	const Nft complemented{complement(empty, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(!empty.is_in_lang(word));
		CHECK(complemented.is_in_lang(word));
	}
}

// complement() does not expand jump transitions: on the transducer below it rejects the word (0, 1),
//  which the input rejects as well, while the two-state-per-level transducer of the same language is
//  complemented correctly. Expected to fail until complement() handles jumps.
TEST_CASE("mata::nft differential: complement of a transducer with a jump transition", "[differential][!shouldfail]") {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	// Both states stay on level 0, so 0 -0-> 1 is one transition over both levels, read as the pair
	//  (0, 0) under RepeatSymbol.
	Nft jumping{Nft::with_levels(2, 2)};
	jumping.initial.insert(0);
	jumping.final.insert(1);
	jumping.delta.add(0, 0, 1);
	const Nft complemented{complement(jumping, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(jumping.is_in_lang(word) != complemented.is_in_lang(word));
	}
}

// complement() ignores DONT_CARE: it treats the transition as one more concrete symbol instead of
//  the set of all of them, so the complement of the transducer below accepts the words the input
//  accepts, while the same language written with concrete symbols is complemented correctly.
//  Expected to fail until complement() expands DONT_CARE.
TEST_CASE("mata::nft differential: complement of a transducer with DONT_CARE", "[differential][!shouldfail]") {
	const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(2)};
	// 0 -DONT_CARE-> 1 -0-> 0 with 0 final: the transducer accepts (x, 0) for every symbol x.
	Nft dont_care{Nft::with_levels(2, 2)};
	dont_care.levels[1] = 1;
	dont_care.initial.insert(0);
	dont_care.final.insert(0);
	dont_care.delta.add(0, DONT_CARE, 1);
	dont_care.delta.add(1, 0, 0);
	const Nft complemented{complement(dont_care, *alphabet)};
	for (const Word& word : all_level_words(2, 4, 2)) {
		CHECK(dont_care.is_in_lang(word) != complemented.is_in_lang(word));
	}
}

// A hidden sweep over larger transducers and longer words; the default run stays fast.
TEST_CASE("mata::nft differential: larger sweep", "[.differential-sweep]") {
	for (unsigned seed{0}; seed < 300; ++seed) {
		const size_t num_of_symbols{1 + seed % 3};
		const size_t num_of_levels{2 + seed % 2};
		const std::shared_ptr<EnumAlphabet> alphabet{alphabet_of(num_of_symbols)};
		std::mt19937 lhs_rng{seed};
		std::mt19937 rhs_rng{seed + 900};
		// No DONT_CARE in the operands of complement(): it does not expand the symbol, which the
		//  [!shouldfail] test case above pins down.
		const Nft lhs{random_nft(lhs_rng, 1 + seed % 4, num_of_symbols, num_of_levels)};
		const Nft rhs{random_nft(rhs_rng, 1 + seed % 4, num_of_symbols, num_of_levels)};
		INFO(describe(seed, lhs) + "\n---\n" + describe(seed + 900, rhs));

		const Nft complemented{complement(lhs, *alphabet)};
		for (const Word& word : all_level_words(num_of_symbols, 3 * num_of_levels, num_of_levels)) {
			CHECK(lhs.is_in_lang(word) != complemented.is_in_lang(word));
		}
		CHECK(
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "naive"}}) ==
			is_included(lhs, rhs, alphabet.get(), JumpMode::RepeatSymbol, {{"algorithm", "antichains"}})
		);
		// No universality here: it disagrees with the complement on every multi-level transducer,
		//  which the [!shouldfail] test case above pins down.

		// The composition modes, on two-level operands that carry DONT_CARE or EPSILON, compared as in the
		//  [differential] test case above.
		for (const bool with_epsilon : {false, true}) {
			std::mt19937 composed_lhs_rng{seed + 1'300};
			std::mt19937 composed_rhs_rng{seed + 1'700};
			const Nft composed_lhs{
				random_nft(composed_lhs_rng, 1 + seed % 3, num_of_symbols, 2, !with_epsilon, with_epsilon)
			};
			const Nft composed_rhs{
				random_nft(composed_rhs_rng, 1 + seed % 3, num_of_symbols, 2, !with_epsilon, with_epsilon)
			};
			INFO(describe(seed + 1'300, composed_lhs) + "\n---\n" + describe(seed + 1'700, composed_rhs));
			const Nft general{
				compose(composed_lhs, composed_rhs, 1, 0, true, JumpMode::NoJump, CompositionMode::General)
			};
			const Nft fast{
				compose(composed_lhs, composed_rhs, 1, 0, true, JumpMode::NoJump, CompositionMode::FastNoJump)
			};
			CHECK(general.levels.num_of_levels == fast.levels.num_of_levels);
			if (with_epsilon) {
				CHECK(bounded_tuples(general, num_of_symbols, 3) == bounded_tuples(fast, num_of_symbols, 3));
			} else {
				for (const Word& word : all_level_words(num_of_symbols, 6, 2)) {
					CHECK(general.is_in_lang(word) == fast.is_in_lang(word));
				}
			}
		}
	}
}

namespace {

/// A random NFT over {0, ..., num_of_symbols - 1} with jumps: a transition from level l goes to any of the levels
///  l + 1, ..., num_of_levels (level 0 standing for num_of_levels), so it spans one level or jumps over several.
///  @p with_dont_care and @p with_epsilon add DONT_CARE and EPSILON transitions; @p with_odd_spans lets a transition
///  also stay on its level or go back, and makes a few states on other levels than 0 final, which unwind_jumps()
///  unwinds as well.
Nft random_nft_with_jumps(
	std::mt19937& rng,
	const size_t num_of_states_per_level,
	const size_t num_of_symbols,
	const size_t num_of_levels,
	const bool with_dont_care,
	const bool with_epsilon,
	const bool with_odd_spans
) {
	Nft nft{Nft::with_levels(num_of_levels)};
	std::vector<std::vector<State>> states_by_level(num_of_levels);
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			states_by_level[level].push_back(nft.add_state_with_level(static_cast<Level>(level)));
		}
	}
	for (size_t initial{0}, initials{1 + draw(rng, 2)}; initial < initials; ++initial) {
		nft.initial.insert(states_by_level[0][draw(rng, num_of_states_per_level)]);
	}
	for (const State state : states_by_level[0]) {
		if (draw(rng, 3) == 0) { nft.final.insert(state); }
	}
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (const State source : states_by_level[level]) {
			if (with_odd_spans && draw(rng, 10) == 0) { nft.final.insert(source); }
			for (size_t transition{0}, transitions{draw(rng, 4)}; transition < transitions; ++transition) {
				Symbol symbol{static_cast<Symbol>(draw(rng, num_of_symbols))};
				if (const size_t kind{draw(rng, 6)}; with_dont_care && kind == 0) {
					symbol = DONT_CARE;
				} else if (with_epsilon && kind == 1) {
					symbol = EPSILON;
				}
				size_t target_level{(level + 1 + draw(rng, num_of_levels - level)) % num_of_levels};
				if (with_odd_spans && draw(rng, 6) == 0) { target_level = draw(rng, num_of_levels); }
				nft.delta.add(source, symbol, states_by_level[target_level][draw(rng, num_of_states_per_level)]);
			}
		}
	}
	return nft;
}

/// Statistics of the lazy-against-unwound inclusion checks, so that the corpus provably covers both answers.
struct LazyInclusionStats {
	size_t included{0};
	size_t not_included{0};
	size_t with_jumps{0};
};

/// Checks the antichain inclusion of the transducers of @p seed, with up to @p max_states_per_level states per level,
///  both ways, and their equivalence, against the inclusion of the unwound transducers, under every jump mode, and
///  replays each counterexample on the unwound ones.
void check_lazy_inclusion(const unsigned seed, const size_t max_states_per_level, LazyInclusionStats& stats) {
	std::mt19937 rng{seed};
	const size_t num_of_symbols{1 + seed % 3};
	const size_t num_of_levels{1 + (seed / 3) % 4};
	const size_t num_of_states_per_level{1 + (seed / 2) % max_states_per_level};
	const bool with_dont_care{seed % 2 == 0};
	const bool with_epsilon{seed % 3 == 0};
	const bool with_odd_spans{seed % 5 == 0};
	const Nft lhs{random_nft_with_jumps(
		rng, num_of_states_per_level, num_of_symbols, num_of_levels, with_dont_care, with_epsilon, with_odd_spans
	)};
	Nft rhs{random_nft_with_jumps(
		rng, num_of_states_per_level, num_of_symbols, num_of_levels, with_dont_care, with_epsilon, with_odd_spans
	)};
	if (seed % 3 == 1) {
		// rhs grows lhs by a few transitions, so lhs is included in it.
		rhs = lhs;
		const size_t num_of_states{rhs.num_of_states()};
		for (size_t transition{0}, transitions{draw(rng, 4)}; transition < transitions; ++transition) {
			const Symbol symbol{draw(rng, 4) == 0 ? DONT_CARE : static_cast<Symbol>(draw(rng, num_of_symbols))};
			rhs.delta.add(
				static_cast<State>(draw(rng, num_of_states)), symbol, static_cast<State>(draw(rng, num_of_states))
			);
		}
	}
	if (lhs.contains_jump_transitions() || rhs.contains_jump_transitions()) { ++stats.with_jumps; }

	// The symbols that replace DONT_CARE: those of both transducers, a full or a partial alphabet, one with EPSILON,
	//  none, DONT_CARE alone (kept as a letter), and DONT_CARE among others.
	std::shared_ptr<EnumAlphabet> alphabet{};
	switch (seed % 7) {
		case 0:
			break;
		case 1:
			alphabet = alphabet_of(num_of_symbols);
			break;
		case 2:
			alphabet = std::make_shared<EnumAlphabet>(EnumAlphabet{0});
			break;
		case 3:
			alphabet = alphabet_of(num_of_symbols);
			alphabet->add_new_symbol(EPSILON);
			break;
		case 4:
			alphabet = std::make_shared<EnumAlphabet>();
			break;
		case 5:
			alphabet = std::make_shared<EnumAlphabet>(EnumAlphabet{DONT_CARE});
			break;
		default:
			alphabet = std::make_shared<EnumAlphabet>(EnumAlphabet{0, DONT_CARE});
			break;
	}
	mata::utils::OrdVector<Symbol> symbols{};
	if (alphabet != nullptr) {
		symbols = alphabet->get_alphabet_symbols();
	} else {
		symbols = create_alphabet(lhs, rhs).get_alphabet_symbols();
		if (symbols.contains(DONT_CARE) && symbols.size() > 1) { symbols.erase(DONT_CARE); }
	}
	INFO(describe(seed, lhs) + "\n---\n" + describe(seed, rhs) + "\nalphabet variant " + std::to_string(seed % 7));

	for (const JumpMode jump_mode : {JumpMode::RepeatSymbol, JumpMode::AppendDontCares, JumpMode::NoJump}) {
		INFO("jump mode " << static_cast<int>(jump_mode));
		std::array<bool, 2> eager_answers{};
		for (const bool forward : {true, false}) {
			const Nft& smaller{forward ? lhs : rhs};
			const Nft& bigger{forward ? rhs : lhs};
			INFO(std::string{forward ? "lhs <= rhs" : "rhs <= lhs"});
			Run cex{};
			const bool lazy{algorithms::is_included_antichains(smaller, bigger, alphabet.get(), &cex, jump_mode)};
			const bool eager{
				algorithms::is_included_antichains_unwound(smaller, bigger, alphabet.get(), nullptr, jump_mode)
			};
			REQUIRE(lazy == eager);
			eager_answers[forward ? 0 : 1] = eager;
			if (lazy) {
				++stats.included;
				continue;
			}
			++stats.not_included;
			// The counterexample is a word of the unwound smaller transducer and not of the unwound bigger one, read
			//  along a path from an initial to a final state of the smaller transducer.
			CHECK(smaller.to_nfa_update_copy(symbols, jump_mode).is_in_lang(cex.word));
			CHECK(!bigger.to_nfa_update_copy(symbols, jump_mode).is_in_lang(cex.word));
			REQUIRE(cex.path.size() == cex.word.size() + 1);
			CHECK(smaller.initial.contains(cex.path.front()));
			CHECK(smaller.final.contains(cex.path.back()));
		}
		CHECK(
			are_equivalent(lhs, rhs, alphabet.get(), jump_mode, {{"algorithm", "antichains"}}) ==
			(eager_answers[0] && eager_answers[1])
		);
	}
}

} // namespace

TEST_CASE(
	"mata::nft differential: inclusion and equivalence explore jumps lazily as unwinding reads them", "[differential]"
) {
	LazyInclusionStats stats{};
	for (unsigned seed{0}; seed < 2'000; ++seed) { check_lazy_inclusion(seed, 5, stats); }
	// The corpus has both answers and jumps.
	CHECK(stats.included > 1'000);
	CHECK(stats.not_included > 1'000);
	CHECK(stats.with_jumps > 1'000);
}

// A hidden sweep over more seeds; the default run stays fast.
TEST_CASE("mata::nft differential: lazy jump exploration in a larger sweep", "[.differential-sweep]") {
	LazyInclusionStats stats{};
	for (unsigned seed{2'000}; seed < 30'000; ++seed) { check_lazy_inclusion(seed, 8, stats); }
}
