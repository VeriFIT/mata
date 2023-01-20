/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    The header file of the abstract translator class.
 *
 *****************************************************************************/

#ifndef _SIMLIB_ABSTRACT_TRANSL_HH_
#define _SIMLIB_ABSTRACT_TRANSL_HH_

namespace Simlib
{
	namespace Util
	{
		template <
			class FromT,
			class ToT>
		class AbstractTranslator
		{
		public:  // methods
			virtual ToT operator()(const FromT& value) = 0;
			virtual ToT operator()(const FromT& value) const = 0;

			ToT at(const FromT& value) const
			{
				return this->operator()(value);
			}

			ToT at(const FromT& value)
			{
				return this->operator()(value);
			}

			ToT operator[](const FromT& value)
			{
				return this->operator()(value);
			}

			virtual ~AbstractTranslator() = default;
		};
	}
}

#endif
