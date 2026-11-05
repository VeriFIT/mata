/** @file
 * @brief Left quotient of an NFA language by another NFA language.
 */

#include "mata/nfa/nfa.hh"
#include "mata/utils/two-dimensional-map.hh"
#include <bit>
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
		uint64_t* const visited_row{bits_.get() + static_cast<size_t>(nfa_state) * 2 * row_words_};
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
		uint64_t* const pending_row{bits_.get() + (static_cast<size_t>(nfa_state) * 2 + 1) * row_words_};
		for (size_t word{0}; word < row_words_; ++word) {
			for (uint64_t bits{std::exchange(pending_row[word], 0)}; bits != 0; bits &= bits - 1) {
				batch.push_back(static_cast<State>(word * 64 + static_cast<size_t>(std::countr_zero(bits))));
			}
		}
		return true;
	}

  private:
	struct Free {
		void operator()(uint64_t* const bits) const noexcept { std::free(bits); }
	};

	const size_t row_words_;
	/// Per state of nfa, the row of visited pairs followed by the row of pending pairs.
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
constexpr size_t max_bit_matrix_bytes{200'000'000};

/**
 * @brief Collect the states of @p nfa reachable by reading a word of @p prefixes.
 *
 * Explores the reachable pairs (prefixes_state, nfa_state) of the product of @p prefixes and @p nfa without building
 *  it. A pair whose prefixes_state is final contributes its nfa_state. Pairs whose prefixes_state cannot reach a
 *  final state of @p prefixes are never visited: they contribute nothing and only enlarge the explored product.
 */
template <class PairStore>
utils::SparseSet<State> reachable_by_prefixes(
	const Nfa& nfa, const Nfa& prefixes, const Symbol first_epsilon, const BoolVector& useful_prefixes_states
) {
	utils::SparseSet<State> result{};
	PairStore pairs{prefixes.num_of_states(), nfa.num_of_states()};

	auto visit_pair = [&](const State prefixes_state, const State nfa_state) {
		if (pairs.visit(prefixes_state, nfa_state) && prefixes.final.contains(prefixes_state)) {
			result.insert(nfa_state);
		}
	};

	for (const State prefixes_initial_state : prefixes.initial) {
		if (!useful_prefixes_states[prefixes_initial_state]) { continue; }
		for (const State nfa_initial_state : nfa.initial) { visit_pair(prefixes_initial_state, nfa_initial_state); }
	}

	State nfa_source{};
	std::vector<State> batch{};
	while (pairs.take_batch(nfa_source, batch)) {
		const StatePost& nfa_state_post{nfa.delta[nfa_source]};
		// Epsilons sort after all normal symbols and move one side only.
		const StatePost::const_iterator nfa_epsilon_begin{nfa_state_post.first_epsilon_it(first_epsilon)};
		for (auto nfa_epsilon_post{nfa_epsilon_begin}; nfa_epsilon_post != nfa_state_post.end(); ++nfa_epsilon_post) {
			for (const State nfa_target : nfa_epsilon_post->targets) {
				for (const State prefixes_source : batch) { visit_pair(prefixes_source, nfa_target); }
			}
		}

		for (const State prefixes_source : batch) {
			const StatePost& prefixes_state_post{prefixes.delta[prefixes_source]};
			const StatePost::const_iterator prefixes_epsilon_begin{prefixes_state_post.first_epsilon_it(first_epsilon)};
			for (auto prefixes_epsilon_post{prefixes_epsilon_begin}; prefixes_epsilon_post != prefixes_state_post.end();
				 ++prefixes_epsilon_post) {
				for (const State prefixes_target : prefixes_epsilon_post->targets) {
					if (useful_prefixes_states[prefixes_target]) { visit_pair(prefixes_target, nfa_source); }
				}
			}

			// Shared symbols: two-pointer merge over both sorted state posts.
			auto prefixes_symbol_post{prefixes_state_post.begin()};
			auto nfa_symbol_post{nfa_state_post.begin()};
			while (prefixes_symbol_post != prefixes_epsilon_begin && nfa_symbol_post != nfa_epsilon_begin) {
				if (prefixes_symbol_post->symbol < nfa_symbol_post->symbol) {
					++prefixes_symbol_post;
					continue;
				}
				if (nfa_symbol_post->symbol < prefixes_symbol_post->symbol) {
					++nfa_symbol_post;
					continue;
				}
				for (const State prefixes_target : prefixes_symbol_post->targets) {
					if (!useful_prefixes_states[prefixes_target]) { continue; }
					for (const State nfa_target : nfa_symbol_post->targets) { visit_pair(prefixes_target, nfa_target); }
				}
				++prefixes_symbol_post;
				++nfa_symbol_post;
			}
		}
	}
	return result;
}

utils::SparseSet<State> reachable_by_prefixes(const Nfa& nfa, const Nfa& prefixes, const Symbol first_epsilon) {
	const size_t nfa_num_of_states{nfa.num_of_states()};
	const size_t prefixes_num_of_states{prefixes.num_of_states()};
	if (nfa_num_of_states == 0 || prefixes_num_of_states == 0) { return {}; }
	const BoolVector useful_prefixes_states{prefixes.get_useful_states()};
	if (BitMatrixPairStore::num_of_bytes(prefixes_num_of_states, nfa_num_of_states) <= max_bit_matrix_bytes) {
		return reachable_by_prefixes<BitMatrixPairStore>(nfa, prefixes, first_epsilon, useful_prefixes_states);
	}
	return reachable_by_prefixes<HashPairStore>(nfa, prefixes, first_epsilon, useful_prefixes_states);
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

} // namespace mata::nfa.
