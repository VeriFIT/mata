/** @file
 * @brief One transition of a relation: what it is, how one is built, and how a relation's are walked.
 *
 * A transition type owes one thing, a static @c parts() that takes it apart into
 *  `(source, keys..., target)`. The hash, the formatter and the transition-shaped members of
 *  @c mata::posts::DeltaBase are all written against that, so a module's own transition type gets
 *  them without anything further.
 */

#ifndef MATA_CORE_TRANSITION_HH
#define MATA_CORE_TRANSITION_HH

#include <cstddef>
#include <format>
#include <functional>
#include <iterator>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "mata/core/traits.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

namespace mata::posts {
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

	/// `(source, symbol, target)`, each by value or by reference as @c mata::utils::ArgOf decides.
	static auto parts(const Transition& t) {
		return std::tuple<utils::ArgOf<St>, utils::ArgOf<K>, utils::ArgOf<T>>{t.source, t.symbol, t.target};
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
		return std::tuple_cat(std::tuple<utils::ArgOf<St>>{t.source}, t.keys, std::tuple<utils::ArgOf<T>>{t.target});
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

namespace mata::posts {
/// Streams a transition through its @c std::formatter. Found by ADL for the shipped shapes; a
///  module's own type in another namespace writes this same line there.
template <TransitionLike T>
	requires std::formattable<T, char>
std::ostream& operator<<(std::ostream& os, const T& t) {
	return os << std::format("{}", t);
}

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
 *         return std::tuple<utils::ArgOf<State>, utils::ArgOf<Symbol>, utils::ArgOf<Symbol>, utils::ArgOf<State>>{t.source, t.input, t.output, t.target};
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
	static Type make(utils::ArgOf<State> source, utils::ArgOf<KeyOf<P, 0>> key, utils::ArgOf<target_of<P>> target) {
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
	static Type make(utils::ArgOf<State> source, const Rest&... rest) {
		return make_(source, std::forward_as_tuple(rest...), std::make_index_sequence<arity_of<P>>{});
	}

  private:
	template <typename Tup, size_t... Is>
	static Type make_(utils::ArgOf<State> source, const Tup& rest, std::index_sequence<Is...>) {
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
 * @brief The transitions of a relation: one @c TransitionType per (source, key path, target).
 *
 * Generic in the arity. The walk is the relation's @c CursorType -- the hand-written per-arity
 *  successor cursor, which also reports the key path it is standing on -- and every step is one
 *  `TransitionTraits::make`. What @c mata::posts::DeltaBase::transitions() returns, named there
 *  @c DeltaBase::Transitions.
 *
 * @tparam D The relation walked. Asked for @c State, @c CursorType, @c TransitionType,
 *  @c TransitionTraits, @c num_of_states() and @c state_post(), and nothing else.
 */
template <typename D> class DeltaTransitions {
	using State = typename D::State;
	using CursorType = typename D::CursorType;
	using TransitionType = typename D::TransitionType;
	using TT = typename D::TransitionTraits;

  public:
	/**
	 * Iterator over transitions.
	 *
	 * @note Inline, like @c PostMoves::const_iterator and for the same reason: as a nested class of
	 *  a template, each out-of-line definition would need two layers of qualification.
	 */
	class const_iterator {
	  private:
		const D* delta_{nullptr};
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
		explicit const_iterator(const D& delta) : const_iterator{delta, 0} {}
		/// The first transition leaving @p from or a later state.
		const_iterator(const D& delta, const State from) : delta_{&delta}, source_{from} { seek(); }

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

	DeltaTransitions() = default;
	explicit DeltaTransitions(const D* delta) : delta_{delta} {}

	const_iterator begin() const { return const_iterator{*delta_}; }
	static const_iterator end() { return const_iterator{}; }

  private:
	const D* delta_{nullptr};
}; // class mata::posts::DeltaTransitions.
} // namespace mata::posts.

#endif // MATA_CORE_TRANSITION_HH
