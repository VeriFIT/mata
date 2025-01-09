// TODO: Insert file header.

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

// #include "mata/alphabet.hh"
// #include "mata/parser/parser.hh"

// Note: Temporary local includes for convenience.
#include "../alphabet.hh"

#include <limits>

// Use this for undefined ID or index.
#define MAX_SIZE_T (std::numeric_limits<size_t>::max())
#define UNDEFINED_ID MAX_SIZE_T

namespace mata::cntnfa {

extern const std::string TYPE_NFA;

using State = unsigned long;
using StateSet = mata::utils::OrdVector<State>;

/// State with an annotation (@c State @c state and @c size_t @c annotation_id).
/// TODO: Move this to the annotation header file.
struct AnnotationState {
    State state; ///< Automaton state.
    size_t annotation_id; ///< Unique ID for the position in the vector of transition annotations.

    AnnotationState() : state(), annotation_id(UNDEFINED_ID) {}
    AnnotationState(const State state, size_t annotation_id) : state(state), annotation_id(annotation_id) {} // NOLINT(*-explicit-constructor)

    AnnotationState(const AnnotationState&) = default;
    AnnotationState(AnnotationState&&) = default;
    AnnotationState& operator=(const AnnotationState&) = default;
    AnnotationState& operator=(AnnotationState&&) = default;

    AnnotationState(const State& state): state{ state }, annotation_id(UNDEFINED_ID) {} // NOLINT(*-explicit-constructor)
    AnnotationState(State&& state): state{ state }, annotation_id(UNDEFINED_ID) {} // NOLINT(*-explicit-constructor)

    auto operator<=>(const State& other) const { return state <=> other; }
    bool operator==(const State other) const { return state == other; }
    auto operator<=>(const AnnotationState&) const = default;
    bool operator==(const AnnotationState& other) const { return state == other; }

    operator State() const { return state; } // NOLINT(*-explicit-constructor)
};

/// Set of states with annotation.
/// TODO: Move this to the annotation header file.
class AnnotationStateSet : public mata::utils::OrdVector<AnnotationState> {
public:
    AnnotationStateSet() = default;

    AnnotationStateSet(State state) { // NOLINT(*-explicit-constructor)
        this->push_back(AnnotationState(state));
    }
    AnnotationStateSet(StateSet& state_set) { // NOLINT(*-explicit-constructor)
        for (const State& state: state_set) {
            this->push_back(state);
        }
    }
    AnnotationStateSet(StateSet&& state_set) { // NOLINT(*-explicit-constructor)
        for (const State& state: state_set) {
            this->push_back(state);
        }
    }
    AnnotationStateSet& operator=(const StateSet& state_set) {
        for (const State& state: state_set) {
            this->push_back(state);
        }
        return *this;
    }
    AnnotationStateSet& operator=(StateSet&& state_set) {
        for (const State& state: state_set) {
            this->push_back(state);
        }
        return *this;
    }
    AnnotationStateSet(const AnnotationStateSet& counter_state_set) = default;
    AnnotationStateSet(AnnotationStateSet&& counter_state_set) noexcept = default;
    AnnotationStateSet& operator=(const AnnotationStateSet& counter_state_set) = default;
    AnnotationStateSet& operator=(AnnotationStateSet&& counter_state_set) noexcept = default;

    // FIXME: This is severely limiting. Basically, this cannot ever be used in production unless explicitly requested.
    //  Should this be explicit? Probably no, but some iteration over CounterStateSet which would behave as a StateSet would be good.
    //  But that is already happening, no?
    operator StateSet() const { // NOLINT(*-explicit-constructor)
        StateSet state_set;
        for (const auto& target : *this) {
            state_set.push_back(target.state);
        }
        return state_set;
    }
};

// Note: Added for better readability.
using Target = AnnotationState;
// Note: Added for better readability.
using TargetSet = AnnotationStateSet;

struct Run {
    Word word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
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
public:
    static const State min_state = std::numeric_limits<State>::min();
    static const State max_state = std::numeric_limits<State>::max();
    static const Symbol min_symbol = std::numeric_limits<Symbol>::min();
    static const Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

struct Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

} // namespace mata::cntnfa.

// Hash specialization for CounterState.
namespace std {
    template<>
    struct hash<mata::cntnfa::AnnotationState> {
        size_t operator()(const mata::cntnfa::AnnotationState& as) const noexcept {
            // Note: Hash the State (unsigned long).
            return std::hash<mata::cntnfa::State>{}(as.state);
        }
    };
} // namespace std.

#endif //MATA_TYPES_HH
