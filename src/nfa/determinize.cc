/* determinize.cc - implementations of the determinization algorithms (default version and version with boost bit vectors)
*/


// MATA headers
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;

Nfa mata::nfa::determinize(
    const Nfa &aut, std::unordered_map<StateSet, State> *subset_map,
    std::optional<std::function<bool(const Nfa &, const State, const StateSet &)>> macrostate_discover)
{
    Nfa result{};
    // assuming all sets targets are non-empty
    std::vector<std::pair<State, StateSet>> worklist{};
    std::unordered_map<StateSet, State> subset_map_local{};
    if (subset_map == nullptr)
    {
        subset_map = &subset_map_local;
    }

    const StateSet S0{aut.initial};
    const State S0id{result.add_state()};
    result.initial.insert(S0id);

    if (aut.final.intersects_with(S0))
    {
        result.final.insert(S0id);
    }
    worklist.emplace_back(S0id, S0);

    (*subset_map)[mata::utils::OrdVector<State>(S0)] = S0id;
    if (aut.delta.empty())
    {
        return result;
    }
    if (macrostate_discover.has_value() && !(*macrostate_discover)(result, S0id, S0))
    {
        return result;
    }

    using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
    SynchronizedExistentialSymbolPostIterator synchronized_iterator;

    while (!worklist.empty())
    {
        const auto Spair = worklist.back();
        worklist.pop_back();
        const StateSet S = Spair.second;
        const State Sid = Spair.first;
        if (S.empty())
        {
            // This should not happen assuming all sets targets are non-empty.
            break;
        }

        // add moves of S to the sync ex iterator
        synchronized_iterator.reset();
        for (State q : S)
        {
            mata::utils::push_back(synchronized_iterator, aut.delta[q]);
        }

        while (synchronized_iterator.advance())
        {
            // extract post from the synchronized_iterator iterator
            const std::vector<Iterator> &symbol_posts = synchronized_iterator.get_current();
            Symbol currentSymbol = (*symbol_posts.begin())->symbol;
            StateSet T = synchronized_iterator.unify_targets();


            const auto existingTitr = subset_map->find(T);

            State Tid;
            if (existingTitr != subset_map->end())
            {
                Tid = existingTitr->second;
            }
            else
            {
                Tid = result.add_state();
                (*subset_map)[T] = Tid;
                if (aut.final.intersects_with(T))
                {
                    result.final.insert(Tid);
                }

                worklist.emplace_back(Tid, T);
            }
            result.delta.mutable_state_post(Sid).insert(SymbolPost(currentSymbol, Tid));
            if (macrostate_discover.has_value() && existingTitr == subset_map->end() && !(*macrostate_discover)(result, Tid, T))
            {
                return result;
            }
        }
    }
    return result;
}

Nfa mata::nfa::determinize_boost(Nfa &aut, std::unordered_map<BoostSet, State> *subset_map,
std::optional<std::function<bool(Nfa&, const State, const BoostSet&)>> macrostate_discover)
{
    using BoostSet = mata::utils::BoostVector;

    std::unordered_map<BoostSet, State> subset_map_local{}; // object of a map of sets to states
    if (subset_map == nullptr)
    {
        subset_map = &subset_map_local;
    }

    Nfa result{}; // result DFA

    // assuming all sets targets are non-empty
    std::vector<std::pair<State, BoostSet>> worklist{};

    // Copy the states from the normal vector to a bitset
    aut.delta.copy_to_boost(true);

    // Initial and final states of the input NFA
    BoostSet S0{aut.initial.begin(), aut.initial.end()};
    S0.resize(aut.num_of_states());
    const BoostSet FinalStates{aut.final.begin(), aut.final.end()};

    // Should be 0, initial state
    const State S0id{result.add_state()}; 
    result.initial.insert(S0id);

    // Is the starting state of the NFA a final state? The DFA final states are all subsets that contain the OG final state
    if (FinalStates.intersects_with(S0)) result.final.insert(S0id);

    /*
        -S0 = Initial set of states of the input NFA (BoostVector)
        -S0id = Initial state of the output DFA (State)
        -Worklist = Pairs(state, boost vector set of states) (std::pair)
    */
    worklist.emplace_back(S0id, S0);

    (*subset_map)[S0] = S0id;

    // Empty delta or a new macrostate found
    if (aut.delta.empty() || (macrostate_discover.has_value() && !(*macrostate_discover)(result, S0id, S0)))
    {
        return result;
    }

    using Iterator = StatePost::const_iterator;
    SynchronizedExistentialSymbolPostIterator synchronized_iterator;

    // While there are states to be processed (at the start there are only the initial states)
    while (!worklist.empty())
    {
        // Pop a pair from the worklist
        const auto Spair = worklist.back();
        worklist.pop_back();

        const State Source = Spair.first;               // State ID
        const BoostSet SourceSet = Spair.second;        // State set representing this state


        if (SourceSet.empty())
        {
            // This should not happen assuming all sets targets are non-empty.
            break;
        }

        // Empty the sync iterator (but keep the allocated space)
        synchronized_iterator.reset();

        // Add the moves (targets) of the current state to the sync ex iterator
        for (State target = SourceSet.find_first(); target != BoostSet::npos; target = SourceSet.find_next(target)) {
            /* NOTE: for a state q, delta[q] returns it's state post, so a OrdVector of SymbolPosts
                - Symbol post contains the targets for that state and a given symbol
                - So basically, we push back all the SymbolPosts of the state q to the sync iterator
            */
            mata::utils::push_back(synchronized_iterator, aut.delta[target]);
        }

        // Process all the current state's targets
        while (synchronized_iterator.advance())
        {
            // extract post from the synchronized_iterator iterator
            const std::vector<Iterator> &symbol_posts = synchronized_iterator.get_current();
            Symbol currentSymbol = (*symbol_posts.begin())->symbol;

            // At the first iteration the synciterator has access to the set of initial states of the NFA
            BoostSet Targets = synchronized_iterator.unify_targets_boost();

            // Check if this set was already processed
            const auto existingTitr = subset_map->find(Targets);
            State TargetID;

            // If yes, retrieve the state ID from the map
            if (existingTitr != subset_map->end())
                TargetID = existingTitr->second;

            // If not, we have to create a new state
            else
            {
                TargetID = result.add_state();
                (*subset_map)[Targets] = TargetID;
                if (FinalStates.intersects_with(Targets))
                {
                    result.final.insert(TargetID);
                }

                // Push the current targets to the worklist for processing
                worklist.emplace_back(TargetID, Targets);
            }
            result.delta.mutable_state_post(Source).insert(SymbolPost(currentSymbol, TargetID));
            if (macrostate_discover.has_value() && existingTitr == subset_map->end() && !(*macrostate_discover)(result, TargetID, Targets))
            {
                return result;
            }
        }
    }
    return result;
}