/** @file
 * @brief One post of a relation, and one entry of it: an ordered map from a key to the post under it.
 *
 * @section nesting Post structure
 *
 * A relation is @c DeltaBase indexed by source state, over a chain of posts ending in a set of
 *  targets -- one post per key, so a relation with @c n keys is @c n posts deep:
 * ```
 * DeltaBase -> Post -> Post -> ... -> Targets
 *               key 0   key 1         (a state, or a state with a payload)
 * ```
 * A @c Post is an ordered map from one key to the post nested under it; iterating one yields its
 *  @c PostEntry objects, each one key paired with the post beneath it. The same two classes make up
 *  every step of the chain; @c mata::StatePost and @c mata::SymbolPost are aliases for the depth-2
 *  instantiation. @c PostChain spells a chain outermost key first. @see @ref arity for how deep one is.
 *
 * @section sortedness Sortedness
 *
 * Every post is sorted by key, and the innermost one by target. Lookups binary-search on it, and
 *  @c Post::first_epsilon_it() walks backwards relying on the reserved keys forming a contiguous
 *  suffix. A post that does not maintain the order still compiles, and silently returns wrong
 *  iterators. The order may be broken *temporarily*: @c push_back and @c emplace_back append without
 *  restoring it, which is faster when building from unordered input; it has to be restored by sorting
 *  before any lookup, iteration order or comparison is relied upon. A post advertises that it
 *  maintains the invariant with @c sorted_by_key / @c sorted_by_target and exposes @c is_sorted().
 */

#ifndef MATA_CORE_POST_HH
#define MATA_CORE_POST_HH

#include <algorithm>
#include <compare>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/core/concepts.hh"
#include "mata/core/moves.hh"
#include "mata/core/traits.hh"
#include "mata/core/walks.hh"
#include "mata/utils/ord-vector.hh"

namespace mata::posts {

/**
 * @brief One entry of a post: a single key, and the post nested under it.
 *
 * For an NFA that is a symbol and the set of target states it leads to, which is what
 *  @c mata::SymbolPost aliases. At a higher @c key_arity the nested type is another post rather than
 *  a target set, and nothing here changes for that — an entry never decides what is below it, it
 *  only holds a key and passes the question down. Which is why it is not called @c SymbolPost: the
 *  same class is every post's entry, and only the outermost one is keyed by a symbol.
 *
 * @tparam K What this entry is keyed by.
 * @tparam N The post nested under that key: another post, or a @c mata::TargetSetLike at the bottom.
 * @tparam R Where this post's ordinary keys stop. Carried by the entry because the entry is where
 *  the key type is named, so at a higher @c key_arity each post gets its own convention from its
 *  own entry rather than sharing one. Defaulted, so no existing spelling of a post changes.
 * @see mata::PostEntryLike, mata::ReservedKeys, and @ref nesting.
 */
template <typename K, typename N, typename R = ReservedKeys<K>> class PostEntry {
  public:
	K symbol{};
	N targets{};

	/// @name Post protocol
	/// @see mata::PostEntryLike.
	///@{
	using Key = K; ///< What this entry is keyed by.
	using Nested = N; ///< The post nested under this key.
	/// Where this post's ordinary keys stop. @see mata::ReservedKeys.
	using Reserved = R;
	/// What a successor walk yields, propagated up from the innermost post. An entry does not decide
	///  what a target is, it only passes the answer along.
	using Target = target_of<Nested>;

	const Key& key() const { return symbol; }
	const Nested& nested() const { return targets; }
	/// Writing a target down a key path has to descend into the nested post, so the entry offers a
	///  mutable way in as well. @see mata::posts::insert_target.
	Nested& nested() { return targets; }
	///@}

	PostEntry() = default;
	explicit PostEntry(const Key symbol) : symbol{symbol} {}
	PostEntry(const Key symbol, const Target state_to) : symbol{symbol}, targets{state_to} {}
	PostEntry(const Key symbol, Nested states_to) : symbol{symbol}, targets{std::move(states_to)} {}

	PostEntry(PostEntry&& rhs) noexcept : symbol{rhs.symbol}, targets{std::move(rhs.targets)} {}
	PostEntry(const PostEntry& rhs) = default;
	PostEntry& operator=(PostEntry&& rhs) noexcept;
	PostEntry& operator=(const PostEntry& rhs) = default;

	std::weak_ordering operator<=>(const PostEntry& other) const { return symbol <=> other.symbol; }
	bool operator==(const PostEntry& other) const { return symbol == other.symbol; }

	typename Nested::iterator begin() { return targets.begin(); }
	typename Nested::iterator end() { return targets.end(); }

	typename Nested::const_iterator cbegin() const { return targets.cbegin(); }
	typename Nested::const_iterator cend() const { return targets.cend(); }

	size_t count(const Target s) const { return targets.count(s); }
	bool empty() const { return targets.empty(); }
	size_t num_of_targets() const { return targets.size(); }

	void insert(Target s);
	void insert(const Nested& states);

	// THIS BREAKS THE SORTEDNESS INVARIANT,
	// dangerous,
	// but useful for adding states in a random order to sort later (supposedly more efficient than inserting in a
	// random order)
	void push_back(const Target s) { targets.push_back(s); }

	template <typename... Args> Nested& emplace_back(Args&&... args) {
		// Forwardinng the variadic template pack of arguments to the emplace_back() of the underlying container.
		return targets.emplace_back(std::forward<Args>(args)...);
	}

	void erase(const Target s) { targets.erase(s); }

	typename std::vector<Target>::const_iterator find(const Target s) const { return targets.find(s); }
	typename std::vector<Target>::iterator find(const Target s) { return targets.find(s); }

	/**
	 * @brief Apply @p fn to every target of this entry.
	 *
	 * The innermost step of the traversal that @c mata::Automaton is written against.
	 *  Callers above it never need to know how the targets are stored.
	 */
	template <typename Fn> void for_each_target(Fn&& fn) const {
		for (const Target target : targets) { fn(target); }
	}

	/**
	 * @brief Is @p target among the targets of this entry?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is among the targets of this entry, false otherwise.
	 */
	bool has_target(const Target target) const { return targets.find(target) != targets.end(); }

	/**
	 * @brief The targets as a contiguous range.
	 *
	 * Used to build a flat successor cursor without exposing storage.
	 */
	std::span<const Target> target_span() const {
		const std::vector<Target>& v{targets.to_vector()};
		return {v.data(), v.size()};
	}
}; // class mata::posts::PostEntry.


/**
 * @brief One post of the relation: an ordered map from a key to the post nested under it.
 *
 * An ordered vector of @p E kept sorted by key, which is what every lookup binary-searches on.
 *  Every step of a chain is one of these; a relation of @c key_arity @c n is @c n of them deep,
 *  ending in a @c mata::TargetSetLike.
 *
 * Named @c Post and not @c StatePost because only the *outermost* one is the post of a state — the
 *  ones below it are keyed by whatever their own entry is keyed by. @c mata::StatePost aliases the
 *  depth-2 instantiation, which is the only post an NFA or NFT has, and is what every existing call
 *  site names. @see @ref nesting.
 *
 * @tparam E The entry type: one key plus what is nested under it. This post is parameterised on its
 *  *entry* rather than on the nested post, per @ref nesting — one post class per step, each free to
 *  be a different implementation.
 * @see mata::PostLike.
 */
template <typename E> class Post : utils::OrdVector<E> {
	using super = utils::OrdVector<E>;
	/// The iterator over this post's entries, spelled once for the @c moves() overload taking a range
	///  of them.
	using post_iterator = typename super::const_iterator;

  public:
	/// @name Post protocol
	/// @see mata::PostLike.
	///@{
	using Entry = E; ///< What iterating this post yields.
	using Key = typename Entry::Key; ///< What this post is keyed by (the symbol, for an NFA).
	using Nested = typename Entry::Nested; ///< The post (or target set) under one key.
	using Target = typename Entry::Target; ///< What a successor walk yields, propagated up.
	/// Where this post's ordinary keys stop, propagated up from the entry, which is where the key
	///  type is named. What makes @c moves_epsilons() and @c moves_symbols() default per
	///  instantiation. @see mata::ReservedKeys, mata::ReservedKeysAtTail.
	using Reserved = typename Entry::Reserved;
	/// The innermost post: what is left once every key has been supplied. Equal to @c Nested at
	///  @c key_arity 1 and deeper than it above that, which is the difference @c get_successors()
	///  turns on. Reached through @c Nested rather than through this post's own name, because the
	///  injected class name is not a complete type where this alias is declared.
	using TargetSet = typename posts::PostAt<Nested, arity_of<Nested>>::type;
	/// Number of keys from here down to a target. One (the symbol) for an NFA. Computed, not
	///  hardcoded: one more than whatever is nested below.
	static constexpr size_t key_arity{arity_of<Nested> + 1};
	/// @see @ref sortedness. Ordered by the key of the contained entries.
	static constexpr bool sorted_by_key{true};
	/// @c OrdVector keeps its own @c is_sorted() private as an assertion helper, so check the range
	///  directly. Entries order by key, which is the invariant lookups rely on.
	bool is_sorted() const { return std::ranges::is_sorted(*this); }
	///@}

	using super::begin, super::end, super::cbegin, super::cend;
	using typename super::iterator;
	using typename super::const_iterator;
	using super::OrdVector;
	using super::operator=;
	using super::operator==;
	Post(const Post&) = default;
	Post(Post&&) = default;
	Post& operator=(const Post&) = default;
	Post& operator=(Post&&) = default;
	bool operator==(const Post&) const = default;
	using super::empty, super::size;
	using super::insert;
	using super::reserve;
	using super::to_vector;
	// dangerous, breaks the sortedness invariant
	using super::push_back, super::emplace_back;
	// is adding non-const version as well ok?
	using super::back;
	using super::clear;
	using super::filter;
	using super::front;
	using super::pop_back;

	using super::erase;

	using super::find;


	iterator find(const utils::ArgOf<Key> key) { return find_(this->begin(), this->end(), key); }
	const_iterator find(const utils::ArgOf<Key> key) const { return find_(this->begin(), this->end(), key); }

	/// returns an iterator to the smallest epsilon, or end() if there is no epsilon
	const_iterator first_epsilon_it(const Key first_epsilon) const {
		// The backwards walk below is only the right answer because the keys at or above the
		//  threshold are exactly the last ones. Two independent facts, neither of which fails to
		//  compile on its own, so they are asserted rather than assumed. @see the concept's docs.
		static_assert(
			ReservedKeysAtTail<Post>,
			"finding the epsilons by walking back from the end needs the reserved keys to be the "
			"last ones: the post ordered by key, and the reserved keys the top of the key order"
		);
		const auto end_it = cend();
		auto it = end_it;
		while (it != begin()) {
			--it;
			if (it->symbol < first_epsilon) { // is it a normal symbol already?
				return it + 1; // Return the previous position, the smallest epsilon or end().
			}
		}

		if (it != end_it && it->symbol >= first_epsilon) {
			// The special case when begin is the smallest epsilon (since the while loop ended before the step back)
			return it;
		}
		return end_it;
	}

	/**
	 * @brief Get the set of all target states in the @c Post.
	 * @return Set of all target states in the @c Post.
	 */
	TargetSet get_successors() const {
		TargetSet successors;
		if constexpr (key_arity == 1) {
			// The shipping form, kept verbatim: one bulk insert per entry, which @c OrdVector does
			//  as a merge. Routing arity 1 through the generic walk below would insert one target
			//  at a time instead — see @c for_each_target for what that costs on the real relation.
			for (const Entry& entry : *this) { successors.insert(entry.targets); }
		} else {
			// Above arity 1 the entries hold posts, not targets, so there is nothing to merge and
			//  the targets have to be walked out from under every remaining key.
			walk_targets(*this, [&successors](const Target& target) { successors.insert(target); });
		}
		return successors;
	}

	/**
	 * @brief The result type of the *keyed* @c get_successors — a @c TargetSet, owned or borrowed.
	 *
	 * Not a new kind of set: it is @c TargetSet either way, and the alias exists only to say
	 *  *how it is handed back*. Hence the name — the unkeyed @c get_successors() returns a plain
	 *  @c TargetSet by value, because it aggregates across every key and must always build one;
	 *  only the keyed overload has anything to borrow.
	 *
	 * At @c key_arity 1 the targets under one key are stored contiguously, so it is a **reference
	 *  into the post**. Callers rely on that: @c mata::nfa::Nfa::post() passes it straight out as
	 *  `const StateSet&`, and a by-value result there would dangle — the compiler says so, which is
	 *  what keeps this honest. Above arity 1 the targets sit under every remaining key and have to
	 *  be gathered, so a fresh set.
	 *
	 * @note Spelled once here and propagated as `using KeyedSuccessors = typename
	 *  PostType::KeyedSuccessors;`, never re-derived. Writing the @c std::conditional_t out again at
	 *  the relation would let the two drift, and a relation that decided "by value" while its post
	 *  decided "by reference" would return a reference to the post's temporary.
	 */
	using KeyedSuccessors = std::conditional_t<key_arity == 1, const TargetSet&, TargetSet>;

	/**
	 * @brief The targets reachable from this post under @p key.
	 *
	 * The *targets*, at every arity — not the post one step down. Getting one step down is what
	 *  @c find() is for, and walking the posts between is what @c moves() and @c for_each_move()
	 *  are for; this member answers "which states can I reach over this key", and that question has
	 *  the same kind of answer however many keys are left below.
	 *
	 * If there is no such key, an empty set is returned.
	 *
	 * @param symbol The key to get the successors for. Named for an NFA's sake, like the field.
	 * @return The targets under the given key. @see KeyedSuccessors for why the return
	 *  type follows the arity.
	 */
	KeyedSuccessors get_successors(const Key symbol) const {
		const auto entry_it = find(symbol);
		if constexpr (key_arity == 1) {
			if (entry_it == this->end()) {
				// Returned by const reference, so it outlives the call and no caller can touch it.
				static const TargetSet empty_set{};
				return empty_set;
			}
			return entry_it->targets;
		} else {
			TargetSet successors{};
			if (entry_it != this->end()) {
				walk_targets(entry_it->nested(), [&successors](const Target& target) {
					successors.insert(target);
				});
			}
			return successors;
		}
	}

	/// Iterator over moves represented as @c Move instances. @see mata::posts::PostMoves.
	using Moves = PostMoves<Post>;

	/**
	 * Iterator over all moves (over all labels) in @c Post represented as @c Move instances.
	 */
	Moves moves() const { return {*this, this->cbegin(), this->cend()}; }
	/**
	 * Iterator over specified moves in @c Post represented as @c Move instances.
	 *
	 * @param[in] symbol_post_it First iterator over symbol posts to iterate over.
	 * @param[in] symbol_post_end End iterator over symbol posts (which functions as an sentinel, is not iterated over).
	 */
	Moves moves(const post_iterator symbol_post_it, const post_iterator symbol_post_end) const {
		return {*this, symbol_post_it, symbol_post_end};
	}
	/**
	 * Iterator over epsilon moves in @c Post represented as @c Move instances.
	 */
	Moves moves_epsilons(const Key first_epsilon = Reserved::min_epsilon) const {
		return {*this, first_epsilon_it(first_epsilon), cend()};
	}
	/**
	 * Iterator over alphabet (normal) symbols (not over epsilons) in @c Post represented as @c Move instances.
	 *
	 * @throws std::runtime_error if @p last_symbol is itself a reserved key, since the range would
	 *  then have to run past the end of the ordinary keys. Asking for the whole ordinary range is
	 *  what the default is for.
	 */
	Moves moves_symbols(const Key last_symbol = Reserved::max_ordinary) const {
		if (last_symbol > Reserved::max_ordinary) {
			throw std::runtime_error("Using a reserved key as a last symbol to iterate over.");
		}
		return {*this, cbegin(), first_epsilon_it(last_symbol + 1)};
	}

	/**
	 * Count the number of all moves in @c Post.
	 */
	size_t num_of_moves() const { return count_targets(*this); }

	/**
	 * @brief Apply @p fn to every target reachable from this post, under any key.
	 */
	template <typename Fn> void for_each_target(Fn&& fn) const {
		if constexpr (key_arity == 1) {
			// The shipping form, kept verbatim for the depth every NFA and NFT uses. Routing arity 1
			//  through the generic recursion measured **+18-19%** on the real relation at 65 536 and
			//  262 144 states -- interleaved against the pre-Phase-3 build, with the untouched
			//  hand-written control rows flat at ~1%. The depth prototype had said the recursion was
			//  free (0.99-1.01x), because its posts were plain structs over `std::vector`; the real
			//  ones wrap `OrdVector` behind private inheritance and virtual `begin()`/`end()`, and
			//  that difference is invisible to a prototype. Same remedy as the cursor: specialise the
			//  arity everything actually runs on, stay generic beyond it.
			for (const Entry& entry : *this) { entry.for_each_target(fn); }
		} else {
			walk_targets(*this, fn);
		}
	}

	/**
	 * @brief Apply @p fn to every @c Move of this post, as a (key, target) pair.
	 */
	template <typename Fn> void for_each_move(Fn&& fn) const {
		if constexpr (key_arity == 1) {
			// See for_each_target: the generic recursion cost ~+19-20% here at every size measured.
			for (const Entry& entry : *this) {
				entry.for_each_target([&](const Target target) { fn(entry.symbol, target); });
			}
		} else {
			walk_moves(*this, fn);
		}
	}

	/**
	 * @brief Is @p target reachable from this post under any key?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is reachable from this post under any key, false otherwise.
	 */
	bool has_target(const Target target) const {
		return any_target(*this, [target](const Target& t) { return t == target; });
	}

  private:
	template <typename It> static It find_(It begin, It end, const utils::ArgOf<Key> key) {
		const auto it = std::lower_bound(begin, end, key, [](const Entry& entry, const utils::ArgOf<Key> k) {
			return entry.key() < k;
		});
		return (it == end || it->key() != key) ? end : it;
	}
}; // class mata::posts::Post.

namespace detail {
/// The recursion behind @c PostChain. A position holding a @c mata::ReservedKeysLike descriptor
///  contributes its @c Key and keeps the descriptor; anything else is a key type taking the default.
template <typename... Ts> struct Chain;

template <typename Targets> struct Chain<Targets> {
	static_assert(TargetSetLike<Targets>, "a post chain has to end in a set of targets");
	using type = Targets;
};

/// A position holding a descriptor: the key comes from it, and it is kept as the post's own.
template <typename R, typename... Rest>
	requires(sizeof...(Rest) >= 1) && ReservedKeysLike<R>
struct Chain<R, Rest...> {
	using type = Post<PostEntry<typename R::Key, typename Chain<Rest...>::type, R>>;
};

/// A position holding a plain key type: the post takes the default reserved tail.
template <typename K, typename... Rest>
	requires(sizeof...(Rest) >= 1) && (!ReservedKeysLike<K>)
struct Chain<K, Rest...> {
	using type = Post<PostEntry<K, typename Chain<Rest...>::type>>;
};
} // namespace mata::posts::detail.

/**
 * @brief Build a post chain from its keys, outermost first, ending in the target set.
 *
 * `PostChain<Symbol, StateSet>` is `Post<PostEntry<Symbol, StateSet>>`, which is
 *  @c mata::Post; `PostChain<Symbol, Symbol, StateSet>` is the arity-2 chain over the same key
 *  type. It reads in the order the structure nests, which spelling a chain by hand does not:
 *  by-hand construction is inside-out, so every post has to be named before it can be referred to
 *  and the arity is only countable by matching brackets.
 *
 * A position may be either a key type or a @c mata::ReservedKeysLike descriptor, so a post with a
 *  non-default reserved tail needs no change of spelling and no hand-written stack:
 * ```cpp
 * using Narrow = PostChain<ReservedKeys<Symbol, 100>, StateSet>;  // one key, epsilon at 100
 * ```
 *
 * @tparam Ts The key of each post from the outside in, then the innermost post. At least two, so a
 *  chain always has a key and always ends in a set of targets.
 */
template <typename... Ts> using PostChain = typename detail::Chain<Ts...>::type;

} // namespace mata::posts.

#include "mata/core/post.tpp"

#endif // MATA_CORE_POST_HH
