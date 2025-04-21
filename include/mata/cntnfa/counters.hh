// TODO: Insert file header.

#ifndef COUNTERS_HH
#define COUNTERS_HH

#include "../utils/ord-vector.hh"

namespace mata::cntnfa {

using CounterValue = int;
using CounterName = std::string;
using CounterValueSet = mata::utils::OrdVector<CounterValue>;

/// Register for counters.
struct CounterRegister {
    CounterValue value; ///< Current counter value.
    CounterValue initial_value; ///< Initial counter value.
    CounterName name; ///< Name of the counter.

    CounterRegister() : value(0), initial_value(0), name() {}
    CounterRegister(CounterValue value, CounterName name)
        : value(value), initial_value(value), name(name) {}

    CounterRegister(const CounterRegister&) = default;
    CounterRegister(CounterRegister&&) = default;
    CounterRegister& operator=(const CounterRegister&) = default;
    CounterRegister& operator=(CounterRegister&&) = default;

    CounterRegister& operator=(CounterValue other) { value = other; return *this; }

    auto operator<=>(const CounterValue& other) const { return value <=> other; }
    auto operator<=>(const CounterRegister&) const = default;

    operator CounterValue() const { return value; }

    // Increment the counter by 1 (or specified value).
    void increment(CounterValue value = 1);

    // Decrement the counter by 1 (or specified value).
    void decrement(CounterValue value = 1);

    // Set the counter to a specific value.
    void set(CounterValue value);

    // Reset the counter to its initial value.
    void reset();
};

/// Set of counter registers.
class CounterRegisterSet {
private:
    std::vector<CounterRegister> counters; ///< Stores counters.
    std::unordered_map<CounterName, size_t> name_to_index; ///< Maps counter name to its index.

public:
    // TODO: Add the necessary constructors later.
    CounterRegisterSet() : counters(), name_to_index() {}

    CounterRegister& operator[](size_t id) { return counters[id]; }
    const CounterRegister& operator[](size_t id) const { return counters[id]; }

    void reserve(const size_t n);

    void allocate(const size_t new_size);

    size_t size() const;

    void clear();

    void set(size_t index, const CounterRegister& counter);
    size_t insert(const CounterName& name, CounterValue value = 0);

    bool has(const CounterName& name) const;
    const CounterName& get_name(size_t index) const;
    std::vector<CounterName> get_names() const;
    size_t get_index(const CounterName& name) const;

    const CounterRegister& get(size_t index) const;
    // TODO: Add iterators later.
};

// Added for better readability.
using Counter = CounterRegister;
using CounterSet = CounterRegisterSet;

} // namespace mata::cntnfa.

#endif // COUNTERS_HH
