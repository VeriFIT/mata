// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"
#include <string>

namespace mata::cntnfa {

/* TransitionAnnotation */

void CounterAssign::execute(CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterAssign: Invalid counter ID.");
    }

    CounterRegister& counter = counters[counter_id];

    if (value > 0) {
        counter.set(value);
    } else {
        counter.set(0);
    }
}

bool CounterAssign::test(const CounterSet& counters) {
    (void)counters;
    return true;
}

std::string CounterAssign::get_type() const {
    return "CounterAssign";
}

void CounterIncrement::execute(CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterIncrement: Invalid counter ID.");
    }

    CounterRegister& counter = counters[counter_id];

    if (value > 0) {
        counter.increment(value);
    } else if (value < 0) {
        counter.decrement(-value);
    }
}

bool CounterIncrement::test(const CounterSet& counters) {
    (void)counters;
    return true;
}

std::string CounterIncrement::get_type() const {
    return "CounterIncrement";
}

void CounterTest::execute(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterTest::test(const CounterSet& counters) {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterTest: Invalid counter ID.");
    }
    return counters[counter_id].value == value;
}

std::string CounterTest::get_type() const {
    return "CounterTest";
}

void CounterGreater::execute(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterGreater::test(const CounterSet& counters) {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterGreater: Invalid counter ID.");
    }
    return counters[counter_id].value > value;
}

std::string CounterGreater::get_type() const {
    return "CounterGreater";
}

void CounterLess::execute(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterLess::test(const CounterSet& counters) {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterLess: Invalid counter ID.");
    }
    return counters[counter_id].value < value;
}

std::string CounterLess::get_type() const {
    return "CounterLess";
}

/* AnnotationCollection */

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

void AnnotationCollection::insert(TransitionAnnotationVariant annotation, size_t index)
{
    if (index >= annotations.size()) {
        this->allocate(index + 1);
    }
    annotations[index].push_back(annotation);
}

} // namespace mata::cntnfa.
