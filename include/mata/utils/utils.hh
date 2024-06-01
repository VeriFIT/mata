/* utils.hh -- various utilities
 */

#ifndef MATA_UTIL_HH_
#define MATA_UTIL_HH_

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <ranges>
#include <cstdint>

/// macro for debug outputs
#define PRINT_VERBOSE_LVL(lvl, title, x) {\
	if (mata::LOG_VERBOSITY >= lvl) {\
		std::cerr << title << ": " << x << "\n";\
	}\
}

#define PRINT_VERBOSE_LVL_LN(lvl, title, x) {\
	PRINT_VERBOSE_LVL(lvl, title, __FILE__ << ":" << __func__ << ":" << __LINE__ << ": ")\
}

// #define DEBUG_PRINT(x) { std::cerr << "debug: " << x << "\n"; }
#define DEBUG_PRINT(x) { PRINT_VERBOSE_LVL(2, "debug", x); }
#define DEBUG_PRINT_LN(x) { PRINT_VERBOSE_LVL_LN(2, "debug", x); }
#define DEBUG_VM_HIGH_PRINT(x) { PRINT_VERBOSE_LVL(3, "debug VM", x); }
#define DEBUG_VM_HIGH_PRINT_LN(x) { PRINT_VERBOSE_LVL_LN(3, "debug VM", x); }
#define DEBUG_VM_LOW_PRINT(x) { PRINT_VERBOSE_LVL(4, "debug VM", x); }
#define DEBUG_VM_LOW_PRINT_LN(x) { PRINT_VERBOSE_LVL_LN(4, "debug VM", x); }
#define WARN_PRINT(x) { PRINT_VERBOSE_LVL(1, "warning", x); }

/**
 * Main namespace including structs and algorithms for all automata.
 *
 * In particular, this includes:
 *   1. Alphabets,
 *   2. Formula graphs and nodes,
 *   3. Mintermization,
 *   4. Closed sets.
 */
namespace mata {

/// Representation of bool vector by a vector of uint8_t.
class BoolVector : public std::vector<uint8_t> {
public:
    BoolVector(size_t size, bool value) : std::vector<uint8_t>(size, value ? 1 : 0) {}
    BoolVector(const BoolVector&) = default;
    BoolVector(BoolVector&&) noexcept = default;
    BoolVector() = default;
    BoolVector(std::initializer_list<uint8_t> uint8_ts): std::vector<uint8_t>(uint8_ts) {}
    explicit BoolVector(const std::vector<uint8_t>& uint8_ts): std::vector<uint8_t>(uint8_ts) {}

    BoolVector& operator=(const BoolVector&) = default;
    BoolVector& operator=(BoolVector&&) noexcept = default;

    /// Count the number of set elements.
    size_t count() const {
        size_t cnt{ 0 };
        for (const uint8_t value : *this) {
            if (value == 1) {
                ++cnt;
            }
        }
        return cnt;
    }

    template<typename T>
    T& get_elements(T& element_set) {
        element_set.clear();
        element_set.resize(count());
        for (size_t i{ 0 }; i < size(); ++i) {
            element_set.push_back(i);
        }
        return element_set;
    }

    template<typename T>
    static T* get_elements(T* element_set, const BoolVector& bool_vec) {
        element_set->clear();
        element_set->reserve(bool_vec.count());
        for (size_t i{ 0 }; i < bool_vec.size(); ++i) {
            element_set->push_back(i);
        }
        return element_set;
    }

}; // class BoolVector.

/// log verbosity
extern unsigned LOG_VERBOSITY;

/// git sha
extern const std::string g_GIT_SHA1;

/**
 * Non-automata-related structures and algorithms.
 *
 * In particular, this includes:
 *   1. Predicates,
 *   2. Ordered Vectors,
 *   3. Iterators,
 *   4. Printers,
 *   5. Other helper functions.
 */
namespace utils {

/** Are two sets disjoint? */
template <class T>
bool are_disjoint(const std::set<T>& lhs, const std::set<T>& rhs)
{ // {{{
	auto itLhs = lhs.begin();
	auto itRhs = rhs.begin();
	while (itLhs != lhs.end() && itRhs != rhs.end())
	{
		if (*itLhs == *itRhs) { return false; }
		else if (*itLhs < *itRhs) { ++itLhs; }
		else {++itRhs; }
	}

	return true;
} // }}}

/** Is there an element in a container? */
template <class T, class Cont>
bool is_in(const T& elem, const Cont& cont)
{ // {{{
  return (std::find(cont.begin(), cont.end(), elem) != cont.end());
} // is_in }}}

/**
 * @brief  Combine two hash values
 *
 * Values taken from
 * http://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp
 *
 * TODO: fix to be more suitable for 64b
 */
template <class T>
inline size_t hash_combine(size_t lhs, const T& rhs)
{ // {{{
	size_t rhs_hash = std::hash<T>{}(rhs);
  lhs ^= rhs_hash + 0x9e3779b9 + (lhs<<6) + (lhs>>2);
	return lhs;
} // hash_combine }}}


/**
 * @brief  Hashes a range
 *
 * Inspired by
 * http://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp
 */
template <typename It>
size_t hash_range(It first, It last)
{ // {{{
	size_t accum = 0;

	for (; first != last; ++first)
	{
		accum = hash_combine(accum, *first);
	}

	return accum;
} // hash_range(It, It) }}}

/// checks whether a container with @p find contains a key
template <class T, class K>
inline bool haskey(const T& cont, const K& key)
{
	return cont.find(key) != cont.cend();
}

/// inverts a map (should work for std::map and std::unordered_map)
template <template <class, class, class...> class Map, class T1, class T2, class... Args>
Map<T2, T1> invert_map(const Map<T1, T2>& mp)
{ // {{{
	Map<T2, T1> result;

	for (const auto& key_val_pair : mp)
	{
		auto it_ins_pair = result.insert({key_val_pair.second, key_val_pair.first});
		if (!it_ins_pair.second)
		{
			throw std::runtime_error("duplicate key when inverting a map");
		}
	}

	return result;
} // invert_map }}}

template<class Tuple, std::size_t N>
struct TuplePrinter;

// Taken from
//   http://en.cppreference.com/w/cpp/utility/tuple/tuple_cat
template<class Tuple, size_t N>
struct TuplePrinter
{
    static std::string print(const Tuple& t)
    {
        std::string res = TuplePrinter<Tuple, N-1>::print(t);
        return res + ", " + std::to_string(std::get<N-1>(t));
    }
};


template<class Tuple>
struct TuplePrinter<Tuple, 1> {
    static std::string print(const Tuple& t)
    {
        return std::to_string(std::get<0>(t));
    }
};

// This reserves space in a vector, to be used before push_back or insert.
// Assuming the doubling extension strategy, it only makes the first reserve large, after that it leaves it to the doubling.
// Might be worth thinking about it.
//  around 30% speedup for fragile revert,
//  more than 50% for simple revert,
// (when testing on a stupid test case)
template<class Vector>
void inline reserve_on_insert(Vector & vec,size_t needed_capacity = 0,size_t extension = 32) {
    //return; //Try this to see the effect of calling this. It should not affect functionality.
    if (vec.capacity() < extension) //if the size is already large enough, leave it to the default doubling strategy. This if seems to make a barely noticeable difference :).
    {
        if (vec.capacity() < std::max(vec.size() + 1, needed_capacity))
            vec.reserve(vec.size() + extension);
    }
}

//This function reindexes vector, that is, the content of each index i will be moved to the index renaming[i].
// It might be useful in revert and trim, but so far it is useless. It was hard to get right, so I am reluctant to remove ....
// It assumes that renaming[i] <= i.
// It assumes that vec is not longer than renaming.
// The function is very fragile.
template<class Vector,typename Index>
void defragment(Vector & vec, const std::vector<Index> & renaming) {
    //assert(vec.size() <= renaming.size());
    size_t i = 0;
    for (size_t rsize=renaming.size(),vsize=vec.size(); i<vsize && i<rsize ; i++) {
        if (renaming[i] != i)
        {
            if(! (renaming[i]<vsize) )
                break;
            assert(renaming[i] < i);
            vec[i] = std::move(vec[renaming[i]]);
        }
    }
    vec.reserve(i);
}

//In a vector of numbers, rename the numbers according to the renaming: renaming[old_name]=new_name
template<class Vector,typename Index>
void rename(Vector & vec, const std::vector<Index> & renaming) {
    for (size_t i = 0,size = vec.size();i < size; ++i)
    {
        if (i != vec[i])
            vec[i] = renaming[vec[i]];
    }
}

template<class Vector, typename Fun>
void filter_indexes(Vector & vec, const Fun && is_staying) {
    // TODO: Rewrite with erase and remove_if.
    size_t last = 0;
    for (size_t i = 0,size = vec.size();i < size; ++i)
    {
        if (is_staying(i)) {
            if (i!=last) {
                vec[last] = std::move(vec[i]);
            }
            last++;
        }
    }
    vec.reserve(last);
}

template<class Vector, typename Fun>
void filter(Vector & vec, const Fun && is_staying) {
    // TODO: Rewrite with erase and remove_if.
    size_t last = 0;
    for (size_t i = 0,size = vec.size();i < size; ++i)
    {
        if (is_staying(vec[i])) {
            if (i!=last) {
                vec[last] = std::move(vec[i]);
            }
            last++;
        }
    }
    vec.reserve(last);
}

template<class Vector>
void inline sort_and_rmdupl(Vector & vec)
{
    //TODO: try this?
    //if (vectorIsSorted()) return;//probably useless

    // sort
    //TODO: is this the best available sorting algo?
    std::sort(vec.begin(), vec.end());

    // remove duplicates
    auto it = std::unique(vec.begin(), vec.end());
    vec.resize(static_cast<size_t>(it - vec.begin()));
}

// CLOSING NAMESPACES AND GUARDS
} /* util */
} /* Mata */


// Some things that need to go to std
namespace std
{ // {{{

/**
 * @brief  A hasher for pairs
 */
template <class A, class B>
struct hash<std::pair<A,B>>
{
	inline size_t operator()(const std::pair<A,B>& k) const
	{ // {{{
		size_t accum = std::hash<A>{}(k.first);
		return mata::utils::hash_combine(accum, k.second);
	} // operator() }}}
};

/**
 * @brief  A hasher for sets
 */
template <class A>
struct hash<std::set<A>>
{
	inline size_t operator()(const std::set<A>& cont) const
	{ // {{{
		return mata::utils::hash_range(cont.begin(), cont.end());
	} // operator() }}}
};

/**
 * @brief  A hasher for vectors
 */
template <class A>
struct hash<std::vector<A>>
{
	inline size_t operator()(const std::vector<A>& cont) const
	{ // {{{
		return mata::utils::hash_range(cont.begin(), cont.end());
	} // operator() }}}
};

/*#######################################################
 #                  std::to_string(TYPE)
 #######################################################*/

/*======================================================
 *                     DECLARATIONS
 *========================================{{{*/

template <class A> std::string to_string(const A& value);
template <class A> std::string to_string(const std::set<A>& st);
template <class A> std::string to_string(const std::vector<A>& vec);
template <class A> std::string to_string(const std::list<A>& vec);
template <class A> std::string to_string(const std::stack<A>& stck);
template <class A> std::string to_string(const std::function<A>& func);
template <class A, class B> std::string to_string(const std::pair<A, B>& p);
template <class A, class B> std::string to_string(const std::map<A, B>& mp);
template <class A, class B> std::string to_string(const std::unordered_map<A, B>& unmap);
template <class A, class B> std::string to_string(const std::unordered_multimap<A, B>& unmmap);

// }}}

/*======================================================
 *                       DEFINITIONS
 *========================================{{{*/

/** Character to string */
inline std::string to_string(char ch)
{ // {{{
	std::string str;
	str += ch;
	return str;
} // to_string(char) }}}

/** String to string */
inline std::string to_string(const std::string& str) { return str; }

/** Vector to string */
template <class A>
std::string to_string(const std::vector<A>& vec)
{ // {{{
	std::string result = "[";
	bool first = true;
	for (auto elem : vec)
	{
		if (!first) { result += ", "; }
		first = false;
		result += std::to_string(elem);
	}
	result += "]";

	return result;
} // to_string(std::vector) }}}

/** List to string */
template <class A>
std::string to_string(const std::list<A>& vec)
{ // {{{
	std::string result = "[";
	bool first = true;
	for (auto elem : vec)
	{
		if (!first) { result += ", "; }
		first = false;
		result += std::to_string(elem);
	}
	result += "]";

	return result;
} // to_string(std::list) }}}

// TODO: the following functions are similar

/** unordered_map to string */
template <class A, class B>
std::string to_string(const std::unordered_map<A, B>& unmap)
{ // {{{
	std::string result = "{";
	bool first = true;
	for (auto key_val_pair : unmap)
	{
		if (!first) { result += ", "; }
		first = false;
		result +=
			std::to_string(key_val_pair.first) +
			" -> " +
			std::to_string(key_val_pair.second);
	}
	result += "}";

	return result;
} // to_string(std::unordered_map) }}}

/** map to string */
template <class A, class B>
std::string to_string(const std::map<A, B>& mp)
{ // {{{
	std::string result = "{";
	bool first = true;
	for (auto key_val_pair : mp)
	{
		if (!first) { result += ", "; }
		first = false;
		result +=
			std::to_string(key_val_pair.first) +
			" -> " +
			std::to_string(key_val_pair.second);
	}
	result += "}";

	return result;
} // to_string(std::map) }}}

/** unordered_multimap to string */
template <class A, class B>
std::string to_string(const std::unordered_multimap<A, B>& unmap)
{ // {{{
	std::string result = "{";
	bool first = true;
	for (auto key_val_pair : unmap)
	{
		if (!first) { result += ", "; }
		first = false;
		result +=
			std::to_string(key_val_pair.first) +
			" -> " +
			std::to_string(key_val_pair.second);
	}
	result += "}";

	return result;
} // to_string(std::unordered_multimap) }}}


/** set to string */
template <class A>
std::string to_string(const std::set<A>& st)
{ // {{{
	std::string result = "{";
	bool first = true;
	for (auto elem : st)
	{
		if (!first) { result += ", "; }
		first = false;
		result += std::to_string(elem);
	}
	result += "}";

	return result;
} // to_string(std::set) }}}

/** stack to string */
template <class A>
std::string to_string(const std::stack<A>& stck)
{ // {{{
	std::stack<A> copy = stck;
	std::vector<A> vec;
	while (!copy.empty()) {
		vec.push_back(copy.top());
		copy.pop();
	}
	std::reverse(vec.begin(), vec.end());
	return std::to_string(vec);
} // to_string(std::stack) }}}

/** function to string */
template <class A>
std::string to_string(const std::function<A>& fun)
{ // {{{
	return std::to_string(static_cast<const void*>(&fun));
} // to_string(std::function) }}}

/** tuple to string */
template <class... Ts>
std::string to_string(const std::tuple<Ts...>& tup)
{ // {{{
	std::string str = "<";
  str += mata::utils::TuplePrinter<decltype(tup), sizeof...(Ts)>::print(tup);
	str += ">";

	return str;
} // to_string(std::tuple) }}}

template <class A, class B>
std::string to_string(const std::pair<A, B>& p)
{ // {{{
	return std::to_string(std::tuple<A, B>(p.first, p.second));
} // to_string(std::pair) }}}

/** arbitrary type with the << operator */
template <class A>
std::string to_string(const A& value)
{ // {{{
	std::ostringstream os;
  os << value;
  return os.str();
} // to_string(T) }}}

// }}}

} // namespace std }}}

#endif /* MATA_UTIL_HH_ */
