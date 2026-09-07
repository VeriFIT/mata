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

template <typename K, typename N>
SymbolPost<K, N>& SymbolPost<K, N>::operator=(SymbolPost&& rhs) noexcept {
	if (*this != rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

template <typename K, typename N> void SymbolPost<K, N>::insert(const Target s) {
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
template <typename K, typename N> void SymbolPost<K, N>::insert(const Nested& states) {
	for (const Target s : states) { insert(s); }
}

template <typename P>
typename Delta<P>::PostType::const_iterator Delta<P>::epsilon_symbol_posts(const State state, const Key epsilon) const {
	return epsilon_symbol_posts(state_post(state), epsilon);
}

template <typename P>
typename Delta<P>::PostType::const_iterator Delta<P>::epsilon_symbol_posts(const PostType& state_post, const Key epsilon) {
	if (!state_post.empty()) {
		if (epsilon == EPSILON) {
			if (const auto& back = state_post.back(); back.symbol == epsilon) { return std::prev(state_post.end()); }
		} else {
			return state_post.find(Entry(epsilon));
		}
	}
	return state_post.end();
}

template <typename P>
typename Delta<P>::Nested Delta<P>::get_successors(const State state) const { return state_post(state).get_successors(); }

template <typename P>
const typename Delta<P>::Nested& Delta<P>::get_successors(const State state, const Key symbol) const {
	return state_post(state).get_successors(symbol);
}

template <typename P>
std::vector<typename Delta<P>::TransitionType> Delta<P>::get_transitions_to(const State state_to) const {
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
std::vector<typename Delta<P>::TransitionType> Delta<P>::get_transitions_between(const State state_from, const State state_to) const {
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
void Delta<P>::add(const State source, Key symbol, const State target) {
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
void Delta<P>::add(const State source, const Key symbol, const Nested& targets) {
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
void Delta<P>::remove(const State source, const Key symbol, const State target) {
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
bool Delta<P>::contains(const State source, const Key symbol, const State target) const { // {{{
	if (state_posts_.empty()) { return false; }
	if (state_posts_.size() <= source) { return false; }

	const PostType& tl = state_posts_[source];
	if (tl.empty()) { return false; }
	const auto symbol_transitions{tl.find(Entry{symbol})};
	if (symbol_transitions == tl.cend()) { return false; }

	return symbol_transitions->targets.find(target) != symbol_transitions->targets.end();
}

template <typename P>
bool Delta<P>::contains(const TransitionType& transition) const {
	return contains(transition.source, transition.symbol, transition.target);
}

template <typename P>
size_t Delta<P>::num_of_transitions() const {
	size_t number_of_transitions{0};
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) { number_of_transitions += symbol_post.num_of_targets(); }
	}
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
		PostType copied_state_post;
		copied_state_post.reserve(state_post.size());
		for (const Entry& symbol_post : state_post) {
			Nested copied_targets;
			copied_targets.reserve(symbol_post.num_of_targets());
			for (const State& state : symbol_post.targets) { copied_targets.push_back(target_renumberer(state)); }
			copied_state_post.push_back(Entry(symbol_post.symbol, copied_targets));
		}
		copied_state_posts.emplace_back(copied_state_post);
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
	auto filter_rename_symbol_post = [&](const typename Delta<P>::Entry& symbol_post) {
		typename Delta<P>::Entry new_symbol_post{symbol_post.symbol};
		for (const State& target : symbol_post.targets) {
			if (!is_staying[target]) { continue; }
			new_symbol_post.push_back(renaming[target]);
		}
		return new_symbol_post;
	};
	auto filter_rename_state_post = [&](const typename Delta<P>::PostType& state_post,
										const std::function<typename Delta<P>::Entry(
											const typename Delta<P>::Entry&)>& transform_symbol_post) {
		typename Delta<P>::PostType result{};
		for (const typename Delta<P>::Entry& symbol_post : state_post) {
			typename Delta<P>::Entry new_symbol_post = transform_symbol_post(symbol_post);
			if (new_symbol_post.empty()) { continue; }
			result.push_back(std::move(new_symbol_post));
		}
		return result;
	};

	Delta<P> delta_defragmented{};
	for (typename Delta<P>::State source{0}; source < delta.num_of_states(); ++source) {
		if (!is_staying[source]) { continue; }
		delta_defragmented.emplace_back(filter_rename_state_post(delta[source], filter_rename_symbol_post));
	}
	return delta_defragmented;
}

template <typename P>
Delta<P>& Delta<P>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	size_t source_new{0};
	for (size_t source_orig{0}, num_of_states{this->num_of_states()}; source_orig < num_of_states; ++source_orig) {
		if (!is_staying[source_orig]) { continue; } // Skip source states not staying.
		PostType& state_post = state_posts_[source_orig];
		for (auto state_post_it{state_post.begin()}; state_post_it != state_post.end();) {
			Nested& targets{state_post_it->targets};
			targets.erase_if([&is_staying](const State& target) { return !is_staying[target]; });
			targets.rename(renaming);
			if (targets.empty()) {
				state_post_it = state_post.erase(state_post_it);
			} else {
				++state_post_it;
			}
		}
		// Move the filtered state post to the new position, if needed.
		if (source_new != source_orig) { state_posts_[source_new] = std::move(state_post); }
		++source_new;
	}
	// Resize to remove filtered-out state posts.
	state_posts_.resize(source_new);
	return *this;
}

template <typename P>
bool Delta<P>::operator==(const Delta& other) const {
	const Transitions this_transitions{transitions()};
	typename Transitions::const_iterator this_transitions_it{this_transitions.begin()};
	const typename Transitions::const_iterator this_transitions_end{this_transitions.end()};
	const Transitions other_transitions{other.transitions()};
	typename Transitions::const_iterator other_transitions_it{other_transitions.begin()};
	const typename Transitions::const_iterator other_transitions_end{other_transitions.end()};
	while (this_transitions_it != this_transitions_end) {
		if (other_transitions_it == other_transitions_end || *this_transitions_it != *other_transitions_it) {
			return false;
		}
		++this_transitions_it;
		++other_transitions_it;
	}
	return other_transitions_it == other_transitions_end;
}



template <typename P>
void Delta<P>::add_symbols_to(OnTheFlyAlphabet& target_alphabet) const {
	const size_t aut_num_of_states{num_of_states()};
	for (mata::State state{0}; state < aut_num_of_states; ++state) {
		for (const Entry& move : state_post(state)) {
			target_alphabet.update_next_symbol_value(move.symbol);
			target_alphabet.try_add_new_symbol(std::to_string(move.symbol), move.symbol);
		}
	}
}

template <typename P>
utils::OrdVector<typename Delta<P>::Key> Delta<P>::get_used_symbols() const {
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
utils::OrdVector<typename Delta<P>::Key> Delta<P>::get_used_symbols_vec() const {
#ifdef _STATIC_STRUCTURES_
	static std::vector<Key> symbols{};
	symbols.clear();
#else
	std::vector<Key> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			utils::reserve_on_insert(symbols);
			symbols.push_back(symbol_post.symbol);
		}
	}
	utils::OrdVector<Key> sorted_symbols(symbols);
	return sorted_symbols;
}

// returns symbols appearing in Delta, inserts to a std::set
template <typename P>
std::set<typename Delta<P>::Key> Delta<P>::get_used_symbols_set() const {
	// static should prevent reallocation, seems to speed things up a little
#ifdef _STATIC_STRUCTURES_
	static std::set<Key> symbols;
	symbols.clear();
#else
	static std::set<Key> symbols{};
#endif
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) { symbols.insert(symbol_post.symbol); }
	}
	return symbols;
	// utils::OrdVector<Key>  sorted_symbols(symbols.begin(),symbols.end());
	// return sorted_symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P>
utils::SparseSet<typename Delta<P>::Key> Delta<P>::get_used_symbols_sps() const {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static utils::SparseSet<Key> symbols(64);
	symbols.clear();
#else
	utils::SparseSet<Key> symbols(64);
#endif
	// symbols.dont_track_elements();
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) { symbols.insert(symbol_post.symbol); }
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
template <typename P>
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
			if (const size_t capacity{symbol_post.symbol + 1}; symbols.size() < capacity) { symbols.resize(capacity); }
			symbols[symbol_post.symbol] = true;
		}
	}
	return symbols;
}

template <typename P>
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
			if (const size_t capacity{symbol_post.symbol + 1}; symbols.size() < capacity) {
				symbols.resize(capacity * 2);
			}
			symbols[symbol_post.symbol] = true;
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

template <typename P>
typename Delta<P>::Key Delta<P>::get_max_symbol() const {
	Key max{0};
	for (const PostType& state_post : state_posts_) {
		for (const Entry& symbol_post : state_post) {
			if (symbol_post.symbol > max) { max = symbol_post.symbol; }
		}
	}
	return max;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
