/* nfa-strings.cc -- Operations on NFAs for string solving.
 */

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/utils/assert.hh"

#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>

using namespace mata::nfa;
using namespace mata::applications::strings;

std::set<mata::Word> mata::applications::strings::get_shortest_words(const Nfa& nfa) {
	// Map mapping states to a set of the shortest words accepted by the automaton from the mapped state.
	// Get the shortest words for all initial states accepted by the whole automaton (not just a part of the automaton).
	return ShortestWordsMap{nfa}.get_shortest_words_from(StateSet{nfa.initial});
}

std::set<mata::Word> ShortestWordsMap::get_shortest_words_from(const StateSet& states) const {
	std::set<Word> result{};

	if (!shortest_words_map_.empty()) {
		WordLength shortest_words_length{-1};
		for (const State state : states) {
			const auto& current_shortest_words_map{shortest_words_map_.find(state)};
			if (current_shortest_words_map == shortest_words_map_.end()) { continue; }

			if (const auto& [length, shortest_words]{current_shortest_words_map->second};
				result.empty() || length < shortest_words_length) {
				// Find a new set of the shortest words.
				result = shortest_words;
				shortest_words_length = length;
			} else if (length == shortest_words_length) {
				// Append the shortest words from other state of the same length to the already found set of the
				// shortest words.
				result.insert(shortest_words.begin(), shortest_words.end());
			}
		}
	}

	return result;
}

std::set<mata::Word> ShortestWordsMap::get_shortest_words_from(const State state) const {
	return get_shortest_words_from(StateSet{state});
}

void ShortestWordsMap::insert_initial_lengths() {
	if (const auto initial_states{reversed_automaton_.initial}; !initial_states.empty()) {
		for (const State state : initial_states) {
			shortest_words_map_.insert(std::make_pair(state, std::make_pair(0, std::set<Word>{std::vector<Symbol>{}})));
		}

		const auto initial_states_begin{initial_states.begin()};
		const auto initial_states_end{initial_states.end()};
		processed_.insert(initial_states_begin, initial_states_end);
		fifo_queue_.insert(fifo_queue_.end(), initial_states_begin, initial_states_end);
	}
}

void ShortestWordsMap::compute() {
	while (!fifo_queue_.empty()) {
		const State state{fifo_queue_.front()};
		fifo_queue_.pop_front();
		// Compute the shortest words for the current state.
		compute_for_state(state);
	}
}

void ShortestWordsMap::compute_for_state(const State state) {
	const LengthWordsPair& dst{map_default_shortest_words(state)};
	const WordLength dst_length_plus_one{dst.first + 1};

	for (const SymbolPost& transition : reversed_automaton_.delta.state_post(state)) {
		for (const State state_to : transition.targets) {
			const LengthWordsPair& orig{map_default_shortest_words(state_to)};
			LengthWordsPair act{orig};

			if ((act.first == -1) || (dst_length_plus_one < act.first)) {
				// Found new shortest words after appending transition symbols.
				act.second.clear();
				update_current_words(act, dst, transition.symbol);
			} else if (dst_length_plus_one == act.first) {
				// Append transition symbol to increase length of the shortest words.
				update_current_words(act, dst, transition.symbol);
			}

			if (orig.second != act.second) { shortest_words_map_[state_to] = act; }

			if (!processed_.contains(state_to)) {
				processed_.insert(state_to);
				fifo_queue_.push_back(state_to);
			}
		}
	}
}

void ShortestWordsMap::update_current_words(LengthWordsPair& act, const LengthWordsPair& dst, const Symbol symbol) {
	for (Word word : dst.second) {
		word.insert(word.begin(), symbol);
		act.second.insert(word);
	}
	act.first = dst.first + 1;
}

std::set<mata::Symbol> mata::applications::strings::get_accepted_symbols(const Nfa& nfa) {
	std::set<mata::Symbol> accepted_symbols;
	for (const State init : nfa.initial) {
		for (const SymbolPost& symbol_post_init : nfa.delta[init]) {
			mata::Symbol sym = symbol_post_init.symbol;
			if (auto symbol_it = accepted_symbols.lower_bound(sym);
				(symbol_it == accepted_symbols.end() || *symbol_it != sym) &&
				nfa.final.intersects_with(symbol_post_init.targets)) {
				accepted_symbols.insert(symbol_it, sym);
			}
		}
	}
	return accepted_symbols;
}

std::set<std::pair<int, int>> mata::applications::strings::get_word_lengths(const Nfa& aut) {
	Nfa one_letter;
	/// if we are interested in lengths of words, it suffices to forget the different symbols on transitions.
	/// The lengths of @p aut are hence equivalent to lengths of the NFA taken from @p aut where all symbols on
	/// transitions are renamed to a single symbol (e.g., `a`).
	aut.get_one_letter_aut(one_letter);
	one_letter = determinize(one_letter).trim();
	if (one_letter.num_of_states() == 0) { return {}; }

	std::set<std::pair<int, int>> ret;
	std::vector<int> handles(one_letter.num_of_states(), 0); // initialized to 0
	MATA_ASSERT(one_letter.initial.size() == 1);
	std::optional<nfa::State> curr_state = *one_letter.initial.begin();
	std::set<nfa::State> visited;
	int cnt = 0; // handle counter
	int loop_size = 0; // loop size
	int loop_start = -1; // cnt where the loop starts

	while (curr_state.has_value()) {
		visited.insert(curr_state.value());
		handles[curr_state.value()] = cnt++;
		nfa::StatePost post = one_letter.delta[curr_state.value()];

		curr_state.reset();
		MATA_ASSERT(post.size() <= 1);
		for (const SymbolPost& move : post) {
			MATA_ASSERT(move.targets.size() == 1);
			if (nfa::State target = *move.targets.begin(); !visited.contains(target)) {
				curr_state = target;
			} else {
				curr_state.reset();
				loop_start = handles[target];
				loop_size = cnt - handles[target];
			}
		}
	}
	for (const nfa::State& fin : one_letter.final) {
		if (handles[fin] >= loop_start) {
			ret.insert({handles[fin], loop_size});
		} else {
			ret.insert({handles[fin], 0});
		}
	}

	return ret;
}

bool mata::applications::strings::is_lang_eps(const Nfa& nfa) {
	const Nfa tr_aut = Nfa{nfa}.trim();
	if (tr_aut.initial.empty()) { return false; }
	for (const auto& ini : tr_aut.initial) {
		if (!tr_aut.final[ini]) { return false; }
		if (!tr_aut.delta[ini].empty()) { return false; }
	}
	return true;
}

namespace {
/// A configuration that the search has not reached yet.
constexpr size_t UNVISITED{std::numeric_limits<size_t>::max()};
/// A configuration the search started from.
constexpr size_t SEARCH_ROOT{UNVISITED - 1};

/// How the search reached a configuration: the previous configuration and the symbol read on its level.
struct SearchNode {
	size_t prev{UNVISITED};
	mata::Symbol symbol{mata::nft::EPSILON};
	mata::nft::Level level{0};
};

/// The number of configurations we are willing to index densely, trading about 24 B each for the hash map.
constexpr size_t DENSE_LIMIT{1 << 20};
} // namespace

std::optional<std::vector<mata::Word>>
	mata::applications::strings::get_words_of_lengths(const Nft& nft, const std::vector<unsigned>& lengths) {
	if (lengths.size() != nft.levels.num_of_levels) {
		throw std::invalid_argument("get_words_of_lengths(): lengths must have one entry per level");
	}
	if (nft.contains_jump_transitions()) {
		throw std::invalid_argument("get_words_of_lengths(): jump transitions are not supported");
	}
	if (nft.initial.empty() || nft.final.empty()) { return std::nullopt; }

	// A configuration is a state plus, per level, the number of symbols already written on that tape. What can still
	// be read depends on nothing else, so a configuration is worth visiting once: the search is then linear in the
	// number of configurations instead of the number of paths, and eps-cycles terminate.
	const size_t num_of_levels{lengths.size()};
	// radix[i] is the stride of level i in the packed count, radix[num_of_levels] the number of count combinations.
	std::vector<size_t> radix(num_of_levels + 1, 1);
	constexpr auto too_large = []() {
		return std::invalid_argument("get_words_of_lengths(): the lengths span too many configurations");
	};
	for (size_t level{0}; level < num_of_levels; ++level) {
		const size_t span{static_cast<size_t>(lengths[level]) + 1};
		if (radix[level] > UNVISITED / span) { throw too_large(); }
		radix[level + 1] = radix[level] * span;
	}
	const size_t num_of_counts{radix[num_of_levels]};
	// The one count combination that is a solution: every tape full.
	const size_t full_counts{num_of_counts - 1};
	const size_t num_of_states{nft.num_of_states()};
	if (num_of_states > UNVISITED / num_of_counts) { throw too_large(); }
	const size_t num_of_configs{num_of_states * num_of_counts};

	const bool dense{num_of_configs <= DENSE_LIMIT};
	std::vector<SearchNode> dense_nodes(dense ? num_of_configs : 0);
	std::unordered_map<size_t, SearchNode> sparse_nodes{};
	// Records how @p config was reached; returns false if it was already visited.
	auto visit = [&](const size_t config, const SearchNode& node) {
		if (dense) {
			if (dense_nodes[config].prev != UNVISITED) { return false; }
			dense_nodes[config] = node;
			return true;
		}
		return sparse_nodes.emplace(config, node).second;
	};
	auto node_of = [&](const size_t config) -> const SearchNode& {
		return dense ? dense_nodes[config] : sparse_nodes.find(config)->second;
	};
	// Walks the parents back to an initial configuration and reads the words off the symbols on the way.
	auto words_leading_to = [&](size_t config) {
		std::vector<Word> words(num_of_levels);
		for (const SearchNode* node{&node_of(config)}; node->prev != SEARCH_ROOT; node = &node_of(config)) {
			if (node->symbol != nft::EPSILON) { words[node->level].push_back(node->symbol); }
			config = node->prev;
		}
		for (Word& word : words) { std::ranges::reverse(word); }
		return words;
	};

	std::vector<size_t> worklist{};
	for (const State initial_state : nft.initial) {
		const size_t config{initial_state * num_of_counts};
		if (!visit(config, SearchNode{.prev = SEARCH_ROOT})) { continue; }
		if (full_counts == 0 && nft.final[initial_state]) { return std::vector<Word>(num_of_levels); }
		worklist.push_back(config);
	}

	for (size_t head{0}; head < worklist.size(); ++head) {
		const size_t config{worklist[head]};
		const State source{config / num_of_counts};
		const size_t counts{config % num_of_counts};
		const nft::Level level{nft.levels[source]};
		// Reading a non-epsilon symbol fills one more place on the tape of the source level.
		const bool tape_full{(counts / radix[level]) % (static_cast<size_t>(lengths[level]) + 1) == lengths[level]};
		for (const SymbolPost& symbol_post : nft.delta[source]) {
			const Symbol symbol{symbol_post.symbol};
			if (symbol != nft::EPSILON && tape_full) { continue; }
			const size_t new_counts{symbol == nft::EPSILON ? counts : counts + radix[level]};
			for (const State target : symbol_post.targets) {
				const size_t new_config{target * num_of_counts + new_counts};
				if (!visit(new_config, SearchNode{.prev = config, .symbol = symbol, .level = level})) { continue; }
				if (new_counts == full_counts && nft.final[target]) { return words_leading_to(new_config); }
				worklist.push_back(new_config);
			}
		}
	}

	return std::nullopt;
}
