/** @file
 * @brief The search nodes and the processed antichain of the antichain inclusion check, shared by the NFA inclusion
 *  and the NFT inclusion that explores jump transitions lazily; not installed.
 */

#ifndef MATA_SRC_NFA_ANTICHAIN_INCLUSION_HH_
#define MATA_SRC_NFA_ANTICHAIN_INCLUSION_HH_

#include <algorithm>
#include <cstdint>
#include <vector>

#include "mata/nfa/types.hh"

namespace mata::nfa::internal {

/// A pair (q, S) of a state of the smaller automaton and a macrostate of the bigger one, as a node of the search.
///  Nodes are never removed from @c nodes, so their indices stay valid as the parents a counterexample is read
///  back through.
struct Node {
	State smaller_state;
	StateSet bigger_set;
	uint64_t signature;
	/// A later node dominates this one: the worklist skips it and its macrostate is released.
	bool dominated;
};
/// How a node was reached, recorded only when a counterexample is requested.
struct Step {
	/// The node this one was reached from, or the node itself for an initial one.
	size_t parent;
	/// The symbol read on the transition from @c parent.
	Symbol symbol;
};

/**
 * @brief The macrostates already processed for one state of the smaller automaton, bucketed by cardinality.
 *
 * All entries stored here share the same state of the smaller automaton, so the dominance relation reduces to
 *  set inclusion between the macrostates of the bigger automaton. Cardinality alone then decides which entries
 *  can possibly be related to a candidate @c succ:
 *  - @c anti ⊆ @c succ requires @c |anti| ≤ @c |succ|, so the subsumption test reads nothing above @c |succ|;
 *  - @c succ ⊆ @c d requires @c |d| ≥ @c |succ|, so pruning reads nothing below @c |succ|;
 *  - at equal cardinality both reduce to equality, which is one comparison of two sorted vectors, and an entry
 *    equal to @c succ subsumes it, so pruning never has to consider that bucket.
 * Inside a bucket the stored signature rejects most of the remaining pairs in O(1), before the node is read and
 *  the two sorted vectors are merged; see @c algorithms::macrostate_signature().
 *
 * Only cardinalities that actually hold an entry get a bucket, kept in increasing order: the macrostates of one
 *  run occupy few distinct cardinalities out of the @c |bigger| possible ones, so indexing by cardinality would
 *  pay for (and scan) a bucket header per unused size.
 *
 * Every node on the worklist is stored here as well, so marking the pruned nodes as dominated also removes them
 *  from the worklist, without scanning it.
 */
class ProcessedAntichain {
  public:
	/// Is some stored macrostate a subset of @p succ_set, and therefore dominates it?
	bool subsumes(const std::vector<Node>& nodes, const StateSet& succ_set, const uint64_t succ_signature) const {
		const size_t cardinality{succ_set.size()};
		for (const Bucket& bucket : buckets_) {
			if (bucket.cardinality > cardinality) { break; }
			if (bucket.cardinality == cardinality) {
				// A subset of the same cardinality is the set itself.
				return std::ranges::any_of(bucket.entries, [&](const Entry& anti) {
					return anti.signature == succ_signature && nodes[anti.node].bigger_set == succ_set;
				});
			}
			for (const Entry& anti : bucket.entries) {
				if ((anti.signature & ~succ_signature) != 0) { continue; }
				if (nodes[anti.node].bigger_set.is_subset_of(succ_set)) { return true; }
			}
		}
		return false;
	}

	/// Removes every stored macrostate that @p succ_set dominates, that is, every strict superset of it, and
	///  marks its node as dominated.
	void prune(std::vector<Node>& nodes, const StateSet& succ_set, const uint64_t succ_signature) {
		auto bucket{std::ranges::upper_bound(buckets_, succ_set.size(), {}, &Bucket::cardinality)};
		while (bucket != buckets_.end()) {
			std::erase_if(bucket->entries, [&](const Entry& stored) {
				if ((succ_signature & ~stored.signature) != 0) { return false; }
				Node& node{nodes[stored.node]};
				if (!succ_set.is_subset_of(node.bigger_set)) { return false; }
				node.dominated = true;
				node.bigger_set = StateSet{};
				return true;
			});
			bucket = bucket->entries.empty() ? buckets_.erase(bucket) : bucket + 1;
		}
	}

	void insert(const std::vector<Node>& nodes, const size_t node_idx) {
		const Node& node{nodes[node_idx]};
		const size_t cardinality{node.bigger_set.size()};
		auto bucket{std::ranges::lower_bound(buckets_, cardinality, {}, &Bucket::cardinality)};
		if (bucket == buckets_.end() || bucket->cardinality != cardinality) {
			bucket = buckets_.insert(bucket, Bucket{.cardinality = cardinality, .entries = {}});
		}
		bucket->entries.push_back(Entry{.signature = node.signature, .node = node_idx});
	}

  private:
	struct Entry {
		uint64_t signature;
		size_t node;
	};
	struct Bucket {
		size_t cardinality;
		std::vector<Entry> entries;
	};

	/// The occupied cardinalities only, ordered by increasing cardinality, never holding an empty bucket.
	std::vector<Bucket> buckets_{};
};

/**
 * @brief Prepend to @p cex the symbols and the smaller states of the search steps that lead from an initial node to
 *  @p node_idx.
 *
 * @p cex holds the part of the counterexample after @p node_idx in reverse order (the last symbol first); on return it
 *  is the whole run up to there in reading order.
 */
inline void trace_back(const std::vector<Node>& nodes, const std::vector<Step>& steps, size_t node_idx, Run& cex) {
	for (; steps[node_idx].parent != node_idx; node_idx = steps[node_idx].parent) {
		cex.word.push_back(steps[node_idx].symbol);
		cex.path.push_back(nodes[steps[node_idx].parent].smaller_state);
	}
	std::ranges::reverse(cex.word);
	std::ranges::reverse(cex.path);
}

} // namespace mata::nfa::internal

#endif // MATA_SRC_NFA_ANTICHAIN_INCLUSION_HH_
