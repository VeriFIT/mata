// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"

namespace mata::cntnfa {

/* TransitionAnnotation */

/* CounterAssign */
void CounterAssign::update(CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterAssign: Invalid counter ID.");
    }

    Register& counter = counters[register_id];

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

AnnotationType CounterAssign::get_type() const {
    return AnnotationType::CounterAssign;
}

/* CounterIncrement */
void CounterIncrement::update(CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterIncrement: Invalid counter ID.");
    }

    Register& counter = counters[register_id];

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

AnnotationType CounterIncrement::get_type() const {
    return AnnotationType::CounterIncrement;
}

/* CounterEqual */
void CounterEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterEqual::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterEqual: Invalid counter ID.");
    }
    return counters[register_id].value == value;
}

bool CounterEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterEqual::get_type() const {
    return AnnotationType::CounterEqual;
}

/* CounterNotEqual */
void CounterNotEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterNotEqual::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterNotEqual: Invalid counter ID.");
    }
    return counters[register_id].value != value;
}

bool CounterNotEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterNotEqual::get_type() const {
    return AnnotationType::CounterNotEqual;
}

/* CounterGreater */
void CounterGreater::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterGreater::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterGreater: Invalid counter ID.");
    }
    return counters[register_id].value > value;
}

bool CounterGreater::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterGreater::get_type() const {
    return AnnotationType::CounterGreater;
}

/* CounterLess */
void CounterLess::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterLess::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterLess: Invalid counter ID.");
    }
    return counters[register_id].value < value;
}

bool CounterLess::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterLess::get_type() const {
    return AnnotationType::CounterLess;
}

/* CounterGreaterEqual */
void CounterGreaterEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterGreaterEqual::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterGreaterEqual: Invalid counter ID.");
    }
    return counters[register_id].value >= value;
}

bool CounterGreaterEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterGreaterEqual::get_type() const {
    return AnnotationType::CounterGreaterEqual;
}

/* CounterLessEqual */
void CounterLessEqual::update(CounterSet& counters) const {
    (void)counters;
    return;
}

bool CounterLessEqual::guard(const CounterSet& counters) const {
    if (register_id >= counters.size()) {
        throw std::runtime_error("CounterLessEqual: Invalid counter ID.");
    }
    return counters[register_id].value <= value;
}

bool CounterLessEqual::apply(CounterSet& counters) const {
    return guard(counters);
}

AnnotationType CounterLessEqual::get_type() const {
    return AnnotationType::CounterLessEqual;
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
