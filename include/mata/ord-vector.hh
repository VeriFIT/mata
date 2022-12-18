/*****************************************************************************
 *  Mata Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    File with the OrdVector class.
 *
 *****************************************************************************/

#ifndef _MATA_ORD_VECTOR_HH_
#define _MATA_ORD_VECTOR_HH_

// Standard library headers
#include <vector>
#include <algorithm>
#include <cassert>

#include <mata/number-predicate.hh>

// insert the class into proper namespace
namespace Mata
{
    namespace Util
    {
        template <class Number> class NumberPredicate;

        template <
            class Key
        >
        class OrdVector;

        template <class Key>
        bool is_sorted(std::vector<Key> vec)
        {
            for (auto itVec = vec.cbegin() + 1; itVec < vec.cend(); ++itVec)
            {	// check that the vector is sorted
                if (!(*(itVec - 1) < *itVec))
                {	// in case there is an unordered pair (or there is one element twice)
                    return false;
                }
            }

            return true;
        }
    }
}

namespace
{
    /**
     * @brief  Converts an object to string
     *
     * Static method for conversion of an object of any class with the << output
     * operator into a string
     *
     * @param[in]  n  The object for the conversion
     *
     * @returns  The string representation of the object
     */
    template <typename T>
    static std::string ToString(const T& n)
    {
        // the output stream for the string
        std::ostringstream oss;
        // insert the object into the stream
        oss << n;
        // return the string
        return oss.str();
    }

}


/**
 * @brief  Implementation of a set using ordered vector
 *
 * This class implements the interface of a set (the same interface as
 * std::set) using ordered vector as the underlying data structure.
 *
 * @tparam  Key  Key type: type of the elements contained in the container.
 *               Each elements in a set is also its key.
 */
template
<
    class Key
>
class Mata::Util::OrdVector
{
private:  // Private data types
    using VectorType = std::vector<Key>;

public:   // Public data types
    using iterator = typename VectorType::iterator ;
    using const_iterator = typename VectorType::const_iterator;
    using const_reference = typename VectorType::const_reference;

private:  // Private data members

    VectorType vec_;


private:  // Private methods

    bool vectorIsSorted() const
    {
        return(Mata::Util::is_sorted(vec_));
    }


public:   // Public methods

    OrdVector() :
        vec_()
    {
        // Assertions
        assert(vectorIsSorted());
    }

    explicit OrdVector(const VectorType& vec) :
        vec_(vec)
    {
        //if (vectorIsSorted()) return;//probably useless

        // sort
        std::sort(vec_.begin(), vec_.end());

        // remove duplicates
        auto it = std::unique(vec_.begin(), vec_.end());
        vec_.resize(it - vec_.begin());

        // Assertions
        assert(vectorIsSorted());
    }

    OrdVector(std::initializer_list<Key> list) :
        vec_(list)
    {
        // sort
        std::sort(vec_.begin(), vec_.end());

        // remove duplicates
        auto it = std::unique(vec_.begin(), vec_.end());
        vec_.resize(it - vec_.begin());

        // Assertions
        assert(vectorIsSorted());
    }

    OrdVector(const OrdVector& rhs) :
        vec_()
    {
        // Assertions
        assert(rhs.vectorIsSorted());

        if (&rhs != this)
        {
            vec_ = rhs.vec_;
        }

        // Assertions
        assert(vectorIsSorted());
    }

    explicit OrdVector(const Key& key) :
        vec_(1, key)
    {
        // Assertions
        assert(vectorIsSorted());
    }

    OrdVector(const NumberPredicate<Key>& p) : OrdVector(p.get_elements()) {};


    template <class InputIterator>
    OrdVector(InputIterator first, InputIterator last) :
        vec_(first, last)
    {
        // sort
        std::sort(vec_.begin(), vec_.end());

        // remove duplicates
        auto it = std::unique(vec_.begin(), vec_.end());
        vec_.resize(it - vec_.begin());

        // Assertions
        assert(vectorIsSorted());
    }

    virtual ~OrdVector() = default;

    /**
     * Create OrdVector with reserved @p capacity.
     * @param[in] capacity Capacity of OrdVector to reserve.
     * @return Newly create OrdVector.
     */
    static OrdVector with_reserved(const size_t capacity) {
        OrdVector ord_vector{};
        ord_vector.vec_.reserve(capacity);
        return ord_vector;
    }

    OrdVector& operator=(const OrdVector& rhs)
    {
        // Assertions
        assert(rhs.vectorIsSorted());

        if (&rhs != this)
        {
            vec_ = rhs.vec_;
        }

        // Assertions
        assert(vectorIsSorted());

        return *this;
    }


    void insert(iterator itr, const Key& x)
    {
        assert(itr == this->end() || x <= *itr);
        vec_.insert(itr,x);
    }

    virtual void insert(const Key& x)
    {
        // Assertions
        assert(vectorIsSorted());

        // perform binary search (cannot use std::binary_search because it is
        // ineffective due to not returning the iterator to the position of the
        // desirable insertion in case the searched element is not present in the
        // range)
        size_t first = 0;
        size_t last = vec_.size();

        if ((last != 0) && (vec_.back() < x))
        {	// for the case which would be prevalent
            vec_.push_back(x);
            return;
        }

        while (first < last)
        {	// while the pointers do not overlap
            size_t middle = first + (last - first) / 2;
            if (vec_[middle] == x)
            {	// in case we found x
                return;
            }
            else if (vec_[middle] < x)
            {	// in case middle is less than x
                first = middle + 1;
            }
            else
            {	// in case middle is greater than x
                last = middle;
            }
        }

        vec_.resize(vec_.size() + 1);
        std::copy_backward(vec_.begin() + first, vec_.end() - 1, vec_.end());

        // insert the new element
        vec_[first] = x;

        // Assertions
        assert(vectorIsSorted());
    }


    virtual void insert(const OrdVector& vec)
    {
        // Assertions
        assert(vectorIsSorted());
        assert(vec.vectorIsSorted());

        OrdVector result = this->Union(vec);
        vec_ = result.vec_;

        // Assertions
        assert(vectorIsSorted());
    }

    inline void clear()
    {
        // Assertions
        assert(vectorIsSorted());

        vec_.clear();
    }

    virtual inline size_t size() const
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.size();
    }


    inline size_t count(const Key& key) const
    {
        // Assertions
        assert(vectorIsSorted());

        for (auto v : this->vec_)
        {
            if (v == key)
                return 1;
            else if (v > key)
                return 0;
        }

        return 0;
    }

    OrdVector intersection(const OrdVector& rhs) const
    {
        // Assertions
        assert(vectorIsSorted());
        assert(rhs.vectorIsSorted());

        VectorType newVector{};

        auto lhsIt = vec_.begin();
        auto rhsIt = rhs.vec_.begin();

        while ((lhsIt != vec_.end()) && (rhsIt != rhs.vec_.end()))
        {	// until we get to the end of both vectors
            if (*lhsIt == *rhsIt)
            {
                newVector.push_back(*lhsIt);

                ++lhsIt;
                ++rhsIt;
            }
            else if (*lhsIt < *rhsIt)
            {
                ++lhsIt;
            }
            else if (*rhsIt < *lhsIt)
            {
                ++rhsIt;
            }
        }

        OrdVector result(newVector);

        // Assertions
        assert(result.vectorIsSorted());

        return result;
    }

    OrdVector Union(const OrdVector& rhs) const
    {
        // Assertions
        assert(vectorIsSorted());
        assert(rhs.vectorIsSorted());

        VectorType newVector;

        auto lhsIt = vec_.begin();
        auto rhsIt = rhs.vec_.begin();

        while ((lhsIt != vec_.end()) || (rhsIt != rhs.vec_.end()))
        {	// until we get to the end of both vectors
            if (lhsIt == vec_.end())
            {	// if we are finished with the left-hand side vector
                newVector.push_back(*rhsIt);
                ++rhsIt;
            }
            else if (rhsIt == rhs.vec_.end())
            {	// if we are finished with the right-hand side vector
                newVector.push_back(*lhsIt);
                ++lhsIt;
            }
            else
            {
                if (*lhsIt < *rhsIt)
                {
                    newVector.push_back(*lhsIt);
                    ++lhsIt;
                }
                else if (*rhsIt < *lhsIt)
                {
                    newVector.push_back(*rhsIt);
                    ++rhsIt;
                }
                else
                {	// in case they are equal
                    newVector.push_back(*rhsIt);
                    ++rhsIt;
                    ++lhsIt;
                }
            }
        }

        OrdVector result(newVector);

        // Assertions
        assert(result.vectorIsSorted());

        return result;
    }

    virtual const_iterator find(const Key& key) const
    {
        // Assertions
        assert(vectorIsSorted());

        size_t first = 0;
        size_t last = vec_.size();

        while (first < last)
        {	// while the pointers do not overlap
            size_t middle = first + (last - first) / 2;
            if (vec_[middle] == key)
            {	// in case we found x
//				return const_iterator(&vec_[middle]);
                return vec_.cbegin() + middle;
            }
            else if (vec_[middle] < key)
            {	// in case middle is less than x
                first = middle + 1;
            }
            else
            {	// in case middle is greater than x
                last = middle;
            }
        }

        return end();
    }

    virtual iterator find(const Key& key)
    {
        // Assertions
        assert(vectorIsSorted());

        size_t first = 0;
        size_t last = vec_.size();

        while (first < last)
        {	// while the pointers do not overlap
            size_t middle = first + (last - first) / 2;
            if (vec_[middle] == key)
            {	// in case we found x
                return vec_.begin() + middle;
            }
            else if (vec_[middle] < key)
            {	// in case middle is less than x
                first = middle + 1;
            }
            else
            {	// in case middle is greater than x
                last = middle;
            }
        }

        return end();
    }

    inline void remove(Key k)
    {
        assert(vectorIsSorted());
        vec_.erase(std::remove(vec_.begin(), vec_.end(), k), vec_.end());
        assert(vectorIsSorted());
    }

    virtual inline bool empty() const
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.empty();
    }

    virtual inline const_reference back() const
    {
        // Assertions
        assert(vectorIsSorted());
        
        return vec_.back();
    }


    virtual inline const_iterator begin() const
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.begin();
    }

    virtual inline const_iterator end() const
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.end();
    }


    virtual inline iterator begin()
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.begin();
    }

    virtual inline iterator end()
    {
        // Assertions
        assert(vectorIsSorted());

        return vec_.end();
    }

	virtual inline const_iterator cbegin() const
	{
		// Assertions
		assert(vectorIsSorted());

		return begin();
	}

	virtual inline const_iterator cend() const
	{
		// Assertions
		assert(vectorIsSorted());

		return end();
	}

	/**
	 * @brief  Overloaded << operator
	 *
	 * Overloaded << operator for output stream.
	 *
	 * @see  to_string()
	 *
	 * @param[in]  os    The output stream
	 * @param[in]  vec   Assignment to the variables
	 *
	 * @returns  Modified output stream
	 */
	friend std::ostream& operator<<(std::ostream& os, const OrdVector& vec)
	{
		// Assertions
		assert(vec.vectorIsSorted());

		std::string result = "{";

		for (auto it = vec.cbegin(); it != vec.cend(); ++it)
		{
			result += ((it != vec.begin())? ", " : " ") + ToString(*it);
		}

		return os << (result + "}");
	}

	bool operator==(const OrdVector& rhs) const
	{
		// Assertions
		assert(vectorIsSorted());
		assert(rhs.vectorIsSorted());

		return (vec_ == rhs.vec_);
	}

    bool operator!=(const OrdVector& rhs) const
    {
        // Assertions
        assert(vectorIsSorted());
        assert(rhs.vectorIsSorted());

        return (vec_ != rhs.vec_);
    }

    bool operator<(const OrdVector& rhs) const
    {
        // Assertions
        assert(vectorIsSorted());
        assert(rhs.vectorIsSorted());

        return std::lexicographical_compare(vec_.begin(), vec_.end(),
            rhs.vec_.begin(), rhs.vec_.end());
    }

    const std::vector<Key>& ToVector() const
    {
        return vec_;
    }

    bool IsSubsetOf(const OrdVector& bigger) const
    {
        return std::includes(bigger.cbegin(), bigger.cend(),
            this->cbegin(), this->cend());
    }

    bool HaveEmptyIntersection(const OrdVector& rhs) const
    {
        // Assertions
        assert(vectorIsSorted());
        assert(rhs.vectorIsSorted());

        const_iterator itLhs = begin();
        const_iterator itRhs = rhs.begin();

        while ((itLhs != end()) && (itRhs != rhs.end()))
        {	// until we drop out of the array (or find a common element)
            if (*itLhs == *itRhs)
            {	// in case there exists a common element
                return false;
            }
            else if (*itLhs < *itRhs)
            {	// in case the element in lhs is smaller
                ++itLhs;
            }
            else
            {	// in case the element in rhs is smaller
                assert(*itLhs > *itRhs);
                ++itRhs;
            }
        }

        return true;
    }
};

namespace std {
    template <class Key>
    struct hash<Mata::Util::OrdVector<Key>>
    {
        std::size_t operator()(const Mata::Util::OrdVector<Key>& vec) const
        {
            return std::hash<std::vector<Key>>{}(vec.ToVector());
        }
    };
}

#endif
