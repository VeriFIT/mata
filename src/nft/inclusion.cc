/* nft-incl.cc -- NFT language inclusion
 */

// MATA headers
#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata::nft;
using namespace mata::utils;

/// naive language inclusion check (complementation + intersection + emptiness)
bool mata::nft::algorithms::is_included_naive(
        const Nft &smaller,
        const Nft &bigger,
        const Alphabet *const alphabet,//TODO: this should not be needed, likewise for equivalence
        Run *cex,
        const JumpMode jump_mode) { // {{{
    Nft bigger_cmpl;
    if (alphabet == nullptr) {
        bigger_cmpl = complement(bigger, create_alphabet(smaller, bigger));
    } else {
        bigger_cmpl = complement(bigger, *alphabet);
    }
    Nft nft_isect = intersection(smaller, bigger_cmpl, nullptr, jump_mode);

    return nft_isect.is_lang_empty(cex);
} // is_included_naive }}}


/// language inclusion check using Antichains
// TODO, what about to construct the separator from this?
bool mata::nft::algorithms::is_included_antichains(
    const Nft&             smaller,
    const Nft&             bigger,
    const Alphabet* const  alphabet, //TODO: this parameter is not used
    Run*                   cex,
    const JumpMode         jump_mode)
{ // {{{
    if (smaller.num_of_levels != bigger.num_of_levels) { return false; }

    OrdVector<mata::Symbol> symbols;
    if (alphabet == nullptr) {
        symbols = create_alphabet(smaller, bigger).get_alphabet_symbols();
        if (symbols.contains(DONT_CARE) && symbols.size() > 1) {
            symbols.erase(DONT_CARE);
        }
    } else {
        symbols = alphabet->get_alphabet_symbols();
    }

    return nfa::algorithms::is_included_antichains(smaller.unwind_jumps(symbols, jump_mode),
                                                   bigger.unwind_jumps(symbols, jump_mode),
                                                   alphabet,
                                                   cex);
} // }}}

namespace {
    using AlgoType = decltype(algorithms::is_included_naive)*;

    AlgoType set_algorithm(const std::string &function_name, const ParameterMap &params) {
        if (!haskey(params, "algorithm")) {
            throw std::runtime_error(function_name +
                                     " requires setting the \"algorithm\" key in the \"params\" argument; "
                                     "received: " + std::to_string(params));
        }

        decltype(algorithms::is_included_naive) *algo;
        const std::string &str_algo = params.at("algorithm");
        if ("naive" == str_algo) {
            algo = algorithms::is_included_naive;
        } else if ("antichains" == str_algo) {
            algo = algorithms::is_included_antichains;
        } else {
            throw std::runtime_error(std::to_string(__func__) +
                                     " received an unknown value of the \"algorithm\" key: " + str_algo);
        }

        return algo;
    }

}

// The dispatching method that calls the correct one based on parameters
bool mata::nft::is_included(
        const Nft &smaller,
        const Nft &bigger,
        Run *cex,
        const Alphabet *const alphabet,
        const JumpMode jump_mode,
        const ParameterMap &params) { // {{{
    AlgoType algo{set_algorithm(std::to_string(__func__), params)};
    return algo(smaller, bigger, alphabet, cex, jump_mode);
} // is_included }}}

bool mata::nft::are_equivalent(const Nft& lhs, const Nft& rhs, const Alphabet *alphabet, const JumpMode jump_mode, const ParameterMap& params)
{
    if (lhs.num_of_levels != rhs.num_of_levels) { return false; }

    OrdVector<mata::Symbol> symbols;
    if (alphabet == nullptr) {
        symbols = create_alphabet(lhs, rhs).get_alphabet_symbols();
        if (symbols.contains(DONT_CARE) && symbols.size() > 1) {
            symbols.erase(DONT_CARE);
        }
    } else {
        symbols = alphabet->get_alphabet_symbols();
    }

    return nfa::are_equivalent(lhs.unwind_jumps(symbols, jump_mode),
                               rhs.unwind_jumps(symbols, jump_mode),
                               alphabet,
                               params);
}

bool mata::nft::are_equivalent(const Nft& lhs, const Nft& rhs, const JumpMode jump_mode, const ParameterMap& params) {
    return are_equivalent(lhs, rhs, nullptr, jump_mode, params);
}
