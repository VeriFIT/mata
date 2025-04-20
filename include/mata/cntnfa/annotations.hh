// TODO: Insert file header.

#ifndef ANNOTATIONS_HH
#define ANNOTATIONS_HH

#include <variant>

#include "mata/utils/ord-vector.hh"
#include "types.hh"
#include "counters.hh"

using namespace mata::utils;

namespace mata::cntnfa {

/// State with an annotation (@c State @c state and @c size_t @c annotation_id).
struct AnnotationState {
    State state; ///< Automaton state.
    size_t annotations_id; ///< Unique ID for the position in the vector of transition annotations collection.

    AnnotationState() : state(), annotations_id(UNDEFINED_ANNOTATIONS) {}
    AnnotationState(const State state, size_t annotations_id) : state(state), annotations_id(annotations_id) {} // NOLINT(*-explicit-constructor)

    AnnotationState(const AnnotationState&) = default;
    AnnotationState(AnnotationState&&) = default;
    AnnotationState& operator=(const AnnotationState&) = default;
    AnnotationState& operator=(AnnotationState&&) = default;

    AnnotationState& operator++() { ++state; return *this; }

    AnnotationState(const State& state): state{ state }, annotations_id(UNDEFINED_ANNOTATIONS) {} // NOLINT(*-explicit-constructor)
    AnnotationState(State&& state): state{ state }, annotations_id(UNDEFINED_ANNOTATIONS) {} // NOLINT(*-explicit-constructor)

    auto operator<=>(const State& other) const { return state <=> other; }
    bool operator==(const State other) const { return state == other; }
    auto operator<=>(const AnnotationState&) const = default;
    bool operator==(const AnnotationState& other) const { return state == other; }

    operator State() const { return state; } // NOLINT(*-explicit-constructor)
};

/// TODO: Write a comment for doxygen.
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
    AnnotationStateSet(const StateSet& state_set) {
        for (const State& state : state_set) {
            this->push_back(AnnotationState(state));
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

    virtual std::string get_type() const = 0;
};

/// Class for assigning a value to a counter by its ID.
// Note: (:= c0 0) is a counter assignment in Mata format.
class CounterAssign : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to assign.
    CounterValue value; ///< The value to assign.

public:
    CounterAssign() : counter_id(UNDEFINED_COUNTER), value(0) {}
    CounterAssign(size_t counter_id, CounterValue value)
        : counter_id(counter_id), value(value) {}

    bool operator==(const CounterAssign& other) const {
        return counter_id == other.counter_id && value == other.value;
    }
    auto operator<=>(const CounterAssign& other) const {
        if (auto cmp = counter_id <=> other.counter_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
    std::string get_type() const override;

    size_t get_counter_id() const { return counter_id; }
    CounterValue get_value() const { return value; }
};

/// Class for incrementing and decrementing a counter by its ID.
// Note: (+ c0 1) or (+ c0 -1) is a counter increment in Mata format.
class CounterIncrement : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to modify.
    CounterValue value; ///< The value to increment (can be negative for decrement).

public:
    CounterIncrement() : counter_id(UNDEFINED_COUNTER), value(0) {}
    CounterIncrement(size_t counter_id, CounterValue value) : counter_id(counter_id), value(value) {}

    bool operator==(const CounterIncrement& other) const {
        return counter_id == other.counter_id && value == other.value;
    }
    auto operator<=>(const CounterIncrement& other) const {
        if (auto cmp = counter_id <=> other.counter_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
    std::string get_type() const override;

    size_t get_counter_id() const { return counter_id; }
    CounterValue get_value() const { return value; }
};

/// Class for testing a counter's value against an expected value.
// Note: (= c0 0) is a counter test in Mata format.
class CounterTest : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to test.
    CounterValue value; ///< Expected value for testing.

public:
    CounterTest() : counter_id(UNDEFINED_COUNTER), value(0) {}
    CounterTest(size_t counter_id, CounterValue value)
        : counter_id(counter_id), value(value) {}

    bool operator==(const CounterTest& other) const {
        return counter_id == other.counter_id && value == other.value;
    }
    auto operator<=>(const CounterTest& other) const {
        if (auto cmp = counter_id <=> other.counter_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
    std::string get_type() const override;

    size_t get_counter_id() const { return counter_id; }
    CounterValue get_value() const { return value; }
};

/// Class for checking if a counter's value is greater than a threshold.
// Note: (> c0 0) is a counter greater than check in Mata format.
class CounterGreater : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to check.
    CounterValue value; ///< The threshold value for comparison.

public:
    CounterGreater() : counter_id(UNDEFINED_COUNTER), value(0) {}
    CounterGreater(size_t counter_id, CounterValue value)
        : counter_id(counter_id), value(value) {}

    bool operator==(const CounterGreater& other) const {
        return counter_id == other.counter_id && value == other.value;
    }
    auto operator<=>(const CounterGreater& other) const {
        if (auto cmp = counter_id <=> other.counter_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
    std::string get_type() const override;

    size_t get_counter_id() const { return counter_id; }
    CounterValue get_value() const { return value; }
};

/// Class for checking if a counter's value is less than a threshold.
// Note: (< c0 0) is a counter less than check in Mata format.
class CounterLess : public TransitionAnnotation {
private:
    size_t counter_id; ///< The ID of the counter to check.
    CounterValue value; ///< The threshold value for comparison.

public:
    CounterLess() : counter_id(UNDEFINED_COUNTER), value(0) {}
    CounterLess(size_t counter_id, CounterValue value)
        : counter_id(counter_id), value(value) {}

    bool operator==(const CounterLess& other) const {
        return counter_id == other.counter_id && value == other.value;
    }
    auto operator<=>(const CounterLess& other) const {
        if (auto cmp = counter_id <=> other.counter_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void execute(CounterSet& counters) const override;
    bool test(const CounterSet& counters) override;
    std::string get_type() const override;

    size_t get_counter_id() const { return counter_id; }
    CounterValue get_value() const { return value; }
};

/*  Store all possible annotation types in a variant.
    Note: All types in TransitionAnnotationVariant must implement comparison operators
    (e.g., operator==, operator<=>) to ensure proper usage in containers requiring ordering.  */
using TransitionAnnotationVariant = std::variant<CounterAssign, CounterIncrement, CounterTest, CounterGreater, CounterLess>;

class AnnotationCollection {
private:
    std::vector<OrdVector<TransitionAnnotationVariant>> annotations;

public:
    AnnotationCollection() : annotations{} {}

    OrdVector<TransitionAnnotationVariant>& operator[](size_t annotations_id) {
        if (annotations_id >= annotations.size()) {
            throw std::out_of_range("Annotation ID is out of range.");
        }
        return annotations[annotations_id];
    }
    const OrdVector<TransitionAnnotationVariant>& operator[](size_t annotations_id) const {
        if (annotations_id >= annotations.size()) {
            throw std::out_of_range("Annotation ID is out of range.");
        }
        return annotations[annotations_id];
    }

    /**
     * Allocate annotation sets up to "size" sets, creating an empty set for yet unallocated set.
     * "size" has to be at least size() + 1.
     */
    void allocate(const size_t size);

    /**
     * Get the total number of annotation sets.
     */
    size_t size() const;

    /**
     * Clear all annotations.
     */
    void clear();

    /**
     * Insert an annotation into annotations vector using index.
     */
    void insert(TransitionAnnotationVariant annotation, size_t index);
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
