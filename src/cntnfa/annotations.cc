// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"
#include <string>

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

std::string CounterIncrement::get_type() const {
    return "increment";
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

std::string CounterTest::get_type() const {
    return "test";
}

void AnnotationCollection::allocate(const size_t size) {
    assert(size >= annotations.size());
    annotations.resize(size);
}

size_t AnnotationCollection::size() const {
    return annotations.size();
}

void AnnotationCollection::clear() {
    annotations.clear();
}

void AnnotationCollection::insert_annotation(TransitionAnnotationVariant annotation, size_t index)
{
    if (index >= annotations.size()) {
        this->allocate(index + 1);
    }
    annotations[index].push_back(annotation);
}

} // namespace mata::cntnfa.
