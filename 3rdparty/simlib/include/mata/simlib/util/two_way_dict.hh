/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    Header file for a two way dictionary.
 *
 *****************************************************************************/

#ifndef _SIMLIB_TWO_WAY_DICT_
#define _SIMLIB_TWO_WAY_DICT_

#include <mata/simlib/util/convert.hh>
#include <mata/simlib/util/simlib.hh>

// Standard library headers
#include <map>


namespace Simlib
{
	namespace Util
	{
		template
		<
			typename T1,
			typename T2,
			class Cont1,
			class Cont2
		>
		class TwoWayDict;
	}
}


/**
 * @brief   Two-way dictionary
 *
 * This class can be used as a two-way dictionary for two different types, @p
 * Type1 and @p Type2
 *
 * @tparam  Type1   First type
 * @tparam  Type2   Second type
 */
template
<
	typename T1,
	typename T2,
	class Cont1 = std::map<T1, T2>,
	class Cont2 = std::map<T2, T1>
>
class Simlib::Util::TwoWayDict
{
public:   // Public data types

	typedef T1 Type1;
	typedef T2 Type2;

    typedef Type1 key_type;
    typedef Type2 mapped_type;

	typedef Cont1 MapFwdType;
	typedef Cont2 MapBwdType;

public:   // Public data types

	typedef typename MapFwdType::const_iterator ConstIteratorFwd;
	typedef typename MapBwdType::const_iterator ConstIteratorBwd;

	typedef typename MapFwdType::const_iterator const_iterator;

private:  // Private data members

	MapFwdType fwdmap_;
	MapBwdType bwdmap_;

public:   // Public methods

	TwoWayDict() :
		fwdmap_(),
		bwdmap_()
	{ }


	/**
	 * @brief  Constructor from the forward map
	 *
	 * This constructor copies the forward map and attempts to infer the backward map.
	 *
	 * @param[in]  fwdMap  The forward mapping
	 */
	explicit TwoWayDict(const MapFwdType& fwdMap) :
		fwdmap_(fwdMap),
		bwdmap_()
	{
		for (auto mappingPair : fwdmap_)
		{
			if (!bwdmap_.insert(std::make_pair(mappingPair.second, mappingPair.first)).second)
			{
				throw std::runtime_error(std::string(__func__) +
					": failed to construct reverse mapping");
			}
		}
	}

    __attribute__((unused)) const Type2& translate_fwd(const Type1& t1) const
	{
		ConstIteratorFwd itFwd;
		if ((itFwd = fwdmap_.find(t1)) == this->end_fwd())
		{	// in case the value that should be stored there is not
			throw std::out_of_range(__func__);
		}

		return itFwd->second;
	}

	const Type1& translate_bwd(const Type2& t2) const
	{
		ConstIteratorBwd itBwd;
		if ((itBwd = bwdmap_.find(t2)) == end_bwd())
		{	// in case the value that should be stored there is not
			throw std::out_of_range(__func__);
		}

		return itBwd->second;
	}

	const_iterator find(const Type1& t1) const
	{
		return this->find_fwd(t1);
	}

	ConstIteratorFwd find_fwd(const Type1& t1) const
	{
		return fwdmap_.find(t1);
	}

    __attribute__((unused)) ConstIteratorBwd find_bwd(const Type2& t2) const
	{
		return bwdmap_.find(t2);
	}

	const Type2& at(const Type1& t1) const
	{
		const_iterator it = this->find(t1);
		if (this->end() == it)
		{
			throw std::out_of_range(__func__);
		}
		else
		{
			return it->second;
		}
	}

	const_iterator begin() const
	{
		return this->begin_fwd();
	}

	const_iterator end() const
	{
		return this->end_fwd();
	}

	ConstIteratorFwd begin_fwd() const
	{
		return fwdmap_.begin();
	}

    __attribute__((unused)) ConstIteratorBwd begin_bwd() const
	{
		return bwdmap_.begin();
	}

	ConstIteratorFwd end_fwd() const
	{
		return fwdmap_.end();
	}

	ConstIteratorBwd end_bwd() const
	{
		return bwdmap_.end();
	}

	std::pair<ConstIteratorFwd, bool> insert(
		const std::pair<Type1, Type2>& value)
	{
		return this->Insert(value);
	}

	std::pair<ConstIteratorFwd, bool> Insert(
		const std::pair<Type1, Type2>&    value)
	{
		auto resPair = fwdmap_.insert(value);
		if (!(resPair.second))
		{	// in case there is already some forward mapping for given value
			assert(false);      // fail gracefully
		}

		if (!(bwdmap_.insert(std::make_pair(value.second, value.first)).second))
		{	// in case there is already some backward mapping for given value
			SIMLIB_ERROR("backward mapping for "
				<< Convert::ToString(value.second)
				<< " already found: "
				<< Convert::ToString(bwdmap_.find(value.second)->second));

			assert(false);      // fail gracefully
		}

		return resPair;
	}

    __attribute__((unused)) TwoWayDict Union(
		const TwoWayDict&         rhs) const
	{
		TwoWayDict result = *this;

		// copy all pairs
		for (ConstIteratorFwd itRhs = rhs.begin_fwd(); itRhs != rhs.end_fwd(); ++itRhs)
		{
			if ((result.fwdmap_.find(itRhs->first) != result.fwdmap_.end()) ||
				(result.bwdmap_.find(itRhs->second) != result.bwdmap_.end()))
			{	// in case the first or the second component is already in the dictionary
				assert(false);    // fail gracefully
			}

			result.Insert(*itRhs);
		}

		return result;
	}

    __attribute__((unused)) const MapBwdType& get_reverse_map() const
	{
		return bwdmap_;
	}

	size_t size() const
	{
		return fwdmap_.size();
	}

	friend std::ostream& operator<<(
		std::ostream&         os,
		const TwoWayDict&     dict)
	{
		return (os << Convert::ToString(dict.fwdmap_));
	}
};

#endif
