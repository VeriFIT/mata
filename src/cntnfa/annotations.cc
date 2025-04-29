// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"
#include <string>

namespace mata::cntnfa {

/* TransitionAnnotation */

/* CounterAssign */
void CounterAssign::update(CounterSet& counters) const {
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

bool CounterAssign::guard(const CounterSet& counters) const {
    (void)counters;
    return true;
}

bool CounterAssign::apply(CounterSet& counters) const {
    update(counters);
    return true;
}

std::string CounterAssign::get_type() const {
    return "CounterAssign";
}

/* CounterIncrement */
void CounterIncrement::update(CounterSet& counters) const {
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

bool CounterIncrement::guard(const CounterSet& counters) const {
    (void)counters;
    return true;
}

bool CounterIncrement::apply(CounterSet& counters) const {
    update(counters);
    return true;
}

std::string CounterIncrement::get_type() const {
    return "CounterIncrement";
}

/* CounterEqual */
void CounterEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterEqual::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterEqual: Invalid counter ID.");
    }
    return counters[counter_id].value == value;
}

bool CounterEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterEqual::get_type() const {
    return "CounterEqual";
}

/* CounterNotEqual */
void CounterNotEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterNotEqual::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterNotEqual: Invalid counter ID.");
    }
    return counters[counter_id].value != value;
}

bool CounterNotEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterNotEqual::get_type() const {
    return "CounterNotEqual";
}

/* CounterGreater */
void CounterGreater::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterGreater::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterGreater: Invalid counter ID.");
    }
    return counters[counter_id].value > value;
}

bool CounterGreater::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterGreater::get_type() const {
    return "CounterGreater";
}

/* CounterLess */
void CounterLess::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterLess::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterLess: Invalid counter ID.");
    }
    return counters[counter_id].value < value;
}

bool CounterLess::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterLess::get_type() const {
    return "CounterLess";
}

/* CounterGreaterEqual */
void CounterGreaterEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterGreaterEqual::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterGreaterEqual: Invalid counter ID.");
    }
    return counters[counter_id].value >= value;
}

bool CounterGreaterEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterGreaterEqual::get_type() const {
    return "CounterGreaterEqual";
}

/* CounterLessEqual */
void CounterLessEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterLessEqual::guard(const CounterSet& counters) const {
    if (counter_id >= counters.size()) {
        throw std::runtime_error("CounterLessEqual: Invalid counter ID.");
    }
    return counters[counter_id].value <= value;
}

bool CounterLessEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

std::string CounterLessEqual::get_type() const {
    return "CounterLessEqual";
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

void AnnotationCollection::insert(std::shared_ptr<TransitionAnnotation> annotation, size_t index) {
    if (index >= annotations.size()) {
        this->allocate(index + 1);
    }
    annotations[index].push_back(annotation);
}

size_t AnnotationCollection::insert(std::shared_ptr<TransitionAnnotation> annotation) {
    annotations.emplace_back();
    annotations.back().push_back(annotation);
    return annotations.size() - 1;
}

} // namespace mata::cntnfa.
