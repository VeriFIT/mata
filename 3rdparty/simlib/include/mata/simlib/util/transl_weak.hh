/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    The header file of the weak translator class.
 *
 *****************************************************************************/

#ifndef _SIMLIB_TRANSL_WEAK_HH_
#define _SIMLIB_TRANSL_WEAK_HH_

#include <functional>

#include <mata/simlib/util/simlib.hh>
#include <mata/simlib/util/abstract_transl.hh>


namespace Simlib
{
	namespace Util
	{
		template <
			class Cont>
		class TranslatorWeak;

		template <
			class Cont>
		class __attribute__((unused)) TranslatorWeak2;
	}
}

/**
 * @brief  Weak translator
 *
 */
template <
	class Cont>
class Simlib::Util::TranslatorWeak :
	public AbstractTranslator<typename Cont::key_type, typename Cont::mapped_type>
{
private:  // data types

	typedef Cont Container;
	typedef typename Container::key_type InputType;
	typedef typename Container::mapped_type ResultType;

	typedef std::function<ResultType(InputType)> ResultAllocFuncType;

private:  // data members

	Container& container_;
	ResultAllocFuncType result_alloc_func_;

public:   // methods

    __attribute__((unused)) TranslatorWeak(
		Container&               container,
		ResultAllocFuncType      resultAllocFunc) :
			container_(container),
			result_alloc_func_(resultAllocFunc)
	{ }

	ResultType operator()(const InputType& value) override
	{
		std::pair<bool, ResultType> res = this->find_if_known(value);
		if (res.first)
		{	// in case the value is known
			return res.second;
		}
		else
		{	// in case there is no translation for the value
			ResultType result = result_alloc_func_(value);
			container_.insert(std::make_pair(value, result));

			return result;
		}
	}

	ResultType operator()(const InputType& value) const override
	{
		std::pair<bool, ResultType> res = this->find_if_known(value);
		if (res.first)
		{	// in case the value is known
			return res.second;
		}
		else
		{	// in case there is no translation for the value
			throw std::runtime_error("Cannot insert value into const translator.");
		}
	}

	/**
	 * @brief  Finds the value if it is known by the translator
	 */
	std::pair<bool, ResultType> find_if_known(const InputType& value) const
	{
		typename Container::const_iterator itCont;
		if ((itCont = container_.find(value)) != container_.end())
		{	// in case the value is known
			return std::make_pair(true, itCont->second);
		}
		else
		{	// in case there is no translation for the value
			return std::make_pair(false, ResultType());
		}
	}
};

/**
 * @brief  Weak translator (ver 2)
 *
 */
template
<
	class Cont
>
class __attribute__((unused)) Simlib::Util::TranslatorWeak2 :
	public AbstractTranslator<typename Cont::key_type, typename Cont::mapped_type>
{
private:  // data types

	typedef Cont Container;
	typedef typename Container::key_type InputType;
	typedef typename Container::mapped_type ResultType;

	typedef std::function<ResultType(const InputType&)> ResultAllocFuncType;

private:  // data members

	Container& container_;
	ResultAllocFuncType result_alloc_func_;

public:   // methods

	__attribute__((unused)) TranslatorWeak2(
		Container&               container,
		ResultAllocFuncType      resultAllocFunc) :
			container_(container),
			result_alloc_func_(resultAllocFunc)
	{ }

	ResultType operator()(const InputType& value) override
	{
		auto p = container_.insert(std::make_pair(value, ResultType()));

		if (p.second)
		{	// in case there is no translation for the value
			p.first->second = result_alloc_func_(p.first->first);
		}

		return p.first->second;
	}

	ResultType operator()(const InputType& value) const override
	{
		typename Container::const_iterator itCont;
		if ((itCont = container_.find(value)) != container_.end())
		{	// in case the value is known
			return itCont->second;
		}
		else
		{
			throw std::runtime_error("Cannot insert value into const translator.");
		}
	}

    __attribute__((unused)) const Container& get_container() const
	{
		return container_;
	}
};

#endif
