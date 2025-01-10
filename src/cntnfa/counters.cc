// TODO: Insert header file.

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

// Note: Custom debug output. This should be removed later.
void CounterRegister::print() const {
    std::cout << "ID: " << id << ", Value: " << value << ", Initial: " << initial_value << "\n";
}

/*
    Methods for CounterRegisterSet.
*/

void CounterRegisterSet::addCounter(CounterValue value) {
    counters.emplace_back(counters.size(), value);
}

CounterRegister& CounterRegisterSet::getCounter(size_t id) {
    if (id >= counters.size()) {
        throw std::runtime_error("CounterRegisterSet: Counter with this ID does not exist.");
    }
    return counters[id];
}

const CounterRegister& CounterRegisterSet::getCounter(size_t id) const {
    if (id >= counters.size()) {
        throw std::runtime_error("CounterRegisterSet: Counter with this ID does not exist.");
    }
    return counters[id];
}

size_t CounterRegisterSet::size() {
    return counters.size();
}

// Note: Custom debug output. This should be removed later.
void CounterRegisterSet::print() const {
    for (const auto& counter : counters) {
        counter.print();
    }
}

} // namespace mata::cntnfa.
