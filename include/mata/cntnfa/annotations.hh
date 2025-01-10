// TODO: Insert file header.

#ifndef ANNOTATIONS_HH
#define ANNOTATIONS_HH

#include <memory>

#include "types.hh"
#include "counters.hh"

namespace mata::cntnfa {

/// State with an annotation (@c State @c state and @c size_t @c annotation_id).
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

// Added for better readability.
using Target = AnnotationState;
using TargetSet = AnnotationStateSet;

/// Class with a virtual interface for different types of annotations.
// Note: This is convenient to implement various operations during transitions.
// TODO: Is this a good idea? Think about better solutions.
class TransitionAnnotation {
public:
    virtual ~TransitionAnnotation() = default;

    virtual void execute(CounterSet& counters) const = 0;
    virtual bool test(const CounterSet& counters);
};

/// Class for incrementing and decrementing a counter by its ID.
class CounterIncrement : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to modify.
    CounterValue increment_value; ///< The value to increment (can be negative for decrement).

public:
    CounterIncrement() = default;
    CounterIncrement(size_t counter_id, CounterValue increment_value) : counter_id(counter_id), increment_value(increment_value) {}

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
};
/// TODO: Add CounterTest class.

using TransitionAnnotationPtr = std::unique_ptr<TransitionAnnotation>;
using TransitionAnnotations = std::vector<TransitionAnnotationPtr>;

// TODO: Try to recreate this like a class to encapsulate the logic.
// TransitionAnnotationsCollection should be similar to Delta? Probably yes. Ask about this approach.
using TransitionAnnotationsCollection = std::vector<TransitionAnnotations>;

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

#endif // ANNOTATIONS_HH
