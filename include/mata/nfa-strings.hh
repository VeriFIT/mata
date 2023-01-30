/* nfa-strings.hh -- Operations on NFAs for string solving.
 *
 * Copyright (c) 2022 David Chocholatý <chocholaty.david@protonmail.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MATA_NFA_STRING_SOLVING_HH_
#define MATA_NFA_STRING_SOLVING_HH_

#include <memory>
#include <optional>

#include <mata/nfa.hh>

namespace {
    using namespace Mata::Nfa;
}

namespace Mata {
namespace Strings {
    /**
     * Class mapping states to the shortest words accepted by languages of the states.
     */
    class ShortestWordsMap {
    public:
        /**
         * Maps states in the automaton @p aut to shortest words accepted by languages of the states.
         * @param aut Automaton to compute shortest words for.
         */
        explicit ShortestWordsMap(const Nfa::Nfa& aut) : reversed_automaton(revert(aut)) {
            insert_initial_lengths();
            compute();
        }

        /**
         * Gets shortest words for the given @p states.
         * @param[in] states States to map shortest words for.
         * @return Set of shortest words.
         */
        WordSet get_shortest_words_for(const StateSet& states) const;

        /**
         * Gets shortest words for the given @p state.
         * @param[in] state State to map shortest words for.
         * @return Set of shortest words.
         */
        WordSet get_shortest_words_for(State state) const;

    private:
        using WordLength = int; ///< A length of a word.
        /// Pair binding the length of all words in the word set and word set with words of the given length.
        using LengthWordsPair = std::pair<WordLength, WordSet>;
        /// Map mapping states to the shortest words accepted by the automaton from the mapped state.
        std::unordered_map<State, LengthWordsPair> shortest_words_map{};
        std::set<State> processed{}; ///< Set of already processed states.
        std::deque<State> fifo_queue{}; ///< FIFO queue for states to process.
        const Nfa::Nfa reversed_automaton; ///< Reversed input automaton.

        /**
         * @brief Inserts initial lengths into the shortest words map.
         *
         * Inserts initial length of length 0 for final state in the automaton (initial states in the reversed automaton).
         */
        void insert_initial_lengths();

        /**
         * Computes shortest words for all states in the automaton.
         */
        void compute();

        /**
         * Computes shortest words for the given @p state.
         * @param[in] state State to compute shortest words for.
         */
        void compute_for_state(State state);

        /**
         * Creates default shortest words mapping for yet unprocessed @p state.
         * @param[in] state State to map default shortest words.
         * @return Created default shortest words map element for the given @p state.
         */
        LengthWordsPair map_default_shortest_words(const State state) {
            return shortest_words_map.emplace(state, std::make_pair(-1, WordSet{})).first->second;
        }

        /**
         * Update words for the current state.
         * @param[out] act Current state shortest words and length.
         * @param[in] dst Transition target state shortest words and length.
         * @param[in] symbol Symbol to update with.
         */
        static void update_current_words(LengthWordsPair& act, const LengthWordsPair& dst, Symbol symbol);
    }; // Class ShortestWordsMap.

    /**
     * Get shortest words (regarding their length) of the automaton using BFS.
     * @return Set of shortest words.
     */
    WordSet get_shortest_words(const Nfa::Nfa& nfa);

    /**
     * @brief Get the lengths of all words in the automaton @p aut. The function returns a set of pairs <u,v> where for each 
     * such a pair there is a word with length u+k*v for all ks. The disjunction of such formulae of all pairs hence precisely 
     * describe lengths of all words in the automaton.
     * 
     * @param aut Input automaton
     * @return Set of pairs describing lengths
     */
    std::set<std::pair<int, int>> get_word_lengths(const Nfa::Nfa& aut);

/**
 * Operations on segment automata.
 */
namespace SegNfa {
    /// Segment automaton.
    /// These are automata whose state space can be split into several segments connected by ε-transitions in a chain.
    /// No other ε-transitions are allowed. As a consequence, no ε-transitions can appear in a cycle.
    /// Segment automaton can have initial states only in the first segment and final states only in the last segment.
    using SegNfa = Nfa::Nfa;
    using VisitedEpsMap = std::map<State, std::map<Symbol, unsigned>>;

    /// Number of visited epsilons
    using EpsCntMap = std::map<Symbol, unsigned>;
    /// Projection of EpsCntMap to sorted keys (desc)
    using EpsCntVector = std::vector<unsigned>;

    /**
    * Class executing segmentation operations for a given segment automaton. Works only with segment automata.
    */
    class Segmentation {
    public:
        using EpsilonDepth = unsigned; ///< Depth of ε-transitions.
        /// Dictionary of lists of ε-transitions grouped by their depth.
        /// For each depth 'i' we have 'depths[i]' which contains a list of ε-transitions of depth 'i'.
        using EpsilonDepthTransitions = std::unordered_map<EpsilonDepth, TransSequence>;
        using EpsilonDepthTransitionMap = std::unordered_map<EpsilonDepth, std::unordered_map<State,TransSequence>>;

        /**
         * Prepare automaton @p aut for segmentation.
         * @param[in] aut Segment automaton to make segments for.
         * @param[in] epsilon Symbol to execute segmentation for.
         */
        explicit Segmentation(const SegNfa& aut, const std::set<Symbol> epsilons) : epsilons(epsilons), automaton(aut) {
            compute_epsilon_depths(); // Map depths to epsilon transitions.
        }

        /**
         * Get segmentation depths for ε-transitions.
         * @return Map of depths to lists of ε-transitions.
         */
        const EpsilonDepthTransitions& get_epsilon_depths() const { return epsilon_depth_transitions; }

        /**
         * Get the epsilon depth trans map object (mapping of depths and states to eps-successors)
         * 
         * @return Map of depths to a map of states to transitions
         */
        const EpsilonDepthTransitionMap& get_epsilon_depth_trans_map() const { return this->eps_depth_trans_map; }

        /**
         * Get segment automata.
         * @return A vector of segments for the segment automaton in the order from the left (initial state in segment automaton)
         * to the right (final states of segment automaton).
         */
        const AutSequence& get_segments();

        /**
         * Get raw segment automata.
         * @return A vector of segments for the segment automaton in the order from the left (initial state in segment automaton)
         * to the right (final states of segment automaton) without trimming (the states are same as in the original automaton).
         */
        const AutSequence& get_untrimmed_segments();

        const VisitedEpsMap& get_visited_eps() const { return this->visited_eps; }

    private:
        const std::set<Symbol> epsilons; ///< Symbol for which to execute segmentation.
        /// Automaton to execute segmentation for. Must be a segment automaton (can be split into @p segments).
        const SegNfa& automaton;
        EpsilonDepthTransitions epsilon_depth_transitions{}; ///< Epsilon depths.
        EpsilonDepthTransitionMap eps_depth_trans_map{}; /// Epsilon depths with mapping of states to epsilon transitions
        AutSequence segments{}; ///< Segments for @p automaton.
        AutSequence segments_raw{}; ///< Raw segments for @p automaton.
        VisitedEpsMap visited_eps{}; /// number of visited eps for each state

        /**
         * Pair of state and its depth.
         */
        struct StateDepthTuple {
            State state; ///< State with a depth.
            EpsilonDepth depth; ///< Depth of a state.
            EpsCntMap eps; /// visited epsilons and their numbers
        };

        /**
         * Compute epsilon depths with their transitions.
         */
        void compute_epsilon_depths();

        /**
         * Split segment @c automaton into @c segments.
         */
        void split_aut_into_segments();

        /**
         * Propagate changes to the current segment automaton to the next segment automaton.
         * @param[in] current_depth Current depth.
         * @param[in] transition Current epsilon transition.
         */
        void update_next_segment(size_t current_depth, const Trans& transition);

        /**
         * Update current segment automaton.
         * @param[in] current_depth Current depth.
         * @param[in] transition Current epsilon transition.
         */
        void update_current_segment(size_t current_depth, const Trans& transition);

        /**
         * Initialize map of visited states.
         * @return Map of visited states.
         */
        std::unordered_map<State, bool> initialize_visited_map() const;

        /**
         * Initialize worklist of states with depths to process.
         * @return Queue of state and its depth pairs.
         */
        std::deque<StateDepthTuple> initialize_worklist() const;

        /**
         * Process pair of state and its depth.
         * @param[in] state_depth_pair Current state depth pair.
         * @param[out] worklist Worklist of state and depth pairs to process.
         */
        void process_state_depth_pair(const StateDepthTuple& state_depth_pair, std::deque<StateDepthTuple>& worklist);

        /**
         * Add states with non-epsilon transitions to the @p worklist.
         * @param state_transitions[in] Transitions from current state.
         * @param depth[in] Current depth.
         * @param worklist[out] Worklist of state and depth pairs to process.
         */
        void add_transitions_to_worklist(const StateDepthTuple& state_depth_pair, const Move& state_transitions,
                                                std::deque<StateDepthTuple>& worklist);

        /**
         * Process epsilon transitions for the current state.
         * @param[in] state_depth_pair Current state depth pair.
         * @param[in] state_transitions Transitions from current state.
         * @param[out] worklist Worklist of state and depth pairs to process.
         */
        void handle_epsilon_transitions(const StateDepthTuple& state_depth_pair, const Move& state_transitions,
                                        std::deque<StateDepthTuple>& worklist);

        /**
         * @brief Remove inner initial and final states.
         *
         * Remove all initial states for all segments but the first one and all final states for all segments but the last one.
         */
        void remove_inner_initial_and_final_states();
    }; // Class Segmentation.

    /// A noodle is represented as a sequence of segments (a copy of the segment automata) created as if there was exactly
    ///  one ε-transition between each two consecutive segments.
    using Noodle = std::vector<SharedPtrAut>;
    using NoodleSequence = std::vector<Noodle>; ///< A sequence of noodles.

    /// Noodles as segments enriched with EpsCntMap
    using NoodleSubst = std::vector<std::pair<SharedPtrAut, EpsCntVector>>;
    using NoodleSubstSequence = std::vector<NoodleSubst>;

    /**
     * @brief segs_one_initial_final
     * 
     * segments_one_initial_final[init, final] is the pointer to automaton created from one of
     * the segments such that init and final are one of the initial and final states of the segment
     * and the created automaton takes this segment, sets initial={init}, final={final}
     * and trims it; also segments_one_initial_final[unused_state, final] is used for the first
     * segment (where we always want all initial states, only final state changes) and
     * segments_one_initial_final[init, unused_state] is similarly for the last segment
     * TODO: should we use unordered_map? then we need hash
     */
    void segs_one_initial_final(const Mata::Nfa::AutSequence& segments, bool include_empty, 
        const State& unused_state, std::map<std::pair<State, State>, std::shared_ptr<Nfa::Nfa>>& out);

    /**
     * @brief Create noodles from segment automaton @p aut.
     *
     * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
     * A noodle is a vector of pointers to copy of the segments automata created as if there was exactly one ε-transition
     *  between each two consecutive segments.
     *
     * @param[in] automaton Segment automaton to noodlify.
     * @param[in] epsilon Epsilon symbol to noodlify for.
     * @param[in] include_empty Whether to also include empty noodles.
     * @return A list of all (non-empty) noodles.
     */
    NoodleSequence noodlify(const SegNfa& aut, Symbol epsilon, bool include_empty = false);

    /**
     * @brief Create noodles from segment automaton @p aut.
     *
     * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
     * A noodle is a vector of pointers to copy of the segments automata created as if there was exactly one ε-transition
     *  between each two consecutive segments.
     *
     * @param[in] automaton Segment automaton to noodlify.
     * @param[in] epsilons Epsilon symbols to noodlify for.
     * @param[in] include_empty Whether to also include empty noodles.
     * @return A list of all (non-empty) noodles.
     */
    NoodleSubstSequence noodlify_mult_eps(const SegNfa& aut, const std::set<Symbol>& epsilons, bool include_empty = false);

    /**
     * @brief Create noodles for left and right side of equation.
     *
     * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
     * A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.
     *
     * Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
     *  the equation in a way that libMata understands. The left side automata represent the left side of the equation
     *  and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
     *  representing the intersection. That can be achieved by computing a product of both sides. First, the left side
     *  has to be concatenated over an epsilon transitions into a single automaton to compute the intersection on, though.
     *
     * @param[in] left_automata Sequence of segment automata for left side of an equation to noodlify.
     * @param[in] right_automaton Segment automaton for right side of an equation to noodlify.
     * @param[in] include_empty Whether to also include empty noodles.
     * @param[in] params Additional parameters for the noodlification:
     *     - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
     *                 minimization before noodlification.
     * @return A list of all (non-empty) noodles.
     */
    NoodleSequence noodlify_for_equation(const AutRefSequence& left_automata, const Nfa::Nfa& right_automaton,
                                         bool include_empty = false, const StringMap& params = {{"reduce", "false"}});

    /**
     * @brief Create noodles for left and right side of equation.
     *
     * Segment automaton is a chain of finite automata (segments) connected via ε-transitions.
     * A noodle is a copy of the segment automaton with exactly one ε-transition between each two consecutive segments.
     *
     * Mata cannot work with equations, queries etc. Hence, we compute the noodles for the equation, but represent
     *  the equation in a way that libMata understands. The left side automata represent the left side of the equation
     *  and the right automaton represents the right side of the equation. To create noodles, we need a segment automaton
     *  representing the intersection. That can be achieved by computing a product of both sides. First, the left side
     *  has to be concatenated over an epsilon transitions into a single automaton to compute the intersection on, though.
     *
     * @param[in] left_automata Sequence of pointers to segment automata for left side of an equation to noodlify.
     * @param[in] right_automaton Segment automaton for right side of an equation to noodlify.
     * @param[in] include_empty Whether to also include empty noodles.
     * @param[in] params Additional parameters for the noodlification:
     *     - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
     *                 minimization before noodlification.
     * @return A list of all (non-empty) noodles.
     */
    NoodleSequence noodlify_for_equation(const AutPtrSequence& left_automata, const Nfa::Nfa& right_automaton,
                                         bool include_empty = false, const StringMap& params = {{"reduce", "false"}});

    /**
     * @brief Create noodles for left and right side of equation (both sides are given as a sequence of automata).
     *
     * @param[in] left_automata Sequence of pointers to segment automata for left side of an equation to noodlify.
     * @param[in] right_automaton Sequence of pointers to segment automata for right side of an equation to noodlify.
     * @param[in] include_empty Whether to also include empty noodles.
     * @param[in] params Additional parameters for the noodlification:
     *     - "reduce": "false", "forward", "backward", "bidirectional"; Execute forward, backward or bidirectional simulation
     *                 minimization before noodlification.
     * @return A list of all (non-empty) noodles together with the positions reached from the beginning of left/right side.
     */
    NoodleSubstSequence noodlify_for_equation(const std::vector<std::shared_ptr<Nfa::Nfa>>& left_automata, const std::vector<std::shared_ptr<Nfa::Nfa>>& right_automata,
                                                     bool include_empty = false, const StringMap& params = {{"reduce", "false"}});

    /**
     * @brief Process epsilon map to a sequence of values (sorted according to key desc)
     * 
     * @param eps_cnt Epsilon count
     * @return Vector of keys (count of epsilons)
     */
    EpsCntVector process_eps_map(const EpsCntMap& eps_cnt);

} // Namespace SegNfa.

} // Namespace Strings.
} // Namespace Mata.

#endif // MATA_NFA_STRING_SOLVING_HH_.
