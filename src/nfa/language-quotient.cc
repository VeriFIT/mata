/** @file
 * @brief Left and right quotients of an NFA language by another NFA language.
 */

#include "mata/nfa/nfa.hh"
#include "mata/utils/two-dimensional-map.hh"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace mata::nfa {

namespace {

/**
 * @brief Pairs (prefixes_state, nfa_state) of the traversal as two bit rows per state of nfa, each with a bit per state
 *  of prefixes: the visited pairs and the visited pairs waiting for processing.
 *
 * The pending pairs of one state of nfa are taken as one batch, so the transitions of the state of nfa are read once
 *  for all its pending states of prefixes. The automaton nfa is usually the larger one, and reading its scattered
 *  transitions once per pair dominates the traversal otherwise. A state of nfa is queued once per batch, first in
 *  first out, so that batches grow while they wait.
 */
class BitMatrixPairStore {
  public:
	BitMatrixPairStore(const size_t prefixes_num_of_states, const size_t nfa_num_of_states)
		: row_words_{words_per_row(prefixes_num_of_states)},
		  // calloc() of a large block maps zeroed pages lazily: rows of states of nfa never reached cost nothing.
		  //  'std::make_unique<uint64_t[]>()' would zero the whole block up front instead.
		  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
		  bits_{static_cast<uint64_t*>(std::calloc(nfa_num_of_states * 2 * row_words_, sizeof(uint64_t)))},
		  queued_(nfa_num_of_states, false) {
		if (bits_ == nullptr) { throw std::bad_alloc{}; }
	}

	static size_t words_per_row(const size_t prefixes_num_of_states) { return (prefixes_num_of_states + 63) / 64; }

	/// Bytes taken by the store for automata of the given sizes.
	static size_t num_of_bytes(const size_t prefixes_num_of_states, const size_t nfa_num_of_states) {
		return nfa_num_of_states * 2 * words_per_row(prefixes_num_of_states) * sizeof(uint64_t);
	}

	/// Mark the pair visited and pending. Returns true iff it was not visited before.
	bool visit(const State prefixes_state, const State nfa_state) {
		uint64_t* const visited_row{bits_.get() + (static_cast<size_t>(nfa_state) * 2 * row_words_)};
		const size_t word{prefixes_state / 64};
		const uint64_t bit{uint64_t{1} << (prefixes_state % 64)};
		if ((visited_row[word] & bit) != 0) { return false; }
		visited_row[word] |= bit;
		visited_row[row_words_ + word] |= bit;
		if (!queued_[nfa_state]) {
			queued_[nfa_state] = true;
			worklist_.push_back(nfa_state);
		}
		return true;
	}

	/// Take all pending pairs of one state of nfa: their states of prefixes into @p batch. Returns false iff none.
	bool take_batch(State& nfa_state, std::vector<State>& batch) {
		if (worklist_.empty()) { return false; }
		nfa_state = worklist_.front();
		worklist_.pop_front();
		queued_[nfa_state] = false;
		batch.clear();
		uint64_t* const pending_row{bits_.get() + ((static_cast<size_t>(nfa_state) * 2 + 1) * row_words_)};
		for (size_t word{0}; word < row_words_; ++word) {
			for (uint64_t bits{std::exchange(pending_row[word], 0)}; bits != 0; bits &= bits - 1) {
				batch.push_back(static_cast<State>((word * 64) + static_cast<size_t>(std::countr_zero(bits))));
			}
		}
		return true;
	}

  private:
	struct Free {
		// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): releases the calloc() above.
		void operator()(uint64_t* const bits) const noexcept { std::free(bits); }
	};

	const size_t row_words_;
	/// Per state of nfa, the row of visited pairs followed by the row of pending pairs.
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): runtime-sized, calloc()-backed block.
	std::unique_ptr<uint64_t[], Free> bits_;
	BoolVector queued_;
	std::deque<State> worklist_{};
};

/// Pairs (prefixes_state, nfa_state) of the traversal in a hash table, for automata too large for the bit matrix.
class HashPairStore {
  public:
	HashPairStore(const size_t prefixes_num_of_states, const size_t nfa_num_of_states)
		: visited_{prefixes_num_of_states, nfa_num_of_states} {}

	/// Mark the pair visited and pending. Returns true iff it was not visited before.
	bool visit(const State prefixes_state, const State nfa_state) {
		if (visited_.get(prefixes_state, nfa_state) != Visited::no_value) { return false; }
		visited_.insert(prefixes_state, nfa_state, 0);
		pending_.emplace_back(prefixes_state, nfa_state);
		return true;
	}

	/// Take the pending pairs of one state of nfa on top of the stack. Returns false iff there is no pending pair.
	bool take_batch(State& nfa_state, std::vector<State>& batch) {
		if (pending_.empty()) { return false; }
		nfa_state = pending_.back().second;
		batch.clear();
		while (!pending_.empty() && pending_.back().second == nfa_state) {
			batch.push_back(pending_.back().first);
			pending_.pop_back();
		}
		return true;
	}

  private:
	/// A zero matrix budget forces the hash table. The stored value is unused.
	using Visited = utils::TwoDimensionalMap<State, false, 0>;
	Visited visited_;
	std::vector<std::pair<State, State>> pending_{};
};

/// The bit matrix is used while it takes at most this many Bytes, the hash table otherwise.
constexpr size_t MAX_BIT_MATRIX_BYTES{200'000'000};

/**
 * @brief The moves of nfa along its transitions, forward: the pairs reached from the batch of one state of nfa by a
 *  move of nfa (a one-sided epsilon or a symbol shared with the word automaton).
 */
class ForwardMoves {
  public:
	explicit ForwardMoves(const Nfa& nfa) : nfa_{nfa} {}

	template <class Visit>
	void expand(
		const State nfa_source,
		const std::vector<State>& batch,
		const Nfa& words,
		const BoolVector& useful_words_states,
		const Symbol first_epsilon,
		Visit&& visit_pair
	) const {
		const StatePost& nfa_state_post{nfa_.delta[nfa_source]};
		// Epsilons sort after all normal symbols and move one side only.
		const StatePost::const_iterator nfa_epsilon_begin{nfa_state_post.first_epsilon_it(first_epsilon)};
		for (auto nfa_epsilon_post{nfa_epsilon_begin}; nfa_epsilon_post != nfa_state_post.end(); ++nfa_epsilon_post) {
			for (const State nfa_target : nfa_epsilon_post->targets) {
				for (const State words_source : batch) { visit_pair(words_source, nfa_target); }
			}
		}

		for (const State words_source : batch) {
			const StatePost& words_state_post{words.delta[words_source]};
			const StatePost::const_iterator words_epsilon_begin{words_state_post.first_epsilon_it(first_epsilon)};
			// Shared symbols: two-pointer merge over both sorted state posts.
			auto words_symbol_post{words_state_post.begin()};
			auto nfa_symbol_post{nfa_state_post.begin()};
			while (words_symbol_post != words_epsilon_begin && nfa_symbol_post != nfa_epsilon_begin) {
				if (words_symbol_post->symbol < nfa_symbol_post->symbol) {
					++words_symbol_post;
					continue;
				}
				if (nfa_symbol_post->symbol < words_symbol_post->symbol) {
					++nfa_symbol_post;
					continue;
				}
				for (const State words_target : words_symbol_post->targets) {
					if (!useful_words_states[words_target]) { continue; }
					for (const State nfa_target : nfa_symbol_post->targets) { visit_pair(words_target, nfa_target); }
				}
				++words_symbol_post;
				++nfa_symbol_post;
			}
		}
	}

  private:
	const Nfa& nfa_;
};

/**
 * @brief The moves of nfa against its transitions, backward, over a flat index of the predecessors of every state.
 *
 * Reverting nfa into another @c Nfa would build a symbol post with its own target set for every pair of a state and
 *  a symbol, which costs several times more than the traversal itself on large automata. Two counting passes over
 *  the transitions build this index instead. The predecessors of each state are sorted by symbol, so that the
 *  traversal merges them with a state post of the word automaton as the forward traversal does.
 */
class BackwardMoves {
  public:
	explicit BackwardMoves(const Nfa& nfa) : offsets_(nfa.num_of_states() + 1, 0) {
		const size_t num_of_sources{nfa.delta.num_of_states()};
		for (State source{0}; source < num_of_sources; ++source) {
			for (const SymbolPost& symbol_post : nfa.delta[source]) {
				for (const State target : symbol_post.targets) { ++offsets_[target + 1]; }
			}
		}
		for (size_t state{1}; state < offsets_.size(); ++state) { offsets_[state] += offsets_[state - 1]; }
		moves_.resize(offsets_.back());
		std::vector<size_t> next_move(offsets_.begin(), offsets_.end() - 1);
		for (State source{0}; source < num_of_sources; ++source) {
			for (const SymbolPost& symbol_post : nfa.delta[source]) {
				for (const State target : symbol_post.targets) {
					moves_[next_move[target]++] = {.symbol = symbol_post.symbol, .source = source};
				}
			}
		}
		for (size_t state{0}; state + 1 < offsets_.size(); ++state) {
			std::sort(
				moves_.begin() + static_cast<std::ptrdiff_t>(offsets_[state]),
				moves_.begin() + static_cast<std::ptrdiff_t>(offsets_[state + 1]),
				[](const Move& lhs, const Move& rhs) { return lhs.symbol < rhs.symbol; }
			);
		}
	}

	template <class Visit>
	void expand(
		const State nfa_target,
		const std::vector<State>& batch,
		const Nfa& words,
		const BoolVector& useful_words_states,
		const Symbol first_epsilon,
		Visit&& visit_pair
	) const {
		const size_t moves_begin{offsets_[nfa_target]};
		const size_t moves_end{offsets_[nfa_target + 1]};
		for (const State words_source : batch) {
			const StatePost& words_state_post{words.delta[words_source]};
			const StatePost::const_iterator words_epsilon_begin{words_state_post.first_epsilon_it(first_epsilon)};
			// Shared symbols: two-pointer merge of the sorted predecessors with the sorted state post.
			auto words_symbol_post{words_state_post.begin()};
			for (size_t move{moves_begin}; move < moves_end; ++move) {
				const auto [symbol, nfa_source] = moves_[move];
				// Epsilons sort after all normal symbols and move one side only.
				if (symbol >= first_epsilon) {
					visit_pair(words_source, nfa_source);
					continue;
				}
				while (words_symbol_post != words_epsilon_begin && words_symbol_post->symbol < symbol) {
					++words_symbol_post;
				}
				if (words_symbol_post == words_epsilon_begin || words_symbol_post->symbol != symbol) { continue; }
				for (const State words_target : words_symbol_post->targets) {
					if (useful_words_states[words_target]) { visit_pair(words_target, nfa_source); }
				}
			}
		}
	}

  private:
	struct Move {
		Symbol symbol;
		State source;
	};

	/// The predecessors of a state are the moves from its offset up to the offset of the next state.
	std::vector<size_t> offsets_;
	std::vector<Move> moves_{};
};

/**
 * @brief Collect the states of nfa paired with an accepting state of @p words in the pairs reachable from the pairs
 *  of a start state of @p words and a state of @p nfa_start.
 *
 * Explores the pairs (words_state, nfa_state) of the product without building it, moving nfa by @p nfa_moves.
 *  Forward from the initial states of nfa with the prefixes as @p words, this gives the states reachable by reading a
 *  word of prefixes. Backward from the final states of nfa with the reverted suffixes as @p words, this gives the
 *  states from which nfa accepts a word of suffixes. Pairs whose words_state cannot reach a final state of @p words
 *  are never visited: they contribute nothing and only enlarge the explored product.
 */
template <class PairStore, class NfaMoves>
utils::SparseSet<State> explore_pairs(
	const NfaMoves& nfa_moves,
	const size_t nfa_num_of_states,
	const utils::SparseSet<State>& nfa_start,
	const Nfa& words,
	const Symbol first_epsilon,
	const BoolVector& useful_words_states
) {
	utils::SparseSet<State> result{};
	PairStore pairs{words.num_of_states(), nfa_num_of_states};

	auto visit_pair = [&](const State words_state, const State nfa_state) {
		if (pairs.visit(words_state, nfa_state) && words.final.contains(words_state)) { result.insert(nfa_state); }
	};

	for (const State words_initial_state : words.initial) {
		if (!useful_words_states[words_initial_state]) { continue; }
		for (const State nfa_start_state : nfa_start) { visit_pair(words_initial_state, nfa_start_state); }
	}

	State nfa_state{};
	std::vector<State> batch{};
	while (pairs.take_batch(nfa_state, batch)) {
		for (const State words_source : batch) {
			const StatePost& words_state_post{words.delta[words_source]};
			for (auto words_epsilon_post{words_state_post.first_epsilon_it(first_epsilon)};
				 words_epsilon_post != words_state_post.end(); ++words_epsilon_post) {
				for (const State words_target : words_epsilon_post->targets) {
					if (useful_words_states[words_target]) { visit_pair(words_target, nfa_state); }
				}
			}
		}
		nfa_moves.expand(nfa_state, batch, words, useful_words_states, first_epsilon, visit_pair);
	}
	return result;
}

template <class NfaMoves>
utils::SparseSet<State> explore_pairs(
	const NfaMoves& nfa_moves,
	const size_t nfa_num_of_states,
	const utils::SparseSet<State>& nfa_start,
	const Nfa& words,
	const Symbol first_epsilon
) {
	const size_t words_num_of_states{words.num_of_states()};
	if (nfa_num_of_states == 0 || words_num_of_states == 0) { return {}; }
	const BoolVector useful_words_states{words.get_useful_states()};
	if (BitMatrixPairStore::num_of_bytes(words_num_of_states, nfa_num_of_states) <= MAX_BIT_MATRIX_BYTES) {
		return explore_pairs<BitMatrixPairStore>(
			nfa_moves, nfa_num_of_states, nfa_start, words, first_epsilon, useful_words_states
		);
	}
	return explore_pairs<HashPairStore>(
		nfa_moves, nfa_num_of_states, nfa_start, words, first_epsilon, useful_words_states
	);
}

/// The states of @p nfa reachable by reading a word of @p prefixes.
utils::SparseSet<State> reachable_by_prefixes(const Nfa& nfa, const Nfa& prefixes, const Symbol first_epsilon) {
	return explore_pairs(ForwardMoves{nfa}, nfa.num_of_states(), nfa.initial, prefixes, first_epsilon);
}

/// The states of @p nfa from which @p nfa accepts a word of @p suffixes.
utils::SparseSet<State> coreachable_by_suffixes(const Nfa& nfa, const Nfa& suffixes, const Symbol first_epsilon) {
	// Reverting the usually small suffixes is cheap; nfa is moved backward over its predecessor index instead.
	return explore_pairs(BackwardMoves{nfa}, nfa.num_of_states(), nfa.final, revert(suffixes), first_epsilon);
}

} // namespace

Nfa left_quotient(const Nfa& nfa, const Nfa& prefixes, const Symbol first_epsilon) {
	Nfa result{nfa};
	result.initial = reachable_by_prefixes(nfa, prefixes, first_epsilon);
	return result;
}

Nfa left_quotient(Nfa&& nfa, const Nfa& prefixes, const Symbol first_epsilon) {
	// Computed before nfa is moved from, as prefixes may alias nfa.
	utils::SparseSet<State> initial{reachable_by_prefixes(nfa, prefixes, first_epsilon)};
	Nfa result{std::move(nfa)};
	result.initial = std::move(initial);
	return result;
}

Nfa right_quotient(const Nfa& nfa, const Nfa& suffixes, const Symbol first_epsilon) {
	Nfa result{nfa};
	result.final = coreachable_by_suffixes(nfa, suffixes, first_epsilon);
	return result;
}

Nfa right_quotient(Nfa&& nfa, const Nfa& suffixes, const Symbol first_epsilon) {
	// Computed before nfa is moved from, as suffixes may alias nfa.
	utils::SparseSet<State> final{coreachable_by_suffixes(nfa, suffixes, first_epsilon)};
	Nfa result{std::move(nfa)};
	result.final = std::move(final);
	return result;
}

} // namespace mata::nfa.
