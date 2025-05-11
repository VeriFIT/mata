// TODO: Insert file header.

#pragma once

#include <memory>

#include "mata/utils/ord-vector.hh"
#include "mata/cntnfa/types.hh"

using namespace mata::utils;

namespace mata::cntnfa {

/// All existing annotation types.
enum class AnnotationType {
    CounterAssign,
    CounterIncrement,
    CounterEqual,
    CounterNotEqual,
    CounterGreater,
    CounterLess,
    CounterGreaterEqual,
    CounterLessEqual,
    Unknown
};

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
class TransitionAnnotation {
protected:
    size_t register_id = UNDEFINED_REGISTER; ///< The ID of the register to be used in the annotation.
    RegisterValue value = 0; ///< The value to be used in the annotation.

public:
    TransitionAnnotation() = default;
    TransitionAnnotation(size_t register_id, RegisterValue value)
        : register_id(register_id), value(value) {}
    virtual ~TransitionAnnotation() = default;

    size_t get_register_id() const { return register_id; }
    RegisterValue get_value() const { return value; }

    virtual void update(CounterSet& counters) const = 0;
    virtual bool guard(const CounterSet& counters) const = 0;
    virtual bool apply(CounterSet&) const = 0;
    virtual AnnotationType get_type() const = 0;
};

/// Class for assigning a value to a counter by its ID.
// Note: (:= c0 0) is a counter assignment in Mata format.
class CounterAssign : public TransitionAnnotation {
public:
    CounterAssign() = default;
    CounterAssign(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterAssign& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterAssign& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for incrementing and decrementing a counter by its ID.
// Note: (+ c0 1) or (+ c0 -1) is a counter increment in Mata format.
class CounterIncrement : public TransitionAnnotation {
public:
    CounterIncrement() = default;
    CounterIncrement(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterIncrement& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterIncrement& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is equal to the value.
// Note: (= c0 0) is a counter equal check in Mata format.
class CounterEqual : public TransitionAnnotation {
public:
    CounterEqual() = default;
    CounterEqual(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterEqual& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterEqual& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is NOT equal to the value.
// Note: (!= c0 0) is a counter NOT equal check in Mata format.
class CounterNotEqual : public TransitionAnnotation {
public:
    CounterNotEqual() = default;
    CounterNotEqual(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterNotEqual& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterNotEqual& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) {
            return cmp;
        }
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is greater than a threshold.
// Note: (> c0 0) is a counter greater than check in Mata format.
class CounterGreater : public TransitionAnnotation {
public:
    CounterGreater() = default;
    CounterGreater(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterGreater& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterGreater& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is less than a threshold.
// Note: (< c0 0) is a counter less than check in Mata format.
class CounterLess : public TransitionAnnotation {
public:
    CounterLess() = default;
    CounterLess(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterLess& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterLess& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is greater than a threshold or equal to it.
// Note: (>= c0 0) is a counter greater or equal check in Mata format.
class CounterGreaterEqual : public TransitionAnnotation {
public:
CounterGreaterEqual() = default;
    CounterGreaterEqual(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterGreaterEqual& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterGreaterEqual& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

/// Class for checking if a counter's value is less than a threshold or equal to it.
// Note: (<= c0 0) is a counter less or equal check in Mata format.
class CounterLessEqual : public TransitionAnnotation {
public:
    CounterLessEqual() = default;
    CounterLessEqual(size_t counter_id, CounterValue value)
        : TransitionAnnotation(counter_id, value) {}

    bool operator==(const CounterLessEqual& other) const {
        return register_id == other.register_id && value == other.value;
    }
    auto operator<=>(const CounterLessEqual& other) const {
        if (auto cmp = register_id <=> other.register_id; cmp != 0) return cmp;
        return value <=> other.value;
    }

    void update(CounterSet& counters) const override;
    bool guard(const CounterSet& counters) const override;
    bool apply(CounterSet& counters) const override;
    AnnotationType get_type() const override;
};

class AnnotationCollection {
private:
    std::vector<OrdVector<std::shared_ptr<TransitionAnnotation>>> annotations;

public:
    AnnotationCollection() : annotations{} {}

    OrdVector<std::shared_ptr<TransitionAnnotation>>& operator[](size_t annotations_id) {
        if (annotations_id >= annotations.size()) {
            throw std::out_of_range("Annotation ID is out of range.");
        }
        return annotations[annotations_id];
    }
    const OrdVector<std::shared_ptr<TransitionAnnotation>>& operator[](size_t annotations_id) const {
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
    void insert(std::shared_ptr<TransitionAnnotation> annotation, size_t index);

    /**
     * Create a new annotation set at the end of the vector, inserting the annotation into it and returning its index.
     */
    size_t insert(std::shared_ptr<TransitionAnnotation> annotation);
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
