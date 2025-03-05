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

void AnnotationCollection::insert_annotation(TransitionAnnotationVariant annotation, size_t index)
{
    if (index >= this->num_of_annotation_sets()) {
        this->allocate(index + 1);
    }
    annotations[index].push_back(annotation);
}

} // namespace mata::cntnfa.
