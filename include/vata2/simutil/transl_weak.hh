/*****************************************************************************
 *  VATA Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    The header file of the weak translator class.
 *
 *****************************************************************************/

#ifndef _VATA2_TRANSL_WEAK_HH_
#define _VATA2_TRANSL_WEAK_HH_

#include <functional>

// VATA headers
#include <vata2/simutil/vata.hh>
#include <vata2/simutil/abstract_transl.hh>


namespace Vata2
{
	namespace Util
	{
		template <
			class Cont>
		class TranslatorWeak;

		template <
			class Cont>
		class TranslatorWeak2;
	}
}

/**
 * @brief  Weak translator
 *
 */
template <
	class Cont>
class Vata2::Util::TranslatorWeak :
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

	TranslatorWeak(
		Container&               container,
		ResultAllocFuncType      resultAllocFunc) :
			container_(container),
			result_alloc_func_(resultAllocFunc)
	{ }

	virtual ResultType operator()(const InputType& value) override
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

	virtual ResultType operator()(const InputType& value) const override
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
class Vata2::Util::TranslatorWeak2 :
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

	TranslatorWeak2(
		Container&               container,
		ResultAllocFuncType      resultAllocFunc) :
			container_(container),
			result_alloc_func_(resultAllocFunc)
	{ }

	virtual ResultType operator()(const InputType& value) override
	{
		auto p = container_.insert(std::make_pair(value, ResultType()));

		if (p.second)
		{	// in case there is no translation for the value
			p.first->second = result_alloc_func_(p.first->first);
		}

		return p.first->second;
	}

	virtual ResultType operator()(const InputType& value) const override
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

	const Container& get_container() const
	{
		return container_;
	}
};

#endif
