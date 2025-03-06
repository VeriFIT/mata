// TODO: Insert header file.

#include <limits>

#include "mata/cntnfa/counters.hh"

namespace mata::cntnfa {

/*
    Methods for CounterRegister.
*/

void CounterRegister::increment(CounterValue amount) {
    if (value > std::numeric_limits<CounterValue>::max() - amount) {
        throw std::overflow_error("CounterRegister: Increment operation would result in overflow.");
    }
    value += amount;
}

void CounterRegister::decrement(CounterValue amount) {
    if (amount > value) {
        throw std::underflow_error("CounterRegister: Decrement operation would result in a negative value.");
    }
    value -= amount;
}

void CounterRegister::reset() {
    value = initial_value;
}

/*
    Methods for CounterRegisterSet.
*/

void CounterRegisterSet::allocate(const size_t size) {
    assert(size >= annotations.size());
    counters.resize(size);
}

size_t CounterRegisterSet::size() const {
    return counters.size();
}

void CounterRegisterSet::clear() {
    counters.clear();
}

void CounterRegisterSet::insert_counter(CounterRegister counter, size_t index) {
    if (index >= counters.size()) {
        this->allocate(index + 1);
    }
    counters.insert(counters.begin() + static_cast<long>(index), counter);
}

} // namespace mata::cntnfa.
