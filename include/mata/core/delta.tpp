/** @file
 * @brief Member definitions for the templated posts of @c mata::Delta.
 *
 * Included at the end of @c mata/core/delta.hh. The bodies live in a header rather than in
 *  `src/core/delta.cc` because a third party instantiating the posts over its own key or target
 *  types needs them; the in-tree depth-2 instantiation is kept out of every translation unit by the
 *  @c extern template declarations in the header.
 */

#ifndef MATA_CORE_DELTA_TPP
#define MATA_CORE_DELTA_TPP

#include <algorithm>
#include <utility>

namespace mata::posts {

template <typename K, typename N, typename R>
PostEntry<K, N, R>& PostEntry<K, N, R>::operator=(PostEntry&& rhs) noexcept {
	if (*this != rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

template <typename K, typename N, typename R> void PostEntry<K, N, R>::insert(const Target s) {
	if (targets.empty() || targets.back() < s) {
		targets.push_back(s);
		return;
	}
	// Find the place where to put the element (if not present).
	// Insert to OrdVector without the searching of a proper position inside insert(const Key&x).
	if (const auto it = std::ranges::lower_bound(targets, s); it == targets.end() || *it != s) {
		targets.insert(it, s);
	}
}

// TODO: slow! This should be doing merge, not inserting one by one.
template <typename K, typename N, typename R> void PostEntry<K, N, R>::insert(const Nested& states) {
	for (const Target s : states) { insert(s); }
}

template <typename P>
typename Delta<P>::PostType::const_iterator Delta<P>::epsilon_symbol_posts(const State state, const Key<0> epsilon) const {
	return epsilon_symbol_posts(state_post(state), epsilon);
}

template <typename P>
typename Delta<P>::PostType::const_iterator Delta<P>::epsilon_symbol_posts(const PostType& state_post, const Key<0> epsilon) {
	if (state_post.empty()) { return state_post.end(); }
	// Fast path: when nothing can sort above the relation's own epsilon, an entry carrying it can
	//  only be the last one, and it is also the *smallest* epsilon because it is the only possible
	//  one. That makes the answer an O(1) look at the back instead of a search -- but only under
	//  both conditions. With a reserved tail wider than one key, or an epsilon that is not the
	//  greatest key, there can be entries above @p epsilon and the back is then the wrong entry;
	//  hence the descriptor is asked rather than the constant assumed. @see mata::ReservedKeys.
	if constexpr (Reserved<0>::epsilon_is_greatest) {
		if (epsilon == Reserved<0>::epsilon) {
			if (const auto& back = state_post.back(); back.key() == epsilon) {
				return std::prev(state_post.end());
			}
			return state_post.end();
		}
	}
	return state_post.find(epsilon);
}

template <typename P>
typename Delta<P>::TargetSet Delta<P>::get_successors(const State state) const {
	return state_post(state).get_successors();
}

template <typename P>
typename Delta<P>::KeyedSuccessors Delta<P>::get_successors(const State state, const Key<0> symbol) const {
	return state_post(state).get_successors(symbol);
}

template <typename P>
std::vector<typename Delta<P>::TransitionType> Delta<P>::get_transitions_to(const State state_to) const
	requires(P::key_arity == 1)
{
	std::vector<TransitionType> transitions_to_state{};
	const size_t num_of_states{this->num_of_states()};
	for (State state_from{0}; state_from < num_of_states; ++state_from) {
		for (const Entry& state_from_move : state_post(state_from)) {
			if (const auto target_state{state_from_move.targets.find(state_to)};
				target_state != state_from_move.targets.end()) {
				transitions_to_state.emplace_back(state_from, state_from_move.symbol, state_to);
			}
		}
	}
	return transitions_to_state;
}

template <typename P>
std::vector<typename Delta<P>::TransitionType>
Delta<P>::get_transitions_between(const State state_from, const State state_to) const
	requires(P::key_arity == 1)
{
	std::vector<TransitionType> transitions_between{};
	for (const Entry& symbol_post : state_post(state_from)) {
		if (const auto state_to_find_it = symbol_post.targets.find(state_to);
			state_to_find_it != symbol_post.targets.end()) {
			transitions_between.emplace_back(state_from, symbol_post.symbol, state_to);
		}
	}
	return transitions_between;
}

template <typename P>
void Delta<P>::add(const State source, Key<0> symbol, const State target)
	requires(P::key_arity == 1)
{
	resize_for_states(source, target);

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, target});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, target});
	} else {
		if (const auto symbol_transitions{state_transitions.find(Entry{symbol})};
			symbol_transitions != state_transitions.end()) {
			// Add transition with symbol already used on transitions from state_from.
			symbol_transitions->insert(target);
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			const Entry new_symbol_transitions{symbol, target};
			state_transitions.insert(new_symbol_transitions);
		}
	}
}

template <typename P>
void Delta<P>::add(const State source, const Key<0> symbol, const Nested& targets)
	requires(P::key_arity == 1)
{
	if (targets.empty()) { return; }
	resize_for_states(source, targets.back());

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, targets});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, targets});
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions != state_transitions.end()) {
			// Add transition with symbolOnTransition already used on transitions from state_from.
			symbol_transitions->insert(targets);

		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			// Move new_symbol_transitions{ symbol, states };
			state_transitions.insert(Entry{symbol, targets});
		}
	}
}

template <typename P>
void Delta<P>::remove(const State source, const Key<0> symbol, const State target)
	requires(P::key_arity == 1)
{
	if (source >= state_posts_.size()) { return; }

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		throw std::invalid_argument(
			"TransitionType [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " + std::to_string(target) +
			"] does not exist."
		);
	} else if (state_transitions.back().symbol < symbol) {
		throw std::invalid_argument(
			"TransitionType [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " + std::to_string(target) +
			"] does not exist."
		);
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions == state_transitions.end()) {
			throw std::invalid_argument(
				"TransitionType [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " +
				std::to_string(target) + "] does not exist."
			);
		} else {
			symbol_transitions->erase(target);
			if (symbol_transitions->empty()) { state_posts_[source].erase(*symbol_transitions); }
		}
	}
}

template <typename P>
bool Delta<P>::contains(const State source, const Key<0> symbol, const State target) const
	requires(P::key_arity == 1)
{ // {{{
	if (state_posts_.empty()) { return false; }
	if (state_posts_.size() <= source) { return false; }

	const PostType& tl = state_posts_[source];
	if (tl.empty()) { return false; }
	const auto symbol_transitions{tl.find(Entry{symbol})};
	if (symbol_transitions == tl.cend()) { return false; }

	return symbol_transitions->targets.find(target) != symbol_transitions->targets.end();
}

template <typename P>
bool Delta<P>::contains(const TransitionType& transition) const
	requires(P::key_arity == 1)
{
	return contains(transition.source, transition.symbol, transition.target);
}

template <typename P>
size_t Delta<P>::num_of_transitions() const {
	size_t number_of_transitions{0};
	for (const PostType& state_post : state_posts_) { number_of_transitions += count_targets(state_post); }
	return number_of_transitions;
}

template <typename P>
bool Delta<P>::empty() const {
	return std::ranges::all_of(state_posts_, [](const PostType& state_post) { return state_post.empty(); });
}


template <typename P>
std::vector<typename Delta<P>::PostType> Delta<P>::renumber_targets(const std::function<State(State)>& target_renumberer) const {
	std::vector<PostType> copied_state_posts;
	copied_state_posts.reserve(num_of_states());
	for (const PostType& state_post : state_posts_) {
		copied_state_posts.emplace_back(renumbered(state_post, target_renumberer));
	}
	return copied_state_posts;
}

template <typename P>
typename Delta<P>::PostType& Delta<P>::mutable_state_post(const State q) {
	if (q >= state_posts_.size()) {
		utils::reserve_on_insert(state_posts_, q);
		const size_t new_size{q + 1};
		state_posts_.resize(new_size);
	}

	return state_posts_[q];
}

template <typename P>
Delta<P> defragment(const Delta<P>& delta, const BoolVector& is_staying,
                    const std::vector<typename Delta<P>::State>& renaming) {
	// One implementation, not two: this used to be a second hand-unrolled copy of the member.
	Delta<P> result{delta};
	result.defragment(is_staying, renaming);
	return result;
}

template <typename P>
Delta<P>& Delta<P>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	// Each level's own job is in `defragmented()`; this one owns only the outer index -- drop the
	//  sources that go, and compact the rest down.
	size_t source_new{0};
	for (size_t source_orig{0}, num_of_states{this->num_of_states()}; source_orig < num_of_states; ++source_orig) {
		if (!is_staying[source_orig]) { continue; }
		state_posts_[source_new] = defragmented(state_posts_[source_orig], is_staying, renaming);
		++source_new;
	}
	state_posts_.resize(source_new);
	return *this;
}

template <typename P>
bool Delta<P>::operator==(const Delta& other) const {
	// Post by post, over the union of the two state spaces. `state_post()` yields the shared empty
	//  post out of range, so a relation with trailing empty posts compares equal to one without --
	//  the same meaning the old transition-by-transition comparison had, without needing a depth-2
	//  transition iterator to express it. Note `posts_equal` and not `!=`: an entry's operator==
	//  compares only its key, so `!=` would ignore differing targets entirely.
	const size_t states{std::max(num_of_states(), other.num_of_states())};
	for (size_t source{0}; source < states; ++source) {
		if (!posts_equal(state_post(source), other.state_post(source))) { return false; }
	}
	return true;
}



template <typename P>
template <ExtensibleAlphabet A, typename K>
	requires SymbolKeyOf<K, typename P::Key>
void Delta<P>::add_symbols_to(A& target_alphabet) const {
	const size_t aut_num_of_states{num_of_states()};
	for (mata::State state{0}; state < aut_num_of_states; ++state) {
		for (const Entry& move : state_post(state)) {
			KeyTraits<K>::for_each_symbol(move.key(), [&target_alphabet](const auto symbol) {
				target_alphabet.update_next_symbol_value(symbol);
				target_alphabet.try_add_new_symbol(std::to_string(symbol), symbol);
			});
		}
	}
}

template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
utils::OrdVector<typename KeyTraits<K>::SymbolType> Delta<P>::get_used_symbols() const {
	// TODO: look at the variants in profiling (there are tests in tests-nfa-profiling.cc),
	//  for instance figure out why NumberPredicate and OrdVector are slow,
	//  try also with _STATIC_DATA_STRUCTURES_, it changes things.

	// below are different variant, with different data structures for accumulating symbols,
	// that then must be converted to an OrdVector
	// measured are times with "mata::get_used_symbols speed, harder", "[.profiling]" now on line 104 of
	// nfa-profiling.cc

	// WITH VECTOR (4.434 s)
	return get_used_symbols_vec();

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
// Returns symbols appearing in Delta, pushes back to vector and then sorts
template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
utils::OrdVector<typename KeyTraits<K>::SymbolType> Delta<P>::get_used_symbols_vec() const {
	using Symbols = typename KeyTraits<K>::SymbolType;
#ifdef _STATIC_STRUCTURES_
	static std::vector<Symbols> symbols{};
	symbols.clear();
#else
	std::vector<Symbols> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [&symbols](const Symbols symbol) {
				utils::reserve_on_insert(symbols);
				symbols.push_back(symbol);
			});
		}
	}
	utils::OrdVector<Symbols> sorted_symbols(symbols);
	return sorted_symbols;
}

// returns symbols appearing in Delta, inserts to a std::set
template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
std::set<typename KeyTraits<K>::SymbolType> Delta<P>::get_used_symbols_set() const {
	using Symbols = typename KeyTraits<K>::SymbolType;
	// static should prevent reallocation, seems to speed things up a little
#ifdef _STATIC_STRUCTURES_
	static std::set<Symbols> symbols;
	symbols.clear();
#else
	static std::set<Symbols> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			// Not captured: @c symbols is @c static in *both* branches above -- which looks like a
			//  slip (every sibling declares it automatic without @c _STATIC_STRUCTURES_, and a
			//  static one accumulates across calls), but it is left exactly as it was rather than
			//  quietly changing what a public member returns. Capturing it would warn.
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [](const Symbols symbol) {
				symbols.insert(symbol);
			});
		}
	}
	return symbols;
	// utils::OrdVector<Key>  sorted_symbols(symbols.begin(),symbols.end());
	// return sorted_symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
utils::SparseSet<typename KeyTraits<K>::SymbolType> Delta<P>::get_used_symbols_sps() const {
	using Symbols = typename KeyTraits<K>::SymbolType;
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
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [&symbols](const Symbols symbol) {
				symbols.insert(symbol);
			});
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
std::vector<bool> Delta<P>::get_used_symbols_bv() const {
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
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [&symbols](const auto symbol) {
				if (const size_t capacity{symbol + 1}; symbols.size() < capacity) {
					symbols.resize(capacity);
				}
				symbols[symbol] = true;
			});
		}
	}
	return symbols;
}

template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
BoolVector Delta<P>::get_used_symbols_chv() const {
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
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [&symbols](const auto symbol) {
				if (const size_t capacity{symbol + 1}; symbols.size() < capacity) {
					symbols.resize(capacity * 2);
				}
				symbols[symbol] = true;
			});
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

template <typename P>
template <typename K>
	requires SymbolKeyOf<K, typename P::Key>
typename KeyTraits<K>::SymbolType Delta<P>::get_max_symbol() const {
	using Symbols = typename KeyTraits<K>::SymbolType;
	Symbols max{0};
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			KeyTraits<K>::for_each_symbol(symbol_post.key(), [&max](const Symbols symbol) {
				if (symbol > max) { max = symbol; }
			});
		}
	}
	return max;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
