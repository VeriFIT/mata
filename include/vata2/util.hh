// TODO: add header

#ifndef _VATA2_UTIL_HH_
#define _VATA2_UTIL_HH_

#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

#define DEBUG_PRINT(x) { std::cout << x << "\n"; }
#define DEBUG_PRINT_LN(x) { DEBUG_PRINT(__func__ << ":" << __LINE__ << ": " << x) }

namespace Vata2
{
namespace util
{

/** Are two sets disjoint? */
template <class T>
bool are_disjoint(const std::set<T>& lhs, const std::set<T>& rhs)
{
	auto itLhs = lhs.begin();
	auto itRhs = rhs.begin();
	while (itLhs != lhs.end() && itRhs != rhs.end())
	{
		if (*itLhs == *itRhs) { return false; }
		else if (*itLhs < *itRhs) { ++itLhs; }
		else {++itRhs; }
	}

	return true;
}

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


// CLOSING NAMESPACES AND GUARDS
} /* util */
} /* Vata2 */


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
		return Vata2::util::hash_combine(accum, k.second);
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
		return Vata2::util::hash_range(cont.begin(), cont.end());
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
		return Vata2::util::hash_range(cont.begin(), cont.end());
	} // operator() }}}
};

/*#######################################################
 #                  std::to_string(TYPE)
 #######################################################*/

/*======================================================
 *                     DECLARATIONS
 *========================================{{{*/

template <class A>
std::string to_string(const std::set<A>& st);

// }}}

/*======================================================
 *                       DEFINITIONS
 *========================================{{{*/

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


#endif /* _VATA2_UTIL_HH_ */
