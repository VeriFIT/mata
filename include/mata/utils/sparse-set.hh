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

#include <concepts>
#include <iterator>
#include <cassert>
#include <vector>
#include <type_traits>

#include "ord-vector.hh"

namespace mata::utils {

template <typename T>
concept Iterable = requires(T t) {
    typename std::iterator_traits<decltype(begin(t))>::value_type;
    { begin(t) } -> std::same_as<decltype(end(t))>;
};

/**
 * @brief  Implementation of a set of non-negative numbers using sparse-set.
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
        std::vector<Number> dense{}; // Dense set of elements.
        std::vector<Number> sparse{}; // Map of elements to dense set indices.

        /// Number of elements which are in the set (current size).
        size_t size_ = 0;

        /// @brief Over-approximation of Number in sparse set. It represents the Numbers which were in the set throughout the
        ///  life of the set until now.
        ///
        /// `truncate()` will update the domain size to the current maximal Number + 1.
        /// Constructor can preallocate the sparse set and set domain size to the requested value.
        /// The structures are preallocated to at least @c domain_size_ size (can be more when maximal Number is removed).
        size_t domain_size_ = 0;

    public:
        using iterator = typename std::vector<Number>::const_iterator;
        using const_iterator = typename std::vector<Number>::const_iterator;

        iterator begin() { return dense.begin(); }

        const_iterator begin() const { return dense.begin(); }

        iterator end() { return dense.begin() + static_cast<long>(size_); }

        const_iterator end() const { return dense.begin() + static_cast<long>(size_); }

        size_t size() const { return size_; }
        size_t domain_size() const { return domain_size_; }

        bool empty() const { return size_ == 0; }

        void clear() { size_ = 0;  }

        // TODO: maybe we could reserve space more efficiently, by something as doubling?
        //  But we should not create havoc with domain_size, which is used outside, namely for determining the states of an automaton.
        void reserve(size_t u) {
            if (u > domain_size_) {
                dense.resize(u, 0);
                sparse.resize(u, 0);
                domain_size_ = u;
            }
            assert(consistent());
        }

        bool contains(const Number val) const {
            return val < domain_size_ &&
                   sparse[val] < size_ &&
                   dense[sparse[val]] == val;
        }

        void insert(const Number val) {
            assert(consistent());

            if (!contains(val)) {
                if (static_cast<size_t>(val) >= domain_size_) {
                    reserve(val + 1);
                }
                dense[size_] = val;
                sparse[val] = static_cast<Number>(size_);
                ++size_;
            }

            assert(consistent());
        }

        /**
         * Erase @p number from the set without checking for its existence.
         * @param number Number to be erased.
         * @pre @p number exists in the set.
         */
        void erase_nocheck(const Number number) {
            dense[sparse[number]] = dense[size_ - 1];
            sparse[dense[size_ - 1]] = sparse[number];
            --size_;
        }

        void erase(const Number val) { if (contains(val)) { erase_nocheck(val); } }

//////////////////////////////////////////////////////////////////////////////////
///     New Mata code
//////////////////////////////////////////////////////////////////////////////////

// Constructors:

        SparseSet() = default;
        /// Basic constructor where you may reserve a domain size.
        explicit SparseSet(Number domain_size): dense(domain_size), sparse(domain_size, 0), size_(0), domain_size_(domain_size) {
            assert(consistent());
        };

        SparseSet(std::initializer_list<Number> list) {
            insert(list.begin(),list.end());
            assert(consistent());
        }

        template <class InputIterator>
        explicit SparseSet(InputIterator first, InputIterator last) {
            insert(first,last);
            assert(consistent());
        }

        template <class T>
        explicit SparseSet(T & container)
        {
            insert(container.begin(),container.end());
            assert(consistent());
        }

        //This is actually weird, what if the vector is not boolean values but values themselves?
        explicit SparseSet(const BoolVector &bv) {
            reserve(bv.size());
            for (size_t i = 0; i < bv.size(); i++) {
                if (bv[i])
                    insert(i);
            }
            assert(consistent());
        }

        SparseSet(const SparseSet<Number>& rhs) = default;
        SparseSet(SparseSet<Number>&& other) noexcept
                : dense{ std::move(other.dense) }, sparse{ std::move(other.sparse) },
                  size_{ other.size_ }, domain_size_{ other.domain_size_ } {
            other.size_ = 0;
            other.domain_size_ = 0;
            assert(consistent());
        }

        SparseSet<Number>& operator=(const SparseSet <Number>& rhs) = default;
        SparseSet<Number>& operator=(SparseSet<Number>&& other) noexcept {
            if (this != &other) {
                dense = std::move(other.dense);
                sparse = std::move(other.sparse);
                size_ = other.size_;
                domain_size_ = other.domain_size_;
                other.size_ = 0;
                other.domain_size_ = 0;
            }
            assert(consistent());
            return *this;
        }

        // TODO: How do we want to define equality of sparse sets? The default one is member-wise, but maybe simply
        //  comparing contained elements should be enough?
        bool operator==(const SparseSet<Number>&) const = default;

// Things

        // Tests the basic invariant of the sparse set.
        bool consistent() {
            return (domain_size_ >= size_ &&
                    (max() < domain_size_ || (size_ == 0 && domain_size_ == 0) ) &&
                    dense.size() >= domain_size_ &&
                    sparse.size() >= domain_size_
            );
        }

        /**
         * @return True if predicate for @p q is set. False otherwise (even if q is out of range of the predicate).
         */
        bool operator[](Number q) const {
            return contains(q);
        }

        template <class InputIterator>
        void insert(InputIterator first, InputIterator last) {
            while (first != last) {
                insert(*first);
                first++;
            }
            assert(consistent());
        }

        template<Iterable T>
        void insert(const T & set) {
            insert(set.begin(),set.end());
        }

        void insert(const std::initializer_list<Number> & list) {
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

        template<Iterable T>
        void erase(const T & set) { erase(set.begin(),set.end()); }

        void erase(const std::initializer_list<Number> & list) { erase(list.begin(),list.end()); }

        //call this (instead of the friend are_disjoint) if you want the other container to be iterated (e.g. if it does not have constant membership)
        template<class T>
        bool intersects_with(const T & set) const {
            for (auto i=set.begin();i<set.end();i++) {
                if (contains(*i))
                    return true;
            }
            return false;
        }

        /*
         * Complements the set with respect to a given number of elements = the maximum number + 1.
         */
        void complement(Number new_domain_size) {
            Number old_domain_size = domain_size_;
            for (Number i = 0; i < new_domain_size; ++i) {
                if (contains(i))
                    erase_nocheck(i);
                else
                    insert(i);
            }
            for (Number i = new_domain_size; i < old_domain_size; ++i) {
                erase(i);
            }

            assert(consistent());
        }

        template<typename F>
        void filter(F && is_staying) {
            for (Number i{ 0 }; i < size_; ++i) {
                while (i < size_ && !is_staying(dense[i])) {
                    erase(dense[i]);
                }
            }

            assert(consistent());
        }

        void sort() {
            std::sort(dense.begin(), dense.begin() + static_cast<long>(size_));
            for (Number i = 0; i < size_; i++) {
                sparse[dense[i]] = i;
            }

            assert(consistent());
        }

        template<typename F>
        void rename(F && renaming) {
            for (Number i = 0; i < size_; i++) {
                if (dense[i] != renaming(dense[i])) {
                    dense[i] = renaming(dense[i]);
                    if (dense[i] >= domain_size_)
                        reserve(dense[i]+1);
                    sparse[dense[i]] = i;
                }
            }

            assert(consistent());
        }

        /// @brief Maximal Number in set.
        ///
        /// Expensive operation as it has to compute the maximal Number in linear time.
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
                domain_size_ = 0;
            else
                domain_size_ = max() + 1;

            assert(consistent());
        }

        // This could be a template with types of A and B both as parameters, in utils?
        friend bool are_disjoint(const SparseSet & A, const SparseSet & B) {
            if (A.size() > B.size())
                return are_disjoint(B,A);
            else {
                for (auto i: A)
                    if (B.contains(i))
                        return false;
            }
            return true;
        }
    };
}
#endif //LIBMATA_SPARSE_SET_HH
