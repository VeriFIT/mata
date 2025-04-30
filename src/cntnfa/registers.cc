// TODO: Insert header file.

#include <limits>
#include <stdexcept>
#include <cassert>

#include "mata/cntnfa/registers.hh"

namespace mata::cntnfa {

/*
    Methods for Register.
*/

void Register::increment(RegisterValue value) {
    if (this->value > std::numeric_limits<RegisterValue>::max() - value) {
        throw std::overflow_error("Register: Increment operation would result in overflow.");
    }
    this->value += value;
}

void Register::decrement(RegisterValue value) {
    if (value > this->value) {
        throw std::underflow_error("Register: Decrement operation would result in a negative value.");
    }
    this->value -= value;
}

void Register::set(RegisterValue value) {
    if (value < 0) {
        throw std::invalid_argument("Register: Value cannot be negative.");
    }
    this->value = value;
}

void Register::reset() {
    this->value = this->initial_value;
}

/*
    Methods for RegisterSet.
*/

void RegisterSet::reserve(const size_t n) {
    counters.reserve(n);
}

void RegisterSet::allocate(const size_t new_size) {
    assert(new_size >= counters.size());
    counters.resize(new_size);
}

size_t RegisterSet::size() const {
    return counters.size();
}

void RegisterSet::clear() {
    counters.clear();
}

void RegisterSet::set(size_t index, const Register& counter) {
    if (index >= counters.size()) {
        this->allocate(index + 1);
    }
    counters[index] = counter;
    name_to_index[counter.name] = index;
}

size_t RegisterSet::add(const RegisterName& name, RegisterValue value) {
    if (name_to_index.contains(name)) {
        throw std::invalid_argument("RegisterSet: A register with the same name already exists.");
    }
    size_t index = counters.size();
    counters.emplace_back(value, name);
    name_to_index[name] = index;
    return index;
}

size_t RegisterSet::add_with_prefix(const std::string& prefix, RegisterValue value) {
    std::string name = prefix + std::to_string(counters.size());
    return add(name, value);
}

bool RegisterSet::has(const RegisterName& name) const {
    return name_to_index.contains(name);
}

const RegisterName& RegisterSet::get_name(size_t index) const {
    return counters.at(index).name;
}

std::vector<RegisterName> RegisterSet::get_names() const {
    std::vector<RegisterName> names;
    names.reserve(counters.size());
    for (size_t i = 0; i < counters.size(); ++i) {
        names.push_back(get_name(i));
    }
    return names;
}

size_t RegisterSet::get_index(const RegisterName& name) const {
    auto it = name_to_index.find(name);
    if (it != name_to_index.end()) {
        return it->second;
    } else {
        return static_cast<size_t>(-1);
    }
}

const Register& RegisterSet::get(size_t index) const {
    if (index >= counters.size()) {
        throw std::out_of_range("RegisterSet: Index out of range.");
    }
    return counters[index];
}

} // namespace mata::cntnfa.
