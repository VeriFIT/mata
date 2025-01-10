// TODO: Insert header file.

#include "mata/cntnfa/annotations.hh"

namespace mata::cntnfa {

bool TransitionAnnotation::test(const CounterSet& counters) {
    (void)counters; // DEBUG
    return true;
}

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

} // namespace mata::cntnfa.
