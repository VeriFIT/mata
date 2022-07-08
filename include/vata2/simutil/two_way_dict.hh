/*****************************************************************************
 *  VATA Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    Header file for a two way dictionary.
 *
 *****************************************************************************/

#ifndef _VATA2_TWO_WAY_DICT_
#define _VATA2_TWO_WAY_DICT_

// VATA headers
#include <vata2/convert.hh>
#include <vata2/simutil/vata.hh>

// Standard library headers
#include <map>


namespace Vata2
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
class Vata2::Util::TwoWayDict
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

	MapFwdType fwdMap_;
	MapBwdType bwdMap_;

public:   // Public methods

	TwoWayDict() :
		fwdMap_(),
		bwdMap_()
	{ }


	/**
	 * @brief  Constructor from the forward map
	 *
	 * This constructor copies the forward map and attempts to infer the backward map.
	 *
	 * @param[in]  fwdMap  The forward mapping
	 */
	explicit TwoWayDict(const MapFwdType& fwdMap) :
		fwdMap_(fwdMap),
		bwdMap_()
	{
		for (auto mappingPair : fwdMap_)
		{
			if (!bwdMap_.insert(std::make_pair(mappingPair.second, mappingPair.first)).second)
			{
				throw std::runtime_error(std::string(__func__) +
					": failed to construct reverse mapping");
			}
		}
	}

	/**
	 * @brief  Copy constructor
	 */
	TwoWayDict(const TwoWayDict&) = default;

	const Type2& TranslateFwd(const Type1& t1) const
	{
		ConstIteratorFwd itFwd;
		if ((itFwd = fwdMap_.find(t1)) == this->EndFwd())
		{	// in case the value that should be stored there is not
			throw std::out_of_range(__func__);
		}

		return itFwd->second;
	}

	const Type1& TranslateBwd(const Type2& t2) const
	{
		ConstIteratorBwd itBwd;
		if ((itBwd = bwdMap_.find(t2)) == EndBwd())
		{	// in case the value that should be stored there is not
			throw std::out_of_range(__func__);
		}

		return itBwd->second;
	}

	const_iterator find(const Type1& t1) const
	{
		return this->FindFwd(t1);
	}

	ConstIteratorFwd FindFwd(const Type1& t1) const
	{
		return fwdMap_.find(t1);
	}

	ConstIteratorBwd FindBwd(const Type2& t2) const
	{
		return bwdMap_.find(t2);
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
		return this->BeginFwd();
	}

	const_iterator end() const
	{
		return this->EndFwd();
	}

	ConstIteratorFwd BeginFwd() const
	{
		return fwdMap_.begin();
	}

	ConstIteratorBwd BeginBwd() const
	{
		return bwdMap_.begin();
	}

	ConstIteratorFwd EndFwd() const
	{
		return fwdMap_.end();
	}

	ConstIteratorBwd EndBwd() const
	{
		return bwdMap_.end();
	}

	std::pair<ConstIteratorFwd, bool> insert(
		const std::pair<Type1, Type2>& value)
	{
		return this->Insert(value);
	}

	std::pair<ConstIteratorFwd, bool> Insert(
		const std::pair<Type1, Type2>&    value)
	{
		auto resPair = fwdMap_.insert(value);
		if (!(resPair.second))
		{	// in case there is already some forward mapping for given value
			assert(false);      // fail gracefully
		}

		if (!(bwdMap_.insert(std::make_pair(value.second, value.first)).second))
		{	// in case there is already some backward mapping for given value
			VATA_ERROR("backward mapping for "
				<< Convert::ToString(value.second)
				<< " already found: "
				<< Convert::ToString(bwdMap_.find(value.second)->second));

			assert(false);      // fail gracefully
		}

		return resPair;
	}

	TwoWayDict Union(
		const TwoWayDict&         rhs) const
	{
		TwoWayDict result = *this;

		// copy all pairs
		for (ConstIteratorFwd itRhs = rhs.BeginFwd(); itRhs != rhs.EndFwd(); ++itRhs)
		{
			if ((result.fwdMap_.find(itRhs->first) != result.fwdMap_.end()) ||
				(result.bwdMap_.find(itRhs->second) != result.bwdMap_.end()))
			{	// in case the first or the second component is already in the dictionary
				assert(false);    // fail gracefully
			}

			result.Insert(*itRhs);
		}

		return result;
	}

	const MapBwdType& GetReverseMap() const
	{
		return bwdMap_;
	}

	size_t size() const
	{
		return fwdMap_.size();
	}

	friend std::ostream& operator<<(
		std::ostream&         os,
		const TwoWayDict&     dict)
	{
		return (os << Convert::ToString(dict.fwdMap_));
	}
};

#endif
