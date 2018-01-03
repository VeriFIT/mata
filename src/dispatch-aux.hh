// TODO: header

#ifndef _VATA2_DISPATCH_AUX_HH_
#define _VATA2_DISPATCH_AUX_HH_

#include <vata2/vm-dispatch.hh>

#include "metaprog.hh"


namespace Vata2
{
namespace dispatch
{

template <class T>
const T& unpack_type(
	const std::string&         expected_type_name,
	const Vata2::VM::VMValue&  val)
{ // {{{
	if (expected_type_name != val.type)
	{
		throw std::runtime_error("unpack_type: invalid type: " +
			std::to_string(val.type) + " (expected " + expected_type_name + ")");
	}

	assert(nullptr != val.get_ptr());
	return *static_cast<const T*>(val.get_ptr());
} // unpack_type }}}

template <class T>
std::tuple<T> construct_args(
	const Vata2::metaprog::tuple_of<1, std::string>&         type_names,
	const Vata2::metaprog::tuple_of<1, Vata2::VM::VMValue>&  vals)
{
	const T& v = unpack_type<T>(std::get<0>(type_names), std::get<0>(vals));
	std::tuple<T> res = { v };
	return res;
}

template <class T, class... Ts>
std::tuple<T, Ts...> construct_args(
	const Vata2::metaprog::tuple_of<sizeof...(Ts) + 1, std::string>&         type_names,
	const Vata2::metaprog::tuple_of<sizeof...(Ts) + 1, Vata2::VM::VMValue>&  vals)
{
	Vata2::metaprog::tuple_of<sizeof...(Ts), std::string> type_names_tail;
	Vata2::metaprog::tuple_of<sizeof...(Ts), Vata2::VM::VMValue> vals_tail;

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
	const Vata2::VM::VMFuncName&    func_name,
	const std::vector<std::string>  args_types_names,
	const Vata2::VM::VMFuncArgs&    args,
	const std::string&              result_type_name,
	Vata2::VM::VMPointer            (*f)(Ts...))
{
	assert(nullptr != f);

	// in case the function name does not match
	if (name != func_name) { return; }

	constexpr size_t arity = sizeof...(Ts);

	if (args_types_names.size() != arity)
	{
		throw std::runtime_error(
			"test_and_call: args_types_names does not match arity of " +
			std::to_string(func_name));
	}

	if (args.size() != arity)
	{
		throw std::runtime_error(
			"test_and_call: args does not match arity of " +
			std::to_string(func_name));
	}

	Vata2::metaprog::tuple_of<arity, std::string> tup_type_names =
		Vata2::metaprog::vector_to_tuple<arity>(args_types_names);
	Vata2::metaprog::tuple_of<arity, Vata2::VM::VMValue> tup_args =
		Vata2::metaprog::vector_to_tuple<arity>(args);

	// DEBUG_PRINT("type names = " + std::to_string(tup_type_names));
	// DEBUG_PRINT("args = " + std::to_string(tup_args));

	auto f_args = construct_args<typename std::decay<Ts>::type...>(
		tup_type_names, tup_args);

	// a local substitute for std::apply from C++17
	Vata2::VM::VMPointer f_res = Vata2::metaprog::apply(f, f_args);

	Vata2::VM::VMValue result{result_type_name, f_res};
	throw result;
}



} /* dispatch */
} /* Vata2 */

#endif /* _VATA2_DISPATCH_AUX_HH_ */
