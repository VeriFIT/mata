// TODO: Insert file header.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace mata::cntnfa {

using RegisterValue = int;
using RegisterName = std::string;

/// Register with operations.
struct Register {
    RegisterValue value; ///< Current register value.
    RegisterValue initial_value; ///< Initial register value.
    RegisterName name; ///< Name of the register.

    Register() : value(0), initial_value(0), name() {}
    Register(RegisterValue value, RegisterName name)
        : value(value), initial_value(value), name(name) {}

    Register(const Register&) = default;
    Register(Register&&) = default;
    Register& operator=(const Register&) = default;
    Register& operator=(Register&&) = default;

    Register& operator=(RegisterValue other) { value = other; return *this; }

    auto operator<=>(const RegisterValue& other) const { return value <=> other; }
    auto operator<=>(const Register&) const = default;

    operator RegisterValue() const { return value; }

    // Increment the register value by 1 (or specified value).
    void increment(RegisterValue value = 1);

    // Decrement the register value by 1 (or specified value).
    void decrement(RegisterValue value = 1);

    // Set the register value to a specific value.
    void set(RegisterValue value);

    // Reset the register to its initial value.
    void reset();
};

/// Set of registers.
class RegisterSet {
private:
    std::vector<Register> counters; ///< Stores registers.
    std::unordered_map<RegisterName, size_t> name_to_index; ///< Maps register name to its index.

public:
    RegisterSet() : counters(), name_to_index() {}

    RegisterSet copy() const {
        RegisterSet copy = *this;
        return copy;
    }

    bool operator==(const RegisterSet& other) const {
        return counters == other.counters;
    }

    Register& operator[](size_t id) { return counters[id]; }
    const Register& operator[](size_t id) const { return counters[id]; }
    RegisterValue get_value(size_t index) const { return counters[index].value; }

    void reserve(const size_t n);

    void allocate(const size_t new_size);

    size_t size() const;

    void clear();

    void set(size_t index, const Register& counter);
    size_t add(const RegisterName& name, RegisterValue value = 0);
    size_t add_with_prefix(const std::string& prefix, RegisterValue value = 0);

    bool has(const RegisterName& name) const;
    const RegisterName& get_name(size_t index) const;
    std::vector<RegisterName> get_names() const;
    size_t get_index(const RegisterName& name) const;

    const Register& get(size_t index) const;
};

} // namespace mata::cntnfa.
