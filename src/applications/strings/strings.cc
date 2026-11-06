/* nfa-strings.cc -- Operations on NFAs for string solving.
 */

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"
#include "mata/utils/assert.hh"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
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

namespace {
/// The length of the longest word to a final state, of a state that is useless or not visited yet.
constexpr State NO_LONGEST_WORD{Limits::max_state};

/// The length of the longest word from each useful state to a final state (@c NO_LONGEST_WORD for useless states), or
///  @c std::nullopt if a cycle passes through a useful state, i.e., the language is infinite.
/// An iterative depth-first search over the useful states: a successor still on the stack closes a cycle, and a state
///  leaves the stack only after all its successors, so its length is complete then.
std::optional<std::vector<State>> get_longest_word_lengths(const Nfa& nfa) {
	const size_t num_of_states{nfa.num_of_states()};
	const mata::BoolVector useful{nfa.get_useful_states()};
	std::vector<State> longest(num_of_states, NO_LONGEST_WORD);
	mata::BoolVector on_stack(num_of_states, false);
	struct Frame {
		State state;
		State longest; ///< The longest word to a final state over the successors searched so far.
		SuccessorCursor::const_iterator successor;
	};
	std::vector<Frame> stack{};
	const auto push{[&](const State state) {
		on_stack[state] = true;
		// The analyzer loses track of the 'nfa' reference captured by the lambda and reports a null call.
		// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
		stack.push_back({.state = state, .longest = 0, .successor = SuccessorCursor{nfa.delta[state]}.begin()});
	}};

	for (const State initial_state : nfa.initial) {
		if (!useful[initial_state] || longest[initial_state] != NO_LONGEST_WORD) { continue; }
		push(initial_state);
		while (!stack.empty()) {
			Frame& frame{stack.back()};
			for (; frame.successor != std::default_sentinel; ++frame.successor) {
				const State target{*frame.successor};
				if (!useful[target]) { continue; }
				if (on_stack[target]) { return std::nullopt; }
				if (longest[target] == NO_LONGEST_WORD) { break; }
				frame.longest = std::max(frame.longest, static_cast<State>(longest[target] + 1));
			}
			// The parent advances past the target once the target is finished.
			if (frame.successor != std::default_sentinel) {
				push(*frame.successor);
				continue;
			}

			// A useful state is final or has a useful successor, so a length of 0 means a final state.
			const State state{frame.state};
			longest[state] = frame.longest;
			on_stack[state] = false;
			stack.pop_back();
			if (!stack.empty()) {
				Frame& parent{stack.back()};
				parent.longest = std::max(parent.longest, static_cast<State>(longest[state] + 1));
				++parent.successor;
			}
		}
	}
	return longest;
}

/// The search behind @c get_longest_words() and @c get_longest_words_lazy(), depth-first in lexicographic order over
///  one shared word. The node of a word of length d is the macrostate of the states reached by the word whose longest
///  word to a final state has length @c length_ - d, so every branch ends in a longest word: a path spelling a
///  longest word passes through no other state. The macrostates, determinized on the fly, visit every word once
///  however many accepting paths spell it.
/// A node keeps one cursor over the symbol posts of each of its states, all nodes in one stack, so a search as deep as
///  the automaton is large allocates nothing per node.
class LongestWordSearch {
  public:
	LongestWordSearch(const Nfa& nfa, std::vector<State> longest) : nfa_{nfa}, longest_{std::move(longest)} {
		for (const State state : nfa_.initial) {
			if (const State length{longest_[state]}; length != NO_LONGEST_WORD) {
				if (macrostate_.empty() || length > length_) {
					macrostate_.clear();
					length_ = length;
				}
				if (length == length_) { macrostate_.push_back(state); }
			}
		}
		word_.reserve(length_);
	}

	/// The next longest word, or @c nullptr once the search is exhausted. Valid until the next call.
	const mata::Word* next() {
		if (!started_) {
			started_ = true;
			if (macrostate_.empty()) { return nullptr; }
			if (length_ == 0) { return &word_; }
			enter();
		}
		while (!node_begins_.empty()) {
			const size_t depth{node_begins_.size() - 1};
			const std::span<Cursor> cursors{std::span{cursors_}.subspan(node_begins_.back())};
			word_.resize(depth);
			mata::Symbol symbol{0};
			bool enabled{false};
			for (const Cursor& cursor : cursors) {
				if (cursor.position != cursor.end && (!enabled || cursor.position->symbol < symbol)) {
					symbol = cursor.position->symbol;
					enabled = true;
				}
			}
			if (!enabled) {
				cursors_.resize(node_begins_.back());
				node_begins_.pop_back();
				continue;
			}

			const State target_length{static_cast<State>(length_ - depth - 1)};
			size_t num_of_symbol_posts{0};
			macrostate_.clear();
			for (Cursor& cursor : cursors) {
				if (cursor.position == cursor.end || cursor.position->symbol != symbol) { continue; }
				for (const State target : cursor.position->targets) {
					if (longest_[target] == target_length) { macrostate_.push_back(target); }
				}
				++cursor.position;
				++num_of_symbol_posts;
			}
			if (macrostate_.empty()) { continue; }
			if (num_of_symbol_posts > 1) {
				std::ranges::sort(macrostate_);
				macrostate_.erase(std::ranges::unique(macrostate_).begin(), macrostate_.end());
			}
			word_.push_back(symbol);
			if (target_length == 0) { return &word_; }
			enter();
		}
		return nullptr;
	}

  private:
	/// The symbol posts of a state not visited yet by its search node.
	struct Cursor {
		StatePost::const_iterator position{};
		StatePost::const_iterator end{};
	};

	/// Push the search node of @c macrostate_.
	void enter() {
		node_begins_.push_back(cursors_.size());
		for (const State state : macrostate_) {
			const StatePost& state_post{nfa_.delta[state]};
			cursors_.push_back({.position = state_post.begin(), .end = state_post.end()});
		}
	}

	const Nfa& nfa_;
	/// The length of the longest word from each state to a final state, see @c get_longest_word_lengths().
	const std::vector<State> longest_;
	State length_{0}; ///< The length of the longest words.
	mata::Word word_{}; ///< The word of the current search node.
	std::vector<State> macrostate_{}; ///< The macrostate of the node being entered.
	std::vector<Cursor> cursors_{}; ///< The cursors of all nodes on the search path, the deepest node last.
	std::vector<size_t> node_begins_{}; ///< The first cursor of each node on the search path.
	bool started_{false};
};

#if MATA_HAS_GENERATOR_SUPPORT
std::generator<mata::Word> longest_words(LongestWordSearch search) {
	while (const mata::Word* word{search.next()}) { co_yield *word; }
}
#endif
} // namespace

std::optional<std::set<mata::Word>> mata::applications::strings::get_longest_words(const Nfa& nfa) {
	std::optional<std::vector<State>> longest{get_longest_word_lengths(nfa)};
	if (!longest.has_value()) { return std::nullopt; }
	LongestWordSearch search{nfa, std::move(*longest)};
	std::set<Word> result{};
	// The words come in lexicographic order, so each is appended in constant time.
	while (const Word* word{search.next()}) { result.emplace_hint(result.end(), *word); }
	return result;
}

#if MATA_HAS_GENERATOR_SUPPORT
std::optional<std::generator<mata::Word>> mata::applications::strings::get_longest_words_lazy(const Nfa& nfa) {
	std::optional<std::vector<State>> longest{get_longest_word_lengths(nfa)};
	if (!longest.has_value()) { return std::nullopt; }
	return longest_words(LongestWordSearch{nfa, std::move(*longest)});
}
#endif

std::set<mata::Symbol> mata::applications::strings::get_accepted_symbols(const Nfa& nfa) {
	std::set<mata::Symbol> accepted_symbols;
	for (const State init : nfa.initial) {
		for (const SymbolPost& symbol_post_init : nfa.delta[init]) {
			const mata::Symbol sym = symbol_post_init.symbol;
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
	one_letter = determinize(one_letter);
	one_letter.trim();
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
		const nfa::StatePost post = one_letter.delta[curr_state.value()];

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
	Nfa tr_aut{nfa};
	tr_aut.trim();
	if (tr_aut.initial.empty()) { return false; }
	return std::ranges::all_of(tr_aut.initial, [&](const auto& ini) {
		return tr_aut.delta[ini].empty() && tr_aut.final[ini];
	});
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
		// On a build where size_t is as wide as unsigned, 'lengths[level] + 1' would wrap to zero and the division
		// below would be undefined, so the length that cannot be spanned is rejected before it is incremented.
		if (static_cast<size_t>(lengths[level]) >= UNVISITED) { throw too_large(); }
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

	// The search descends into the first successor it has not seen yet and only looks at the next transition of a
	// configuration once it comes back, so reaching an accepting configuration costs one step per symbol written
	// instead of expanding every transition on the way. Breadth first would expand the whole frontier below the
	// requested lengths before it ever reached one.
	struct Frame {
		size_t config;
		size_t counts;
		nft::Level level;
		/// Nothing more can be written on the tape of @c level, so only epsilon transitions are left to take.
		bool tape_full;
		StatePost::const_iterator symbol_post;
		StatePost::const_iterator symbol_post_end;
		SmallStateSet::const_iterator target;
	};
	std::vector<Frame> stack{};
	auto enter = [&](const size_t config) {
		const auto source{static_cast<State>(config / num_of_counts)};
		const size_t counts{config % num_of_counts};
		const nft::Level level{nft.levels[source]};
		const StatePost& state_post{nft.delta[source]};
		Frame frame{
			.config = config,
			.counts = counts,
			.level = level,
			// Reading a non-epsilon symbol fills one more place on the tape of the source level.
			.tape_full = (counts / radix[level]) % (static_cast<size_t>(lengths[level]) + 1) == lengths[level],
			.symbol_post = state_post.cbegin(),
			.symbol_post_end = state_post.cend(),
			.target = {},
		};
		if (frame.symbol_post != frame.symbol_post_end) { frame.target = frame.symbol_post->targets.cbegin(); }
		stack.push_back(frame);
	};

	for (const State initial_state : nft.initial) {
		const size_t config{initial_state * num_of_counts};
		if (!visit(config, SearchNode{.prev = SEARCH_ROOT})) { continue; }
		if (full_counts == 0 && nft.final[initial_state]) { return std::vector<Word>(num_of_levels); }
		enter(config);
	}

	while (!stack.empty()) {
		Frame& frame{stack.back()};
		// Skip the symbol posts that are used up and the ones that would write past the requested length.
		while (frame.symbol_post != frame.symbol_post_end &&
			   (frame.target == frame.symbol_post->targets.cend() ||
				(frame.symbol_post->symbol != nft::EPSILON && frame.tape_full))) {
			++frame.symbol_post;
			if (frame.symbol_post != frame.symbol_post_end) { frame.target = frame.symbol_post->targets.cbegin(); }
		}
		if (frame.symbol_post == frame.symbol_post_end) {
			stack.pop_back();
			continue;
		}
		const Symbol symbol{frame.symbol_post->symbol};
		const State target{*frame.target};
		frame.target = std::next(frame.target);
		const size_t new_counts{symbol == nft::EPSILON ? frame.counts : frame.counts + radix[frame.level]};
		const size_t new_config{(target * num_of_counts) + new_counts};
		if (!visit(new_config, SearchNode{.prev = frame.config, .symbol = symbol, .level = frame.level})) { continue; }
		if (new_counts == full_counts && nft.final[target]) { return words_leading_to(new_config); }
		enter(new_config); // Invalidates 'frame'.
	}

	return std::nullopt;
}
