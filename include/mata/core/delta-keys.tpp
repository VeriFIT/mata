/** @file
 * @brief Member definitions of @c mata::posts::DeltaBase for the keys in use: which keys the
 *  transitions carry, the greatest one, and feeding them to an alphabet.
 *
 * Included at the end of @c mata/core/delta.hh, after @c mata/core/delta.tpp.
 */

#ifndef MATA_CORE_DELTA_KEYS_TPP
#define MATA_CORE_DELTA_KEYS_TPP

#include <format>
#include <optional>
#include <set>
#include <vector>

namespace mata::posts {

template <typename P, typename TT>
template <ExtensibleAlphabet A>
	requires std::same_as<typename A::Symbol, typename P::Key> && Printable<typename P::Key>
void DeltaBase<P, TT>::add_keys_to(A& target_alphabet) const {
	const size_t aut_num_of_states{num_of_states()};
	for (State state{0}; state < aut_num_of_states; ++state) {
		for (const Entry& move : state_post(state)) {
			target_alphabet.update_next_symbol_value(move.key());
			target_alphabet.try_add_new_symbol(std::format("{}", move.key()), move.key());
		}
	}
}

template <typename P, typename TT>
utils::OrdVector<typename P::Key> DeltaBase<P, TT>::get_used_keys() const
	requires std::totally_ordered<typename P::Key> {
	// TODO: look at the variants in profiling (there are tests in tests-nfa-profiling.cc),
	//  for instance figure out why NumberPredicate and OrdVector are slow,
	//  try also with _STATIC_DATA_STRUCTURES_, it changes things.

	// below are different variant, with different data structures for accumulating symbols,
	// that then must be converted to an OrdVector
	// measured are times with "mata::get_used_symbols speed, harder", "[.profiling]" now on line 104 of
	// nfa-profiling.cc

	// WITH VECTOR (4.434 s)
	return get_used_keys_vec();

	// WITH SET (26.5 s)
	// auto from_set = get_used_symbols_set();
	// return utils::OrdVector<Key> (from_set .begin(),from_set.end());

	// WITH NUMBER PREDICATE (4.857s) (NP removed)
	// return utils::OrdVector(get_used_symbols_np().get_elements());

	// WITH SPARSE SET (haven't tried)
	// return utils::OrdVector<State>(get_used_symbols_sps());

	// WITH BOOL VECTOR (error !!!!!!!):
	// return utils::OrdVector<Key>(utils::NumberPredicate<Key>(get_used_symbols_bv()));

	// WITH BOOL VECTOR (1.9s): (The fastest, it seems.)
	//  However, it will try to allocate a vector indexed by the symbols. If there are epsilons in the automaton,
	//   for example, the bool vector implementation will implode.
	//  std::vector<bool> bv{ get_used_symbols_bv() };
	//  utils::OrdVector<Key> ov{};
	//  const size_t bv_size{ bv.size() };
	//  for (Key i{ 0 }; i < bv_size; ++i) { if (bv[i]) { ov.push_back(i); } }
	//  return ov;

	/// WITH BOOL VECTOR, DIFFERENT VARIANT? (1.9s):
	// std::vector<bool> bv = get_used_symbols_bv();
	// utils::OrdVector<Key> ov{};
	// ov.reserve(static_cast<size_t>(std::count(bv.begin(), bv.end(), true)));
	// const size_t bv_size{ bv.size() };
	// for (Key i = 0; i < bv_size; i++) {
	//     if (bv[i]) {
	//         ov.push_back(i);
	//     }
	// }
	// return ov;

	// WITH CHAR VECTOR (should be the fastest, haven't tried in this branch):
	// BEWARE: failing in one noodlificatoin test ("Simple automata -- epsilon result") ... strange
	//  BoolVector chv = get_used_symbols_chv();
	//  utils::OrdVector<Key> ov;
	//  for(Key i = 0;i<chv.size();i++)
	//     if (chv[i]) {
	//         ov.push_back(i);
	//     }
	//  return ov;
}

// Other versions, maybe an interesting experiment with speed of data structures.
// Returns symbols appearing in the relation, pushes back to vector and then sorts
template <typename P, typename TT>
utils::OrdVector<typename P::Key> DeltaBase<P, TT>::get_used_keys_vec() const
	requires std::totally_ordered<typename P::Key> {
	using Symbols = typename P::Key;
#ifdef _STATIC_STRUCTURES_
	static std::vector<Symbols> symbols{};
	symbols.clear();
#else
	std::vector<Symbols> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			utils::reserve_on_insert(symbols);
			symbols.push_back(symbol_post.key());
		}
	}
	utils::OrdVector<Symbols> sorted_symbols(symbols);
	return sorted_symbols;
}

// returns symbols appearing in the relation, inserts to a std::set
template <typename P, typename TT>
std::set<typename P::Key> DeltaBase<P, TT>::get_used_keys_set() const
	requires std::totally_ordered<typename P::Key> {
	using Symbols = typename P::Key;
	// static should prevent reallocation, seems to speed things up a little
#ifdef _STATIC_STRUCTURES_
	static std::set<Symbols> symbols;
	symbols.clear();
#else
	static std::set<Symbols> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			// @c symbols is @c static in *both* branches above -- which looks like a slip (every
			//  sibling declares it automatic without @c _STATIC_STRUCTURES_, and a static one
			//  accumulates across calls), but it is left exactly as it was rather than quietly
			//  changing what a public member returns.
			symbols.insert(symbol_post.key());
		}
	}
	return symbols;
	// utils::OrdVector<Key>  sorted_symbols(symbols.begin(),symbols.end());
	// return sorted_symbols;
}

// returns symbols appearing in the relation, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P, typename TT>
utils::SparseSet<typename P::Key> DeltaBase<P, TT>::get_used_keys_sps() const
	requires std::integral<typename P::Key> {
	using Symbols = typename P::Key;
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static utils::SparseSet<Symbols> symbols(64);
	symbols.clear();
#else
	utils::SparseSet<Symbols> symbols(64);
#endif
	// symbols.dont_track_elements();
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			symbols.insert(symbol_post.key());
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

// returns symbols appearing in the relation, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P, typename TT>
std::vector<bool> DeltaBase<P, TT>::get_used_keys_bv() const
	requires std::integral<typename P::Key> {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static std::vector<bool> symbols(64, false);
	symbols.clear();
#else
	std::vector<bool> symbols(64, false);
#endif
	// symbols.dont_track_elements();
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			const typename P::Key symbol{symbol_post.key()};
			if (const size_t capacity{symbol + 1}; symbols.size() < capacity) {
				symbols.resize(capacity);
			}
			symbols[symbol] = true;
		}
	}
	return symbols;
}

template <typename P, typename TT>
BoolVector DeltaBase<P, TT>::get_used_keys_chv() const
	requires std::integral<typename P::Key> {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static BoolVector symbols(64, false);
	symbols.clear();
#else
	BoolVector symbols(64, false);
#endif
	// symbols.dont_track_elements();
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			const typename P::Key symbol{symbol_post.key()};
			if (const size_t capacity{symbol + 1}; symbols.size() < capacity) {
				symbols.resize(capacity * 2);
			}
			symbols[symbol] = true;
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

template <typename P, typename TT>
std::optional<typename P::Key> DeltaBase<P, TT>::get_max_key() const
	requires std::totally_ordered<typename P::Key> {
	std::optional<typename P::Key> max{};
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			if (!max || symbol_post.key() > *max) { max = symbol_post.key(); }
		}
	}
	return max;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_KEYS_TPP
