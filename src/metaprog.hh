// TODO: header


namespace Vata2
{
namespace metaprog
{

namespace plumbing
{ // {{{
/**
 * @brief  A tuple of N elements of type T
 *
 * Taken from https://stackoverflow.com/a/38894158
 */
template <size_t N, class T>
struct tuple_n
{
	template <typename... Args>
	using type = typename tuple_n<N-1, T>::template type<T, Args...>;
};

template <class T>
struct tuple_n<0, T>
{
	template <typename... Args>
	using type = std::tuple<Args...>;
};
} // plumbing }}}

/// A tuple of N elements of type T
template <size_t N, typename T>
using tuple_of = typename plumbing::tuple_n<N, T>::template type<>;

namespace plumbing
{ // {{{
template <size_t N>
struct VecToTuple
{
	template <class T>
	inline tuple_of<N, T> operator()(const std::vector<T>& vec) const
	{
		assert(vec.size() == N);
		std::vector<T> vec_tail(vec.begin() + 1, vec.end());
		tuple_of<N-1, T> tup_tail = VecToTuple<N-1>{ }(vec_tail);
		return std::tuple_cat(std::tuple<T>(*vec.begin()), tup_tail);
	}
};

template <>
struct VecToTuple<0>
{
	template <class T>
	inline tuple_of<0, T> operator()(const std::vector<T>& vec) const
	{
		assert(vec.empty());
		return { };
	}
};
} // plumbing }}}

/// Converts a vector of size @p N to an @p N -tuple
template <size_t N, class T>
inline tuple_of<N, T> vector_to_tuple(const std::vector<T>& vec)
{ // {{{
	return plumbing::VecToTuple<N>{ }(vec);
} // vector_to_tuple }}}


//
// Commented out because it needs the "if constexpr(...)" feature of C++17
//
// template <size_t N, class T>
// tuple_of<N, T> vector_to_tuple(const std::vector<T>& vec)
// { // {{{
// 	// assert(vec.size() == N);
//   //
// 	// if constexpr(N == 1) { return tuple_of<1, T>(*vec.begin()); }
// 	// else
// 	// {
// 	// 	std::vector<T> vec_tail(vec.begin() + 1, vec.end());
// 	// 	tuple_of<N-1, T> tup_tail = vector_to_tuple<N-1>(vec_tail);
// 	// 	return std::tuple_cat(std::tuple<T>(*vec.begin()), tup_tail);
// 	// }
// 	tuple_of<N, T> tup = VecToTuple<N>(vec);
// 	return tup;
// } // vector_to_tuple }}}


namespace plumbing
{ // {{{
template<typename F, typename Tuple, size_t... S>
decltype(auto) apply_tuple_impl(F&& fn, Tuple&& t, std::index_sequence<S...>)
{ // {{{
	return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
} // apply_tuple_impl }}}
} // plumbing }}}

/**
 * @brief  Applies a tuple on a function
 *
 * This substitutes std::apply from C++17.  Copied from
 *
 *   http://www.cppsamples.com/common-tasks/apply-tuple-to-function.html
 */
template<typename F, typename Tuple>
decltype(auto) apply(F&& fn, Tuple&& t)
{ // {{{
	size_t constexpr tSize =
		std::tuple_size<typename std::remove_reference<Tuple>::type>::value;

	return plumbing::apply_tuple_impl(
		std::forward<F>(fn),
		std::forward<Tuple>(t),
		std::make_index_sequence<tSize>());
} // apply }}}




} /* metaprog */
} /* Vata2 */

