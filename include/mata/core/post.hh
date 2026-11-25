#ifndef MATA_CORE_POST_HH
#define MATA_CORE_POST_HH

/** @file
 * @brief Definition of one post of a relation, and one entry of it.
 *
 * @section Nesting Post structure
 *
 * A relation is @c DeltaBase indexed by source state, over a chain of posts ending in a set of
 *  targets. There is one post per key, so a relation with @c n keys is @c n posts deep:
 * ```
 * DeltaBase -> Post -> Post -> ... -> Targets
 *               key 0   key 1         (a state, or a state with a payload)
 * ```
 * A @c Post is an ordered map from one key to the post nested under it. Iterating over one yields its
 *  @c PostEntry objects, each one key paired with the post beneath it. The same two classes make up
 *  every step of the chain. @c mata::StatePost and @c mata::SymbolPost are aliases for the depth-2
 *  instantiation. @c PostChain spells a chain outermost key first.
 *
 * @section Sortedness Sortedness
 *
 * Every post is sorted by key, and the innermost one by target. Lookups binary-search on it, and
 *  @c Post::first_epsilon_it() scans backwards relying on that order alone: a sorted range
 *  splits at any threshold into "below" then "at or above". A post that does not maintain the
 *  order still compiles, and silently returns wrong iterators. The order may be broken
 *  *temporarily*: @c push_back and @c emplace_back append without restoring it, which is faster
 *  when building from unordered input. It has to be then restored by sorting before any lookup,
 *  iteration order or comparison is relied upon. A post advertises that it
 *  maintains the invariant with @c sorted_by_key / @c sorted_by_target and exposes @c is_sorted().
 */

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
#include "mata/core/visits.hh"
#include "mata/utils/ord-vector.hh"

namespace mata::posts {

namespace detail {
/**
 * @brief The body of @c mata::posts::PostEntry.
 *
 * @c PostEntry re-exposes @c symbol and @c targets only where those names are accurate: when the
 *  nested type is the target set. Everywhere else an entry is read through @c key() and @c nested().
 *  The members that treat the nested type as the target set are constrained to that case the same
 *  way, so above it they are absent rather than quietly answering about the nested post.
 */
template <typename K, typename N, typename R> class PostEntryCore {
  protected:
	// `targets` first: a small target set (such as @c mata::nfa::SmallStateSet) then leaves tail padding that
	//  `symbol` fills, so an entry with a single inline target takes 16 bytes and no allocation.
	[[no_unique_address]] N targets{}; ///< The nested post, a target set only at @c key_arity 1.
	K symbol{}; ///< The key.

  public:
	/// @name Post protocol
	///@{
	using Key = K; ///< What this entry is keyed by.
	using Nested = N; ///< The post nested under this key.
	using Reserved = R; ///< Where this post's ordinary keys stop. @see mata::ReservedKeys.
	using Target = target_of<Nested>; ///< The target type, in the innermost post.

	const Key& key() const { return symbol; }
	const Nested& nested() const { return targets; }
	Nested& nested() { return targets; }
	///@}

	PostEntryCore() = default;
	explicit PostEntryCore(const Key symbol) : symbol{symbol} {}
	/// Parentheses, not braces: braces pick the @c initializer_list constructor, which sorts the one-element list.
	PostEntryCore(const Key symbol, const Target state_to)
		requires(arity_of<N> == 0)
		: targets(state_to),
		  symbol{symbol} {}
	/// @p states_to are sorted and duplicate-free, such as a @c StateSet or another entry's targets.
	PostEntryCore(const Key symbol, const std::span<const Target> states_to)
		requires(arity_of<N> == 0 && std::constructible_from<N, std::span<const Target>>)
		: targets(states_to),
		  symbol{symbol} {}
	PostEntryCore(const Key symbol, Nested states_to) : targets{std::move(states_to)}, symbol{symbol} {}

	PostEntryCore(PostEntryCore&& rhs) noexcept : targets{std::move(rhs.targets)}, symbol{rhs.symbol} {}
	PostEntryCore(const PostEntryCore& rhs) = default;
	PostEntryCore& operator=(PostEntryCore&& rhs) noexcept;
	PostEntryCore& operator=(const PostEntryCore& rhs) = default;

	std::weak_ordering operator<=>(const PostEntryCore& other) const { return symbol <=> other.symbol; }
	bool operator==(const PostEntryCore& other) const { return symbol == other.symbol; }

	typename Nested::iterator begin() { return targets.begin(); }
	typename Nested::iterator end() { return targets.end(); }

	typename Nested::const_iterator cbegin() const { return targets.cbegin(); }
	typename Nested::const_iterator cend() const { return targets.cend(); }

	bool empty() const { return targets.empty(); }

	/// @name Target set
	/// Only where the nested type is the target set.
	///@{
	/// Returns
	size_t count(const Target s) const
		requires(arity_of<N> == 0)
	{
		return targets.count(s);
	}
	size_t num_of_targets() const
		requires(arity_of<N> == 0)
	{
		return targets.size();
	}

	/// @return Whether @p s was inserted.
	bool insert(Target s)
		requires(arity_of<N> == 0);
	void insert(const Nested& states)
		requires(arity_of<N> == 0);
	/// Insert the sorted, duplicate-free @p states, such as a @c StateSet or another entry's targets.
	/// @return Number of states inserted.
	size_t insert(std::span<const Target> states)
		requires(arity_of<N> == 0);

	void push_back(const Target s)
		requires(arity_of<N> == 0)
	{
		targets.push_back(s);
	}

	template <typename... Args>
		requires(arity_of<N> == 0)
	decltype(auto) emplace_back(Args&&... args) {
		// Forwardinng the variadic template pack of arguments to the emplace_back() of the underlying container.
		return targets.emplace_back(std::forward<Args>(args)...);
	}

	/// @return Whether @p s was erased.
	bool erase(const Target s)
		requires(arity_of<N> == 0)
	{
		return targets.erase(s) != 0;
	}

	auto find(const Target s) const
		requires(arity_of<N> == 0)
	{
		return targets.find(s);
	}
	auto find(const Target s)
		requires(arity_of<N> == 0)
	{
		return targets.find(s);
	}

	/**
	 * @brief Apply @p fn to every target of this entry.
	 *
	 * The innermost step of the traversal that @c mata::Automaton is written against.
	 *  Callers above it never need to know how the targets are stored.
	 */
	template <typename Fn>
		requires(arity_of<N> == 0)
	void for_each_target(Fn&& fn) const {
		for (const Target target : targets) { fn(target); }
	}

	/**
	 * @brief Is @p target among the targets of this entry?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is among the targets of this entry, false otherwise.
	 */
	bool has_target(const Target target) const
		requires(arity_of<N> == 0)
	{
		return targets.contains(target);
	}

	/**
	 * @brief Return targets as a contiguous span.
	 *
	 * For iteration without exposing storage.
	 */
	std::span<const Target> target_span() const
		requires(arity_of<N> == 0)
	{
		if constexpr (requires { targets.span(); }) {
			return targets.span();
		} else {
			const std::vector<Target>& v{targets.to_vector()};
			return {v.data(), v.size()};
		}
	}
	///@}
}; // class mata::posts::detail::PostEntryCore.
} // namespace detail

/**
 * @brief One entry of a post: a single key, and the post nested under it.
 *
 * When @p N is a @c mata::TargetSetLike (an entry of a relation of @c key_arity 1), the entry also names its
 *  key @c symbol and its targets @c targets, following the NFA's naming, and supports structured bindings:
 *  `auto& [symbol, targets] = entry`.
 *
 * @tparam K What this entry is keyed by.
 * @tparam N The post nested under that key: another post, or a @c mata::TargetSetLike at the bottom.
 * @tparam R Where this post's ordinary keys stop.
 */
template <typename K, typename N, typename R = ReservedKeys<K>>
class PostEntry : public detail::PostEntryCore<K, N, R> {
  public:
	using detail::PostEntryCore<K, N, R>::PostEntryCore;
}; // class mata::posts::PostEntry.

/// @cond
/**
 * @brief One entry of a post of an relation of @c key_arity = 1.
 *
 * The key is named @c symbol and bottommoust targets are named @c targets, based on the NFA's naming.
 *
 * @tparam K What this post is keyed by.
 * @tparam N The bottommost post nested under this key: a @c mata::TargetSetLike.
 * @tparam R Where this post's ordinary keys stop.
 */
template <typename K, typename N, typename R>
	requires(arity_of<N> == 0)
class PostEntry<K, N, R> : public detail::PostEntryCore<K, N, R> {
  public:
	using detail::PostEntryCore<K, N, R>::PostEntryCore;
	using detail::PostEntryCore<K, N, R>::symbol;
	using detail::PostEntryCore<K, N, R>::targets;

	/**
	 * @name Structured bindings
	 * `auto& [symbol, targets] = entry`. Binding to the fields directly needs them public in the
	 *  class that declares them, which the core is not, so the tuple protocol stands in.
	 *  Each @c get() is a plain member reference and inlines to the same access.
	 */
	///@{
	template <std::size_t I> auto& get() & {
		static_assert(I < 2);
		if constexpr (I == 0) {
			return symbol;
		} else {
			return targets;
		}
	}
	template <std::size_t I> const auto& get() const& {
		static_assert(I < 2);
		if constexpr (I == 0) {
			return symbol;
		} else {
			return targets;
		}
	}
	template <std::size_t I> auto&& get() && {
		static_assert(I < 2);
		if constexpr (I == 0) {
			return std::move(symbol);
		} else {
			return std::move(targets);
		}
	}
	template <std::size_t I> const auto&& get() const&& {
		static_assert(I < 2);
		if constexpr (I == 0) {
			return std::move(symbol);
		} else {
			return std::move(targets);
		}
	}
	///@}
}; // class mata::posts::PostEntry.
/// @endcond

/**
 * @brief One post of the relation: an ordered map from a key to the post nested under it.
 *
 * An ordered vector of @p E kept sorted by key, which is what every lookup binary-searches on.
 *  Every step of a chain is one of these. A relation of @c key_arity @c n is @c n of them deep,
 *  ending in a @c mata::TargetSetLike.
 *
 * For relation with @c key_arity 1, the top level post uses a name alias StatePost,
 *  and the next level down uses SymbolPost.
 *
 * @tparam E The entry type: one key plus what is nested under it.
 * @see mata::PostLike.
 */
template <typename E> class Post : utils::OrdVector<E> {
	using super = utils::OrdVector<E>;
	using post_iterator = typename super::const_iterator;

  public:
	/// @name Post protocol
	/// @see mata::PostLike.
	///@{
	using Entry = E; ///< What iterating this post yields.
	using Key = typename Entry::Key; ///< What this post is keyed by (the symbol, for an NFA).
	using Nested = typename Entry::Nested; ///< The post (or target set) under one key.
	using Target = typename Entry::Target; ///< What visiting the successors yields, propagated up.
	using Reserved = typename Entry::Reserved; ///< Where this post's ordinary keys stop
	/// The innermost post: what is left once every key has been supplied.
	using TargetSet = typename posts::PostAt<Nested, arity_of<Nested>>::type;
	/// Number of keys from here down to a target.
	static constexpr size_t key_arity{arity_of<Nested> + 1};
	/// @see @ref sortedness. Ordered by the key of the contained entries.
	static constexpr bool sorted_by_key{true};
	/// Teturn whether the post is sorted by key. @see @ref sortedness.
	bool is_sorted() const { return std::ranges::is_sorted(*this); }
	///@}

	using super::begin, super::end, super::cbegin, super::cend;
	using utils::OrdVector<E>::OrdVector;
	using typename super::const_iterator;
	using typename super::iterator;
	using typename super::VectorType; ///< The underlying vector of entries.
	using super::operator=;
	using super::operator==;
	Post(const Post&) = default;
	Post(Post&&) = default;
	Post& operator=(const Post&) = default;
	Post& operator=(Post&&) = default;
	/// Compares the nested posts too: an entry alone compares only its key, which @c find() and @c erase() rely on.
	bool operator==(const Post& other) const {
		return std::ranges::equal(*this, other, [](const Entry& lhs, const Entry& rhs) {
			if (lhs.key() != rhs.key()) { return false; }
			if constexpr (arity_of<Nested> == 0) {
				return std::ranges::equal(lhs.nested(), rhs.nested());
			} else {
				return lhs.nested() == rhs.nested();
			}
		});
	}
	using super::back;
	using super::clear;
	using super::emplace_back;
	using super::empty, super::size;
	using super::erase;
	using super::filter;
	using super::find;
	using super::front;
	using super::insert;
	using super::pop_back;
	using super::push_back;
	using super::reserve;
	using super::to_vector;

	iterator find(const utils::ArgOf<Key> key) { return find_(this->begin(), this->end(), key); }
	const_iterator find(const utils::ArgOf<Key> key) const { return find_(this->begin(), this->end(), key); }

	/**
	 * @brief Returns an iterator to the smallest epsilon, or end() if there is no epsilon.
	 *
	 * @param first_epsilon The first epsilon key to look for. Defaults to the smallest reserved epsilon.
	 * @return An iterator to the smallest epsilon, or end() if there is no epsilon.
	 */
	const_iterator first_epsilon_it(const Key first_epsilon) const {
		// The post need to be ordered in order for the backward scan to work.
		static_assert(
			SortedWithReservedKeys<Post>,
			"finding the epsilons by scanning back from the end needs the post ordered by key"
		);
		const auto end_it = cend();
		auto it = end_it;
		while (it != begin()) {
			--it;
			if (it->key() < first_epsilon) {
				// We crossed the threshold, we need to return the previous position.
				return it + 1;
			}
		}

		if (it != end_it && it->key() >= first_epsilon) {
			// Begin is the smallest epsilon.
			return it;
		}
		return end_it;
	}

	/// Returns the set of all target states in the @c Post.
	TargetSet get_successors() const {
		TargetSet successors;
		if constexpr (key_arity == 1) {
			for (const Entry& entry : *this) { successors.insert(entry.targets); }
		} else {
			// Above arity 1 the entries hold posts, not targets, so there is nothing to merge and
			//  the targets have to be gathered from under every remaining key.
			visit_targets(*this, [&successors](const Target& target) { successors.insert(target); });
		}
		return successors;
	}

	/**
	 * @brief The result type of the keyed @c get_successors.
	 *
	 * At @c key_arity 1, the set of targets can be returned by reference.
	 * Above that, the set of targets has to be gathered.
	 */
	using KeyedSuccessors = std::conditional_t<key_arity == 1, const TargetSet&, TargetSet>;

	/**
	 * @brief The targets reachable from this post under @p key.
	 *
	 * @param symbol The key to get the successors for.
	 * @return The targets under the given key.
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
				visit_targets(entry_it->nested(), [&successors](const Target& target) { successors.insert(target); });
			}
			return successors;
		}
	}

	using Moves = PostMoves<Post>;

	/// @brief Returns an iterator over all moves.
	Moves moves() const
		requires(key_arity == 1)
	{
		return {*this, this->cbegin(), this->cend()};
	}

	/**
	 * @brief Returns an iterator over specified moves.
	 *
	 * @param[in] symbol_post_it First iterator over symbol posts to iterate over.
	 * @param[in] symbol_post_end End iterator over symbol posts.
	 * @return An iterator over specified moves in @c Post represented as @c Move instances.
	 */
	Moves moves(const post_iterator symbol_post_it, const post_iterator symbol_post_end) const
		requires(key_arity == 1)
	{
		return {*this, symbol_post_it, symbol_post_end};
	}

	/**
	 * @brief Returns an iterator over epsilon moves.
	 *
	 * @param[in] first_epsilon The first epsilon key to look for. Defaults to the smallest reserved epsilon.
	 * @return An iterator over epsilon moves in @c Post represented as @c Move instances
	 */
	Moves moves_epsilons(const Key first_epsilon = Reserved::min_epsilon) const
		requires(key_arity == 1)
	{
		return {*this, first_epsilon_it(first_epsilon), cend()};
	}

	/**
	 * @brief Return an Iterator over alphabet (normal) symbols (not over epsilons).
	 *
	 * @param[in] last_symbol The last symbol to iterate over. Defaults to the largest ordinary key.
	 * @return An iterator over alphabet moves in @c Post represented as @c Move instances
	 * @throws std::runtime_error if @p last_symbol is itself a reserved key, since the range would
	 *  then have to run past the end of the ordinary keys.
	 */
	Moves moves_symbols(const Key last_symbol = Reserved::max_ordinary) const
		requires(key_arity == 1)
	{
		if (last_symbol > Reserved::max_ordinary) {
			throw std::runtime_error("Using a reserved key as a last symbol to iterate over.");
		}
		return {*this, cbegin(), first_epsilon_it(last_symbol + 1)};
	}

	/**
	 * @brief Counts the number of all moves in @c Post.
	 * @return The number of all moves in @c Post.
	 */
	size_t num_of_moves() const { return count_targets(*this); }

	/**
	 * @brief Apply @p fn to every target reachable from this post, under any key.
	 */
	template <typename Fn> void for_each_target(Fn&& fn) const {
		if constexpr (key_arity == 1) {
			// NOTE: Routing arity 1 through the generic recursion measured +18-19% on the real relation.
			//  A depth prototype had showd the recursion was free (0.99-1.01x).
			for (const Entry& entry : *this) { entry.for_each_target(fn); }
		} else {
			visit_targets(*this, fn);
		}
	}

	/**
	 * @brief Apply @p fn to every @c Move of this post, as a (key, target) pair.
	 */
	template <typename Fn> void for_each_move(Fn&& fn) const {
		if constexpr (key_arity == 1) {
			// See for_each_target: the generic recursion cost +19-20% here at every size measured.
			for (const Entry& entry : *this) {
				entry.for_each_target([&](const Target target) { fn(entry.symbol, target); });
			}
		} else {
			visit_moves(*this, fn);
		}
	}

	/**
	 * @brief Does this post have @p target reachable under any key?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is reachable under any key, false otherwise.
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
///  contributes its @c Key and keeps the descriptor. Anything else is a key type taking the default.
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
} // namespace detail

/**
 * @brief Build a post chain from its keys, outermost first, ending in the target set.
 *
 * `PostChain<Symbol, StateSet>` is `Post<PostEntry<Symbol, StateSet>>`, which is
 *  @c mata::Post; `PostChain<Symbol, Symbol, StateSet>` is the arity-2 chain over the same key type.
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

/// Tuple protocol for the entries that expose their fields, for structured bindings.
///  @see mata::posts::PostEntry::get().
///@{
template <typename K, typename N, typename R>
	requires(mata::posts::arity_of<N> == 0)
struct std::tuple_size<mata::posts::PostEntry<K, N, R>> : std::integral_constant<std::size_t, 2> {};
template <typename K, typename N, typename R>
	requires(mata::posts::arity_of<N> == 0)
struct std::tuple_element<0, mata::posts::PostEntry<K, N, R>> {
	using type = K;
};
template <typename K, typename N, typename R>
	requires(mata::posts::arity_of<N> == 0)
struct std::tuple_element<1, mata::posts::PostEntry<K, N, R>> {
	using type = N;
};
///@}

#include "mata/core/post.tpp"

#endif // MATA_CORE_POST_HH
