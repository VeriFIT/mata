/* nfa-noodlification.cc -- Noodlification of NFAs
 */

#include "mata/utils/utils.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/strings.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::nfa;
using namespace mata::strings;
using namespace mata::nfa::algorithms;

namespace {

/**
 * Get a number of permutations for computed epsilon depths.
 * @param[in] epsilon_depths Computed list of epsilon transitions for each depth.
 * @return Number of permutations.
 */
size_t get_num_of_permutations(const seg_nfa::Segmentation::EpsilonDepthTransitions& epsilon_depths)
{
    size_t num_of_permutations{ 1 };
    for (const auto& segment: epsilon_depths)
    {
        num_of_permutations *= segment.second.size();
    }
    return num_of_permutations;
}

} // namespace

std::vector<seg_nfa::Noodle> seg_nfa::noodlify(const SegNfa& aut, const Symbol epsilon, bool include_empty) {

    const std::set<Symbol> epsilons({epsilon});
    // return noodlify_reach(aut, epsilons, include_empty);

    Segmentation segmentation{ aut, epsilons };
    const auto& segments{ segmentation.get_untrimmed_segments() };

    if (segments.size() == 1) {
        std::shared_ptr<Nfa> segment = std::make_shared<Nfa>(segments[0]);
        segment->trim();
        if (segment->num_of_states() > 0 || include_empty) {
            return {{ segment }};
        } else {
            return {};
        }
    }

    State unused_state = aut.num_of_states(); // get some State not used in aut
    std::map<std::pair<State, State>, std::shared_ptr<Nfa>> segments_one_initial_final;
    segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

    const auto& epsilon_depths{ segmentation.get_epsilon_depths() };

    // Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
    size_t num_of_permutations{ get_num_of_permutations(epsilon_depths) };
    size_t epsilon_depths_size{ epsilon_depths.size() };

    std::vector<Noodle> noodles{};
    // noodle of epsilon transitions (each from different depth)
    std::vector<Transition> epsilon_noodle(epsilon_depths_size);
    // for each combination of ε-transitions, create the automaton.
    // based on https://stackoverflow.com/questions/48270565/create-all-possible-combinations-of-multiple-vectors
    for (size_t index{ 0 }; index < num_of_permutations; ++index) {
        size_t temp{ index };
        for (size_t depth{ 0 }; depth < epsilon_depths_size; ++depth) {
            size_t num_of_trans_at_cur_depth = epsilon_depths.at(depth).size();
            size_t computed_index = temp % num_of_trans_at_cur_depth;
            temp /= num_of_trans_at_cur_depth;
            epsilon_noodle[depth] = epsilon_depths.at(depth)[computed_index];
        }

        Noodle noodle;

        // epsilon_noodle[0] for sure exists, as we sorted out the case of only one segment at the beginning
        auto first_segment_iter = segments_one_initial_final.find(std::make_pair(unused_state, epsilon_noodle[0].source));
        if (first_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(first_segment_iter->second);
        } else {
            continue;
        }

        bool all_segments_exist = true;
        for (auto iter = epsilon_noodle.begin(); iter + 1 != epsilon_noodle.end(); ++iter) {
            auto next_iter = iter + 1;
            auto segment_iter = segments_one_initial_final.find(std::make_pair(iter->target, next_iter->source));
            if (segment_iter != segments_one_initial_final.end()) {
                noodle.push_back(segment_iter->second);
            } else {
                all_segments_exist = false;
                break;
            }
        }

        if (!all_segments_exist) {
            continue;
        }

        auto last_segment_iter = segments_one_initial_final.find(
                std::make_pair(epsilon_noodle.back().target, unused_state));
        if (last_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(last_segment_iter->second);
        } else {
            continue;
        }

        noodles.push_back(noodle);
    }
    return noodles;
}

//todo: is this taking all final times all initial?
// can it be done more efficiently? (only connected combinations through dfs)
void seg_nfa::segs_one_initial_final(
    const std::vector<Nfa>& segments,
    bool include_empty,
    const State& unused_state,
    std::map<std::pair<State, State>, std::shared_ptr<Nfa>>& out) {

    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        if (iter == segments.begin()) { // first segment will always have all initial states in noodles
            for (const State final_state: iter->final) {
                Nfa segment_one_final = *iter;
                segment_one_final.final = {final_state };
                segment_one_final = reduce(segment_one_final.trim());

                if (segment_one_final.num_of_states() > 0 || include_empty) {
                    out[std::make_pair(unused_state, final_state)] = std::make_shared<Nfa>(segment_one_final);
                }
            }
        } else if (iter + 1 == segments.end()) { // last segment will always have all final states in noodles
            for (const State init_state: iter->initial) {
                Nfa segment_one_init = *iter;
                segment_one_init.initial = {init_state };
                segment_one_init = reduce(segment_one_init.trim());

                if (segment_one_init.num_of_states() > 0 || include_empty) {
                    out[std::make_pair(init_state, unused_state)] = std::make_shared<Nfa>(segment_one_init);
                }
            }
        } else { // the segments in-between
            for (const State init_state: iter->initial) {
                for (const State final_state: iter->final) {
                    Nfa segment_one_init_final = *iter;
                    segment_one_init_final.initial = {init_state };
                    segment_one_init_final.final = {final_state };
                    segment_one_init_final = reduce(segment_one_init_final.trim());
                    if (segment_one_init_final.num_of_states() > 0 || include_empty) {
                        out[std::make_pair(init_state, final_state)] = std::make_shared<Nfa>(segment_one_init_final);
                    }
                }
            }
        }
    }
}

std::vector<seg_nfa::NoodleWithEpsilonsCounter> seg_nfa::noodlify_mult_eps(const SegNfa& aut, const std::set<Symbol>& epsilons, bool include_empty) {
    Segmentation segmentation{ aut, epsilons };
    const auto& segments{ segmentation.get_untrimmed_segments() };

    VisitedEpsilonsCounterMap def_eps_map;
    for(const Symbol& eps : epsilons) {
        def_eps_map[eps] = 0;
    }
    VisitedEpsilonsCounterVector def_eps_vector = process_eps_map(def_eps_map);

    if (segments.size() == 1) {
        std::shared_ptr<Nfa> segment = std::make_shared<Nfa>(segments[0]);
        segment->trim();
        if (segment->num_of_states() > 0 || include_empty) {
            return {{ {segment, def_eps_vector} } };
        } else {
            return {};
        }
    }

    State unused_state = aut.num_of_states(); // get some State not used in aut
    std::map<std::pair<State, State>, std::shared_ptr<Nfa>> segments_one_initial_final;
    segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

    const auto& epsilon_depths_map{ segmentation.get_epsilon_depth_trans_map() };

    struct SegItem {
        NoodleWithEpsilonsCounter noodle{};
        State fin{};
        size_t seg_id{};
    };

    std::vector<NoodleWithEpsilonsCounter> noodles{};
    std::deque<SegItem> lifo;

    for(const State& fn : segments[0].final) {
        SegItem new_item;
        std::shared_ptr<Nfa> seg = segments_one_initial_final[{unused_state, fn}];
        if(seg->final.size() != 1 || seg->delta.num_of_transitions() > 0) { // L(seg_iter) != {epsilon}
            new_item.noodle.emplace_back(seg, def_eps_vector);
        }
        new_item.seg_id = 0;
        new_item.fin = fn;
        lifo.push_back(new_item);
    }
    /// Number of visited epsilons for each state.
    auto visited_eps = segmentation.get_visited_eps();

    while(!lifo.empty()) {
        SegItem item = lifo.front();
        lifo.pop_front();

        if(item.seg_id + 1 == segments.size()) {
            // check if the noodle is already there
            if(!std::any_of(noodles.begin(), noodles.end(),
                [&](NoodleWithEpsilonsCounter &s) {
                    return s == item.noodle;
            } )) {
                noodles.push_back(item.noodle);
            }
            continue;
        }

        for(const Transition& tr : epsilon_depths_map.at(item.seg_id).at(item.fin)) {
            //TODO: is the use of SparseSet here good? It may take a lot of space. Do you need constant test? Otherwise what about StateSet?
            mata::utils::SparseSet<mata::nfa::State> fins = segments[item.seg_id + 1].final; // final states of the segment
            if(item.seg_id + 1 == segments.size() - 1) { // last segment
                fins = mata::utils::SparseSet<mata::nfa::State>({ unused_state});
            }

            for(const State& fn : fins) {
                auto seg_iter = segments_one_initial_final.find({ tr.target, fn});
                if(seg_iter == segments_one_initial_final.end())
                    continue;

                SegItem new_item = item; // deep copy
                new_item.seg_id++;
                // do not include segmets with trivial epsilon language
                if(seg_iter->second->final.size() != 1 || seg_iter->second->delta.num_of_transitions() > 0) { // L(seg_iter) != {epsilon}
                    new_item.noodle.emplace_back(seg_iter->second, process_eps_map(visited_eps[tr.target]));
                }
                new_item.fin = fn;
                lifo.push_back(new_item);
            }
        }
    }
    return noodles;
}

std::vector<seg_nfa::Noodle> seg_nfa::noodlify_for_equation(
    const std::vector<std::reference_wrapper<Nfa>>& lhs_automata, const Nfa& rhs_automaton,
    bool include_empty, const ParameterMap& params) {
    const auto lhs_aut_begin{ lhs_automata.begin() };
    const auto lhs_aut_end{ lhs_automata.end() };
    for (auto lhs_aut_iter{ lhs_aut_begin }; lhs_aut_iter != lhs_aut_end;
         ++lhs_aut_iter) {
        (*lhs_aut_iter).get().unify_initial();
        (*lhs_aut_iter).get().unify_final();
    }

    if (lhs_automata.empty() || rhs_automaton.is_lang_empty()) { return {}; }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa concatenated_lhs{ *lhs_aut_begin };
    for (auto next_lhs_aut_it{ lhs_aut_begin + 1 }; next_lhs_aut_it != lhs_aut_end;
         ++next_lhs_aut_it) {
        concatenated_lhs = concatenate(concatenated_lhs, *next_lhs_aut_it, EPSILON);
    }

    auto product_pres_eps_trans{
            intersection(concatenated_lhs, rhs_automaton).trim() };
    if (product_pres_eps_trans.is_lang_empty()) {
        return {};
    }
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
    return noodlify(product_pres_eps_trans, EPSILON, include_empty);
}

std::vector<seg_nfa::Noodle> seg_nfa::noodlify_for_equation(
    const std::vector<Nfa*>& lhs_automata, const Nfa& rhs_automaton, bool include_empty,
    const ParameterMap& params) {
    const auto lhs_aut_begin{ lhs_automata.begin() };
    const auto lhs_aut_end{ lhs_automata.end() };

    std::string reduce_value{};
    if (utils::haskey(params, "reduce")) {
        reduce_value = params.at("reduce");
    }

    if (!reduce_value.empty()) {
        if (reduce_value == "forward" || reduce_value == "backward" || reduce_value == "bidirectional") {
            for (auto lhs_aut_iter{ lhs_aut_begin }; lhs_aut_iter != lhs_aut_end;
                 ++lhs_aut_iter) {
                (*lhs_aut_iter)->unify_initial();
                (*lhs_aut_iter)->unify_final();
            }
        }
    }

    if (lhs_automata.empty() || rhs_automaton.is_lang_empty()) { return {}; }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa concatenated_lhs{ *(*lhs_aut_begin) };
    for (auto next_lhs_aut_it{ lhs_aut_begin + 1 }; next_lhs_aut_it != lhs_aut_end;
         ++next_lhs_aut_it) {
        concatenated_lhs = concatenate(concatenated_lhs, *(*next_lhs_aut_it), EPSILON);
    }

    auto product_pres_eps_trans{
            intersection(concatenated_lhs, rhs_automaton).trim() };
    if (product_pres_eps_trans.is_lang_empty()) {
        return {};
    }
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
    return noodlify(product_pres_eps_trans, EPSILON, include_empty);
}


std::vector<seg_nfa::NoodleWithEpsilonsCounter> seg_nfa::noodlify_for_equation(
    const std::vector<std::shared_ptr<Nfa>>& lhs_automata,
    const std::vector<std::shared_ptr<Nfa>>& rhs_automata, bool include_empty, const ParameterMap& params) {
    if (lhs_automata.empty() || rhs_automata.empty()) { return {}; }

    const auto lhs_aut_begin{ lhs_automata.begin() };
    const auto lhs_aut_end{ lhs_automata.end() };
    const auto rhs_aut_begin{ rhs_automata.begin() };
    const auto rhs_aut_end{ rhs_automata.end() };

    std::unordered_set<std::shared_ptr<Nfa>> unified_nfas; // Unify each automaton only once.
    for (auto lhs_aut_iter{ lhs_aut_begin }; lhs_aut_iter != lhs_aut_end; ++lhs_aut_iter) {
        if (unified_nfas.find(*lhs_aut_iter) == unified_nfas.end()) {
            lhs_aut_iter->get()->unify_initial();
            lhs_aut_iter->get()->unify_final();
            unified_nfas.insert(*lhs_aut_iter);
        }
    }
    for (auto rhs_aut_iter{ rhs_aut_begin }; rhs_aut_iter != rhs_aut_end; ++rhs_aut_iter) {
        if (unified_nfas.find(*rhs_aut_iter) == unified_nfas.end()) {
            rhs_aut_iter->get()->unify_initial();
            rhs_aut_iter->get()->unify_final();
            unified_nfas.insert(*rhs_aut_iter);
        }
    }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa concatenated_lhs{ **lhs_aut_begin };
    for (auto next_lhs_aut_it{ lhs_aut_begin + 1 }; next_lhs_aut_it != lhs_aut_end;
         ++next_lhs_aut_it) {
        concatenated_lhs = concatenate_eps(concatenated_lhs, **next_lhs_aut_it, EPSILON, true);
    }
    Nfa concatenated_rhs{ **rhs_aut_begin };
    for (auto next_rhs_aut_it{ rhs_aut_begin + 1 }; next_rhs_aut_it != rhs_aut_end;
         ++next_rhs_aut_it) {
        concatenated_rhs = concatenate_eps(concatenated_rhs, **next_rhs_aut_it, EPSILON-1, true); // we use EPSILON-1
    }

    auto product_pres_eps_trans{
            intersection(concatenated_lhs, concatenated_rhs, EPSILON-1).trim() };

    if (product_pres_eps_trans.is_lang_empty()) {
        return {};
    }
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
    return noodlify_mult_eps(product_pres_eps_trans, { EPSILON, EPSILON-1 }, include_empty);
}

seg_nfa::VisitedEpsilonsCounterVector seg_nfa::process_eps_map(const VisitedEpsilonsCounterMap& eps_cnt) {
    VisitedEpsilonsCounterVector ret;
    for (auto it = eps_cnt.rbegin(); it != eps_cnt.rend(); ++it) {
        ret.push_back(it->second);
    }
    return ret;
}
