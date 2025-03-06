// TODO: Insert file header.

#ifndef COUNTERS_HH
#define COUNTERS_HH

#include "../utils/ord-vector.hh"

namespace mata::cntnfa {

using CounterValue = int;
using CounterValueSet = mata::utils::OrdVector<CounterValue>;

/// Register for counters.
struct CounterRegister {
    CounterValue value; ///< Current counter value.
    CounterValue initial_value; ///< Initial counter value.

    CounterRegister() : value(0), initial_value(0) {}
    CounterRegister(CounterValue value) : value(value), initial_value(value) {}

    CounterRegister(const CounterRegister&) = default;
    CounterRegister(CounterRegister&&) = default;
    CounterRegister& operator=(const CounterRegister&) = default;
    CounterRegister& operator=(CounterRegister&&) = default;

    CounterRegister& operator=(CounterValue other) { value = other; return *this; }

    auto operator<=>(const CounterValue& other) const { return value <=> other; }
    auto operator<=>(const CounterRegister&) const = default;

    operator CounterValue() const { return value; }

    // Increment the counter by 1 (or specified amount).
    void increment(CounterValue amount = 1);

    // Decrement the counter by 1 (or specified amount).
    void decrement(CounterValue amount = 1);

    // Reset the counter to its initial value.
    void reset();
};

/// Set of counter registers.
class CounterRegisterSet {
private:
    std::vector<CounterRegister> counters; ///< Stores counters.

public:
    // TODO: Add the necessary constructors later.
    CounterRegisterSet() = default;

    CounterRegister& operator[](size_t id) { return counters[id]; }
    const CounterRegister& operator[](size_t id) const { return counters[id]; }

    void allocate(const size_t size);
    size_t size() const;
    void clear();
    void insert_counter(CounterRegister counter, size_t index);
    // TODO: Add iterators later.
};

// Added for better readability.
using Counter = CounterRegister;
using CounterSet = CounterRegisterSet;

} // namespace mata::cntnfa.

#endif // COUNTERS_HH
