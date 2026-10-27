/** @file
 * @brief nfa-noodlification.cc -- Noodlification of NFAs
 */

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

std::vector<seg_nfa::NoodleWithEpsilonsCounter>
	seg_nfa::noodlify_mult_eps(const SegNfa& aut, const std::set<Symbol>& epsilons, bool include_empty) {
	Segmentation segmentation{aut, epsilons};
	const auto& segments{segmentation.get_untrimmed_segments()};

	VisitedEpsilonsCounterMap def_eps_map;
	for (const Symbol& eps : epsilons) { def_eps_map[eps] = 0; }
	VisitedEpsilonsCounterVector def_eps_vector = process_eps_map(def_eps_map);

	if (segments.size() == 1) {
		if (auto segment{make_whole_segment(trim(segments[0]), include_empty)}) {
			return {{{std::move(*segment), def_eps_vector}}};
		}
		return {};
	}

	auto unused_state{static_cast<State>(aut.num_of_states())}; // get some State not used in aut
	std::map<std::pair<State, State>, NoodleSegment> segments_one_initial_final;
	segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

	using SegmentKey = std::pair<State, State>;
	struct SegItem {
		NoodleWithEpsilonsCounter noodle{};
		/// The (initial, final) pair of every segment of `noodle`.
		std::vector<SegmentKey> keys{};
		State fin{};
		size_t seg_id{};
	};

	std::vector<NoodleWithEpsilonsCounter> noodles{};
	std::vector<std::vector<SegmentKey>> noodle_keys{};
	// Duplicates do occur: a segment whose language is {epsilon} is skipped, and two epsilon transitions with
	//  different epsilon symbols between the same pair of states finish the same noodle twice. Comparing every
	//  finished noodle with all noodles found so far is quadratic in their number, and an ordered set of the
	//  noodles themselves still compares whole noodles log-many times per insertion and keeps a second copy of
	//  every noodle. The finished noodles are therefore indexed by a hash over the pairs of their segments, which
	//  also fix the epsilon counters, and the set holds only positions in `noodles`. Segments of different pairs
	//  may share their automaton and state, so the pairs, not the segments, tell the noodles apart.
	const auto hash_noodle = [&](const size_t index) {
		size_t accum{utils::hash_combine(utils::HASH_SEED, noodle_keys[index].size())};
		for (const auto& [init, fin] : noodle_keys[index]) {
			accum = utils::hash_combine(accum, init);
			accum = utils::hash_combine(accum, fin);
		}
		return accum;
	};
	const auto same_noodle = [&](const size_t lhs, const size_t rhs) { return noodle_keys[lhs] == noodle_keys[rhs]; };
	std::unordered_set<size_t, decltype(hash_noodle), decltype(same_noodle)> seen_noodles{0, hash_noodle, same_noodle};
	std::deque<SegItem> lifo;

	for (const State& fn : segments[0].final) {
		SegItem new_item;
		const auto seg{segments_one_initial_final.find({unused_state, fn})};
		if (seg == segments_one_initial_final.end()) { continue; }
		if (!seg->second.is_epsilon_only()) {
			new_item.noodle.emplace_back(seg->second, def_eps_vector);
			new_item.keys.push_back(seg->first);
		}
		new_item.seg_id = 0;
		new_item.fin = fn;
		lifo.push_back(new_item);
	}
	/// Number of visited epsilons for each state.
	auto visited_eps = segmentation.get_visited_eps();

	// Position of every final state in the final states of its segment. The final states of the inner segments are
	//  disjoint, as every state lies at a single depth; the auxiliary final state of the last segment gets position 0.
	std::vector<size_t> final_position(unused_state + 1, 0);
	for (size_t seg_id{1}; seg_id + 1 < segments.size(); ++seg_id) {
		size_t position{0};
		for (const State fin : segments[seg_id].final) { final_position[fin] = position++; }
	}
	using SegmentsIter = decltype(segments_one_initial_final)::const_iterator;
	std::vector<SegmentsIter> next_segments{};

	while (!lifo.empty()) {
		SegItem item = std::move(lifo.front());
		lifo.pop_front();

		if (item.seg_id + 1 == segments.size()) {
			// Appended in completion order, so the output order is the one the quadratic scan produced. The
			//  noodle has to live in `noodles` before it can be hashed, so a duplicate is popped again.
			noodles.push_back(std::move(item.noodle));
			noodle_keys.push_back(std::move(item.keys));
			if (!seen_noodles.insert(noodles.size() - 1).second) {
				noodles.pop_back();
				noodle_keys.pop_back();
			}
			continue;
		}

		for (const Transition& tr : segmentation.get_epsilon_transitions(item.fin)) {
			// The pairs of one initial state are contiguous in the ordered map, so one scan finds exactly the final
			//  states of the next segment connected to tr.target instead of probing every final state.
			next_segments.clear();
			for (auto seg_iter{segments_one_initial_final.lower_bound({tr.target, 0})};
				 seg_iter != segments_one_initial_final.end() && seg_iter->first.first == tr.target; ++seg_iter) {
				MATA_ASSERT(
					item.seg_id + 2 == segments.size()
						? seg_iter->first.second == unused_state
						: segments[item.seg_id + 1].final.contains(seg_iter->first.second)
				);
				next_segments.push_back(seg_iter);
			}
			// Visit them in the order of the final states of the segment, which fixes the order of the noodles.
			std::ranges::sort(next_segments, {}, [&](const SegmentsIter seg_iter) {
				return final_position[seg_iter->first.second];
			});

			for (const SegmentsIter seg_iter : next_segments) {
				SegItem new_item = item; // deep copy
				new_item.seg_id++;
				// do not include segments with trivial epsilon language
				if (!seg_iter->second.is_epsilon_only()) {
					new_item.noodle.emplace_back(seg_iter->second, process_eps_map(visited_eps[tr.target]));
					new_item.keys.push_back(seg_iter->first);
				}
				new_item.fin = seg_iter->first.second;
				lifo.push_back(new_item);
			}
		}
	}
	return noodles;
}

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

std::vector<seg_nfa::NoodleWithEpsilonsCounter> seg_nfa::noodlify_for_equation(
	const std::vector<std::shared_ptr<Nfa>>& lhs_automata,
	const std::vector<std::shared_ptr<Nfa>>& rhs_automata,
	bool include_empty,
	const ParameterMap& params
) {
	if (lhs_automata.empty() || rhs_automata.empty()) { return {}; }

	std::unordered_set<std::shared_ptr<Nfa>> unified_nfas; // Unify each automaton only once.
	unify_initial_and_final_states(lhs_automata, unified_nfas);
	unify_initial_and_final_states(rhs_automata, unified_nfas);

	// Automata representing the left/right side concatenated over different epsilon transitions.
	Nfa concatenated_lhs = concatenate_with(lhs_automata, mata::nfa::EPSILON);
	Nfa concatenated_rhs = concatenate_with(rhs_automata, mata::nfa::EPSILON - 1);

	Nfa product_pres_eps_trans{intersection(concatenated_lhs, concatenated_rhs, mata::nfa::EPSILON - 1)};
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
	return noodlify_mult_eps(product_pres_eps_trans, {mata::nfa::EPSILON, mata::nfa::EPSILON - 1}, include_empty);
}

seg_nfa::VisitedEpsilonsCounterVector seg_nfa::process_eps_map(const VisitedEpsilonsCounterMap& eps_cnt) {
	VisitedEpsilonsCounterVector ret;
	for (auto count : std::ranges::reverse_view(eps_cnt) | std::views::values) { ret.push_back(count); }
	return ret;
}

std::vector<seg_nfa::TransducerNoodle> seg_nfa::noodlify_for_transducer(
	const std::shared_ptr<Nft>& nft,
	const std::vector<std::shared_ptr<Nfa>>& input_automata,
	const std::vector<std::shared_ptr<Nfa>>& output_automata,
	bool reduce_intersection,
	bool use_homomorphic_heuristic
) {
	if (input_automata.empty() || output_automata.empty()) { return {}; }

	// delimiters, we cannot use mata::nfa::EPSILON, because that is normal mata::nfa::EPSILON which can be used in nft
	// (non-preserving lengths nfts are allowed) and mata::nfa::EPSILON-1 is DONT_CARE
	constexpr Symbol input_delimiter{nfa::EPSILON - 2};
	constexpr Symbol output_delimiter{nfa::EPSILON - 3};

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

	std::vector<TransducerNoodle> result;
	// we create for each segment NFAi only one NFTi and keep it here; segments of different pairs may share one
	std::map<std::pair<const Nfa*, State>, TransducerNoodleElement> seg_nfa_to_transducer_el;
	for (const auto& noodle : noodlify_mult_eps(intersection_nfa, {input_delimiter, output_delimiter}, false)) {
		TransducerNoodle new_noodle;
		for (const auto& [element_segment, element_indices] : noodle) {
			// element.first is NFAi
			const std::pair<const Nfa*, State> element_key{element_segment.automaton.get(), element_segment.initial};

			// element.second then keeps the index representing which input/output automaton it is connected with

			if (const auto cached{seg_nfa_to_transducer_el.find(element_key)};
				cached != seg_nfa_to_transducer_el.end()) {
				// we already processed this NFAi so we can find NFTi in seg_nfa_to_transducer_el
				TransducerNoodleElement transd_el = cached->second;
				// however, we need to update the indexes of input/output automaton
				transd_el.input_index = element_indices[0];
				transd_el.output_index = element_indices[1];
				new_noodle.push_back(transd_el);
				continue;
			}

			// We need to create NFTi, therefore we add levels to NFAi by simple DFS which adds to each state
			// the level opposite of the level of the previous state.
			std::shared_ptr<Nft> element_nft =
				std::make_shared<Nft>(nft::builder::from_nfa_with_levels_advancing(element_segment.to_nfa(), 2));

			TransducerNoodleElement transd_el{
				element_nft,
				// the language of the input automaton is the projection to input track
				std::make_shared<Nfa>(nfa::reduce(nfa::remove_epsilon(nft::project_to(*element_nft, 0).to_nfa_move()))),
				element_indices[0],
				// the language of the output automaton is the projection to output track
				std::make_shared<Nfa>(nfa::reduce(nfa::remove_epsilon(nft::project_to(*element_nft, 1).to_nfa_move()))),
				element_indices[1]
			};
			seg_nfa_to_transducer_el.insert({element_key, transd_el});
			new_noodle.push_back(transd_el);
		}
		result.push_back(new_noodle);
	}
	return result;
}
