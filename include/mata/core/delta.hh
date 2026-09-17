/** @file
 * @brief Data structures representing the transition relation:
 *  a mapping from states and keys to target states.
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
 *  instantiation.
 *
 * @section arity Counting posts
 *
 * @c key_arity is the number of keys between a source state and a target: 1 for an NFA (the symbol),
 *  2 for a two-tape relation, and so on. Keys, not containers -- counting containers gives a number
 *  that depends on where you start. Unrelated to @c mata::Level and @c mata::nft::Levels, which are
 *  an NFT's tape levels. Posts are named by key index: `DeltaBase::Key<I>` is key @p I's type,
 *  `Reserved<I>` its reserved-key convention, and `PostAt<I>` the post that far in.
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

#ifndef MATA_CORE_DELTA_HH
#define MATA_CORE_DELTA_HH

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <format>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "mata/core/concepts.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/synchronized-iterator.hh"

namespace mata {

namespace detail {
/**
 * @brief Render a value for an exception message.
 *
 * Prints the value when its type says how. Every arithmetic type does,
 *  and a user type does by specialising @c std::formatter.
 *
 * An example for a user type @c Interval:
 * ```cpp
 * template <> struct std::formatter<Interval> {
 *     constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
 *     template <typename Ctx> auto format(const Interval& i, Ctx& ctx) const {
 *         return std::format_to(ctx.out(), "[{},{}]", i.lo, i.hi);
 *     }
 * };
 * ```
 */
template <Printable T> std::string name_of(const T& value) {
	// to_string is much faster than format.
	if constexpr (std::is_arithmetic_v<T>) { return std::to_string(value); }
	else { return std::format("{}", value); }
}

/**
 * @brief Render a value for an exception message, or "<unprintable>" if it cannot be printed.
 *
 * Prints the value when its type says how. Every arithmetic type does,
 *  and a user type does by specialising @c std::formatter.
 *
 * An example for a user type @c Interval:
 * ```cpp
 * template <> struct std::formatter<Interval> {
 *     constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
 *     template <typename Ctx> auto format(const Interval& i, Ctx& ctx) const {
 *         return std::format_to(ctx.out(), "[{},{}]", i.lo, i.hi);
 *     }
 * };
 */
template <typename T> std::string describe(const T& value) {
	if constexpr (Printable<T>) { return name_of(value); }
	else { return "<unprintable>"; }
}
} // namespace mata::detail.

namespace posts {
/// How a parameter of type @p X is taken: by value when it is small and trivially copyable (a bare
///  state stays in a register), by reference otherwise (a payload owning memory is not copied to be
///  looked at). A reference to a small trivial value would force it into memory at every call that
///  does not inline, since a reference needs something to point at.
template <typename X>
using ArgOf = std::conditional_t<std::is_trivially_copyable_v<X> && sizeof(X) <= 2 * sizeof(void*), const X, const X&>;

/**
 * @brief One transition: a source state, a key, and a target.
 *
 * The key field is spelled @c symbol, which suits an NFA. A relation that wants other names, or more
 *  than one key, passes its own traits to @c DeltaBase instead. @see DefaultTransitionTraits.
 *
 * An aggregate: `Transition{source, symbol, target}` builds one, and @c parts() takes it apart again
 *  in the same order -- which is all the generic hash, formatter and transition-shaped members need.
 *  @see TransitionLike.
 */
template <typename St, typename K, typename T> struct Transition {
	St source{}; ///< Source state.
	K symbol{}; ///< Transition key, spelled for an NFA's sake.
	T target{}; ///< Target.

	auto operator<=>(const Transition&) const = default;

	/// `(source, symbol, target)`, each by value or by reference as @c ArgOf decides.
	static auto parts(const Transition& t) {
		return std::tuple<ArgOf<St>, ArgOf<K>, ArgOf<T>>{t.source, t.symbol, t.target};
	}
	/// `"(source, symbol, target)"`, as the @c std::formatter below prints it.
	std::string to_string() const { return std::format("{}", *this); }
};

/**
 * @brief One transition of a relation with any number of keys: a source, the keys as a tuple, a target.
 *
 * The default above arity 1, where a single named key field has no meaning. @c keys is
 *  `std::tuple<Key<0>, ..., Key<key_arity - 1>>`: one element per key, outermost first.
 */
template <typename St, typename Keys, typename T> struct KeyedTransition {
	St source{}; ///< Source state.
	Keys keys{}; ///< One key per post, outermost first.
	T target{}; ///< Target.

	auto operator<=>(const KeyedTransition&) const = default;

	/// `(source, keys..., target)` with the keys spread out, so it forwards straight to a keyed overload.
	static auto parts(const KeyedTransition& t) {
		return std::tuple_cat(std::tuple<ArgOf<St>>{t.source}, t.keys, std::tuple<ArgOf<T>>{t.target});
	}
	/// `"(source, keys..., target)"`, as the @c std::formatter below prints it.
	std::string to_string() const { return std::format("{}", *this); }
};

namespace detail {
template <typename T> struct is_tuple : std::false_type {};
template <typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};
} // namespace mata::posts::detail.

/**
 * @brief A transition type that can be taken apart: a static @c parts() giving `(source, keys..., target)` as a tuple.
 *
 * The one thing a transition type owes. Everything generic is written in terms of it -- the hash,
 *  the formatter, and the transition-shaped members of @c DeltaBase, which forward the tuple to the
 *  keyed overload of the matching arity.
 */
template <typename T>
concept TransitionLike = requires(const T& t) {
	requires detail::is_tuple<std::remove_cvref_t<decltype(T::parts(t))>>::value;
};

/// A type @c std::hash is enabled for.
template <typename T>
concept Hashable = std::is_default_constructible_v<std::hash<T>>;

/// The hash of a transition, folded over its parts. The body behind every @c std::hash of one.
template <TransitionLike T> size_t hash_of(const T& t) noexcept {
	return std::apply(
		[](const auto&... v) {
			size_t acc{0};
			((acc = utils::hash_combine(acc, v)), ...);
			return acc;
		},
		T::parts(t)
	);
}

/// A transition written as `(source, keys..., target)`. The body behind every @c std::formatter of one.
template <TransitionLike T, typename Out> Out format_parts(Out out, const T& t) {
	*out++ = '(';
	std::apply(
		[&](const auto& first, const auto&... rest) {
			out = std::format_to(out, "{}", first);
			((out = std::format_to(out, ", {}", rest)), ...);
		},
		T::parts(t)
	);
	*out++ = ')';
	return out;
}
} // namespace mata::posts.
} // namespace mata.

/// @name Hashing and formatting the two shipped transition shapes
///
/// Partial specialisations over mata's own templates, which is what the standard permits. A module's
///  own transition type adds an explicit one-liner for each, delegating to the same two bodies -- and
///  declares them *before* the first use, or the primary template gets instantiated first.
///@{
template <typename St, typename K, typename T>
	requires(mata::posts::Hashable<St> && mata::posts::Hashable<K> && mata::posts::Hashable<T>)
struct std::hash<mata::posts::Transition<St, K, T>> {
	size_t operator()(const mata::posts::Transition<St, K, T>& t) const noexcept { return mata::posts::hash_of(t); }
};
template <typename St, typename... Ks, typename T>
	requires(mata::posts::Hashable<St> && (mata::posts::Hashable<Ks> && ...) && mata::posts::Hashable<T>)
struct std::hash<mata::posts::KeyedTransition<St, std::tuple<Ks...>, T>> {
	size_t operator()(const mata::posts::KeyedTransition<St, std::tuple<Ks...>, T>& t) const noexcept {
		return mata::posts::hash_of(t);
	}
};
template <typename St, typename K, typename T>
	requires(std::formattable<St, char> && std::formattable<K, char> && std::formattable<T, char>)
struct std::formatter<mata::posts::Transition<St, K, T>> {
	constexpr auto parse(std::format_parse_context& ctx) {
		auto it = ctx.begin();
		if (it != ctx.end() && *it != '}') { throw std::format_error("a transition takes no format spec"); }
		return it;
	}
	template <typename Ctx> auto format(const mata::posts::Transition<St, K, T>& t, Ctx& ctx) const {
		return mata::posts::format_parts(ctx.out(), t);
	}
};
template <typename St, typename... Ks, typename T>
	requires(std::formattable<St, char> && (std::formattable<Ks, char> && ...) && std::formattable<T, char>)
struct std::formatter<mata::posts::KeyedTransition<St, std::tuple<Ks...>, T>> {
	constexpr auto parse(std::format_parse_context& ctx) {
		auto it = ctx.begin();
		if (it != ctx.end() && *it != '}') { throw std::format_error("a transition takes no format spec"); }
		return it;
	}
	template <typename Ctx> auto format(const mata::posts::KeyedTransition<St, std::tuple<Ks...>, T>& t, Ctx& ctx) const {
		return mata::posts::format_parts(ctx.out(), t);
	}
};
///@}

namespace mata {
namespace posts {
/// Streams a transition through its @c std::formatter. Found by ADL for the shipped shapes; a
///  module's own type in another namespace writes this same line there.
template <TransitionLike T>
	requires std::formattable<T, char>
std::ostream& operator<<(std::ostream& os, const T& t) {
	return os << std::format("{}", t);
}

/**
 * @brief The post @p I steps down a chain.
 *
 * `PostAt<P, 0>::type` is @p P itself and `PostAt<P, P::key_arity>::type` is the innermost post,
 *  the set of targets. What @c mata::posts::DeltaBase::Key and @c mata::posts::DeltaBase::Reserved are
 *  spelled in terms of: a post has exactly one key so @c PostLike::Key is unambiguous and stays
 *  singular, but a *relation* spans every post, and there the same name would silently mean "the
 *  outermost one" — right at @c key_arity 1 and quietly wrong above it.
 */
template <typename P, size_t I> struct PostAt {
	static_assert(I <= arity_of<P>, "no post that deep in this chain");
	using type = typename PostAt<typename P::Nested, I - 1>::type;
};
/// @copydoc PostAt
template <typename P> struct PostAt<P, 0> {
	using type = P;
};

/// Key @p I's type, taken from the chain rather than from a relation. What
///  @c mata::posts::DeltaBase::Key is spelled in terms of, and what the per-arity keyed writes need in
///  their *signatures* — a member of the relation would not do there, because the parameter types
///  have to depend on the member's own template parameter to stay lazy at the wrong arity.
template <typename P, size_t I> using KeyOf = typename PostAt<P, I>::type::Key;

namespace detail {
template <typename P, size_t... Is>
auto keys_of(std::index_sequence<Is...>) -> std::tuple<KeyOf<P, Is>...>;
} // namespace mata::posts::detail.
/// The keys of a chain as one tuple type, outermost first: `std::tuple<KeyOf<P, 0>, ..., KeyOf<P, arity - 1>>`.
template <typename P> using KeysOf = decltype(detail::keys_of<P>(std::make_index_sequence<arity_of<P>>{}));

/**
 * @brief What one transition of a relation looks like and how to build one: the traits @c DeltaBase takes.
 *
 * A traits names a @c Type and builds one with `make(source, keys..., target)`, one key per post.
 *  The type itself owes a static @c parts() (@see TransitionLike), which is what lets the hash, the
 *  formatter and the transition-shaped members of @c DeltaBase work for it without anything further.
 *
 * Two are shipped. @c SymbolTransitionTraits yields the `{source, symbol, target}` triple and is the
 *  default at arity 1 -- the NFA and the NFT both use it, so `nfa::Transition` and `nft::Transition`
 *  stay one type. @c KeyedTransitionTraits yields `{source, keys, target}` with the keys as a tuple
 *  and is the default above arity 1. @c DefaultTransitionTraits picks between them.
 *
 * A module that wants its own field names writes a traits and passes it as the second argument of
 *  @c DeltaBase. Nothing is specialised, so two modules over the *same* chain can name their fields
 *  differently without touching each other:
 * ```cpp
 * struct Transition {
 *     State source{}; Symbol input{}, output{}; State target{};
 *     bool operator==(const Transition&) const = default;
 *     static auto parts(const Transition& t) {
 *         return std::tuple<ArgOf<State>, ArgOf<Symbol>, ArgOf<Symbol>, ArgOf<State>>{t.source, t.input, t.output, t.target};
 *     }
 * };
 * struct TransitionTraits {
 *     using Type = Transition;
 *     static Type make(State s, Symbol in, Symbol out, State t) { return {s, in, out, t}; }
 * };
 * // Right after the type, before anything hashes or formats one:
 * template <> struct std::hash<Transition> {
 *     size_t operator()(const Transition& t) const noexcept { return mata::posts::hash_of(t); }
 * };
 * template <> struct std::formatter<Transition> { ... return mata::posts::format_parts(ctx.out(), t); ... };
 *
 * class Delta : public mata::posts::DeltaBase<StatePost, TransitionTraits> { ... };
 * ```
 */
///@{
/// The `{source, symbol, target}` triple: one key, named as an NFA names it. The default at arity 1.
template <typename P> struct SymbolTransitionTraits {
	static_assert(arity_of<P> == 1, "a symbol-named transition has room for exactly one key");
	using State = typename TargetTraits<target_of<P>>::State;
	using Type = Transition<State, KeyOf<P, 0>, target_of<P>>;
	static Type make(ArgOf<State> source, ArgOf<KeyOf<P, 0>> key, ArgOf<target_of<P>> target) {
		return Type{source, key, target};
	}
};

/// The `{source, keys, target}` transition with one key per post. The default above arity 1.
template <typename P> struct KeyedTransitionTraits {
	using State = typename TargetTraits<target_of<P>>::State;
	using Type = KeyedTransition<State, KeysOf<P>, target_of<P>>;
	/// `make(source, keys..., target)`. The target comes last, as in every keyed overload, so the keys
	///  are peeled off the front of the pack.
	template <typename... Rest>
		requires(sizeof...(Rest) == arity_of<P> + 1)
	static Type make(ArgOf<State> source, const Rest&... rest) {
		return make_(source, std::forward_as_tuple(rest...), std::make_index_sequence<arity_of<P>>{});
	}

  private:
	template <typename Tup, size_t... Is>
	static Type make_(ArgOf<State> source, const Tup& rest, std::index_sequence<Is...>) {
		return Type{source, KeysOf<P>{std::get<Is>(rest)...}, std::get<sizeof...(Is)>(rest)};
	}
};

namespace detail {
template <typename P, size_t Arity = arity_of<P>> struct DefaultTransitionTraitsOf {
	using type = KeyedTransitionTraits<P>;
};
template <typename P> struct DefaultTransitionTraitsOf<P, 1> {
	using type = SymbolTransitionTraits<P>;
};
} // namespace mata::posts::detail.
/// The traits a relation gets when it names none: symbol-named at arity 1, tuple-keyed above it.
template <typename P> using DefaultTransitionTraits = typename detail::DefaultTransitionTraitsOf<P>::type;
///@}

/**
 * @brief Apply @p fn to every target below @p post, at any nesting depth.
 *
 * Replaces the hand-unrolled descent the posts used to carry. `if constexpr` on @c key_arity turns
 *  the recursion into the same nested loops a caller would write by hand — measured at 0.99-1.01x of
 *  hand-written loops at every arity from 1 to 9, with no trend against depth, so depth genericity
 *  here is free. @p Fn is a template parameter and never a @c std::function: routing the same walk
 *  through one costs 1.08x to 2.18x.
 */
template <typename P, typename Fn> void walk_targets(const P& post, Fn&& fn) {
	if constexpr (arity_of<P> == 0) {
		for (const target_of<P>& target : post) { fn(target); }
	} else {
		for (const auto& entry : post) { walk_targets(entry.nested(), fn); }
	}
}

/**
 * @brief Does any target below @p post satisfy @p pred? Stops at the first that does.
 *
 * The short-circuiting counterpart to @c walk_targets. Needed rather than merely nice: @c has_target
 *  feeds @c is_successor and @c has_self_loop, which @c mata::AutomatonBase::is_acyclic() calls once
 *  per SCC. Expressing it as a full walk that sets a flag is a real regression on that path, and one
 *  the delta-access benchmark does not cover.
 */
template <typename P, typename Pred> bool any_target(const P& post, Pred&& pred) {
	if constexpr (arity_of<P> == 0) {
		for (const target_of<P>& target : post) {
			if (pred(target)) { return true; }
		}
	} else {
		for (const auto& entry : post) {
			if (any_target(entry.nested(), pred)) { return true; }
		}
	}
	return false;
}

/**
 * @brief How many targets are below @p post?
 *
 * Recurses to the innermost post and takes its @c size() rather than counting targets one by one, so
 *  it stays O(entries) instead of O(targets).
 */
template <typename P> size_t count_targets(const P& post) {
	if constexpr (arity_of<P> == 0) {
		return post.size();
	} else {
		size_t total{0};
		for (const auto& entry : post) { total += count_targets(entry.nested()); }
		return total;
	}
}

/**
 * @brief Apply @p fn to every move below @p post, as `fn(keys..., target)`.
 *
 * The keys accumulate down the nesting and arrive as a pack, so at @c key_arity 1 this expands to
 *  exactly `fn(key, target)` — the signature every existing call site and lambda already binds. A
 *  deeper relation simply passes more key arguments; nothing at depth 2 changes shape.
 */
template <typename P, typename Fn, typename... Keys>
void walk_moves(const P& post, Fn&& fn, const Keys&... keys) {
	if constexpr (arity_of<P> == 0) {
		for (const target_of<P>& target : post) { fn(keys..., target); }
	} else {
		for (const auto& entry : post) { walk_moves(entry.nested(), fn, keys..., entry.key()); }
	}
}

/**
 * @brief Insert @p target under the key path @p keys, creating posts along the way.
 *
 * The write-side counterpart to @c walk_moves, and the pair of them is all reverting needs: walk out
 *  the moves of one relation, write each back into another with the source and target exchanged and
 *  **the keys in the same order**.
 *
 * Declared as a free function with the keys *last* on purpose. A member `add(source, keys..., target)`
 *  cannot be written — a parameter pack has to be last for deduction — which is what made reverting
 *  look like it needed a variadic @c DeltaBase::add and a shape change to every call site. It does not:
 *  the target simply comes first here.
 */
template <typename P> void insert_target(P& post, const target_of<P>& target) {
	static_assert(arity_of<P> == 0, "the key path ran out before the innermost post");
	post.insert(target);
}

template <typename P, typename K0, typename... Rest>
void insert_target(P& post, const target_of<P>& target, const K0& k0, const Rest&... rest) {
	static_assert(arity_of<P> == sizeof...(Rest) + 1, "the key path must be one key per post");
	auto entry_it{post.find(k0)};
	if (entry_it == post.end()) {
		post.insert(typename P::Entry{k0, typename P::Nested{}});
		entry_it = post.find(k0); // insert may reallocate, so look the position up again
	}
	insert_target(entry_it->nested(), target, rest...);
}

/**
 * @brief Is @p target reachable from @p post down exactly the key path @p k0, @p rest?
 *
 * Pure descent, short-circuiting on the first key that is not there. The counterpart of
 *  @c insert_target, and like it, the keys come *last* so the pack can be deduced.
 */
template <typename P> bool has_target_at(const P& post, const target_of<P>& target) {
	static_assert(arity_of<P> == 0, "the key path ran out before the innermost post");
	return post.contains(target);
}

template <typename P, typename K0, typename... Rest>
bool has_target_at(const P& post, const target_of<P>& target, const K0& k0, const Rest&... rest) {
	static_assert(arity_of<P> == sizeof...(Rest) + 1, "the key path must be one key per post");
	const auto entry_it{post.find(k0)};
	return entry_it != post.end() && has_target_at(entry_it->nested(), target, rest...);
}

/**
 * @brief Erase @p target from under the key path @p k0, @p rest, pruning every post it empties.
 *
 * The only one of the three key-path walks that does its work on the way back *up*. It descends to
 *  the innermost post, erases there, and then each post asks whether the post beneath it has just
 *  become empty and drops its own entry if so — so a path that held the last target disappears
 *  entirely rather than leaving a chain of empty posts behind. The return value **is** that
 *  question ("am I now empty?"), which is why no extra state is needed to carry it back up.
 *
 * Generalises what the @c key_arity 1 @c mata::posts::DeltaBase::remove does by hand at a single post.
 *
 * @throws std::invalid_argument if any key on the path is missing. The message does not name the
 *  keys, unlike the arity-1 member's: a key here may be an interval or anything else ordered, and
 *  naming it would take @c mata::detail::describe at every post, for an error path.
 * @return Whether @p post is empty once the erase and any pruning below it are done.
 */
template <typename P> bool erase_target(P& post, const target_of<P>& target) {
	static_assert(arity_of<P> == 0, "the key path ran out before the innermost post");
	post.erase(target);
	return post.empty();
}

template <typename P, typename K0, typename... Rest>
bool erase_target(P& post, const target_of<P>& target, const K0& k0, const Rest&... rest) {
	static_assert(arity_of<P> == sizeof...(Rest) + 1, "the key path must be one key per post");
	const auto entry_it{post.find(k0)};
	if (entry_it == post.end()) { throw std::invalid_argument("The transition does not exist."); }
	if (erase_target(entry_it->nested(), target, rest...)) { post.erase(*entry_it); }
	return post.empty();
}

/**
 * @brief Are @p a and @p b structurally equal, keys and targets alike?
 *
 * @warning Not `a == b`. An entry's @c operator== compares **only its key** — deliberately, because
 *  the post is an ordered map and @c OrdVector orders and searches by that key — so comparing posts
 *  with @c operator== silently ignores every difference in their targets. This is why the older
 *  @c DeltaBase::operator== compared transition *sequences* rather than posts, and why the recursion here
 *  has to descend into @c nested() explicitly.
 */
template <typename P> bool posts_equal(const P& a, const P& b) {
	if constexpr (arity_of<P> == 0) {
		return std::ranges::equal(a, b);
	} else {
		if (a.size() != b.size()) { return false; }
		auto it_a{a.begin()};
		auto it_b{b.begin()};
		for (; it_a != a.end(); ++it_a, ++it_b) {
			if (!(it_a->key() == it_b->key())) { return false; }
			if (!posts_equal(it_a->nested(), it_b->nested())) { return false; }
		}
		return true;
	}
}

/**
 * @brief Rebuild @p post with every target renamed by @p rename.
 *
 * Recurses to the innermost post and rebuilds by appending, so it works at any depth. @p rename must
 *  be **monotonic** — the order of targets must not change — because appending is what preserves
 *  sortedness. The callers (renumbering, trimming) both rename densely in ascending order.
 */
template <typename P, typename Fn> P renumbered(const P& post, Fn&& rename) {
	P out{};
	if constexpr (arity_of<P> == 0) {
		for (const target_of<P>& target : post) {
			using Traits = TargetTraits<target_of<P>>;
			out.push_back(Traits::with_state(target, rename(Traits::state_of(target))));
		}
	} else {
		for (const auto& entry : post) {
			out.push_back(typename P::Entry{entry.key(), renumbered(entry.nested(), rename)});
		}
	}
	return out;
}

/**
 * @brief Rebuild @p post keeping only the targets @p is_staying admits, renamed by @p renaming.
 *
 * The write-side mirror of @c walk_targets, and the reason it is written this way: each post knows
 *  only its own job — the innermost one filters and renames, every post above drops the entries
 *  whose nested post came back empty — so trimming generalises with the nesting instead of assuming
 *  two steps of descent, which is what the hand-unrolled version did.
 *
 * Rebuilds rather than mutating, so an implementer owes only @c push_back and not a filter-and-rename
 *  pair. @see @ref sortedness for why appending is safe here.
 */
template <typename P, typename Renaming>
P defragmented(const P& post, const BoolVector& is_staying, const Renaming& renaming) {
	P out{};
	if constexpr (arity_of<P> == 0) {
		for (const target_of<P>& target : post) {
			using Traits = TargetTraits<target_of<P>>;
			const auto state{Traits::state_of(target)};
			if (is_staying[state]) { out.push_back(Traits::with_state(target, renaming[state])); }
		}
	} else {
		for (const auto& entry : post) {
			typename P::Nested nested{defragmented(entry.nested(), is_staying, renaming)};
			if (!nested.empty()) { out.push_back(typename P::Entry{entry.key(), std::move(nested)}); }
		}
	}
	return out;
}

/**
 * @brief One step out of a post: a key together with one target it leads to.
 *
 * What iterating @c mata::Post::moves() yields. Templated only so that it follows the post it
 *  comes from; it holds no logic of its own.
 */
template <typename K, typename T> class Move {
  public:
	K symbol;
	T target;

	bool operator==(const Move&) const = default;
}; // class mata::posts::Move.

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
	 * @brief Apply @p fn to every target state of this symbol post.
	 *
	 * The innermost step of the traversal that @c mata::Automaton is written against.
	 *  Callers above it never need to know how the targets are stored.
	 */
	template <typename Fn> void for_each_target(Fn&& fn) const {
		for (const Target target : targets) { fn(target); }
	}

	/**
	 * @brief Is @p target among the targets of this symbol post?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is among the targets of this symbol post, false otherwise.
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
	/// Spelled once, and before first use, so the nested @c Moves classes can name the post's own
	///  iterator: inside @c Moves::const_iterator the name @c const_iterator is that class itself.
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
	iterator find(const Key symbol) {
		static Entry entry{};
		entry.symbol = symbol;
		return super::find(entry);
	}
	const_iterator find(const Key symbol) const {
		static Entry entry{};
		entry.symbol = symbol;
		return super::find(entry);
	}

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
	 * @brief The target states reachable from this post over @p symbol.
	 *
	 * The *targets*, at every arity — not the post one step down. Getting one step down is what
	 *  @c find() is for, and walking the posts between is what @c moves() and @c for_each_move()
	 *  are for; this member answers "which states can I reach over this key", and that question has
	 *  the same kind of answer however many keys are left below.
	 *
	 * If there is no such symbol, an empty set is returned.
	 *
	 * @param symbol Symbol to get the successors for.
	 * @return Set of target states for the given symbol. @see KeyedSuccessors for why the return
	 *  type follows the arity.
	 */
	KeyedSuccessors get_successors(const Key symbol) const {
		const auto entry_it = find(symbol);
		if constexpr (key_arity == 1) {
			if (entry_it == this->end()) {
				static TargetSet empty_set{};
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

	/**
	 * @brief Iterator over moves represented as @c Move instances.
	 *
	 * It iterates over pairs (symbol, target) for the given @c Post.
	 */
	class Moves {
	  public:
		using MoveType = Move<Key, Target>; ///< One (key, target) pair, shaped by this post.

		/**
		 * Iterator over moves.
		 *
		 * @note Defined inside @c Moves rather than out of line. As a nested class of a nested class
		 *  of a template, each out-of-line member definition would need three layers of
		 *  qualification; inline is the same code and far harder to get wrong.
		 */
		class const_iterator {
		  private:
			const Post* state_post_{nullptr};
			post_iterator symbol_post_it_{};
			typename Nested::const_iterator target_it_{};
			post_iterator symbol_post_end_{};
			bool is_end_{false};
			/// Internal allocated instance of @c Move which is set for the move currently iterated over and returned
			///  as a reference with @c operator*().
			MoveType move_{};

		  public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = MoveType;
			using difference_type = size_t;
			using pointer = MoveType*;
			using reference = MoveType&;

			/// Construct end iterator.
			const_iterator() : is_end_{true} {}

			/// Const all moves iterator.
			explicit const_iterator(const Post& state_post)
				: state_post_{&state_post},
				  symbol_post_it_{state_post.begin()},
				  symbol_post_end_{state_post.end()} {
				if (symbol_post_it_ == symbol_post_end_) {
					is_end_ = true;
					return;
				}
				move_.symbol = symbol_post_it_->symbol;
				target_it_ = symbol_post_it_->targets.cbegin();
				move_.target = *target_it_;
			}

			/// Construct iterator from @p symbol_post_it (including) to @p symbol_post_end (excluding).
			const_iterator(
				const Post& state_post, const post_iterator symbol_post_it, const post_iterator symbol_post_end
			)
				: state_post_{&state_post},
				  symbol_post_it_{symbol_post_it},
				  symbol_post_end_{symbol_post_end} {
				if (symbol_post_it_ == symbol_post_end_) {
					is_end_ = true;
					return;
				}
				move_.symbol = symbol_post_it_->symbol;
				target_it_ = symbol_post_it_->targets.cbegin();
				move_.target = *target_it_;
			}

			const_iterator(const const_iterator& other) noexcept = default;
			const_iterator(const_iterator&&) = default;

			const MoveType& operator*() const { return move_; }
			const MoveType* operator->() const { return &move_; }

			const_iterator& operator++() {
				++target_it_;
				if (target_it_ != symbol_post_it_->targets.end()) {
					move_.target = *target_it_;
					return *this;
				}

				// Iterate over to the next symbol post, which can be either an end iterator, or symbol post whose
				//  symbol <= symbol_post_end_.
				++symbol_post_it_;
				if (symbol_post_it_ == symbol_post_end_) {
					is_end_ = true;
					return *this;
				}
				// The current symbol post is valid (not equal symbol_post_end_).
				move_.symbol = symbol_post_it_->symbol;
				target_it_ = symbol_post_it_->targets.begin();
				move_.target = *target_it_;
				return *this;
			}

			// Postfix increment
			const_iterator operator++(int) {
				const const_iterator tmp{*this};
				++(*this);
				return tmp;
			}

			const_iterator& operator=(const const_iterator& other) noexcept = default;
			const_iterator& operator=(const_iterator&&) = default;

			bool operator==(const const_iterator& other) const {
				if (is_end_ && other.is_end_) {
					return true;
				} else if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) {
					return false;
				}
				return symbol_post_it_ == other.symbol_post_it_ && target_it_ == other.target_it_ &&
					   symbol_post_end_ == other.symbol_post_end_;
			}
		}; // class const_iterator.

		Moves() = default;

		/**
		 * @brief construct moves iterating over a range @p symbol_post_it (including) to @p symbol_post_end
		 * (excluding).
		 *
		 * @param[in] state_post State post to iterate over.
		 * @param[in] symbol_post_it First iterator over symbol posts to iterate over.
		 * @param[in] symbol_post_end End iterator over symbol posts (which functions as an sentinel; is not iterated
		 * over).
		 */
		Moves(const Post& state_post, const post_iterator symbol_post_it, const post_iterator symbol_post_end)
			: state_post_{&state_post},
			  symbol_post_it_{symbol_post_it},
			  symbol_post_end_{symbol_post_end} {}

		Moves(Moves&&) = default;
		Moves(Moves&) = default;

		Moves& operator=(Moves&& other) noexcept {
			if (&other != this) {
				state_post_ = other.state_post_;
				symbol_post_it_ = other.symbol_post_it_;
				symbol_post_end_ = other.symbol_post_end_;
			}
			return *this;
		}
		Moves& operator=(const Moves& other) noexcept {
			if (&other != this) {
				state_post_ = other.state_post_;
				symbol_post_it_ = other.symbol_post_it_;
				symbol_post_end_ = other.symbol_post_end_;
			}
			return *this;
		}

		const_iterator begin() const { return {*state_post_, symbol_post_it_, symbol_post_end_}; }
		static const_iterator end() { return const_iterator{}; }

	  private:
		const Post* state_post_{nullptr};
		post_iterator symbol_post_it_{}; ///< Current symbol post iterator to iterate over.
		/// End symbol post iterator which is no longer iterated over (one after the last symbol post iterated over or
		///  end()).
		post_iterator symbol_post_end_{};
	}; // class Moves.

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
	 * @brief Apply @p fn to every target state reachable from this state post, over any symbol.
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
	 * @brief Apply @p fn to every @c Move of this state post, as a (symbol, target) pair.
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
	 * @brief Is @p target reachable from this state post over any symbol?
	 *
	 * @param[in] target Target state to check.
	 * @return True if @p target is reachable from this state post over any symbol, false otherwise.
	 */
	bool has_target(const Target target) const {
		return any_target(*this, [target](const Target& t) { return t == target; });
	}
}; // class mata::posts::Post.


/**
 * @brief A resumable cursor over every target reachable from one state post.
 *
 * Flattens the walk down the post chain into a single pointer walk, so a traversal position can be
 *  stored and continued later. Tarjan's SCC discovery is the only thing that needs that, and it is
 *  what @c mata::AutomatonBase drives @c get_useful_states(), @c is_acyclic(), @c is_lang_empty() and
 *  @c trim() through.
 *
 * @tparam P The whole post chain, not one post of it. Deliberately: this is a *flat* cursor, holding
 *  every post's position as its own member and carrying between them explicitly. It is hand-written
 *  once per @c key_arity, with a specialisation below for each supported depth.
 *
 * @note Composing it out of per-post cursors instead — one cursor struct per post, each delegating
 *  downwards — is tidier and measurably slower: **24.4% at arity 2 and 18.6% at arity 3**, against
 *  hand-written nested loops on the same shapes, with the flat form holding at 0.79-0.85x. At arity 1
 *  the two are within 1%, which is why the older note here claimed composition was slower without
 *  saying where; the gap only opens once each post has real branching.
 *
 * @note Header-defined so it inlines. The inner range is loaded without a branch.
 */
template <typename P, size_t Arity = P::key_arity> class SuccessorCursor {
	static_assert(
		sizeof(P) == 0,
		"SuccessorCursor is hand-written per key arity, 1 to 3 (structure depth 2 to 4). A deeper "
		"relation needs a new specialisation. This is deliberately a hard error "
		"rather than a silent fallback to a composed cursor, which would keep working and quietly "
		"lose 18-24%."
	);
};

/**
 * @brief The depth-2 cursor: symbol posts, then the targets under each.
 *
 * Byte-for-byte the implementation that shipped before the posts were templated. Invariant 4 says
 *  `key_arity == 1` must not regress; keeping this specialisation means there is nothing to regress.
 */
template <typename P> class SuccessorCursor<P, 1> {
  public:
	using Target = typename P::Target;

	class const_iterator {
	  public:
		typename P::const_iterator symbol_post_it_{}, symbol_post_end_{};
		const Target *target_it_{nullptr}, *target_end_{nullptr};

		void load_targets() {
			const std::span<const Target> targets{symbol_post_it_->target_span()};
			target_it_ = targets.data();
			target_end_ = target_it_ + targets.size();
		}
		/// Restore the invariant "positioned on a target, or exhausted".
		void seek() {
			while (target_it_ == target_end_) {
				if (++symbol_post_it_ == symbol_post_end_) {
					target_it_ = target_end_ = nullptr;
					return;
				}
				load_targets();
			}
		}
		const Target& operator*() const { return *target_it_; }
		const_iterator& operator++() {
			if (++target_it_ == target_end_) { seek(); }
			return *this;
		}
		/// Compares against @c std::default_sentinel: the end is stateless, so no end iterator is stored.
		bool operator==(std::default_sentinel_t) const { return symbol_post_it_ == symbol_post_end_; }
		bool operator==(const const_iterator&) const = default;
		/// The key path to the current target: one key, at arity 1.
		auto keys() const { return std::tuple<typename P::Key>{symbol_post_it_->key()}; }
	};

	explicit SuccessorCursor(const P& state_post) : state_post_{&state_post} {}

	const_iterator begin() const {
		const_iterator it;
		it.symbol_post_it_ = state_post_->begin();
		it.symbol_post_end_ = state_post_->end();
		if (it.symbol_post_it_ == it.symbol_post_end_) { return it; }
		it.load_targets();
		it.seek();
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const P* state_post_;
};

/**
 * @brief The depth-3 cursor: two keys, then the targets.
 *
 * Written flat, like the arity-1 case: every post's position is a named member and the carry between
 *  them is explicit. Composing this out of per-post cursors instead measures **24.4% slower** on the
 *  same shapes, so the duplication is bought deliberately. @c scan_b and @c scan_a are the carry: each
 *  advances its own post and re-descends, returning false when its subtree holds no target.
 *
 * @warning A mistake in the carry silently *skips targets* -- no compile error, no crash, a wrong
 *  answer. `tests/core/cursor.cc` cross-checks every specialisation against @c walk_targets.
 */
template <typename P> class SuccessorCursor<P, 2> {
  public:
	using Target = typename P::Target;
	using PostB = typename P::Entry::Nested; ///< The post nested under an outermost key.

	class const_iterator {
	  public:
		typename P::const_iterator a_{}, a_end_{};
		typename PostB::const_iterator b_{}, b_end_{};
		const Target *target_it_{nullptr}, *target_end_{nullptr};

		/// Position on the first non-empty target run at or after @c b_.
		bool scan_b() {
			for (; b_ != b_end_; ++b_) {
				const std::span<const Target> targets{b_->target_span()};
				if (!targets.empty()) {
					target_it_ = targets.data();
					target_end_ = target_it_ + targets.size();
					return true;
				}
			}
			return false;
		}
		/// Descend into @c a_ and onwards until a target is found.
		bool scan_a() {
			for (; a_ != a_end_; ++a_) {
				b_ = a_->nested().begin();
				b_end_ = a_->nested().end();
				if (scan_b()) { return true; }
			}
			return false;
		}
		void exhaust() { target_it_ = target_end_ = nullptr; }

		const Target& operator*() const { return *target_it_; }
		const_iterator& operator++() {
			if (++target_it_ != target_end_) { return *this; }
			++b_;
			if (scan_b()) { return *this; }
			++a_;
			if (!scan_a()) { exhaust(); }
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return a_ == a_end_; }
		bool operator==(const const_iterator&) const = default;
		/// The key path to the current target, outermost first.
		auto keys() const { return std::tuple<typename P::Key, typename PostB::Key>{a_->key(), b_->key()}; }
	};

	explicit SuccessorCursor(const P& post) : post_{&post} {}

	const_iterator begin() const {
		const_iterator it;
		it.a_ = post_->begin();
		it.a_end_ = post_->end();
		if (!it.scan_a()) { it.exhaust(); }
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const P* post_;
};

/**
 * @brief The depth-4 cursor: three keys, then the targets. The cap.
 *
 * Same flat carry as arity 2, one post deeper. Composed costs 18.6% here. Past this arity the primary
 *  template is a hard error rather than a fallback.
 *
 * @warning As at arity 2: a wrong carry skips targets silently. Cross-checked in `tests/core/cursor.cc`.
 */
template <typename P> class SuccessorCursor<P, 3> {
  public:
	using Target = typename P::Target;
	using PostB = typename P::Entry::Nested;
	using PostC = typename PostB::Entry::Nested;

	class const_iterator {
	  public:
		typename P::const_iterator a_{}, a_end_{};
		typename PostB::const_iterator b_{}, b_end_{};
		typename PostC::const_iterator c_{}, c_end_{};
		const Target *target_it_{nullptr}, *target_end_{nullptr};

		bool scan_c() {
			for (; c_ != c_end_; ++c_) {
				const std::span<const Target> targets{c_->target_span()};
				if (!targets.empty()) {
					target_it_ = targets.data();
					target_end_ = target_it_ + targets.size();
					return true;
				}
			}
			return false;
		}
		bool scan_b() {
			for (; b_ != b_end_; ++b_) {
				c_ = b_->nested().begin();
				c_end_ = b_->nested().end();
				if (scan_c()) { return true; }
			}
			return false;
		}
		bool scan_a() {
			for (; a_ != a_end_; ++a_) {
				b_ = a_->nested().begin();
				b_end_ = a_->nested().end();
				if (scan_b()) { return true; }
			}
			return false;
		}
		void exhaust() { target_it_ = target_end_ = nullptr; }

		const Target& operator*() const { return *target_it_; }
		const_iterator& operator++() {
			if (++target_it_ != target_end_) { return *this; }
			++c_;
			if (scan_c()) { return *this; }
			++b_;
			if (scan_b()) { return *this; }
			++a_;
			if (!scan_a()) { exhaust(); }
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return a_ == a_end_; }
		bool operator==(const const_iterator&) const = default;
		/// The key path to the current target, outermost first.
		auto keys() const {
			return std::tuple<typename P::Key, typename PostB::Key, typename PostC::Key>{a_->key(), b_->key(), c_->key()};
		}
	};

	explicit SuccessorCursor(const P& post) : post_{&post} {}

	const_iterator begin() const {
		const_iterator it;
		it.a_ = post_->begin();
		it.a_end_ = post_->end();
		if (!it.scan_a()) { it.exhaust(); }
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const P* post_;
};


/**
 * @brief The generic transition relation: a vector of posts indexed by source state.
 *
 * The template behind @c mata::Delta, as @c mata::AutomatonBase is behind @c mata::Automaton; the
 *  shipped relation derives from `DeltaBase<StatePost>` in @c mata/relation.hh.
 *
 * A vector of posts indexed by source state, over a chain of posts ending in a set of targets. For
 *  an NFA that chain is one key deep — a symbol — so it is this class, one @c Post, targets, and the
 *  aliases @c mata::StatePost and @c mata::SymbolPost name that one post and its entry. The depth is
 *  not baked in here: @p P is the whole chain, and @c key_arity is read off it rather than declared;
 *  going deeper adds another @c Post, not another kind of class. Posts are named by key index, not by container count:
 *  @c Key<0> is the symbol an NFA keys by, and @c PostAt<key_arity> is the innermost post.
 *  @see @ref arity for why counting keys and not containers.
 *
 * @tparam P The post reached from one source state, itself parameterised down to the targets. See
 *  @ref nesting.
 * @tparam TT What one transition of this relation looks like and how to build one: a traits with a
 *  @c Type and a @c make(). Defaults to the symbol-named triple at arity 1 and to the tuple-keyed
 *  transition above it. @see mata::posts::DefaultTransitionTraits.
 * @see mata::DeltaLike.
 */
template <typename P, typename TT = DefaultTransitionTraits<P>> class DeltaBase {
  public:
	/// @name Post protocol
	/// @see mata::DeltaLike -- the only way @c mata::Automaton reaches a successor.
	///@{
	using PostType = P; ///< The post reached from one source state.
	using Target = typename PostType::Target; ///< What a successor walk yields.
	/// How the keyed members take a target. @see mata::posts::ArgOf.
	using TargetArg = ArgOf<Target>;
	/// What indexes this relation and the automaton's state sets. Derived from the target type
	///  rather than propagated through the posts: which state a target denotes is a property of
	///  the target, not of any post above it. @see mata::TargetTraits.
	using State = typename TargetTraits<Target>::State;
	static constexpr size_t key_arity{PostType::key_arity};

	/// The post @p I steps in: @c PostAt<0> is the whole chain and @c PostAt<key_arity> is the
	///  innermost post, the set of targets.
	template <size_t I> using PostAt = typename posts::PostAt<PostType, I>::type;
	/**
	 * @brief The key at index @p I, counted from the source state.
	 *
	 * Indexed rather than singular because a relation spans every post: at @c key_arity 2 there is
	 *  no "the key", and a member named @c Key would have meant the outermost one — right at arity 1
	 *  and quietly wrong above it. Key 0 is the one an NFA calls the symbol.
	 *
	 * @c mata::AutomatonBase never names this. It transports keys (in @c reverted(), as
	 *  `const auto&`) without inspecting one, which is why @c mata::DeltaLike does not ask for a key
	 *  type at all.
	 */
	template <size_t I> using Key = typename PostAt<I>::Key;
	/// Where key @p I's ordinary keys stop. Indexed for the same reason as @c Key: each post
	///  carries its own convention, on its own entry. @see mata::ReservedKeys.
	template <size_t I> using Reserved = typename PostAt<I>::Reserved;
	/// The innermost post: what a successor walk collects into. @c PostAt<key_arity> spelled for the
	///  one post that has a name of its own.
	using TargetSet = PostAt<key_arity>;
	/// @copydoc mata::posts::Post::KeyedSuccessors
	using KeyedSuccessors = typename PostType::KeyedSuccessors;
	/// @copydoc mata::TargetTraits::state_of
	static State state_of(const Target& target) { return TargetTraits<Target>::state_of(target); }
	/// The targets under one fully-supplied key, and a transition of this relation.
	using Entry = typename PostType::Entry;
	using Nested = typename PostType::Nested;
	/// The transition traits this relation was instantiated with, and the type they name. What the
	///  shape *is*, and what its fields are called, comes from @p TT so a module can name its own.
	///  @see mata::posts::DefaultTransitionTraits.
	using TransitionTraits = TT;
	using TransitionType = typename TT::Type;
	using CursorType = posts::SuccessorCursor<PostType>;
	///@}

	inline static const PostType empty_state_post; // When posts[q] is not allocated, then delta[q] returns this.

	DeltaBase() : state_posts_{} {}
	DeltaBase(const DeltaBase& other) = default;
	DeltaBase(DeltaBase&& other) = default;
	explicit DeltaBase(const size_t n) : state_posts_{n} {}

	DeltaBase& operator=(const DeltaBase& other) = default;
	DeltaBase& operator=(DeltaBase&& other) = default;

	bool operator==(const DeltaBase& other) const;

	void reserve(const size_t n) { state_posts_.reserve(n); };

	/**
	 * @brief Get constant reference to the state post of @p source.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, an @c empty_post is returned. Hence, the function has no side
	 *  effects (no allocation is performed; iterators remain valid).
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	const PostType& state_post(const State source) const {
		if (source >= num_of_states()) { return empty_state_post; }
		return state_posts_[source];
	}

	/**
	 * @brief Get constant reference to the state post of @p source.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, an @c empty_post is returned. Hence, the function has no side
	 *  effects (no allocation is performed; iterators remain valid).
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	const PostType& operator[](const State source) const { return state_post(source); }

	/**
	 * @brief Get mutable (non-constant) reference to the state post of @p source.
	 *
	 * The function allows modifying the state post.
	 *
	 * BEWARE, IT HAS A SIDE EFFECT.
	 *
	 * If we try to access a state post of a @p source which is present in the automaton as an initial/final state,
	 *  yet does not have allocated space in the relation, a new state post for @p source will be allocated along with
	 *  all state posts for all previous states. This in turn may cause that the entire post data structure is
	 *  re-allocated. Iterators into the relation will get invalidated.
	 * Use the constant 'state_post()' is possible. Or, to prevent the side effect from causing issues, one might want
	 *  to make sure that posts of all states in the automaton are allocated, e.g., write an NFA method that allocate
	 *  the relation for all states of the NFA.
	 * @param source[in] Source state of a state post to access.
	 * @return State post of @p source.
	 */
	PostType& mutable_state_post(State source);

	/**
	 * @brief Defragment the relation.
	 *
	 * This function removes all state posts which are not in @p is_staying and renames the remaining state posts
	 * according to @p renaming.
	 *
	 * @param[in] is_staying Boolean vector indicating which states are staying in the relation.
	 * @param[in] renaming Vector of states to rename the remaining state posts to.
	 * @return Self with defragmented delta.
	 */
	DeltaBase& defragment(const BoolVector& is_staying, const std::vector<State>& renaming);

	template <typename... Args> PostType& emplace_back(Args&&... args) {
		// Forwarding the variadic template pack of arguments to the emplace_back() of the underlying container.
		return state_posts_.emplace_back(std::forward<Args>(args)...);
	}

	void clear() { state_posts_.clear(); }

	/**
	 * @brief Allocate state posts up to @p num_of_states states, creating empty @c PostType for yet unallocated state
	 *  posts.
	 *
	 * @param[in] num_of_states Number of states in the relation to allocate state posts for. Have to be at least
	 *  num_of_states() + 1.
	 */
	void allocate(const size_t num_of_states) {
		MATA_ASSERT(num_of_states >= this->num_of_states());
		state_posts_.resize(num_of_states);
	}

	/**
	 * @return Number of states in the whole relation, including both source and target states.
	 */
	size_t num_of_states() const { return state_posts_.size(); }

	/**
	 * Check whether the @p state is used in the relation.
	 */
	bool uses_state(const State state) const { return state < num_of_states(); }

	/**
	 * @return Number of transitions in the relation.
	 */
	size_t num_of_transitions() const;

	void add(State source, Key<0> symbol, TargetArg target)
		requires(P::key_arity == 1);

	/// @name Keyed writes above arity 1
	///
	/// One overload per supported arity rather than one variadic member, for the same reason the
	/// cursor is hand-written per arity: it is the only shape that keeps *declared* parameter types.
	/// A trailing pack would be deduced from the caller, so `add(0, 1, 2, 3)` would carry `int` down
	/// and convert it to the key type once per post inside @c insert_target — a narrowing and a
	/// sign-conversion warning each, which `-Werror` rejects. Declared parameters convert at the
	/// call, exactly as the arity-1 overload above does. The cap is @c key_arity 3, so this is three
	/// overloads, not an open-ended family.
	///
	/// Each is a member template so that its parameter types depend on its own @p Q: `KeyOf<P, 1>`
	/// at @c key_arity 1 would name a post that does not exist, and a member's declared type is
	/// formed when the *class* is instantiated, before any constraint on it is looked at.
	///
	/// The arity-1 overloads are deliberately **not** routed through these recursions. `add` there
	/// has an append fast path (`back().key() < symbol`) that skips the search entirely when a
	/// relation is built in sorted order, which is the common case; @c insert_target always searches
	/// first, and searches twice on a miss.
	///@{
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void add(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) {
		resize_for_states(source, state_of(target));
		posts::insert_target(mutable_state_post(source), target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void add(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) {
		resize_for_states(source, state_of(target));
		posts::insert_target(mutable_state_post(source), target, k0, k1, k2);
	}

	/// @copydoc remove(State, Key<0>, State)
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void remove(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) {
		if (source >= state_posts_.size()) { return; }
		posts::erase_target(state_posts_[source], target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void remove(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) {
		if (source >= state_posts_.size()) { return; }
		posts::erase_target(state_posts_[source], target, k0, k1, k2);
	}

	/// @copydoc contains(State, Key<0>, State) const
	template <typename Q = P>
		requires(Q::key_arity == 2)
	bool contains(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, TargetArg target) const {
		return source < state_posts_.size() && posts::has_target_at(state_posts_[source], target, k0, k1);
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	bool contains(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		TargetArg target
	) const {
		return source < state_posts_.size() && posts::has_target_at(state_posts_[source], target, k0, k1, k2);
	}
	///@}

	/// @name Transition-shaped members
	/// Generic in the arity: a transition is taken apart with its type's @c parts() and the pieces
	///  are forwarded to the keyed overload of the matching arity. What a transition *is* comes from
	///  the traits; only @c Transitions, which has to walk the keys, is still arity 1.
	///@{
	void add(const TransitionType& transition) {
		std::apply([this](const auto&... p) { add(p...); }, TransitionType::parts(transition));
	}
	void remove(State source, Key<0> symbol, TargetArg target)
		requires(P::key_arity == 1);
	void remove(const TransitionType& transition) {
		std::apply([this](const auto&... p) { remove(p...); }, TransitionType::parts(transition));
	}
	/// Check whether the relation contains a passed transition.
	bool contains(State source, Key<0> symbol, TargetArg target) const
		requires(P::key_arity == 1);
	bool contains(const TransitionType& transition) const {
		return std::apply([this](const auto&... p) { return contains(p...); }, TransitionType::parts(transition));
	}
	///@}

	/**
	 * Check whether automaton contains no transitions.
	 * @return True if there are no transitions in the automaton, false otherwise.
	 */
	bool empty() const;

	/**
	 * @brief Append post vector to the delta.
	 *
	 * @param post_vector Vector of posts to be appended.
	 */
	void append(const std::vector<PostType>& post_vector) {
		for (const PostType& pst : post_vector) { this->state_posts_.push_back(pst); }
	}

	/**
	 * @brief Copy posts of delta and apply a lambda update function on each state from
	 * targets.
	 *
	 * IMPORTANT: In order to work properly, the lambda function needs to be
	 * monotonic, that is, the order of states in targets cannot change.
	 *
	 * @param target_renumberer Monotonic lambda function mapping states to different states.
	 * @return std::vector<Post> Copied posts.
	 */
	std::vector<PostType> renumber_targets(const std::function<State(State)>& target_renumberer) const;

	/**
	 * @brief Add transitions to multiple destinations
	 *
	 * @param source From
	 * @param symbol Key
	 * @param targets Set of states to
	 */
	void add(State source, Key<0> symbol, const Nested& targets)
		requires(P::key_arity == 1);
	/// @copydoc add(State,Key<0>,const Nested&)
	/// One key per post, then the whole set of targets, at arity 2 and 3.
	template <typename Q = P>
		requires(Q::key_arity == 2)
	void add(const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, const TargetSet& targets) {
		for (const Target& target : targets) {
			resize_for_states(source, state_of(target));
			posts::insert_target(mutable_state_post(source), target, k0, k1);
		}
	}
	template <typename Q = P>
		requires(Q::key_arity == 3)
	void add(
		const State source, posts::KeyOf<Q, 0> k0, posts::KeyOf<Q, 1> k1, posts::KeyOf<Q, 2> k2,
		const TargetSet& targets
	) {
		for (const Target& target : targets) {
			resize_for_states(source, state_of(target));
			posts::insert_target(mutable_state_post(source), target, k0, k1, k2);
		}
	}

	/**
	 * @brief Apply @p fn to every target state reachable from @p source, over any symbol.
	 */
	template <typename Fn> void for_each_successor(const State source, Fn&& fn) const {
		state_post(source).for_each_target(fn);
	}

	/**
	 * @brief Apply @p fn to every @c Move leaving @p source, as a (symbol, target) pair.
	 */
	template <typename Fn> void for_each_move(const State source, Fn&& fn) const {
		state_post(source).for_each_move(fn);
	}

	/**
	 * @brief Does @p source have @p target among its successors, over any symbol?
	 *
	 * @param[in] source Source state to look from.
	 * @param[in] target Target state to look for.
	 */
	bool is_successor(const State source, const State target) const {
		// By the state a target denotes, so a payload target counts whatever it carries.
		return any_target(state_post(source), [target](const Target& t) { return state_of(t) == target; });
	}

	/**
	 * @brief Does @p state have a transition back to itself over any symbol?
	 *
	 * @param[in] state State to check for self-loop.
	 * @return True if @p state has a transition back to itself over any symbol, false otherwise.
	 */
	bool has_self_loop(const State state) const { return is_successor(state, state); }

	/**
	 * @brief A resumable cursor over the successors of @p source. See @c SuccessorCursor.
	 *
	 * @param source Source state to get the successors of.
	 * @return A resumable cursor over the successors of @p source.
	 */
	CursorType successor_cursor(const State source) const { return CursorType{state_post(source)}; }

	using const_iterator = std::vector<PostType>::const_iterator;
	const_iterator cbegin() const { return state_posts_.cbegin(); }
	const_iterator cend() const { return state_posts_.cend(); }
	const_iterator begin() const { return state_posts_.begin(); }
	const_iterator end() const { return state_posts_.end(); }

	/**
	 * @brief The transitions of this relation: one @c TransitionType per (source, key path, target).
	 *
	 * Generic in the arity. The walk is @c CursorType -- the hand-written per-arity successor cursor,
	 *  which also reports the key path it is standing on -- and every step is one @c TT::make. At
	 *  arity 1 that is the same descent the previous arity-1-only iterator performed by hand.
	 */
	class Transitions {
	  public:
		/**
		 * Iterator over transitions.
		 *
		 * @note Inline, like @c Post::Moves::const_iterator and for the same reason: as a nested
		 *  class of a nested class of a template, each out-of-line definition would need three
		 *  layers of qualification.
		 */
		class const_iterator {
		  private:
			const DeltaBase* delta_{nullptr};
			State source_{};
			typename CursorType::const_iterator cursor_{};
			bool is_end_{true};
			TransitionType transition_{};

			void load() {
				transition_ = std::apply(
					[&](const auto&... keys) { return TT::make(source_, keys..., *cursor_); }, cursor_.keys()
				);
			}
			/// Position on the first transition leaving @c source_ or a later state, or become the end.
			void seek() {
				for (const size_t num_of_states{delta_->num_of_states()}; source_ < num_of_states; ++source_) {
					cursor_ = CursorType{delta_->state_post(source_)}.begin();
					if (cursor_ != std::default_sentinel) {
						is_end_ = false;
						load();
						return;
					}
				}
				is_end_ = true;
			}

		  public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = TransitionType;
			using difference_type = size_t;
			using pointer = TransitionType*;
			using reference = TransitionType&;

			const_iterator() = default; ///< The end.
			explicit const_iterator(const DeltaBase& delta) : const_iterator{delta, 0} {}
			/// The first transition leaving @p from or a later state.
			const_iterator(const DeltaBase& delta, const State from) : delta_{&delta}, source_{from} { seek(); }

			const TransitionType& operator*() const { return transition_; }
			const TransitionType* operator->() const { return &transition_; }

			const_iterator& operator++() {
				MATA_ASSERT(!is_end_);
				if (++cursor_ != std::default_sentinel) {
					load();
					return *this;
				}
				++source_;
				seek();
				return *this;
			}
			const_iterator operator++(int) {
				const const_iterator tmp{*this};
				++(*this);
				return tmp;
			}

			bool operator==(const const_iterator& other) const {
				if (is_end_ || other.is_end_) { return is_end_ == other.is_end_; }
				return source_ == other.source_ && cursor_ == other.cursor_;
			}
		}; // class const_iterator.

		Transitions() = default;
		explicit Transitions(const DeltaBase* delta) : delta_{delta} {}

		const_iterator begin() const { return const_iterator{*delta_}; }
		static const_iterator end() { return const_iterator{}; }

	  private:
		const DeltaBase* delta_{nullptr};
	}; // class Transitions.

	/**
	 * Iterator over transitions represented as @c Transition instances.
	 */
	Transitions transitions() const { return Transitions{this}; }

	/**
	 * Get transitions leading to @p state_to.
	 * @param state_to[in] Target state for transitions to get.
	 * @return Transitions leading to @p state_to.
	 *
	 * Operation is slow, traverses over all symbol posts.
	 */
	std::vector<TransitionType> get_transitions_to(State state_to) const;

	/**
	 * Get transitions from @p state_from to @p state_to.
	 * @param state_from[in] Source state.
	 * @param state_from[in] Target state.
	 * @return Transitions from @p source to @p state_to.
	 *
	 * Operation is slow, traverses over all symbol posts.
	 */
	std::vector<TransitionType> get_transitions_between(State state_from, State state_to) const;

	/**
	 * @brief Resize the delta to fit the given @p states.
	 * @tparam States A variadic parameter pack of states to resize the delta for.
	 * @param states States to resize the delta for.
	 */
	template <typename... States>
		requires utils::AllOfType<State, States...>
	DeltaBase& resize_for_states(States... states) {
		if constexpr (sizeof...(states) > 0) {
			if (const State max_state{std::max({static_cast<State>(states)...})}; max_state >= num_of_states()) {
				reserve_on_insert(state_posts_, max_state);
				state_posts_.resize(max_state + 1);
			}
		}
		return *this;
	}

	/**
	 * Get the set of states that are successors of the given @p state.
	 * @param[in] state State from which successors are checked.
	 * @return Set of states that are successors of the given @p state.
	 */
	TargetSet get_successors(State state) const;

	/**
	 * @brief The target states reachable from @p state over @p symbol.
	 *
	 * The *targets*, at every arity. Reaching one step down is @c PostType::find()'s job and
	 *  walking the posts between is @c for_each_move()'s; this answers "which states can I reach
	 *  over this key", which means the same thing however many keys are left below it.
	 *
	 * @see PostType::KeyedSuccessors for why the return type follows the arity — a reference into the
	 *  relation at arity 1, a fresh set above it.
	 */
	KeyedSuccessors get_successors(State state, Key<0> symbol) const;

	/**
	 * Iterate over @p epsilon symbol posts under the given @p state.
	 * @param[in] state State from which epsilon transitions are checked.
	 * @param[in] epsilon User can define his favourite epsilon or used default.
	 * @return An iterator to @c PostEntry with epsilon symbol. End iterator when there are no epsilon transitions.
	 */
	PostType::const_iterator epsilon_symbol_posts(State state, Key<0> epsilon = Reserved<0>::min_epsilon) const;

	/**
	 * Iterate over @p epsilon symbol posts under the given @p state_post.
	 * @param[in] state_post State post from which epsilon transitions are checked.
	 * @param[in] epsilon User can define his favourite epsilon or used default.
	 * @return An iterator to @c PostEntry with epsilon symbol. End iterator when there are no epsilon transitions.
	 */
	static PostType::const_iterator
	epsilon_symbol_posts(const PostType& state_post, Key<0> epsilon = Reserved<0>::min_epsilon);

  protected:
	/// @name Keys on the transitions
	///
	/// Which outermost keys are in use. They ask nothing of a key beyond the ordering the posts already
	///  rely on, so a relation keyed by an interval or a weight has them too; what those keys *stand
	///  for* is that relation's business. Protected for the same reason: a relation re-exports the
	///  ones it wants under the names that fit its keys, with a using-declaration or a forwarder.
	///  @c mata::Delta does the latter, as symbols, since for the shipped relation a key is one.
	///
	/// The three variants indexed *by* the key's value need an integer for it.
	///@{

	/**
	 * @brief Expand @p target_alphabet by the keys of this relation, as the symbols it hands out.
	 *
	 * Only for an alphabet dealing in this relation's key type, and only for a key that can print
	 *  itself (@c mata::Printable): a new symbol is named by its printed form, and a key without one
	 *  is refused here rather than every symbol getting the same placeholder. The value of the
	 *  already existing symbols will NOT be overwritten.
	 */
	template <ExtensibleAlphabet A>
		requires std::same_as<typename AlphabetTraits<A>::Symbol, typename P::Key> && Printable<typename P::Key>
	void add_keys_to(A& target_alphabet) const;

	/**
	 * @brief Get the set of keys used on the transitions.
	 *
	 * Does not necessarily have to equal the set of symbols in the alphabet used by the automaton.
	 */
	utils::OrdVector<typename P::Key> get_used_keys() const
		requires std::totally_ordered<typename P::Key>;

	utils::OrdVector<typename P::Key> get_used_keys_vec() const
		requires std::totally_ordered<typename P::Key>;
	std::set<typename P::Key> get_used_keys_set() const
		requires std::totally_ordered<typename P::Key>;
	utils::SparseSet<typename P::Key> get_used_keys_sps() const
		requires std::integral<typename P::Key>;
	/// @note Indexed *by* key, so it allocates up to the largest key used. Already unusable when
	///  the automaton has epsilons.
	std::vector<bool> get_used_keys_bv() const
		requires std::integral<typename P::Key>;
	/// @copydoc get_used_keys_bv
	BoolVector get_used_keys_chv() const
		requires std::integral<typename P::Key>;

	/**
	 * @brief The greatest key used, or nothing when there are no transitions.
	 *
	 * Over *every* key, epsilons included. That is what the one caller needs: NFT's simulation mints
	 *  fresh symbols at `max + 1` and above, which has to clear the epsilons too or the fresh
	 *  symbols collide with real transitions.
	 */
	std::optional<typename P::Key> get_max_key() const
		requires std::totally_ordered<typename P::Key>;
	///@}

	/// Append every transition from @p from to @p to. At arity 1 over bare states each entry's targets
	///  are searched directly; otherwise the key paths are walked and the target checked at the bottom.
	void collect_transitions_to_(const State from, const State to, std::vector<TransitionType>& out) const {
		if constexpr (key_arity == 1 && std::same_as<Target, State>) {
			for (const Entry& entry : state_post(from)) {
				if (entry.targets.find(to) != entry.targets.end()) { out.push_back(TT::make(from, entry.key(), to)); }
			}
		} else {
			state_post(from).for_each_move([&](const auto&... move) {
				if (state_of(std::get<sizeof...(move) - 1>(std::forward_as_tuple(move...))) == to) {
					out.push_back(TT::make(from, move...));
				}
			});
		}
	}

	std::vector<PostType> state_posts_;
}; // class mata::posts::DeltaBase.



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

/// A whole relation from its keys, spelled in terms of @c PostChain:
///  `RelationOf<Symbol, StateSet>` is @c mata::Delta.
template <typename... Ts> using RelationOf = DeltaBase<PostChain<Ts...>>;

/**
 * @brief The contract every concrete relation owes, stated once.
 *
 * Each check is its own @c static_assert rather than one concept, so that a failure names the check
 *  that failed. A module asks for all of them with a single line,
 *  `static_assert(posts::relation_contract_holds<Delta>());`, which may be repeated in every
 *  translation unit that includes the module's header -- unlike an explicit instantiation
 *  definition, which may appear only once in a program. Checks that belong to one module (its epsilon
 *  constant, its argument-passing policy) stay beside that module's relation.
 */
template <typename D> constexpr bool relation_contract_holds() {
	using P = typename D::PostType;
	static_assert(PostLike<P>, "the relation's PostType must be one post of the relation.");
	static_assert(PostEntryLike<typename D::Entry>, "the relation's Entry must be its PostType's entry type.");
	static_assert(
		ReservedKeysAtTail<P>,
		"the epsilon lookups walk back from the end of a post, which needs the reserved keys to be the last ones."
	);
	static_assert(TargetSetLike<typename D::TargetSet>, "the innermost post must be a set of targets.");
	static_assert(DeltaLike<D>, "the relation must satisfy the contract mata::Automaton is written against.");
	// A relation class deriving from DeltaBase exists only to shorten a name in diagnostics and must
	//  add nothing: the empty-base rules then make it the same size as its base, and the base converts
	//  to it without slicing. Compared against the base with the relation's *own* traits, not the
	//  default ones, or a relation naming other traits would be measured against a different type.
	using Base = DeltaBase<P, typename D::TransitionTraits>;
	static_assert(
		sizeof(D) == sizeof(Base) && alignof(D) == alignof(Base),
		"a relation class must add no members to DeltaBase; it exists only to shorten a name in diagnostics."
	);
	// Already required by DeltaLike; repeated for the message. The cursor is hand-written per arity.
	static_assert(
		D::key_arity <= 3,
		"a relation is capped at key_arity 3 (structure depth 4), because SuccessorCursor is hand-written "
		"per arity and three is where that stops paying. Past it, add a specialisation."
	);
	return true;
}

} // namespace mata::posts.

namespace posts {
/**
 * @brief Defragment the relation.
 *
 * Removes all state posts which are not in @p is_staying and renames the remaining ones according to
 *  @p renaming. Uses only the public interface, so it needs no friendship.
 *
 * @param[in] delta Relation to defragment.
 * @param[in] is_staying Boolean vector indicating which states are staying in the relation.
 * @param[in] renaming Vector of states to rename the remaining state posts to.
 * @return The defragmented relation.
 */
template <typename P, typename TT>
DeltaBase<P, TT> defragment(const DeltaBase<P, TT>& delta, const BoolVector& is_staying,
                    const std::vector<typename DeltaBase<P, TT>::State>& renaming);
} // namespace mata::posts.

/// Callers name this @c mata::defragment. Declared once, in @c posts, and re-exported here: having
///  it in both namespaces makes every unqualified call ambiguous through ADL.
using posts::defragment;

} // namespace mata.

#include "mata/core/delta.tpp"

#endif // MATA_CORE_DELTA_HH
