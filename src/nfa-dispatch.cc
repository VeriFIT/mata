// TODO: add header

#include <tuple>

#include <vata2/nfa.hh>
#include <vata2/vm-dispatch.hh>

using namespace Vata2::Nfa;
using namespace Vata2::VM;

using Vata2::Parser::ParsedSection;

// #define BLAH(name, ...)


template <class T>
const T& unpack_type(const std::string& expected_type_name, const VMValue& val)
{ // {{{
	if (expected_type_name != val.first)
	{
		assert(false);
	}

	return *static_cast<const T *>(val.second);
} // unpack_type }}}


template <size_t I, class T>
struct tuple_n
{
	template <typename... Args> using type = typename tuple_n<I-1, T>::template type<T, Args...>;
};

template <class T>
struct tuple_n<0, T> {
	template <typename... Args> using type = std::tuple<Args...>;
};
template <size_t I,typename T> using tuple_of = typename tuple_n<I,T>::template type<>;


template <size_t N, class TTuple, class TVec>
void vector_to_tuple_copy_elem(TTuple& tup, const std::vector<TVec> vec)
{ // {{{
	static_assert(N < std::tuple_size<TTuple>::value, "Accessing invalid tuple index");

	std::get<N>(tup) = vec[N];
} // vector_to_tuple_copy_elem }}}


template <size_t N, class T>
tuple_of<N, T> vector_to_tuple(const std::vector<T>& vec)
{ // {{{
	assert(vec.size() == N);

	if constexpr(N == 1) { return tuple_of<1, T>(*vec.begin()); }
	else
	{
		std::vector<T> vec_tail(vec.begin() + 1, vec.end());
		tuple_of<N-1, T> tup_tail = vector_to_tuple<N-1>(vec_tail);
		return std::tuple_cat(std::tuple<T>(*vec.begin()), tup_tail);
	}
} // vector_to_tuple }}}


template <class T>
std::tuple<T> construct_args(
	const tuple_of<1, std::string>& type_names,
	const tuple_of<1, VMValue>& vals)
{
	const T& v = unpack_type<T>(std::get<0>(type_names), std::get<0>(vals));
	std::tuple<T> res = { v };
	return res;
}


template <class T, class... Ts>
std::tuple<T, Ts...> construct_args(
	const tuple_of<sizeof...(Ts) + 1, std::string>& type_names,
	const tuple_of<sizeof...(Ts) + 1, VMValue>& vals)
{
	tuple_of<sizeof...(Ts), std::string> type_names_tail;
	tuple_of<sizeof...(Ts), VMValue> vals_tail;

	tie(std::ignore, type_names_tail) = type_names;
	tie(std::ignore, vals_tail) = vals;

	std::tuple<T> res_head = construct_args<T>(
		std::make_tuple(std::get<0>(type_names)),
		std::make_tuple(std::get<0>(vals)));
	std::tuple<Ts...> res_tail = construct_args<Ts...>(type_names_tail, vals_tail);

	return std::tuple_cat(res_head, res_tail);
}


template <class... Ts>
void BLAH(
	const std::string&   name,
	const VMFuncName&    func_name,
	const std::vector<std::string> args_types_names,
	const VMFuncArgs&    args,
	VMValue (*f)(Ts...))
	// const std::function<VMValue(Ts...)>& f)
{
	if (name != func_name) { return; }

	constexpr size_t arity = sizeof...(Ts);

	if ((args_types_names.size() != arity) || (args.size() != arity))
	{ // if the number of arguments and arity do not match
		assert(false);
	}

	tuple_of<arity, std::string> tup_type_names =
		vector_to_tuple<arity>(args_types_names);
	tuple_of<arity, VMValue> tup_args = vector_to_tuple<arity>(args);

	DEBUG_PRINT("type names = " + std::to_string(tup_type_names));
	DEBUG_PRINT("args = " + std::to_string(tup_args));

	auto f_args = construct_args<typename std::decay<Ts>::type...>(
		tup_type_names, tup_args);
	std::apply(f, f_args);
}


namespace
{
	VMValue nfa_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for NFA");

		BLAH("construct", func_name, {"Parsec"}, func_args,
			*[](const ParsedSection& parsec) -> VMValue {
				DEBUG_PRINT("In <noname>()!");
				DEBUG_PRINT(std::to_string(parsec));
				return std::make_pair(
					"NFA",
					static_cast<const void*>(new Nfa(construct(parsec))));
			});

		assert(false);

		// TODO
	}
}


void Vata2::Nfa::init()
{
	reg_dispatcher("NFA", nfa_dispatch);
}
