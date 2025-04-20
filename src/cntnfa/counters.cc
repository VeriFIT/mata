// TODO: Insert header file.

#include <limits>

#include "mata/cntnfa/counters.hh"

namespace mata::cntnfa {

/*
    Methods for CounterRegister.
*/

void CounterRegister::increment(CounterValue value) {
    if (this->value > std::numeric_limits<CounterValue>::max() - value) {
        throw std::overflow_error("CounterRegister: Increment operation would result in overflow.");
    }
    this->value += value;
}

void CounterRegister::decrement(CounterValue value) {
    if (value > this->value) {
        throw std::underflow_error("CounterRegister: Decrement operation would result in a negative value.");
    }
    this->value -= value;
}

void CounterRegister::set(CounterValue value) {
    if (value < 0) {
        throw std::invalid_argument("CounterRegister: Value cannot be negative.");
    }
    this->value = value;
}

void CounterRegister::reset() {
    this->value = this->initial_value;
}

/*
    Methods for CounterRegisterSet.
*/

void CounterRegisterSet::allocate(const size_t size) {
    assert(size >= counters.size());
    counters.resize(size);
}

size_t CounterRegisterSet::size() const {
    return counters.size();
}

void CounterRegisterSet::clear() {
    counters.clear();
}

void CounterRegisterSet::set(size_t index, const CounterRegister& counter) {
    if (index >= counters.size()) {
        this->allocate(index + 1);
    }
    counters[index] = counter;
    name_to_index[counter.name] = index;
}

size_t CounterRegisterSet::insert(const CounterName& name, CounterValue value) {
    auto it = name_to_index.find(name);
    if (it != name_to_index.end()) {
        return it->second;
    }
    size_t index = counters.size();
    counters.emplace_back(value, name);
    name_to_index[name] = index;
    return index;
}

bool CounterRegisterSet::has(const CounterName& name) const {
    return name_to_index.contains(name);
}

const CounterName& CounterRegisterSet::get_name(size_t index) const {
    return counters.at(index).name;
}

size_t CounterRegisterSet::get_index(const CounterName& name) const {
    auto it = name_to_index.find(name);
    if (it != name_to_index.end()) {
        return it->second;
    } else {
        return static_cast<size_t>(-1);
    }
}

} // namespace mata::cntnfa.
