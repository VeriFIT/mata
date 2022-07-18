/* dispatch.aux -- auxiliary functions for virtual machine dispatcher
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MATA_DISPATCH_AUX_HH_
#define _MATA_DISPATCH_AUX_HH_

#include <mata/vm.hh>
#include <mata/vm-dispatch.hh>

#include "metaprog.hh"


namespace Mata
{
namespace dispatch
{

template <class T>
const T& unpack_type(
	const std::string&         expected_type_name,
	const Mata::VM::VMValue&  val)
{ // {{{
	if (expected_type_name != val.type)
	{
		throw Mata::VM::VMException("unpack_type: invalid type: " +
                                    std::to_string(val.type) + " (expected " + expected_type_name + ")");
	}

	assert(nullptr != val.get_ptr());
	return *static_cast<const T*>(val.get_ptr());
} // unpack_type }}}

template <class T>
std::tuple<T> construct_args(
	const Mata::metaprog::tuple_of<1, std::string>&         type_names,
	const Mata::metaprog::tuple_of<1, Mata::VM::VMValue>&  vals)
{
	const T& v = unpack_type<T>(std::get<0>(type_names), std::get<0>(vals));
	std::tuple<T> res = { v };
	return res;
}

template <class T, class... Ts, typename std::enable_if<(sizeof...(Ts) != 0)>::type = true>
std::tuple<T, Ts...> construct_args(
	const Mata::metaprog::tuple_of<sizeof...(Ts) + 1, std::string>&         type_names,
	const Mata::metaprog::tuple_of<sizeof...(Ts) + 1, Mata::VM::VMValue>&  vals)
{
	Mata::metaprog::tuple_of<sizeof...(Ts), std::string> type_names_tail;
	Mata::metaprog::tuple_of<sizeof...(Ts), Mata::VM::VMValue> vals_tail;

	tie(std::ignore, type_names_tail) = type_names;
	tie(std::ignore, vals_tail) = vals;

	std::tuple<T> res_head = construct_args<T>(
		std::make_tuple(std::get<0>(type_names)),
		std::make_tuple(std::get<0>(vals)));
	std::tuple<Ts...> res_tail = construct_args<Ts...>(type_names_tail, vals_tail);

	return std::tuple_cat(res_head, res_tail);
}


template <class... Ts>
void test_and_call(
        const std::string&              name,
        const Mata::VM::VMFuncName&    func_name,
        const std::vector<std::string>  args_types_names,
        const Mata::VM::VMFuncArgs&    args,
        const std::string&              result_type_name,
        Mata::VM::VMPointer            (*f)(Ts...))
{
	assert(nullptr != f);

	// in case the function name does not match
	if (name != func_name) { return; }

	constexpr size_t arity = sizeof...(Ts);

	if (args_types_names.size() != arity)
	{
		throw Mata::VM::VMException(
			"test_and_call: args_types_names does not match arity of " +
			std::to_string(func_name));
	}

	if (args.size() != arity)
	{
		throw Mata::VM::VMException(
			"test_and_call: args does not match arity of " +
			std::to_string(func_name));
	}

	Mata::metaprog::tuple_of<arity, std::string> tup_type_names =
		Mata::metaprog::vector_to_tuple<arity>(args_types_names);
	Mata::metaprog::tuple_of<arity, Mata::VM::VMValue> tup_args =
		Mata::metaprog::vector_to_tuple<arity>(args);

	// DEBUG_PRINT("type names = " + std::to_string(tup_type_names));
	// DEBUG_PRINT("args = " + std::to_string(tup_args));

	auto f_args = construct_args<typename std::decay<Ts>::type...>(
		tup_type_names, tup_args);

	// a local substitute for std::apply from C++17
	Mata::VM::VMPointer f_res = Mata::metaprog::apply(f, f_args);

	Mata::VM::VMValue result{result_type_name, f_res};
	throw result;
}



} /* dispatch */
} /* Mata */

#endif /* _MATA_DISPATCH_AUX_HH_ */
