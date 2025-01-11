// TODO: Insert file header.

#ifndef ANNOTATIONS_HH
#define ANNOTATIONS_HH

#include <variant>

#include "mata/utils/ord-vector.hh"
#include "types.hh"
#include "counters.hh"

namespace mata::cntnfa {

/// State with an annotation (@c State @c state and @c size_t @c annotation_id).
struct AnnotationState {
    State state; ///< Automaton state.
    size_t annotations_id; ///< Unique ID for the position in the vector of transition annotations collection.

    AnnotationState() : state(), annotations_id(UNDEFINED_ID) {}
    AnnotationState(const State state, size_t annotations_id) : state(state), annotations_id(annotations_id) {} // NOLINT(*-explicit-constructor)

    AnnotationState(const AnnotationState&) = default;
    AnnotationState(AnnotationState&&) = default;
    AnnotationState& operator=(const AnnotationState&) = default;
    AnnotationState& operator=(AnnotationState&&) = default;

    AnnotationState(const State& state): state{ state }, annotations_id(UNDEFINED_ID) {} // NOLINT(*-explicit-constructor)
    AnnotationState(State&& state): state{ state }, annotations_id(UNDEFINED_ID) {} // NOLINT(*-explicit-constructor)

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
    virtual bool test(const CounterSet& counters) = 0;
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

class CounterTest : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to test.
    CounterValue expected_value; ///< Expected value for testing.

public:
    CounterTest() = default;
    CounterTest(size_t counter_id, CounterValue expected_value) : counter_id(counter_id), expected_value(expected_value) {}

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
};

// Store all possible annotation types in a variant.
using TransitionAnnotationVariant = std::variant<CounterIncrement, CounterTest>;

class AnnotationCollection {
private:
    std::vector<std::vector<TransitionAnnotationVariant>> annotations;

public:
    AnnotationCollection() : annotations{} {}

    /**
     * Create a new annotation set and return its ID (index).
     */
    size_t createAnnotations();

    /**
     * Add an operation to the set at the given annotations_id.
     */
    void addAnnotation(size_t annotations_id, const TransitionAnnotationVariant& annotation);

    /**
     * Retrieve the set of operations for the given annotations_id.
     */
    const std::vector<TransitionAnnotationVariant>& getAnnotations(size_t annotations_id) const;

    /**
     * Get the total number of annotation sets.
     */
    size_t size() const;

    /**
     * Clear all annotations.
     */
    void clear();
};

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
