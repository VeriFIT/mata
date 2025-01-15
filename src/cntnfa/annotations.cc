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

bool CounterIncrement::test(const CounterSet& counters) {
    (void)counters;
    return true;
}

void CounterTest::execute(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterTest::test(const CounterSet& counters) {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterTest: Invalid counter ID.");
    }
    return counters[counter_id].value == expected_value;
}

size_t AnnotationCollection::createAnnotations() {
    annotations.emplace_back();
    return annotations.size() - 1;
}

void AnnotationCollection::addAnnotation(size_t annotations_id, const TransitionAnnotationVariant& annotation) {
    if (annotations_id >= annotations.size()) {
        throw std::out_of_range("Invalid annotation ID.");
    }
    annotations[annotations_id].push_back(annotation);
}

const OrdVector<TransitionAnnotationVariant>& AnnotationCollection::getAnnotations(size_t annotations_id) const {
    if (annotations_id >= annotations.size()) {
        throw std::out_of_range("Invalid annotation ID.");
    }
    return annotations[annotations_id];
}

void AnnotationCollection::allocate(const size_t num_of_annotation_sets) {
    assert(num_of_annotation_sets >= this->size());
    annotations.resize(num_of_annotation_sets);
}

size_t AnnotationCollection::num_of_annotation_sets() const {
    return annotations.size();
}

void AnnotationCollection::clear() {
    annotations.clear();
}

} // namespace mata::cntnfa.
