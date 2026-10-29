/** @file
 * @brief nfa-noodlification.cc -- Noodlification of NFAs
 */

#include <algorithm>
#include <map>
#include <random>
#include <ranges>
#include <set>
#include <unordered_set>

#include "mata/applications/strings.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/builder.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

using namespace mata::applications::strings;

namespace {
/**
 * Get a number of permutations for computed epsilon depths.
 * @param[in] epsilon_depths Computed list of epsilon transitions for each depth.
 * @return Number of permutations.
 */
size_t get_num_of_permutations(const std::vector<std::vector<Transition>>& epsilon_depths) {
	size_t num_of_permutations{1};
	for (const auto& transitions_at_depth : epsilon_depths) { num_of_permutations *= transitions_at_depth.size(); }
	return num_of_permutations;
}

/**
 * @brief Unify (as best as possible) the initial states and the final states of NFAs in @p nfas
 *
 * The unification happens only if the given automaton is not already unified, i.e. it is in @p unified_nfas.
 * We also add the newly unified automata to @p unified_nfas.
 *
 * @param[in] nfas The automata to unify
 * @param[in,out] unified_nfas The set of already unified automata
 */
void unify_initial_and_final_states(
	const std::vector<std::shared_ptr<Nfa>>& nfas, std::unordered_set<std::shared_ptr<Nfa>>& unified_nfas
) {
	for (const auto& nfa : nfas) {
		if (!unified_nfas.contains(nfa)) {
			nfa->unify_initial();
			nfa->unify_final();
			unified_nfas.insert(nfa);
		}
	}
}

Nfa concatenate_with(const std::vector<std::shared_ptr<Nfa>>& nfas, const mata::Symbol delimiter) {
	Nfa concatenation{*nfas[0]};
	for (size_t i = 1; i < nfas.size(); ++i) {
		concatenation = mata::nfa::algorithms::concatenate_eps(concatenation, *nfas[i], delimiter, true);
	}
	return concatenation;
}

/**
 * @brief Check if transducer @p nft T is for sure homomorphic, that is whether for each two words u,v, it holds T(u.v)
 * = T(u).T(v).
 *
 * @warning The check is only a heuristic, it can return false even if T is homomorphic.
 *
 * We check whether there is exactly one initial and final state (and they are the same), and every transition other
 * than those coming from the initial state have epsilon on the first tape.
 */
bool is_nft_homomorphic(const std::shared_ptr<Nft>& nft) {
	if (nft->levels.num_of_levels != 2) { return false; }

	// the only initial and the only final state must be the same state
	if (nft->final.size() != 1 || nft->initial.size() != 1 || *nft->final.begin() != *nft->initial.begin()) {
		return false;
	}

	const size_t num_of_states = nft->num_of_states();
	for (State q = 0; q < num_of_states; ++q) {
		if (nft->levels[q] == mata::nft::DEFAULT_LEVEL) {
			for (const SymbolPost& first_tape_transition : nft->delta[q]) {
				const bool first_tape_is_epsilon = (first_tape_transition.symbol == mata::nft::EPSILON);
				// there can be a concrete symbol on the first tape only on the transitions from the initial state
				if (const bool is_initial = (q == *nft->initial.begin()); !first_tape_is_epsilon && !is_initial) {
					return false;
				}
			}
		}
	}

	return true;
}

/// A segment with the empty language, for the pairs kept with `include_empty`: a single non-final state.
std::shared_ptr<const Nfa> make_empty_segment_automaton() {
	Nfa empty{};
	empty.initial.insert(empty.add_state());
	return std::make_shared<const Nfa>(std::move(empty));
}

/// The segment of all initial states of trimmed @p segment, which are unified into one; std::nullopt if @p segment
///  is empty and @p include_empty is false.
std::optional<seg_nfa::NoodleSegment> make_whole_segment(Nfa&& segment, const bool include_empty) {
	if (segment.initial.empty()) {
		if (!include_empty) { return std::nullopt; }
		return seg_nfa::NoodleSegment{make_empty_segment_automaton(), 0};
	}
	segment.unify_initial();
	const State initial{*segment.initial.begin()};
	return seg_nfa::NoodleSegment{std::make_shared<const Nfa>(std::move(segment)), initial};
}
} // namespace

std::vector<seg_nfa::Noodle> seg_nfa::noodlify(const SegNfa& aut, const Symbol epsilon, const bool include_empty) {
	const std::set<Symbol> epsilons({epsilon});
	// return noodlify_reach(aut, epsilons, include_empty);

	Segmentation segmentation{aut, epsilons};
	const auto& segments{segmentation.get_untrimmed_segments()};

	if (segments.size() == 1) {
		if (auto segment{make_whole_segment(trim(segments[0]), include_empty)}) { return {{std::move(*segment)}}; }
		return {};
	}

	auto unused_state{static_cast<State>(aut.num_of_states())}; // get some State not used in aut
	std::map<std::pair<State, State>, NoodleSegment> segments_one_initial_final;
	segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

	const auto& epsilon_depths{segmentation.get_epsilon_transitions_by_depth()};

	// Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
	const size_t num_of_permutations{get_num_of_permutations(epsilon_depths)};
	const size_t epsilon_depths_size{epsilon_depths.size()};

	std::vector<Noodle> noodles{};
	// noodle of epsilon transitions (each from different depth)
	std::vector<Transition> epsilon_noodle(epsilon_depths_size);
	// for each combination of ε-transitions, create the automaton.
	// based on https://stackoverflow.com/questions/48270565/create-all-possible-combinations-of-multiple-vectors
	for (size_t index{0}; index < num_of_permutations; ++index) {
		size_t temp{index};
		for (size_t depth{0}; depth < epsilon_depths_size; ++depth) {
			const size_t num_of_trans_at_cur_depth = epsilon_depths[depth].size();
			const size_t computed_index = temp % num_of_trans_at_cur_depth;
			temp /= num_of_trans_at_cur_depth;
			epsilon_noodle[depth] = epsilon_depths[depth][computed_index];
		}

		Noodle noodle;

		// epsilon_noodle[0] for sure exists, as we sorted out the case of only one segment at the beginning
		if (auto first_segment_iter{
				segments_one_initial_final.find(std::make_pair(unused_state, epsilon_noodle[0].source))
			};
			first_segment_iter != segments_one_initial_final.end()) {
			noodle.push_back(first_segment_iter->second);
		} else {
			continue;
		}

		bool all_segments_exist = true;
		for (auto iter = epsilon_noodle.begin(); iter + 1 != epsilon_noodle.end(); ++iter) {
			const auto next_iter{iter + 1};
			if (auto segment_iter = segments_one_initial_final.find(std::make_pair(iter->target, next_iter->source));
				segment_iter != segments_one_initial_final.end()) {
				noodle.push_back(segment_iter->second);
			} else {
				all_segments_exist = false;
				break;
			}
		}

		if (!all_segments_exist) { continue; }

		auto last_segment_iter =
			segments_one_initial_final.find(std::make_pair(epsilon_noodle.back().target, unused_state));
		if (last_segment_iter != segments_one_initial_final.end()) {
			noodle.push_back(last_segment_iter->second);
		} else {
			continue;
		}

		noodles.push_back(noodle);
	}
	return noodles;
}

Nfa seg_nfa::NoodleSegment::to_nfa() const {
	const utils::SparseSet<State> initial_states{initial};
	return trim(*automaton, nullptr, initial_states, std::nullopt);
}

bool seg_nfa::NoodleSegment::is_epsilon_only() const {
	// Every state of `automaton` reaches a final state, so a state without transitions accepts ε or nothing. `empty()`
	//  stops at the first non-empty symbol post; trim() and the simulation quotient create no symbol post without
	//  targets.
	return automaton->final.contains(initial) && automaton->delta[initial].empty();
}

namespace {
/**
 * Calls @p emit with each initial state s of @p segment and the segment of s in reduce(trim(segment, all initial
 *  states, final_states)), or std::nullopt if s reaches none of @p final_states.
 *
 * All segments share the one reduced automaton: trim(segment, {s}, final_states) is the part reachable from s of
 *  useful = trim(segment, all initial states, final_states), and the simulation quotient keeps the language of every
 *  state. Reducing the part of each s on its own, as done before, stores one automaton per pair although the parts of
 *  one final state overlap almost completely.
 */
template <class Emit>
void share_per_initial_state(
	const Nfa& segment,
	const std::optional<std::reference_wrapper<const mata::utils::SparseSet<State>>> final_states,
	Emit&& emit
) {
	mata::nfa::StateRenaming to_useful;
	const Nfa useful{trim(segment, &to_useful, std::nullopt, final_states)};
	mata::nfa::StateRenaming to_reduced;
	const auto reduced{std::make_shared<const Nfa>(mata::nfa::algorithms::reduce_simulation(useful, to_reduced))};
	for (const State init_state : segment.initial) {
		if (const auto useful_init{to_useful.find(init_state)}; useful_init != to_useful.end()) {
			emit(init_state, std::optional{seg_nfa::NoodleSegment{reduced, to_reduced.at(useful_init->second)}});
		} else {
			emit(init_state, std::optional<seg_nfa::NoodleSegment>{});
		}
	}
}
} // namespace

void seg_nfa::segs_one_initial_final(
	const std::vector<Nfa>& segments,
	bool include_empty,
	const State& unused_state,
	std::map<std::pair<State, State>, NoodleSegment>& out
) {
	std::shared_ptr<const Nfa> empty_automaton{};
	const auto insert = [&](const State init_state, const State final_state, std::optional<NoodleSegment>&& segment) {
		if (segment) {
			out.insert_or_assign(std::make_pair(init_state, final_state), std::move(*segment));
		} else if (include_empty) {
			if (!empty_automaton) { empty_automaton = make_empty_segment_automaton(); }
			out.insert_or_assign(std::make_pair(init_state, final_state), NoodleSegment{empty_automaton, 0});
		}
	};

	for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
		if (iter == segments.begin()) { // first segment will always have all initial states in noodles
			for (const State final_state : iter->final) {
				const utils::SparseSet<State> final_states{final_state};
				insert(
					unused_state, final_state,
					make_whole_segment(reduce(trim(*iter, nullptr, std::nullopt, final_states)), false)
				);
			}
		} else if (iter + 1 == segments.end()) { // last segment will always have all final states in noodles
			share_per_initial_state(
				*iter, std::nullopt, [&](const State init_state, std::optional<NoodleSegment>&& seg) {
					insert(init_state, unused_state, std::move(seg));
				}
			);
		} else { // the segments in-between
			for (const State final_state : iter->final) {
				const utils::SparseSet<State> final_states{final_state};
				share_per_initial_state(
					*iter, final_states, [&](const State init_state, std::optional<NoodleSegment>&& seg) {
						insert(init_state, final_state, std::move(seg));
					}
				);
			}
		}
	}
}

namespace {
using mata::Symbol;
using mata::nfa::Limits;

/**
 * Depth-first search for the noodles of a segment automaton, one noodle per call of next().
 *
 * A node of the search at depth k is a pair (initial, final) of segment k, its children the pairs of segment k + 1
 *  whose initial state is a target of an ε-transition from its final state. Leaves are noodles. In the forward order,
 *  the children are ordered by the ε-transitions and then by the final states of the segment, so the forward order of
 *  the leaves is the order in which the breadth-first search that preceded this one completed them; the other orders
 *  (seg_nfa::NoodleOrder) reorder the children of every node. The stack of the search holds one path instead of a
 *  whole level of partial noodles.
 *
 * The shared automaton of a final state (see seg_nfa::NoodleSegment) is built when the search first reaches the
 *  final state, the final states that an initial state reaches when the search first reaches the initial state.
 *
 * A noodle is finished more than once by two ε-transitions with different symbols to the same target, and by two
 *  routes that differ only in left out segments with the language {ε}. Each target is visited once per final state,
 *  which removes the former; a noodle without a left out segment is then finished once, as its segments fix its
 *  route. Only the noodles with a left out segment are remembered, by the pairs of their segments, to remove the
 *  latter.
 */
class NoodleEnumerator {
  public:
	NoodleEnumerator(
		const seg_nfa::SegNfa& aut,
		const std::set<Symbol>& epsilons,
		const bool include_empty,
		const seg_nfa::NoodleOrder order
	)
		: segmentation_{aut, epsilons},
		  segments_{segmentation_.get_untrimmed_segments()},
		  include_empty_{include_empty},
		  order_{order},
		  rng_{order.seed},
		  unused_state_{static_cast<State>(aut.num_of_states())} {
		seg_nfa::VisitedEpsilonsCounterMap def_eps_map;
		for (const Symbol eps : epsilons) { def_eps_map[eps] = 0; }
		def_eps_vector_ = seg_nfa::process_eps_map(def_eps_map);

		if (segments_.size() == 1) {
			single_ = make_whole_segment(trim(segments_[0]), include_empty_);
			return;
		}

		initial_position_.assign(unused_state_, 0);
		for (size_t seg_id{1}; seg_id < segments_.size(); ++seg_id) {
			size_t position{0};
			for (const State init : segments_[seg_id].initial) { initial_position_[init] = position++; }
		}
		is_target_.assign(unused_state_, false);

		Frame first{.segment = 0};
		const mata::BoolVector reached{reached_states(segments_[0], segments_[0].initial)};
		for (const State fin : segments_[0].final) {
			if (include_empty_ || reached[fin]) { first.children.emplace_back(unused_state_, fin); }
		}
		order_children(first);
		frames_.push_back(std::move(first));
	}

	NoodleEnumerator(const NoodleEnumerator&) = delete;
	NoodleEnumerator& operator=(const NoodleEnumerator&) = delete;

	/// The next noodle, or std::nullopt if there is none left.
	std::optional<seg_nfa::NoodleWithEpsilonsCounter> next() {
		if (segments_.size() == 1) {
			if (single_done_ || !single_) { return std::nullopt; }
			single_done_ = true;
			return seg_nfa::NoodleWithEpsilonsCounter{{*single_, def_eps_vector_}};
		}

		while (!frames_.empty()) {
			Frame& frame{frames_.back()};
			if (frame.next == frame.children.size()) {
				frames_.pop_back();
				if (!frames_.empty()) { pop_segment(); } // The pair whose children the frame held.
				continue;
			}
			const size_t index{frame.next};
			++frame.next;
			const size_t seg_id{frame.segment};
			const auto [init, fin]{frame.children[index]};

			push_segment(seg_id, init, fin);
			if (seg_id + 1 == segments_.size()) {
				std::optional<seg_nfa::NoodleWithEpsilonsCounter> noodle{};
				if (num_of_left_out_ == 0 || seen_.insert(keys_).second) { noodle = noodle_; }
				pop_segment();
				if (noodle) { return noodle; }
				continue;
			}
			frames_.push_back(make_frame(seg_id + 1, fin));
		}
		return std::nullopt;
	}

  private:
	using SegmentKey = std::pair<State, State>;

	/// The shared automaton of one final state and the state of every initial state of its segment in it.
	struct Part {
		std::shared_ptr<const Nfa> automaton{};
		/// Indexed by the position of the initial state in its segment; Limits::max_state if it is not in the part.
		std::vector<State> state_of_initial{};
	};

	struct Frame {
		size_t segment{};
		/// The pairs (initial, final) of segment `segment` to visit.
		std::vector<SegmentKey> children{};
		/// The number of children visited so far.
		size_t next{};
	};

	struct KeysHash {
		size_t operator()(const std::vector<SegmentKey>& keys) const {
			size_t accum{mata::utils::hash_combine(mata::utils::HASH_SEED, keys.size())};
			for (const auto& [init, fin] : keys) {
				accum = mata::utils::hash_combine(accum, init);
				accum = mata::utils::hash_combine(accum, fin);
			}
			return accum;
		}
	};

	seg_nfa::Segmentation segmentation_;
	const std::vector<Nfa>& segments_;
	const bool include_empty_;
	const seg_nfa::NoodleOrder order_;
	std::mt19937_64 rng_;
	/// A state not in the automaton: the initial state of the pairs of the first segment, the final of the last.
	const State unused_state_;
	seg_nfa::VisitedEpsilonsCounterVector def_eps_vector_{};

	std::optional<seg_nfa::NoodleSegment> single_{};
	bool single_done_{false};

	std::vector<size_t> initial_position_{};
	/// Parts by their final state; the part of the last segment by unused_state_.
	std::unordered_map<State, Part> parts_{};
	/// The final states (unused_state_ in the last segment) reached from each initial state, in the order of the
	///  final states of its segment.
	std::unordered_map<State, std::vector<State>> reached_finals_{};
	std::shared_ptr<const Nfa> empty_automaton_{};
	std::vector<bool> is_target_{};

	std::vector<Frame> frames_{};
	/// The noodle on the path of the search, without the left out segments.
	seg_nfa::NoodleWithEpsilonsCounter noodle_{};
	/// The pairs of the segments of noodle_.
	std::vector<SegmentKey> keys_{};
	/// Whether the segment of each depth on the path is in noodle_.
	std::vector<bool> kept_{};
	size_t num_of_left_out_{0};
	std::unordered_set<std::vector<SegmentKey>, KeysHash> seen_{};
	/// The number of states of a shared automaton reachable from a state of it, for the orders by size.
	std::map<std::pair<const Nfa*, State>, size_t> segment_sizes_{};

	static mata::BoolVector reached_states(const Nfa& segment, const mata::utils::SparseSet<State>& from) {
		mata::BoolVector reached(segment.num_of_states(), false);
		std::vector<State> stack{};
		for (const State state : from) {
			if (!reached[state]) {
				reached[state] = true;
				stack.push_back(state);
			}
		}
		while (!stack.empty()) {
			const State state{stack.back()};
			stack.pop_back();
			for (const SymbolPost& symbol_post : segment.delta[state]) {
				for (const State target : symbol_post.targets) {
					if (!reached[target]) {
						reached[target] = true;
						stack.push_back(target);
					}
				}
			}
		}
		return reached;
	}

	const std::vector<State>& finals_reached_from(const size_t seg_id, const State init) {
		const auto [iter, inserted]{reached_finals_.try_emplace(init)};
		if (!inserted) { return iter->second; }
		const Nfa& segment{segments_[seg_id]};
		const mata::BoolVector reached{
			include_empty_ ? mata::BoolVector{} : reached_states(segment, mata::utils::SparseSet<State>{init})
		};
		const auto is_reached = [&](const State fin) { return include_empty_ || reached[fin]; };
		if (seg_id + 1 == segments_.size()) {
			if (std::ranges::any_of(segment.final, is_reached)) { iter->second.push_back(unused_state_); }
		} else {
			for (const State fin : segment.final) {
				if (is_reached(fin)) { iter->second.push_back(fin); }
			}
		}
		return iter->second;
	}

	Frame make_frame(const size_t seg_id, const State previous_final) {
		Frame frame{.segment = seg_id};
		std::vector<State> targets{};
		for (const Transition& transition : segmentation_.get_epsilon_transitions(previous_final)) {
			if (!is_target_[transition.target]) {
				is_target_[transition.target] = true;
				targets.push_back(transition.target);
			}
		}
		for (const State target : targets) {
			is_target_[target] = false;
			for (const State fin : finals_reached_from(seg_id, target)) { frame.children.emplace_back(target, fin); }
		}
		order_children(frame);
		return frame;
	}

	size_t segment_size(const seg_nfa::NoodleSegment& seg) {
		const auto [iter, inserted]{segment_sizes_.try_emplace({seg.automaton.get(), seg.initial})};
		if (inserted) {
			const mata::BoolVector reached{reached_states(*seg.automaton, mata::utils::SparseSet<State>{seg.initial})};
			iter->second = static_cast<size_t>(std::ranges::count(reached, true));
		}
		return iter->second;
	}

	void order_children(Frame& frame) {
		using Kind = seg_nfa::NoodleOrder::Kind;
		std::vector<SegmentKey>& children{frame.children};
		switch (order_.kind) {
			case Kind::Forward:
				return;
			case Kind::Reverse:
				std::ranges::reverse(children);
				return;
			case Kind::Random:
				std::ranges::shuffle(children, rng_);
				return;
			case Kind::SmallestFirst:
			case Kind::LargestFirst: {
				std::vector<std::pair<size_t, SegmentKey>> sized{};
				sized.reserve(children.size());
				for (const auto& [init, fin] : children) {
					sized.emplace_back(segment_size(segment(frame.segment, init, fin)), SegmentKey{init, fin});
				}
				if (order_.kind == Kind::SmallestFirst) {
					std::ranges::stable_sort(sized, std::less{}, &std::pair<size_t, SegmentKey>::first);
				} else {
					std::ranges::stable_sort(sized, std::greater{}, &std::pair<size_t, SegmentKey>::first);
				}
				for (size_t i{0}; i < sized.size(); ++i) { children[i] = sized[i].second; }
				return;
			}
		}
	}

	const Part& part(const size_t seg_id, const State fin) {
		const auto [iter, inserted]{parts_.try_emplace(fin)};
		if (!inserted) { return iter->second; }
		Part& part{iter->second};
		const Nfa& segment{segments_[seg_id]};
		const mata::utils::SparseSet<State> final_states{fin};
		if (seg_id == 0) {
			if (auto whole{make_whole_segment(reduce(trim(segment, nullptr, std::nullopt, final_states)), false)}) {
				part.automaton = whole->automaton;
				part.state_of_initial = {whole->initial};
			}
			return part;
		}
		std::optional<std::reference_wrapper<const mata::utils::SparseSet<State>>> part_finals{};
		if (seg_id + 1 < segments_.size()) { part_finals = final_states; }
		part.state_of_initial.reserve(segment.initial.size());
		share_per_initial_state(segment, part_finals, [&](State, std::optional<seg_nfa::NoodleSegment>&& seg) {
			if (seg) {
				part.automaton = seg->automaton;
				part.state_of_initial.push_back(seg->initial);
			} else {
				part.state_of_initial.push_back(Limits::max_state);
			}
		});
		return part;
	}

	seg_nfa::NoodleSegment segment(const size_t seg_id, const State init, const State fin) {
		const Part& part_of_fin{part(seg_id, fin)};
		State state{Limits::max_state};
		if (part_of_fin.automaton != nullptr) {
			state = part_of_fin.state_of_initial[seg_id == 0 ? 0 : initial_position_[init]];
		}
		if (state != Limits::max_state) { return {part_of_fin.automaton, state}; }
		// Only the pairs of `include_empty_` reach no final state.
		if (!empty_automaton_) { empty_automaton_ = make_empty_segment_automaton(); }
		return {empty_automaton_, 0};
	}

	void push_segment(const size_t seg_id, const State init, const State fin) {
		seg_nfa::NoodleSegment seg{segment(seg_id, init, fin)};
		if (seg.is_epsilon_only()) {
			kept_.push_back(false);
			++num_of_left_out_;
			return;
		}
		seg_nfa::VisitedEpsilonsCounterVector counters{};
		if (seg_id == 0) {
			counters = def_eps_vector_;
		} else if (const auto visited{segmentation_.get_visited_eps().find(init)};
				   visited != segmentation_.get_visited_eps().end()) {
			counters = seg_nfa::process_eps_map(visited->second);
		}
		noodle_.emplace_back(std::move(seg), std::move(counters));
		keys_.emplace_back(init, fin);
		kept_.push_back(true);
	}

	void pop_segment() {
		if (kept_.back()) {
			noodle_.pop_back();
			keys_.pop_back();
		} else {
			--num_of_left_out_;
		}
		kept_.pop_back();
	}
};

#ifdef __cpp_lib_generator
template <class T> std::generator<T> no_noodles() { co_return; }
#endif
} // namespace

std::vector<seg_nfa::NoodleWithEpsilonsCounter>
	seg_nfa::noodlify_mult_eps(const SegNfa& aut, const std::set<Symbol>& epsilons, bool include_empty) {
	NoodleEnumerator enumerator{aut, epsilons, include_empty, NoodleOrder{}};
	std::vector<NoodleWithEpsilonsCounter> noodles{};
	while (auto noodle{enumerator.next()}) { noodles.push_back(std::move(*noodle)); }
	return noodles;
}

#ifdef __cpp_lib_generator
std::generator<seg_nfa::NoodleWithEpsilonsCounter>
	seg_nfa::noodlify_mult_eps_lazy(SegNfa aut, std::set<Symbol> epsilons, bool include_empty, NoodleOrder order) {
	NoodleEnumerator enumerator{aut, epsilons, include_empty, order};
	while (auto noodle{enumerator.next()}) { co_yield std::move(*noodle); }
}
#endif

std::vector<seg_nfa::Noodle> seg_nfa::noodlify_for_equation(
	const std::vector<std::reference_wrapper<Nfa>>& lhs_automata,
	const Nfa& rhs_automaton,
	bool include_empty,
	const ParameterMap& params
) {
	const auto lhs_aut_begin{lhs_automata.begin()};
	const auto lhs_aut_end{lhs_automata.end()};
	for (auto lhs_aut_iter{lhs_aut_begin}; lhs_aut_iter != lhs_aut_end; ++lhs_aut_iter) {
		lhs_aut_iter->get().unify_initial();
		lhs_aut_iter->get().unify_final();
	}

	if (lhs_automata.empty() || rhs_automaton.is_lang_empty()) { return {}; }

	// Automaton representing the left side concatenated over epsilon transitions.
	Nfa concatenated_lhs{*lhs_aut_begin};
	for (auto next_lhs_aut_it{lhs_aut_begin + 1}; next_lhs_aut_it != lhs_aut_end; ++next_lhs_aut_it) {
		concatenated_lhs = concatenate(concatenated_lhs, *next_lhs_aut_it, true);
	}

	Nfa product_pres_eps_trans{intersection(concatenated_lhs, rhs_automaton)};
	product_pres_eps_trans.trim();
	// After `trim()` every state lies on an accepting path, so the language is empty iff nothing is left.
	if (product_pres_eps_trans.initial.empty()) { return {}; }
	if (utils::haskey(params, "reduce")) {
		const std::string& reduce_value = params.at("reduce");
		if (reduce_value == "forward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = reduce(product_pres_eps_trans);
		}
		if (reduce_value == "backward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = revert(product_pres_eps_trans);
			product_pres_eps_trans = reduce(product_pres_eps_trans);
			product_pres_eps_trans = revert(product_pres_eps_trans);
		}
	}
	return noodlify(product_pres_eps_trans, mata::nfa::EPSILON, include_empty);
}

std::vector<seg_nfa::Noodle> seg_nfa::noodlify_for_equation(
	const std::vector<Nfa*>& lhs_automata, const Nfa& rhs_automaton, bool include_empty, const ParameterMap& params
) {
	const auto lhs_aut_begin{lhs_automata.begin()};
	const auto lhs_aut_end{lhs_automata.end()};

	std::string reduce_value{};
	if (utils::haskey(params, "reduce")) { reduce_value = params.at("reduce"); }

	if (!reduce_value.empty()) {
		if (reduce_value == "forward" || reduce_value == "backward" || reduce_value == "bidirectional") {
			for (auto lhs_aut_iter{lhs_aut_begin}; lhs_aut_iter != lhs_aut_end; ++lhs_aut_iter) {
				(*lhs_aut_iter)->unify_initial();
				(*lhs_aut_iter)->unify_final();
			}
		}
	}

	if (lhs_automata.empty() || rhs_automaton.is_lang_empty()) { return {}; }

	// Automaton representing the left side concatenated over epsilon transitions.
	Nfa concatenated_lhs{*(*lhs_aut_begin)};
	for (auto next_lhs_aut_it{lhs_aut_begin + 1}; next_lhs_aut_it != lhs_aut_end; ++next_lhs_aut_it) {
		concatenated_lhs = concatenate(concatenated_lhs, *(*next_lhs_aut_it), true);
	}

	Nfa product_pres_eps_trans{intersection(concatenated_lhs, rhs_automaton)};
	product_pres_eps_trans.trim();
	// After `trim()` every state lies on an accepting path, so the language is empty iff nothing is left.
	if (product_pres_eps_trans.initial.empty()) { return {}; }
	if (!reduce_value.empty()) {
		if (reduce_value == "forward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = reduce(product_pres_eps_trans);
		}
		if (reduce_value == "backward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = revert(product_pres_eps_trans);
			product_pres_eps_trans = reduce(product_pres_eps_trans);
			product_pres_eps_trans = revert(product_pres_eps_trans);
		}
	}
	return noodlify(product_pres_eps_trans, mata::nfa::EPSILON, include_empty);
}

namespace {
/// The product of both sides of an equation whose noodles noodlify_for_equation() computes; std::nullopt if empty.
std::optional<Nfa> equation_product(
	const std::vector<std::shared_ptr<Nfa>>& lhs_automata,
	const std::vector<std::shared_ptr<Nfa>>& rhs_automata,
	const mata::nfa::ParameterMap& params
) {
	if (lhs_automata.empty() || rhs_automata.empty()) { return std::nullopt; }

	std::unordered_set<std::shared_ptr<Nfa>> unified_nfas; // Unify each automaton only once.
	unify_initial_and_final_states(lhs_automata, unified_nfas);
	unify_initial_and_final_states(rhs_automata, unified_nfas);

	// Automata representing the left/right side concatenated over different epsilon transitions.
	Nfa concatenated_lhs = concatenate_with(lhs_automata, mata::nfa::EPSILON);
	Nfa concatenated_rhs = concatenate_with(rhs_automata, mata::nfa::EPSILON - 1);

	Nfa product_pres_eps_trans{intersection(concatenated_lhs, concatenated_rhs, mata::nfa::EPSILON - 1)};
	product_pres_eps_trans.trim();

	// After `trim()` every state lies on an accepting path, so the language is empty iff nothing is left.
	if (product_pres_eps_trans.initial.empty()) { return std::nullopt; }
	if (mata::utils::haskey(params, "reduce")) {
		const std::string& reduce_value = params.at("reduce");
		if (reduce_value == "forward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = reduce(product_pres_eps_trans);
		}
		if (reduce_value == "backward" || reduce_value == "bidirectional") {
			product_pres_eps_trans = revert(product_pres_eps_trans);
			product_pres_eps_trans = reduce(product_pres_eps_trans);
			product_pres_eps_trans = revert(product_pres_eps_trans);
		}
	}
	return product_pres_eps_trans;
}
} // namespace

std::vector<seg_nfa::NoodleWithEpsilonsCounter> seg_nfa::noodlify_for_equation(
	const std::vector<std::shared_ptr<Nfa>>& lhs_automata,
	const std::vector<std::shared_ptr<Nfa>>& rhs_automata,
	bool include_empty,
	const ParameterMap& params
) {
	const std::optional<Nfa> product{equation_product(lhs_automata, rhs_automata, params)};
	if (!product) { return {}; }
	return noodlify_mult_eps(*product, {mata::nfa::EPSILON, mata::nfa::EPSILON - 1}, include_empty);
}

#ifdef __cpp_lib_generator
std::generator<seg_nfa::NoodleWithEpsilonsCounter> seg_nfa::noodlify_for_equation_lazy(
	const std::vector<std::shared_ptr<Nfa>>& lhs_automata,
	const std::vector<std::shared_ptr<Nfa>>& rhs_automata,
	bool include_empty,
	const ParameterMap& params,
	NoodleOrder order
) {
	std::optional<Nfa> product{equation_product(lhs_automata, rhs_automata, params)};
	if (!product) { return no_noodles<NoodleWithEpsilonsCounter>(); }
	return noodlify_mult_eps_lazy(
		std::move(*product), {mata::nfa::EPSILON, mata::nfa::EPSILON - 1}, include_empty, order
	);
}
#endif

seg_nfa::VisitedEpsilonsCounterVector seg_nfa::process_eps_map(const VisitedEpsilonsCounterMap& eps_cnt) {
	VisitedEpsilonsCounterVector ret;
	for (auto count : std::ranges::reverse_view(eps_cnt) | std::views::values) { ret.push_back(count); }
	return ret;
}

namespace mata::applications::strings::seg_nfa {
namespace {
// delimiters, we cannot use mata::nfa::EPSILON, because that is normal mata::nfa::EPSILON which can be used in nft
// (non-preserving lengths nfts are allowed) and mata::nfa::EPSILON-1 is DONT_CARE
constexpr Symbol input_delimiter{nfa::EPSILON - 2};
constexpr Symbol output_delimiter{nfa::EPSILON - 3};

/// The segment automaton whose noodles noodlify_for_transducer() turns into transducer noodles; std::nullopt if its
///  language is empty.
std::optional<Nfa> transducer_product(
	const std::shared_ptr<Nft>& nft,
	const std::vector<std::shared_ptr<Nfa>>& input_automata,
	const std::vector<std::shared_ptr<Nfa>>& output_automata,
	bool reduce_intersection,
	bool use_homomorphic_heuristic
) {
	if (input_automata.empty() || output_automata.empty()) { return {}; }

	// to have fewer noodles, we try to have one initial and one final state for each input/output automaton
	std::unordered_set<std::shared_ptr<Nfa>> unified_nfas;
	unify_initial_and_final_states(input_automata, unified_nfas);
	unify_initial_and_final_states(output_automata, unified_nfas);

	// concatenate input and output automata to one input/output automaton connected with
	// INPUT_DELIMITER/OUTPUT_DELIMITER
	Nfa concatenated_input = concatenate_with(input_automata, input_delimiter);
	Nfa concatenated_output = concatenate_with(output_automata, output_delimiter);

	// we will work with nfts, so we just transfer nfas to nfts
	const Nft concatenated_input_nft(std::move(concatenated_input));
	Nft concatenated_output_nft(std::move(concatenated_output));

	auto add_self_loop_for_every_default_state = [](Nft& nft, const Symbol symbol) {
		const Word sym_word(nft.levels.num_of_levels, symbol);

		const size_t original_num_of_states = nft.num_of_states();
		for (State s{0}; s < original_num_of_states; s++) {
			if (nft.levels[s] == 0) { nft.insert_word(s, sym_word, s); }
		}
	};

	// We will now construct a concatenation of nft with the input automaton on the input track
	// and then continue by intersecting with the output automaton on the output track.
	// We want to keep the delimiter in the concatenation in such a way, that if it is there,
	// then it is on both tracks of intersection together. We therefore add self loops with
	// INPUT_DELIMITER/INPUT_DELIMITER or OUTPUT_DELIMITER/OUTPUT_DELIMITER for each state
	// of the transducer, so that intersection works correctly (i.e. the delimiters behave
	// as epsilon transitions).

	Nft intersection;

	if (use_homomorphic_heuristic && is_nft_homomorphic(nft)) {
		// For the case that input nft T is homomorphic, i.e., for each two words u,v, we have T(u.v) = T(u).T(v),
		// we can do the following optimization. Let I1, ..., In be the input automata. Because T is homomorphic,
		// we can take the concatenation T(I1).T(I2)...T(In) connected with INPUT_DELIMITER instead of computing
		// the composition with concatenated_input_nft.
		mata::nft::Nft concatenation{
			mata::nft::compose(mata::nft::Nft(*input_automata[0]), *nft, 0, 0, false, mata::nft::JumpMode::NoJump)
		};
		for (size_t i = 1; i < input_automata.size(); ++i) {
			const Nft composition =
				mata::nft::compose(mata::nft::Nft(*input_automata[i]), *nft, 0, 0, false, mata::nft::JumpMode::NoJump);
			concatenation = mata::nft::algorithms::concatenate_eps(concatenation, composition, input_delimiter, true);
		}
		intersection = std::move(concatenation);
	} else {
		intersection = *nft;
		// we intersect input nfa with nft on the input track, but we need to add INPUT_DELIMITER as an "epsilon
		// transition" of nft
		add_self_loop_for_every_default_state(intersection, input_delimiter);

		intersection =
			mata::nft::compose(concatenated_input_nft, intersection, 0, 0, false, mata::nft::JumpMode::NoJump);
	}
	intersection.trim();

	if (intersection.final.empty()) { return {}; }

	// We intersect output nfa with nft on the output track, but we need to add OUTPUT_DELIMITER as an
	//  "epsilon transition" of nft.
	// We also need to INPUT_DELIMITER as "epsilon transition" of the output nfa, so that we do not lose it.
	add_self_loop_for_every_default_state(concatenated_output_nft, input_delimiter);
	add_self_loop_for_every_default_state(intersection, output_delimiter);
	intersection = mata::nft::compose(concatenated_output_nft, intersection, 0, 1, false, mata::nft::JumpMode::NoJump);
	intersection.trim();

	if (intersection.final.empty()) { return {}; }

	// we assume that the operations did not add jump transitions
	MATA_ASSERT(!intersection.contains_jump_transitions());

	if (reduce_intersection) {
		Nft without_epsilon{mata::nft::remove_epsilon(intersection)};
		without_epsilon.trim();
		intersection = mata::nft::reduce(without_epsilon);
		intersection.trim();
	}

	// Delimiters are always on both tracks together, but we want it to become
	// a jump transition, so that noodlify_mult_eps works correctly.
	// To be more precise, in the nfa representation of the transducer, we will
	// have transitions of the form
	//     source ---DELIMITER---> middle ---DELIMITER---> target
	// where source and target are level 0 states (input track) and middle
	// will be a level 1 state (output track).
	std::map<State, std::vector<Transition>> middle_state_to_delimiter_transition_as_target;
	// maps middle state to each "source ---DELIMITER---> middle" transition
	std::map<State, std::vector<Transition>> middle_state_to_delimiter_transition_as_source;
	// maps middle state to each "middle ---DELIMITER---> target" transition
	for (const Transition& trans : intersection.delta.transitions()) {
		if (trans.symbol == input_delimiter || trans.symbol == output_delimiter) {
			if (intersection.levels[trans.source] == nft::DEFAULT_LEVEL) {
				// source ---DELIMITER---> middle
				middle_state_to_delimiter_transition_as_target[trans.target].push_back(trans);
			} else {
				// middle ---DELIMITER---> target
				middle_state_to_delimiter_transition_as_source[trans.source].push_back(trans);
			}
		}
	}

	// we now take the nfa representation and remove all the transitions
	//     source ---DELIMITER---> middle
	//     middle ---DELIMITER---> target
	// and replace it with one transition
	//     source ---DELIMITER---> target
	Nfa intersection_nfa{intersection.to_nfa_move()};
	// add "source ---DELIMITER---> target" transitions
	for (const auto& [middle_state, first_transitions] : middle_state_to_delimiter_transition_as_target) {
		const std::vector<Transition>& second_transitions =
			middle_state_to_delimiter_transition_as_source.at(middle_state);
		for (const Transition& first_transition : first_transitions) {
			for (const Transition& second_transition : second_transitions) {
				MATA_ASSERT(first_transition.symbol == second_transition.symbol);
				intersection_nfa.delta.add(first_transition.source, first_transition.symbol, second_transition.target);
			}
		}
	}
	// remove "source ---DELIMITER---> middle" transitions
	for (const auto& first_transitions : middle_state_to_delimiter_transition_as_target | std::views::values) {
		for (const Transition& first_transition : first_transitions) {
			intersection_nfa.delta.remove(first_transition);
		}
	}
	// remove "middle ---DELIMITER---> target" transitions
	for (const auto& second_transitions : middle_state_to_delimiter_transition_as_source | std::views::values) {
		for (const Transition& second_transition : second_transitions) {
			intersection_nfa.delta.remove(second_transition);
		}
	}

	// intersection_nfa should now be an NFA that has NFT segments, where segments are divided by
	// delimiters. We would have something like
	//    NFT1  ----possibly multiple transitions with the same delimiter symbols---> NFT2 --->....
	// We can therefore use noodlify_mult_eps, to get noodles, where each NFTi is connected with the
	// next one by one selected delimiter transition. Furthermore, we have only the nfa representation
	// NFAi of NFTi, therefore, we need to add the levels. That is easy because we assume each NFTi
	// does not contain long jumps, therefore every other state of NFAi should have the same level.
	// That is, it should be either 0 or 1, starting with 0 for initials.
	return intersection_nfa;
}

/// Turns noodles of transducer_product() into transducer noodles. The transducer and the projections of a segment are
///  built once while any of them is held, so the noodles that are alive at once share them.
class TransducerElements {
  public:
	TransducerNoodle make(const NoodleWithEpsilonsCounter& noodle) {
		TransducerNoodle result;
		result.reserve(noodle.size());
		for (const auto& [segment, indices] : noodle) {
			// indices keep the index of the input/output automaton the segment is connected with
			std::weak_ptr<Element>& cached{elements_[{segment.automaton.get(), segment.initial}]};
			std::shared_ptr<Element> element{cached.lock()};
			if (element == nullptr) {
				// We need to create NFTi, therefore we add levels to NFAi by simple DFS which adds to each state
				// the level opposite of the level of the previous state.
				Nft transducer{nft::builder::from_nfa_with_levels_advancing(segment.to_nfa(), 2)};
				// the languages of the input/output automata are the projections to the input/output track
				Nfa input_aut{nfa::reduce(nfa::remove_epsilon(nft::project_to(transducer, 0).to_nfa_move()))};
				Nfa output_aut{nfa::reduce(nfa::remove_epsilon(nft::project_to(transducer, 1).to_nfa_move()))};
				element = std::make_shared<Element>(std::move(transducer), std::move(input_aut), std::move(output_aut));
				cached = element;
			}
			// The three share the lifetime of `element`, which `cached` observes.
			result.emplace_back(
				std::shared_ptr<Nft>{element, &element->transducer}, std::shared_ptr<Nfa>{element, &element->input_aut},
				indices[0], std::shared_ptr<Nfa>{element, &element->output_aut}, indices[1]
			);
		}
		return result;
	}

  private:
	struct Element {
		Nft transducer;
		Nfa input_aut;
		Nfa output_aut;
	};
	/// Segments of different pairs may share their automaton and state, and with them the element.
	std::map<std::pair<const Nfa*, State>, std::weak_ptr<Element>> elements_{};
};

#ifdef __cpp_lib_generator
std::generator<TransducerNoodle> transducer_noodles(Nfa product, const NoodleOrder order) {
	NoodleEnumerator noodles{product, {input_delimiter, output_delimiter}, false, order};
	TransducerElements elements{};
	while (auto noodle{noodles.next()}) { co_yield elements.make(*noodle); }
}
#endif
} // namespace

std::vector<TransducerNoodle> noodlify_for_transducer(
	const std::shared_ptr<Nft>& nft,
	const std::vector<std::shared_ptr<Nfa>>& input_automata,
	const std::vector<std::shared_ptr<Nfa>>& output_automata,
	bool reduce_intersection,
	bool use_homomorphic_heuristic
) {
	const std::optional<Nfa> product{
		transducer_product(nft, input_automata, output_automata, reduce_intersection, use_homomorphic_heuristic)
	};
	if (!product) { return {}; }
	NoodleEnumerator noodles{*product, {input_delimiter, output_delimiter}, false, NoodleOrder{}};
	TransducerElements elements{};
	std::vector<TransducerNoodle> result;
	while (auto noodle{noodles.next()}) { result.push_back(elements.make(*noodle)); }
	return result;
}

#ifdef __cpp_lib_generator
std::generator<TransducerNoodle> noodlify_for_transducer_lazy(
	const std::shared_ptr<Nft>& nft,
	const std::vector<std::shared_ptr<Nfa>>& input_automata,
	const std::vector<std::shared_ptr<Nfa>>& output_automata,
	bool reduce_intersection,
	bool use_homomorphic_heuristic,
	NoodleOrder order
) {
	std::optional<Nfa> product{
		transducer_product(nft, input_automata, output_automata, reduce_intersection, use_homomorphic_heuristic)
	};
	if (!product) { return no_noodles<TransducerNoodle>(); }
	return transducer_noodles(std::move(*product), order);
}
#endif
} // namespace mata::applications::strings::seg_nfa
