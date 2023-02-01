/* nfa-noodlification.cc -- Noodlification of NFAs
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mata/nfa.hh"
#include "mata/nfa-strings.hh"
#include "mata/util.hh"
#include <mata/nfa-algorithms.hh>

using namespace Mata::Nfa;
using namespace Mata::Strings;
using namespace Mata::Nfa::Algorithms;

namespace {

/**
 * Get a number of permutations for computed epsilon depths.
 * @param[in] epsilon_depths Computed list of epsilon transitions for each depth.
 * @return Number of permutations.
 */
size_t get_num_of_permutations(const SegNfa::Segmentation::EpsilonDepthTransitions& epsilon_depths)
{
    size_t num_of_permutations{ 1 };
    for (const auto& segment: epsilon_depths)
    {
        num_of_permutations *= segment.second.size();
    }
    return num_of_permutations;
}

} // namespace

SegNfa::NoodleSequence SegNfa::noodlify(const SegNfa& aut, const Symbol epsilon, bool include_empty) {

    const std::set<Symbol> epsilons({epsilon});
    // return noodlify_reach(aut, epsilons, include_empty);

    Segmentation segmentation{ aut, epsilons };
    const auto& segments{ segmentation.get_untrimmed_segments() };

    if (segments.size() == 1) {
        std::shared_ptr<Nfa::Nfa> segment = std::make_shared<Nfa::Nfa>(segments[0]);
        segment->trim();
        if (segment->size() > 0 || include_empty) {
            return {{ segment }};
        } else {
            return {};
        }
    }

    State unused_state = aut.size(); // get some State not used in aut
    std::map<std::pair<State, State>, std::shared_ptr<Nfa::Nfa>> segments_one_initial_final;
    segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

    const auto& epsilon_depths{ segmentation.get_epsilon_depths() };

    // Compute number of all combinations of ε-transitions with one ε-transitions from each depth.
    size_t num_of_permutations{ get_num_of_permutations(epsilon_depths) };
    size_t epsilon_depths_size{ epsilon_depths.size() };

    NoodleSequence noodles{};
    // noodle of epsilon transitions (each from different depth)
    TransSequence epsilon_noodle(epsilon_depths_size);
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
        auto first_segment_iter = segments_one_initial_final.find(std::make_pair(unused_state, epsilon_noodle[0].src));
        if (first_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(first_segment_iter->second);
        } else {
            continue;
        }

        bool all_segments_exist = true;
        for (auto iter = epsilon_noodle.begin(); iter + 1 != epsilon_noodle.end(); ++iter) {
            auto next_iter = iter + 1;
            auto segment_iter = segments_one_initial_final.find(std::make_pair(iter->tgt, next_iter->src));
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
                std::make_pair(epsilon_noodle.back().tgt, unused_state));
        if (last_segment_iter != segments_one_initial_final.end()) {
            noodle.push_back(last_segment_iter->second);
        } else {
            continue;
        }

        noodles.push_back(noodle);
    }
    return noodles;
}

void SegNfa::segs_one_initial_final(
    const Mata::Nfa::AutSequence& segments, 
    bool include_empty, 
    const State& unused_state, 
    std::map<std::pair<State, State>, std::shared_ptr<Nfa::Nfa>>& out) {

    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        if (iter == segments.begin()) { // first segment will always have all initial states in noodles
            for (const State final_state: iter->final) {
                SharedPtrAut segment_one_final = std::make_shared<Nfa::Nfa>(*iter);
                segment_one_final->final = {final_state };
                /**
                 * TODO: reduce
                 */
                segment_one_final->trim();

                if (segment_one_final->size() > 0 || include_empty) {
                    out[std::make_pair(unused_state, final_state)] = segment_one_final;
                }
            }
        } else if (iter + 1 == segments.end()) { // last segment will always have all final states in noodles
            for (const State init_state: iter->initial) {
                SharedPtrAut segment_one_init = std::make_shared<Nfa::Nfa>(*iter);
                segment_one_init->initial = {init_state };
                segment_one_init->trim();

                if (segment_one_init->size() > 0 || include_empty) {
                    out[std::make_pair(init_state, unused_state)] = segment_one_init;
                }
            }
        } else { // the segments in-between
            for (const State init_state: iter->initial) {
                for (const State final_state: iter->final) {
                    SharedPtrAut segment_one_init_final = std::make_shared<Nfa::Nfa>(*iter);
                    segment_one_init_final->initial = {init_state };
                    segment_one_init_final->final = {final_state };
                    segment_one_init_final->trim();

                    if (segment_one_init_final->size() > 0 || include_empty) {
                        out[std::make_pair(init_state, final_state)] = segment_one_init_final;
                    }
                }
            }
        }
    }
}

SegNfa::NoodleSubstSequence SegNfa::noodlify_mult_eps(const SegNfa& aut, const std::set<Symbol>& epsilons, bool include_empty) {
    Segmentation segmentation{ aut, epsilons };
    const auto& segments{ segmentation.get_untrimmed_segments() };

    EpsCntMap def_eps_map;
    for(const Symbol& eps : epsilons) {
        def_eps_map[eps] = 0;
    }
    EpsCntVector def_eps_vector = process_eps_map(def_eps_map);

    if (segments.size() == 1) {
        std::shared_ptr<Nfa::Nfa> segment = std::make_shared<Nfa::Nfa>(segments[0]);
        segment->trim();
        if (segment->size() > 0 || include_empty) {
            return {{ {segment, def_eps_vector} } };
        } else {
            return {};
        }
    }

    State unused_state = aut.size(); // get some State not used in aut
    std::map<std::pair<State, State>, std::shared_ptr<Nfa::Nfa>> segments_one_initial_final;
    segs_one_initial_final(segments, include_empty, unused_state, segments_one_initial_final);

    const auto& epsilon_depths_map{ segmentation.get_epsilon_depth_trans_map() };

    struct SegItem {
        NoodleSubst noodle;
        State fin;
        size_t seg_id;
    };

    NoodleSubstSequence noodles{};
    std::deque<SegItem> lifo;

    for(const State& fn : segments[0].final) {
        SegItem new_item;
        Mata::Nfa::SharedPtrAut seg = segments_one_initial_final[{unused_state, fn}];
        if(seg->final.size() != 1 || seg->get_num_of_trans() > 0) { // L(seg_iter) != {epsilon}
            new_item.noodle.push_back({seg, def_eps_vector});
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
                [&](NoodleSubst &s) { 
                    return s == item.noodle; 
            } )) {
                noodles.push_back(item.noodle);
            }
            continue;
        }

        for(const Trans& tr : epsilon_depths_map.at(item.seg_id).at(item.fin)) {
            Mata::Util::NumberPredicate<Mata::Nfa::State> fins = segments[item.seg_id+1].final; // final states of the segment
            if(item.seg_id + 1 == segments.size() - 1) { // last segment
                fins = Mata::Util::NumberPredicate<Mata::Nfa::State>({unused_state});
            }

            for(const State& fn : fins) {
                auto seg_iter = segments_one_initial_final.find({tr.tgt, fn});
                if(seg_iter == segments_one_initial_final.end())
                    continue;

                SegItem new_item = item; // deep copy
                new_item.seg_id++;
                // do not include segmets with trivial epsilon language
                if(seg_iter->second->final.size() != 1 || seg_iter->second->get_num_of_trans() > 0) { // L(seg_iter) != {epsilon}
                    new_item.noodle.push_back({seg_iter->second, process_eps_map(visited_eps[tr.tgt])});
                }
                new_item.fin = fn;
                lifo.push_back(new_item);
            }
        }
    }
    return noodles;
}

SegNfa::NoodleSequence SegNfa::noodlify_for_equation(const AutRefSequence& left_automata, const Nfa::Nfa& right_automaton,
                                                     bool include_empty, const StringMap& params) {
    const auto left_automata_begin{ left_automata.begin() };
    const auto left_automata_end{ left_automata.end() };
    for (auto left_aut_iter{ left_automata_begin }; left_aut_iter != left_automata_end;
         ++left_aut_iter) {
        (*left_aut_iter).get().unify_initial();
        (*left_aut_iter).get().unify_final();
    }

    if (left_automata.empty() || is_lang_empty(right_automaton)) { return NoodleSequence{}; }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa::Nfa concatenated_left_side{ *left_automata_begin };
    for (auto next_left_automaton_it{ left_automata_begin + 1 }; next_left_automaton_it != left_automata_end;
         ++next_left_automaton_it) {
        concatenated_left_side = concatenate(concatenated_left_side, *next_left_automaton_it, EPSILON);
    }

    auto product_pres_eps_trans{
            intersection(concatenated_left_side, right_automaton, true) };
    product_pres_eps_trans.trim();
    if (is_lang_empty(product_pres_eps_trans)) {
        return NoodleSequence{};
    }
    if (Util::haskey(params, "reduce")) {
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

SegNfa::NoodleSequence SegNfa::noodlify_for_equation(const AutPtrSequence& left_automata, const Nfa::Nfa& right_automaton,
                                                     bool include_empty, const StringMap& params) {
    const auto left_automata_begin{ left_automata.begin() };
    const auto left_automata_end{ left_automata.end() };

    std::string reduce_value{};
    if (Util::haskey(params, "reduce")) {
        reduce_value = params.at("reduce");
    }

    if (!reduce_value.empty()) {
        if (reduce_value == "forward" || reduce_value == "backward" || reduce_value == "bidirectional") {
            for (auto left_aut_iter{ left_automata_begin }; left_aut_iter != left_automata_end;
                 ++left_aut_iter) {
                (*left_aut_iter)->unify_initial();
                (*left_aut_iter)->unify_final();
            }
        }
    }

    if (left_automata.empty() || is_lang_empty(right_automaton)) { return NoodleSequence{}; }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa::Nfa concatenated_left_side{ *(*left_automata_begin) };
    for (auto next_left_automaton_it{ left_automata_begin + 1 }; next_left_automaton_it != left_automata_end;
         ++next_left_automaton_it) {
        concatenated_left_side = concatenate(concatenated_left_side, *(*next_left_automaton_it), EPSILON);
    }

    auto product_pres_eps_trans{
            intersection(concatenated_left_side, right_automaton, true) };
    product_pres_eps_trans.trim();
    if (is_lang_empty(product_pres_eps_trans)) {
        return NoodleSequence{};
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


SegNfa::NoodleSubstSequence SegNfa::noodlify_for_equation(const std::vector<std::shared_ptr<Nfa::Nfa>>& left_automata, 
    const std::vector<std::shared_ptr<Nfa::Nfa>>& right_automata, bool include_empty, const StringMap& params) {
    const auto left_automata_begin{ left_automata.begin() };
    const auto left_automata_end{ left_automata.end() };
    const auto right_automata_begin{ right_automata.begin() };
    const auto right_automata_end{ right_automata.end() };

    for (auto left_aut_iter{ left_automata_begin }; left_aut_iter != left_automata_end;
         ++left_aut_iter) {
        (*left_aut_iter).get()->unify_initial();
        (*left_aut_iter).get()->unify_final();
    }
    for (auto right_aut_iter{ right_automata_begin }; right_aut_iter != right_automata_end;
         ++right_aut_iter) {
        (*right_aut_iter).get()->unify_initial();
        (*right_aut_iter).get()->unify_final();
    }

    if (left_automata.empty() || right_automata.empty()) { return NoodleSubstSequence{}; }

    // Automaton representing the left side concatenated over epsilon transitions.
    Nfa::Nfa concatenated_left_side{ *left_automata_begin->get() };
    for (auto next_left_automaton_it{ left_automata_begin + 1 }; next_left_automaton_it != left_automata_end;
         ++next_left_automaton_it) {
        concatenated_left_side = concatenate_eps(concatenated_left_side, *next_left_automaton_it->get(), EPSILON, true);
    }
    Nfa::Nfa concatenated_right_side{ *right_automata_begin->get() };
    for (auto next_right_automaton_it{ right_automata_begin + 1 }; next_right_automaton_it != right_automata_end;
         ++next_right_automaton_it) {
        concatenated_right_side = concatenate_eps(concatenated_right_side, *next_right_automaton_it->get(), EPSILON-1, true); // we use EPSILON-1
    }

    const std::set<Symbol> epsilons({EPSILON, EPSILON-1});
    auto product_pres_eps_trans{
            intersection_eps(concatenated_left_side, concatenated_right_side, true, epsilons) };

    product_pres_eps_trans.trim();
    if (is_lang_empty(product_pres_eps_trans)) {
        return NoodleSubstSequence{};
    }
    if (Util::haskey(params, "reduce")) {
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
    return noodlify_mult_eps(product_pres_eps_trans, epsilons, include_empty);
}

SegNfa::EpsCntVector SegNfa::process_eps_map(const EpsCntMap& eps_cnt) {
    EpsCntVector ret;
    for(auto it = eps_cnt.rbegin(); it != eps_cnt.rend(); it++) {
        ret.push_back(it->second);
    }
    return ret;
}
