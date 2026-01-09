/* @file
 * @brief NFT universality.
 */

#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata::nft;
using namespace mata::utils;

//TODO: this could be merged with inclusion, or even removed, universality could be implemented using inclusion,
// it is not something needed in practice, so some little overhead is ok

#ifdef MATA_NFT_NOT_IMPLEMENTED
/// naive universality check (complementation + emptiness)
bool mata::nft::algorithms::is_universal_naive(
    const Nft&         aut,
    const Alphabet&    alphabet,
    Run*               cex)
{ // {{{
    Nft cmpl = complement(aut, alphabet);

    return cmpl.is_lang_empty(cex);
} // is_universal_naive }}}
#endif

/// universality check using Antichains
bool mata::nft::algorithms::is_universal_antichains(
    const Nft&         aut,
    const Alphabet&    alphabet,
    Run*               cex)
{ // {{{

    using WorklistType = std::list<StateSet>;
    using ProcessedType = std::list<StateSet>;

    auto subsumes = [](const StateSet& lhs, const StateSet& rhs) {
        if (lhs.size() > rhs.size()) { // bigger set cannot be subset
            return false;
        }

        return std::ranges::includes(rhs, lhs);
    };

    // check the initial state
    if (are_disjoint(aut.initial, aut.final)) {
        if (nullptr != cex) { cex->word.clear(); }
        return false;
    }

    // initialize
    WorklistType worklist = { StateSet(aut.initial) };
    ProcessedType processed = { StateSet(aut.initial) };
    mata::utils::OrdVector<Symbol> alph_symbols = alphabet.get_alphabet_symbols();

    // 'paths[s] == t' denotes that state 's' was accessed from state 't',
    // 'paths[s] == s' means that 's' is an initial state
    std::map<StateSet, std::pair<StateSet, Symbol>> paths =
        { {StateSet(aut.initial), {StateSet(aut.initial), 0}} };

    while (!worklist.empty()) {
        // get a next state
        StateSet state;

        // process parameters
        // TODO: set correctly!!!!
        constexpr bool is_dfs = true;
        if (is_dfs) {
            state = *worklist.rbegin();
            worklist.pop_back();
        } else { // BFS
            state = *worklist.begin();
            worklist.pop_front();
        }

        // process it
        for (Symbol symb : alph_symbols) {
            StateSet succ = aut.post(state, symb);
            if (!aut.final.intersects_with(succ)) {
                if (nullptr != cex) {
                    cex->word.clear();
                    cex->word.push_back(symb);
                    StateSet trav = state;
                    while (paths[trav].first != trav)
                    { // go back until initial state
                        cex->word.push_back(paths[trav].second);
                        trav = paths[trav].first;
                    }

                    std::ranges::reverse(cex->word);
                }

                return false;
            }

            bool is_subsumed = false;
            for (const auto& anti_state : processed) {
                // trying to find a smaller state in processed
                if (subsumes(anti_state, succ)) {
                    is_subsumed = true;
                    break;
                }
            }

            if (is_subsumed) { continue; }

            // prune data structures and insert succ inside
            for (std::list<StateSet>* ds : {&processed, &worklist}) {
                auto it = ds->begin();
                while (it != ds->end()) {
                    if (subsumes(succ, *it)) {
                        auto to_remove = it;
                        ++it;
                        ds->erase(to_remove);
                    } else {
                        ++it;
                    }
                }

                // TODO: set pushing strategy
                ds->push_back(succ);
            }

            // also set that succ was accessed from state
            paths[succ] = {state, symb};
        }
    }

    return true;
} // }}}

// The dispatching method that calls the correct one based on parameters.
bool mata::nft::Nft::is_universal(const Alphabet& alphabet, Run* cex, const ParameterMap& params) const {
    // TODO(nft): Revert back to the naive algorithm when implemented for NFTs?
    // Setting the default algorithm.
    decltype(algorithms::is_universal_antichains)* algo = algorithms::is_universal_antichains;
    if (!haskey(params, "algorithm")) {
            throw std::runtime_error(std::to_string(__func__) +
                    " requires setting the \"algorithm\" key in the \"params\" argument; "
                    "received: " + std::to_string(params));
    }

    if (const std::string& str_algo = params.at("algorithm"); "naive" == str_algo) {
        /* default */
        throw std::runtime_error(std::to_string(__func__) +
                 " naive algorithm is not implemented for NFTs");
    }
    else if ("antichains" == str_algo) {
            algo = algorithms::is_universal_antichains;
    } else {
            throw std::runtime_error(std::to_string(__func__) +
                    " received an unknown value of the \"algorithm\" key: " + str_algo);
    }
    return algo(*this, alphabet, cex);
} // is_universal()

bool mata::nft::Nft::is_universal(const Alphabet& alphabet, const ParameterMap& params) const {
    return this->is_universal(alphabet, nullptr, params);
}
