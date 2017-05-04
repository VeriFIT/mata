// TODO: add header

#ifndef _VATA_NG_UTIL_HH_
#define _VATA_NG_UTIL_HH_

namespace VataNG
{
namespace util
{

/** Are two sets disjoint? */
template <class T>
bool are_disjoint(const std::set<T>* lhs, const std::set<T>* rhs)
{
	assert(nullptr != lhs);
	assert(nullptr != rhs);

	auto itLhs = lhs->begin();
	auto itRhs = rhs->begin();
	while (itLhs != lhs->end() && itRhs != rhs->end())
	{
		if (*itLhs == *itRhs) { return false; }
		else if (*itLhs < *itRhs) { ++itLhs; }
		else {++itRhs; }
	}

	return true;
}

// CLOSING NAMESPACES AND GUARDS
} /* util */
} /* VataNG */

#endif /* _VATA_NG_UTIL_HH_ */
