#ifndef BOOST_VECTOR_HH
#define BOOST_VECTOR_HH

#ifdef USE_BOOST

#include <vector>
#include <algorithm>
#include <cassert>
#include "boost/dynamic_bitset.hpp"
#include "utils.hh"

namespace mata::utils {

class BoostVector {
    // Data types
    public:
        using State = unsigned long;
        using BoostSet = boost::dynamic_bitset<State>;

    // Members
    public:
        BoostSet states;
        static constexpr std::size_t npos = BoostSet::npos; // Out of bounds value

    // Methods
    public:
        // Constructors
        BoostVector() : states() {}
        BoostVector(size_t num_bits) : states(num_bits) {}

        // Construct with std::vector of states
        explicit BoostVector(const std::vector<State>& set) : states() {
            if (!set.empty()) {
                states.resize(*std::max_element(set.begin(), set.end()) + 1);
                for (const auto& elem : set) states[elem] = 1;
            }
        }

        // Vector of size count with all bits on a certain value
        BoostVector(size_t count, bool value) : states(count, value) {}

        // Vector of size "target + 1" (since to index with target we need target + 1 size) with target set to "value_target"
        // Note: other bits are set to value_others
        BoostVector(State target, bool value_other, bool value_target) : states(target + 1, value_other)
        {
            if(states[target] != value_target) set(target, value_target);
        }

        // Template constructor for any container type (like sparse sets) with begin/end iterators
        template <class SetIterator>
        BoostVector(SetIterator first, SetIterator last) : states() {
            if (first != last) {
                auto max_elem = *std::max_element(first, last);
                states.resize(max_elem + 1); // so we can index with max elem
                for (auto it = first; it != last; ++it) states.set(*it, true);
            }
        }

        // Friend (two inputs) operators

        // Set union
        friend BoostVector operator|(const BoostVector& lhs, const BoostVector& rhs) {
            BoostVector result(lhs);
            result |= rhs;
            return result;
        }

        // Set intersection
        friend BoostVector operator&(const BoostVector& lhs, const BoostVector& rhs) {
            BoostVector result(lhs);
            result &= rhs;
            return result;
        }

        // Set difference
        friend BoostVector operator-(const BoostVector& lhs, const BoostVector& rhs) {
            BoostVector result(lhs);
            result -= rhs;
            return result;
        }

        // "Operator=" type operators (update the current value)

        // Or operator, useful for unifying with another boost vector
        BoostVector& operator|=(const BoostVector& rhs) {
            if(states.size() > rhs.states.size())
            {
                BoostVector tmp {rhs};
                tmp.resize(states.size());
                states |= tmp.states;
                return *this;
            }

            // Resize if needed to accommodate the rhs bitset
            if (rhs.states.size() > states.size()) {
                states.resize(rhs.states.size(), false);
            }
    
            states |= rhs.states;
    
            return *this;
        }

        // Invertion unary operator
        BoostVector& operator ~()
        {
            states.flip();
            return *this;
        }

        // Inversion operator with copy
        BoostVector operator ~() const
        {
            BoostVector result(*this);
            result.states.flip();
            return result;
        }

        // And operator
        BoostVector& operator&=(const BoostVector& rhs) {
            if(states.size() > rhs.states.size())
            {
                BoostVector tmp {rhs};
                tmp.resize(states.size());
                states &= tmp.states;
                return *this;
            }

            // Resize if needed to accommodate the rhs bitset
            if (rhs.states.size() > states.size()) {
                states.resize(rhs.states.size(), false);
            }

            states &= rhs.states;

            return *this;
        }

        // Set difference operator ({this} / {rhs})
        BoostVector& operator -=(const BoostVector& rhs)
        {
            // Resize if needed to accommodate the rhs bitset
            if (rhs.states.size() > states.size()) {
                states.resize(rhs.states.size());
            }

            // Up to rhs's size
            const size_t rhs_size = rhs.states.size();

            // All bits that are set in rhs are unset in this
            for(size_t i = 0; i < rhs_size; ++i) {
                states[i] &= !rhs.states[i];
            }

            return *this;
        }

        // Indexing operator, indexes into the vector (also returns false if out of bounds)
        bool operator[](State index) const { return index < states.size() && states[index]; }


        // Find methods

        // Returns the first position with a true value
        size_t find_first() const {
            return states.find_first();
        }

        // Returns the first position after "pos" with a true value
        State find_next(boost::dynamic_bitset<>::size_type pos) const {
            return states.find_next(pos);
        }

        // Set a bit on a specific position to "value", resize if needed
        void set(State index, bool value = true) {
            if (index >= states.size()) states.resize(index + 1);
            states[index] = value;
        }

        // Basically just a wrapper for set
        void insert(State s) { set(s, true); }

        // Resets the states vector
        void clear() { states.reset(); }

        // Ord vector has this tbh 
        inline void reserve(size_t size) { states.reserve(size); }

        size_t size() const { return states.count(); }

        // Return true if the vector has the state "S"
        bool get_value(const State &S) const { return (S < states.size() && states[S]); }

        // Wrapper for get_value to integrate with mata
        bool contains(const State &S) const { return get_value(S); }

        bool empty() const { return states.none(); }

        bool any() const { return states.any(); }

        // Set operations

        // If i invert the superset, an AND will give me a zero vector if the "states" vector is really a subset
        bool is_subset_of(const BoostVector &super_set) const {
            if(states.size() > super_set.states.size()) return false;

            else if(states.size() != super_set.states.size())
            {
                BoostVector tmp {super_set};
                tmp.resize(states.size());
                return (states.is_subset_of(tmp.states));
            }

            return (states.is_subset_of(super_set.states));
        }

        // One mutual state is enough
        bool intersects_with(const BoostVector &compared) const {
            if(states.size() != compared.states.size())
            {
                BoostVector tmp {compared};
                tmp.resize(states.size());
                return (states.intersects(tmp.states));
            }

            return (states.intersects(compared.states));
        }

        // Normal operators
        bool operator==(const BoostVector &compared) const { return states == compared.states; }
        bool operator<(const BoostVector &compared) const {
            return states.to_ulong() < compared.states.to_ulong();
        }

        void resize(size_t new_size) {
            states.resize(new_size);
        }

        inline bool none() const { return states.none(); }

        /* Returns a vector of indices which have their values set to true
            - For example, the BoostVector 01001101 will be converted into {1, 4, 5, 7}
        */
        std::vector<State> to_vector() const {
            std::vector<State> elements;
            for (size_t i = 0; i < states.size(); ++i) {
                
                if (states[i]) elements.push_back(i);
            }
            return elements;
        }

        // Returns the first element in the bit vector
        State first() const {
            return states.find_first();
        }

        // Returns the last element in the bit vector
        State last() const {
            return states.find_next(states.size());
        }

        // Debug function, prints out the vector
        void print(bool show_nonincluded = false) const {
            for (size_t i = 0; i < states.size(); ++i) {
                if (states[i]) std::cout << i << " ";
                else if(show_nonincluded) std::cout << "State " << i << " is not included" << std::endl;
            }
        }

}; // Class BoostVector.

} // Utils namespace end


// To allow use of unordered map
namespace std {
    template<>
    struct hash<mata::utils::BoostVector> {
        std::size_t operator()(const mata::utils::BoostVector &bs) const {
            // Use Boost's hash_range function to hash the internal block representation
            return boost::hash_value(bs.states);
        }
    };
}

#endif // USE_BOOST
#endif // BOOST_VECTOR_HH
