// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"

namespace mata::cntnfa {

void CounterIncrement::execute(CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterOperation: Invalid counter ID.");
    }

    CounterRegister& counter = counters[counter_id];

    if (increment_value > 0) {
        counter.increment(increment_value);
    } else if (increment_value < 0) {
        counter.decrement(-increment_value);
    }
}

bool CounterTest::test(const CounterSet& counters) {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterTest: Invalid counter ID.");
    }
    return counters[counter_id].value == expected_value;
}

size_t TransitionAnnotationsCollection::createAnnotations() {
    annotations.emplace_back();
    return annotations.size() - 1;
}

void TransitionAnnotationsCollection::addAnnotation(size_t annotations_id, const TransitionAnnotationVariant& annotation) {
    if (annotations_id >= annotations.size()) {
        throw std::out_of_range("Invalid annotation ID.");
    }
    annotations[annotations_id].push_back(annotation);
}

const std::vector<TransitionAnnotationVariant>& TransitionAnnotationsCollection::getAnnotations(size_t annotations_id) const {
    if (annotations_id >= annotations.size()) {
        throw std::out_of_range("Invalid annotation ID.");
    }
    return annotations[annotations_id];
}

size_t TransitionAnnotationsCollection::size() const {
    return annotations.size();
}

} // namespace mata::cntnfa.
