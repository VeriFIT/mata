/** @file
 * @brief The moves out of a post: each key paired with one target it leads to.
 */

#ifndef MATA_CORE_MOVES_HH
#define MATA_CORE_MOVES_HH

#include <cstddef>
#include <iterator>

namespace mata::posts {

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
 * @brief The moves of a post as @c Move instances: every (symbol, target) pair, or a range of them.
 *
 * What @c mata::posts::Post::moves() returns, named there @c Post::Moves. Walks the targets of each
 *  entry directly, so it is for a post whose entries hold targets: @c key_arity 1.
 *
 * @tparam P The post walked. Asked for @c Key, @c Target, @c Nested, @c const_iterator, and the
 *  entries' @c symbol and @c targets.
 */
template <typename P> class PostMoves {
	/// The iterator over the post's entries, which this walks.
	using post_iterator = typename P::const_iterator;

  public:
	using MoveType = Move<typename P::Key, typename P::Target>; ///< One (key, target) pair, shaped by the post.

	/**
	 * Iterator over moves.
	 *
	 * @note Defined inside @c PostMoves rather than out of line. As a nested class of a template,
	 *  each out-of-line member definition would need two layers of qualification; inline is the
	 *  same code and far harder to get wrong.
	 */
	class const_iterator {
	  private:
		const P* state_post_{nullptr};
		post_iterator symbol_post_it_{};
		typename P::Nested::const_iterator target_it_{};
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
		explicit const_iterator(const P& state_post)
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
			const P& state_post, const post_iterator symbol_post_it, const post_iterator symbol_post_end
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

	PostMoves() = default;

	/**
	 * @brief construct moves iterating over a range @p symbol_post_it (including) to @p symbol_post_end
	 * (excluding).
	 *
	 * @param[in] state_post State post to iterate over.
	 * @param[in] symbol_post_it First iterator over symbol posts to iterate over.
	 * @param[in] symbol_post_end End iterator over symbol posts (which functions as an sentinel; is not iterated
	 * over).
	 */
	PostMoves(const P& state_post, const post_iterator symbol_post_it, const post_iterator symbol_post_end)
		: state_post_{&state_post},
		  symbol_post_it_{symbol_post_it},
		  symbol_post_end_{symbol_post_end} {}

	PostMoves(PostMoves&&) = default;
	PostMoves(PostMoves&) = default;

	PostMoves& operator=(PostMoves&& other) noexcept {
		if (&other != this) {
			state_post_ = other.state_post_;
			symbol_post_it_ = other.symbol_post_it_;
			symbol_post_end_ = other.symbol_post_end_;
		}
		return *this;
	}
	PostMoves& operator=(const PostMoves& other) noexcept {
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
	const P* state_post_{nullptr};
	post_iterator symbol_post_it_{}; ///< Current symbol post iterator to iterate over.
	/// End symbol post iterator which is no longer iterated over (one after the last symbol post iterated over or
	///  end()).
	post_iterator symbol_post_end_{};
}; // class mata::posts::PostMoves.

} // namespace mata::posts.

#endif // MATA_CORE_MOVES_HH
