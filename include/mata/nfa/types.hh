/** @file
 * @brief Basic types used in the @c mata::nfa module for NFAs.
 */

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/config.hh"
#include "mata/utils/small-ord-vector.hh"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace mata::nfa {

extern const std::string TYPE_NFA;

static_assert(MATA_STATE_BITS == 32 || MATA_STATE_BITS == 64, "MATA_STATE_BITS must be 32 or 64");
/// A state. Its width is chosen when configuring Mata (CMake option @c MATA_STATE_BITS, 32 by default).
using State = std::conditional_t<MATA_STATE_BITS == 32, std::uint32_t, std::uint64_t>;
// State counts and indices into state-indexed vectors are `size_t`; they must hold every state.
static_assert(sizeof(State) <= sizeof(size_t), "MATA_STATE_BITS=64 needs a 64-bit target");
using StateSet = utils::OrdVector<State>;
/// Target states of a @c SymbolPost: an ordered set that stores small sets inline.
using SmallStateSet = utils::SmallOrdVector<State>;

struct Run {
	Word word{}; ///< A finite-length word.
	std::vector<State> path{}; ///< A finite-length path through automaton.
};

/**
 * @brief Where an epsilon closure is applied around a transition step.
 *
 * The values are bit flags, so @c BeforeAndAfter must be tested with @c closes_before() and @c closes_after()
 *  rather than compared for equality. For @c 0 -ε-> 1 -a-> 2 -ε-> 3 and the step over @c a from @c {0}:
 *
 * | Option | Applied | Result |
 * |---|---|---|
 * | @c None | post(S) | @c {} |
 * | @c Before | post(closure(S)) | @c {2} |
 * | @c After | closure(post(S)) | @c {} |
 * | @c BeforeAndAfter | closure(post(closure(S))) | @c {2,3} |
 */
enum class EpsilonClosureOpt : std::uint8_t {
	None = 1 << 0, ///< No epsilon closure.
	Before = 1 << 1, ///< Epsilon closure before the transition.
	After = 1 << 2, ///< Epsilon closure after the transition.
	BeforeAndAfter = Before | After ///< Epsilon closure before and after the transition.
};

/// Does @p opt ask for an epsilon closure before the transition step?
constexpr bool closes_before(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::Before)) != 0;
}

/// Does @p opt ask for an epsilon closure after the transition step?
constexpr bool closes_after(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::After)) != 0;
}

enum class ProductFinalStateCondition : std::uint8_t {
	And, ///< Both original states have to be final.
	Or, ///< At least one of the original states has to be final.
};

using StateRenaming = std::unordered_map<State, State>;

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
using ParameterMap = std::unordered_map<std::string, std::string>;

struct Limits {
	static constexpr State min_state = std::numeric_limits<State>::min();
	/// The "no state" sentinel; the largest usable state is @c max_state - 1.
	static constexpr State max_state = std::numeric_limits<State>::max();
	/// The largest number of states an automaton can have (states @c 0 to @c max_state - 1).
	static constexpr size_t max_num_of_states = max_state;
	static constexpr Symbol min_symbol = std::numeric_limits<Symbol>::min();
	static constexpr Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

/// Throws @c std::overflow_error: a state number does not fit below @c Limits::max_state.
[[noreturn]] void throw_state_overflow();

/// Throws @c std::overflow_error unless @p num_of_states states can be numbered by @c State.
inline void check_num_of_states(const size_t num_of_states) {
	if (num_of_states > Limits::max_num_of_states) [[unlikely]] { throw_state_overflow(); }
}

/// Converts @p value to a usable @c State, throwing @c std::overflow_error if it is @c max_state or larger.
inline State to_state(const size_t value) {
	if (value >= Limits::max_num_of_states) [[unlikely]] { throw_state_overflow(); }
	return static_cast<State>(value);
}

/// The number of states needed to contain @p state, @p state + 1, computed without wrapping around.
/// Throws @c std::overflow_error if @p state is the @c Limits::max_state sentinel.
inline size_t num_of_states_for(const State state) {
	if (state == Limits::max_state) [[unlikely]] { throw_state_overflow(); }
	return static_cast<size_t>(state) + 1;
}

class Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON{Limits::max_symbol};

} // namespace mata::nfa.

#endif // MATA_TYPES_HH
