/** @file
 * @brief The moves of a post with @c key_arity 1, as @c Move instances.
 */

#ifndef MATA_CORE_MOVES_HH
#define MATA_CORE_MOVES_HH

#include <cstddef>
#include <iterator>

namespace mata::posts {

/**
 * @brief A move is a (key, target) pair, as yielded by @c PostMoves.
 *
 * Templated only so that it follows the post it comes from.
 */
template <typename K, typename T> class Move {
  public:
	K symbol;
	T target;

	bool operator==(const Move&) const = default;
}; // class mata::posts::Move.

namespace detail {
/// The move type of the post @p P: the @c MoveType its entries name, or @c Move of its key and target.
template <typename P> struct MoveTypeOf {
	using type = Move<typename P::Key, typename P::Target>;
};
template <typename P>
	requires requires { typename P::Entry::MoveType; }
struct MoveTypeOf<P> {
	using type = typename P::Entry::MoveType;
};
} // namespace detail

/**
 * @brief The moves of a post as @c Move instances: every (symbol, target) pair, or a range of them.
 *
 * Available only at @c key_arity 1, since a @c Move holds a single key. Above that, use @c for_each_move().
 *
 * @tparam P The post visited.
 */
template <typename P> class PostMoves {
	static_assert(P::key_arity == 1, "a Move holds one key: iterate a deeper post with for_each_move()");

	using post_iterator = typename P::const_iterator;

  public:
	using MoveType = typename detail::MoveTypeOf<P>::type;

	/**
	 * Iterator over moves.
	 *
	 * @note Defined here rather than out of line. As a nested class of a template,
	 *  each out-of-line member definition would need two layers of qualification.
	 */
	class const_iterator {
	  private:
		post_iterator symbol_post_it_{};
		typename P::Nested::const_iterator target_it_{};
		post_iterator symbol_post_end_{};
		MoveType move_{};

		bool at_end() const { return symbol_post_it_ == symbol_post_end_; }

	  public:
		using iterator_category = std::forward_iterator_tag;
		using iterator_concept = std::forward_iterator_tag;
		using value_type = MoveType;
		using difference_type = std::ptrdiff_t;
		using pointer = const MoveType*;
		/// By value: a reference into the iterator would dangle once the copy that produced it is destroyed.
		using reference = MoveType;

		/// Construct end iterator.
		const_iterator() = default;

		/// @brief Construct iterator over all moves of @p state_post.
		explicit const_iterator(const P& state_post) : const_iterator{state_post.begin(), state_post.end()} {}

		/// @brief Construct iterator from @p symbol_post_it (including) to @p symbol_post_end (excluding).
		const_iterator(const post_iterator symbol_post_it, const post_iterator symbol_post_end)
			: symbol_post_it_{symbol_post_it},
			  symbol_post_end_{symbol_post_end} {
			if (at_end()) { return; }
			move_.symbol = symbol_post_it_->symbol;
			target_it_ = symbol_post_it_->targets.cbegin();
			move_.target = *target_it_;
		}

		const_iterator(const const_iterator& other) noexcept = default;
		const_iterator(const_iterator&&) = default;
		const_iterator& operator=(const const_iterator& other) noexcept = default;
		const_iterator& operator=(const_iterator&&) = default;
		~const_iterator() = default;

		MoveType operator*() const { return move_; }
		const MoveType* operator->() const { return &move_; }

		const_iterator& operator++() {
			target_it_ = std::next(target_it_);
			if (target_it_ != symbol_post_it_->targets.end()) {
				move_.target = *target_it_;
				return *this;
			}
			// Iterate over to the next symbol post, check for end.
			++symbol_post_it_;
			if (at_end()) { return *this; }
			move_.symbol = symbol_post_it_->symbol;
			target_it_ = symbol_post_it_->targets.begin();
			move_.target = *target_it_;
			return *this;
		}

		const_iterator operator++(int) {
			const const_iterator tmp{*this};
			++(*this);
			return tmp;
		}

		bool operator==(const const_iterator& other) const {
			// A default-constructed end equals the end of any range, even if the actual end iterators do not match.
			const bool this_at_end{at_end()};
			const bool other_at_end{other.at_end()};
			if (this_at_end || other_at_end) { return this_at_end && other_at_end; }
			return symbol_post_it_ == other.symbol_post_it_ && target_it_ == other.target_it_ &&
				   symbol_post_end_ == other.symbol_post_end_;
		}
	}; // class const_iterator.

	PostMoves() = default;

	/// @brief Construct moves iterating over a range @p symbol_post_it (including) to @p symbol_post_end (excluding).
	PostMoves(const post_iterator symbol_post_it, const post_iterator symbol_post_end)
		: symbol_post_it_{symbol_post_it},
		  symbol_post_end_{symbol_post_end} {}
	/// @copydoc PostMoves(post_iterator, post_iterator)
	PostMoves(const P& /*state_post*/, const post_iterator symbol_post_it, const post_iterator symbol_post_end)
		: PostMoves{symbol_post_it, symbol_post_end} {}

	PostMoves(PostMoves&&) = default;
	PostMoves(const PostMoves&) = default;
	PostMoves& operator=(PostMoves&& other) noexcept = default;
	PostMoves& operator=(const PostMoves& other) noexcept = default;
	~PostMoves() = default;

	const_iterator begin() const { return {symbol_post_it_, symbol_post_end_}; }
	static const_iterator end() { return const_iterator{}; }

  private:
	post_iterator symbol_post_it_{}; ///< Current symbol post iterator to iterate over.
	post_iterator symbol_post_end_{}; ///< End symbol post iterator.
}; // class mata::posts::PostMoves.

} // namespace mata::posts.

#endif // MATA_CORE_MOVES_HH
