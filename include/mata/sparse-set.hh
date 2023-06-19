//
// Created by Lukáš Holík on 10.06.2023.
// Based on
/**
	SparseSet.h
	Implements a class template of a sparse
	set of unsigned integers.
	@author Sam Griffiths
	www.computist.xyz
    https://gist.github.com/sjgriffiths/06732c6076b9db8a7cf4dfe3a7aed43a
*/
//there may be better implementations out there, something in boost, or
//https://github.com/lluchs/sparsearray
//https://stackoverflow.com/questions/76205534/is-there-high-performance-data-structure-for-sparse-bit-map-set-semantic-queries
//https://gist.github.com/Gpinchon/19dd224d5e7fc92596e01f5df6508e2a
//https://www.codeproject.com/Articles/859324/Fast-Implementations-of-Sparse-Sets-in-Cplusplus

#ifndef LIBMATA_SPARSE_SET_HH
#define LIBMATA_SPARSE_SET_HH

#include <cassert>
#include <vector>
#include <type_traits>
#include <mata/ord-vector.hh>

namespace Mata::Util {

/**
 * @brief  Implementation of a set of non-negative numbers using sparse-set.
 *
 *
 * This class implements the interface of a set (similar to std::set) using sparse-set date structure,
 * that is, a pair of vectors dense and sparse (... google it).
 * Importantly
 * - Insertion and removal are constant time.
 * - Iteration is linear in the number of stored elements.
 * - It takes a lot of space, the sparse and dense vectors allocate as many indexes as the maximal stored number.
 *
 * @tparam  Number  Number type: type of numbers contained in the container.
 */
    template<typename Number>
    class SparseSet {
        static_assert(std::is_unsigned<Number>::value, "SparseSet can only contain unsigned integers");

    private:
        std::vector<Number> dense;    //Dense set of elements
        std::vector<Number> sparse;    //Map of elements to dense set indices

        size_t size_ = 0;    //Current size (number of elements)
        size_t capacity_ = 0;    //Current capacity (maximum value + 1 or more (can be more when maximum is removed))

    public:
        using iterator = typename std::vector<Number>::const_iterator;
        using const_iterator = typename std::vector<Number>::const_iterator;

        iterator begin() { return dense.begin(); }

        const_iterator begin() const { return dense.begin(); }

        iterator end() { return dense.begin() + size_; }

        const_iterator end() const { return dense.begin() + size_; }

        size_t size() const { return size_; }

        size_t capacity() const { return capacity_; }

        bool empty() const { return size_ == 0; }

        void clear() { size_ = 0;  }

        // TODO: maybe we could reserve space more efficiently, by something as doubling?
        //  But we should not create havoc with capacity, which is used outside, namely for determining the states of an automaton.
        void reserve(size_t u) {
            if (u > capacity_) {
                dense.resize(u, 0);
                sparse.resize(u, 0);
                capacity_ = u;
            }

            assert(consistent());
        }

        bool has(const Number val) const {
            return val < capacity_ &&
                   sparse[val] < size_ &&
                   dense[sparse[val]] == val;
        }

        void insert(const Number val) {
            assert(consistent());

            if (!has(val)) {
                if (val >= capacity_)
                    reserve(val + 1);

                dense[size_] = val;
                sparse[val] = size_;
                ++size_;
            }

            assert(consistent());
        }

        void erase(const Number val) {
            if (has(val)) {
                dense[sparse[val]] = dense[size_ - 1];
                sparse[dense[size_ - 1]] = sparse[val];
                --size_;
            }
        }



//////////////////////////////////////////////////////////////////////////////////
///     New Mata code
//////////////////////////////////////////////////////////////////////////////////

// Constructors:

        SparseSet(Number size = 0, bool val = false): sparse(size, val), dense(size) {
            if (val) {
                size_ = size;
                for (Number e = 0; e < size; ++e)
                    dense.push_back(e);
            }
            else
                size_ = 0;

            assert(consistent());
        };

        //TODO: should this be explicit too? it would mean a lot of fixing expecially in tests, unfortunatelly
        SparseSet(std::initializer_list<Number> list) {
            insert(list.begin(),list.end());

            assert(consistent());
        }

        template <class InputIterator>
        explicit SparseSet(InputIterator first, InputIterator last)
        {
            insert(first,last);

            assert(consistent());
        }

        template <class T>
        explicit SparseSet(T & container)
        {
            insert(container.begin(),container.end());

            assert(consistent());
        }

        //This is actually weird, what if the vector is not boolean valaues but values themselves?
        explicit SparseSet(const std::vector<char> &bv) {
            reserve(bv.size());
            for (size_t i = 0; i < bv.size(); i++) {
                if (bv[i])
                    insert(i);
            }

            assert(consistent());
        }

        SparseSet(const SparseSet<Number>& rhs) = default;
        SparseSet(SparseSet<Number>&& other) noexcept
                : sparse{ std::move(other.sparse) }, dense{ std::move(other.dense) },
                  size_{ other.size_}, capacity_{ other.capacity_} {
            other.size_ = 0;
            other.capacity_ = 0;

            assert(consistent());
        }

        SparseSet<Number>& operator=(const SparseSet <Number>& rhs) = default;
        SparseSet<Number>& operator=(SparseSet<Number>&& other) noexcept {
            if (this != &other) {
                dense = std::move(other.dense);
                sparse = std::move(other.sparse);
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.size_ = 0;
                other.capacity_ = 0;
            }

            assert(consistent());

            return *this;
        }

// Things

        bool consistent() {
            return ( capacity_ >= size_ &&
                     (max() < capacity_ || (size_ == 0 && capacity_ == 0) ) &&
                     dense.size() >= capacity_ &&
                     sparse.size() >= capacity_
            );
        }

        /**
         * @return True if predicate for @p q is set. False otherwise (even if q is out of range of the predicate).
         */
        bool operator[](Number q) const {
            return has(q);
        }

        template <class InputIterator>
        void insert(InputIterator first, InputIterator last) {
            while (first != last) {
                insert(*first);
                first++;
            }
            assert(consistent());
        }

        template<typename T>
        void insert_all(const T & set) {
            insert(set.begin(),set.end());
        }

        void insert_all(const std::initializer_list<Number> & list) {
            insert(list.begin(),list.end());
        }

        template <class InputIterator>
        void erase(InputIterator first, InputIterator last) {
            while (first != last) {
                erase(*first);
                first++;
            }
            assert(consistent());
        }

        template<class T>
        void erase_all(const T & set) {
            erase(set.begin(),set.end());
        }

        void erase_all(const std::initializer_list<Number> & list) {
            erase(list.begin(),list.end());
        }

        //call this (instead of the fried are_disjoint) if you want the other container to be iterated (e.g. if it does not have constant membership)
        template<class T>
        bool intersects_with(const T & set) const {
            for (auto i=set.begin();i<set.end();i++) {
                if (has(*i))
                    return true;
            }
            return false;
        }

        /*
         * Complements the set with respect to a given number of elements = the maximum number + 1.
         */
        //probably can be done little faster
        void complement(Number domain_size) {
            Number orig_capacity = capacity_;
            for (Number i = 0; i<domain_size; ++i) {
                if (has(i))
                    erase(i);
                else
                    insert(i);
            }
            for (Number i = domain_size; i<orig_capacity; ++i) {
                erase(i);
            }

            assert(consistent());
        }

        template<typename F>
        void filter(F && is_staying) {
            for (Number i = 0; i < size_; i++) {
                while (i<size_ && !is_staying(dense[i])) {
                    erase(dense[i]);
                }
            }

            assert(consistent());
        }

        void sort() {
            std::sort(dense.begin(),dense.begin()+size_);
            for (Number i = 0; i<size_; i++) {
                sparse[dense[i]] = i;
            }

            assert(consistent());
        }

        template<typename F>
        void rename(F && renaming) {
            for (Number i = 0; i < size_; i++) {
                if (dense[i] != renaming(dense[i])) {
                    dense[i] = renaming(dense[i]);
                    if (dense[i] >= capacity_)
                        reserve(dense[i]+1);
                    sparse[dense[i]] = i;
                }
            }

            assert(consistent());
        }

        Number max() {
            Number max = 0;
            for (Number i = 0;i<size_;i++) {
                if (max < dense[i])
                    max = dense[i];
            }
            return max;
        }

        void truncate() {
            if (size_ == 0)
                capacity_ = 0;
            else
                capacity_ = max()+1;

            assert(consistent());
        }

        // This could be a template with types of A and B both as parameters, in Util?
        friend bool are_disjoint(const SparseSet & A, const SparseSet & B) {
            if (A.size() > B.size())
                return are_disjoint(B,A);
            else {
                for (auto i: A)
                    if (B.has(i))
                        return false;
            }
            return true;
        }


//Alternative names to make it compatible with the previously used NumberPredicate and other data structures. Let's choose just one naming.

        void add(const Number val) {insert(val);}
        void add(const std::initializer_list<Number> & list) {
            insert_all(list);
        }
        void add(const std::vector<Number> & set) {
            insert_all(set);
        }

        void remove(const Number val) {erase(val);}


    };
}
#endif //LIBMATA_SPARSE_SET_HH