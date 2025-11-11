/** @file
 * @brief Basic types used in the @c mata::nft module for NFTs.
 */

#ifndef MATA_NFT_TYPES_HH
#define MATA_NFT_TYPES_HH

#include <utility>

#include "mata/alphabet.hh"

#include "mata/nfa/types.hh"

namespace mata::nft {

extern const std::string TYPE_NFT;

using Level = unsigned;

/**
 * @brief Classes for levels ordering in NFTs.
 */
class LevelsOrdering {
public:
    using Compare = std::function<bool(Level, Level)>;
    /**
     * @brief Ordering for Levels in NFTs where lower levels precede higher levels.
     *
     * That is, levels are ordered as follows: 0 < 1 < 2 < ... < num_of_levels-1.
     */
    static bool Minimal(const Level lhs, const Level rhs) { return std::less()(lhs, rhs); }
    /**
     * @brief Ordering for levels in NFTs where lower levels precede higher levels, except for 0 which is the highest level.
     *
     * That is, levels are ordered as follows: 1 < 2 < ... < num_of_levels-1 < 0.
     * This ordering is used when handling intermediate states (with non-zero levels) in NFTs for determining the next lowest
     *  level of the next state.
     */
    static bool Next(const Level lhs, const Level rhs) { return lhs == 0 ? false : rhs == 0 ? true : lhs < rhs; }
};

using State = mata::nfa::State;
using StateSet = mata::nfa::StateSet;

using Run = mata::nfa::Run;
using EpsilonClosureOpt = mata::nfa::EpsilonClosureOpt;

using StateRenaming = mata::nfa::StateRenaming;

/**
 * @brief Map of additional parameter name and value pairs.
 *
 * Used by certain functions for specifying some additional parameters in the following format:
 * ```cpp
 * ParameterMap {
 *     { "algorithm", "classical" },
 *     { "minimize", "true" }
 * }
 * ```
 */
using ParameterMap = mata::nfa::ParameterMap;

using Limits = mata::nfa::Limits;

class Nft; ///< A non-deterministic finite transducer.

enum class JumpMode {
    RepeatSymbol, ///< Repeat the symbol on the jump.
    AppendDontCares ///< Append a sequence of DONT_CAREs to the symbol on the jump.
};

using ProductFinalStateCondition = mata::nfa::ProductFinalStateCondition;

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = mata::nfa::EPSILON;
constexpr Symbol DONT_CARE = EPSILON - 1;

constexpr Level DEFAULT_LEVEL{ 0 };
constexpr Level DEFAULT_NUM_OF_LEVELS{ 2 };

} // namespace mata::nft.

#endif //MATA_TYPES_HH
