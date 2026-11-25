/** @file
 * @brief Member definitions of @c mata::posts::DeltaBase for the keys in use.
 */

#ifndef MATA_CORE_DELTA_KEYS_TPP
#define MATA_CORE_DELTA_KEYS_TPP

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

namespace mata::posts {

template <typename P, typename TT>
template <ExtensibleAlphabet A>
	requires std::same_as<typename A::Symbol, typename P::Key> && Printable<typename P::Key>
void DeltaBase<P, TT>::add_keys_to(A& target_alphabet) const {
	// Distinct keys only: a key used by n posts would otherwise be converted and hashed n times.
	for (const auto key : get_used_keys()) {
		// The reserved keys, epsilons included, sort last and are no letters.
		if (key > Reserved<0>::max_ordinary) { break; }
		target_alphabet.update_next_symbol_value(key);
		target_alphabet.try_add_new_symbol(std::format("{}", key), key);
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
// Returns symbols appearing in the relation.
template <typename P, typename TT>
utils::OrdVector<typename P::Key> DeltaBase<P, TT>::get_used_keys_vec() const
	requires std::totally_ordered<typename P::Key> {
	using Symbol = typename P::Key;
	// Keep small alphabets sorted incrementally; retain the full sort/deduplicate for wider alphabets,
	// so many distinct symbols cannot make insertion quadratic.
	std::vector<Symbol> symbols{};
	size_t num_distinct{0};
	bool incremental{true};
	for (const PostType& state_post : state_posts_) {
		// For sorted symobls, we can search from the last found. (For small automata this is up to 20% faster.)
		size_t search_from{0};
		for (const Entry& symbol_post : state_post) {
			const Symbol symbol{symbol_post.key()};
			if (!incremental) {
				symbols.push_back(symbol);
			} else if (symbols.empty() || symbols.back() < symbol) {
				if (num_distinct == 64) {
					incremental = false;
				} else {
					++num_distinct;
				}
				symbols.push_back(symbol);
			} else if (symbols.back() != symbol) {
				if (num_distinct == 64) {
					incremental = false;
					symbols.push_back(symbol);
				} else {
					const auto position{std::lower_bound(
						symbols.begin() + static_cast<std::ptrdiff_t>(search_from), symbols.end(), symbol
					)};
					search_from = static_cast<size_t>(position - symbols.begin()) + 1;
					if (position == symbols.end() || *position != symbol) {
						symbols.insert(position, symbol);
						++num_distinct;
					}
				}
			}
		}
	}
	if (incremental) { return utils::OrdVector<Symbol>{utils::sorted_unique, std::move(symbols)}; }
	return utils::OrdVector<Symbol>{std::move(symbols)};
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
	std::set<Symbols> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
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
	requires std::unsigned_integral<typename P::Key> {
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
	requires std::unsigned_integral<typename P::Key> {
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
			const auto index{static_cast<size_t>(symbol_post.key())};
			if (index >= symbols.size()) {
				// Checked before `index + 1`, which would wrap for the largest key of a type as wide as size_t.
				if (index >= symbols.max_size()) { throw std::length_error("mata: a key too large to index by"); }
				symbols.resize(index + 1);
			}
			symbols[index] = true;
		}
	}
	return symbols;
}

template <typename P, typename TT>
BoolVector DeltaBase<P, TT>::get_used_keys_chv() const
	requires std::unsigned_integral<typename P::Key> {
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
			const auto index{static_cast<size_t>(symbol_post.key())};
			if (index >= symbols.size()) {
				// Checked before `index + 1`, which would wrap for the largest key of a type as wide as size_t.
				if (index >= symbols.max_size()) { throw std::length_error("mata: a key too large to index by"); }
				// Doubled to grow geometrically, but never past what the vector can hold.
				symbols.resize(std::min(symbols.max_size(), 2 * (index + 1)));
			}
			symbols[index] = true;
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
