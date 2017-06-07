// TODO: add header

#ifndef _VATA2_UTIL_HH_
#define _VATA2_UTIL_HH_

#include <iostream>

#define DEBUG_PRINT(x) { std::cout << x << "\n"; }

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
	size_t operator()(const std::pair<A,B>& k) const
	{ // {{{
		// TODO: check whether it is OK
		size_t seed = k.first;
		seed ^= k.second + 0x9e3779b9 + (seed<<6) + (seed>>2);
		return seed;
	} // operator() }}}
};

/**
 * @brief  A hasher for sets
 */
template <class A>
struct hash<std::set<A>>
{
	size_t operator()(const std::set<A>& k) const
	{ // {{{
		// TODO: check whether it is OK
		size_t seed = 0;
		for (auto i : k)
		{
			seed ^= i + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	} // operator() }}}
};

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
} // to_string }}}

} // namespace std }}}


#endif /* _VATA2_UTIL_HH_ */
