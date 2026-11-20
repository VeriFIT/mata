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

/**
 * @brief The moves of a post as @c Move instances: every (symbol, target) pair, or a range of them.
 *
 * Available only at @c key_arity 1, since a @c Move holds a single key. Above that, use @c for_each_move().
 *
 * @tparam P The post visited.
 */
template <typename P> class PostMoves {
	using post_iterator = typename P::const_iterator; 

  public:
	using MoveType = Move<typename P::Key, typename P::Target>; 

	/**
	 * Iterator over moves.
	 *
	 * @note Defined here rather than out of line. As a nested class of a template,
	 *  each out-of-line member definition would need two layers of qualification.
	 */
	class const_iterator {
	  private:
		const P* state_post_{nullptr};
		post_iterator symbol_post_it_{};
		typename P::Nested::const_iterator target_it_{};
		post_iterator symbol_post_end_{};
		bool is_end_{false};
		MoveType move_{};

	  public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = MoveType;
		using difference_type = std::ptrdiff_t;
		using pointer = const MoveType*;
		using reference = const MoveType&;

		/// Construct end iterator.
		const_iterator() : is_end_{true} {}

		/// @brief Construct iterator over all moves of @p state_post.
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

		/**
		 * @brief Construct iterator from @p symbol_post_it (including) to @p symbol_post_end (excluding).
		 * 
		 * @param state_post State post to iterate over.
		 * @param symbol_post_it First iterator over symbol posts to iterate over.
		 * @param symbol_post_end End iterator over symbol posts.
		 */
		const_iterator(const P& state_post, const post_iterator symbol_post_it, const post_iterator symbol_post_end)
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

			// Iterate over to the next symbol post, check for end.
			++symbol_post_it_;
			if (symbol_post_it_ == symbol_post_end_) {
				is_end_ = true;
				return *this;
			}
			// The current symbol post is valid.
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
	 * @brief Construct moves iterating over a range @p symbol_post_it (including) to @p symbol_post_end (excluding).
	 *
	 * @param[in] state_post State post to iterate over.
	 * @param[in] symbol_post_it First iterator over symbol posts to iterate over.
	 * @param[in] symbol_post_end End iterator over symbol posts.
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
	post_iterator symbol_post_end_{}; ///< End symbol post iterator.
}; // class mata::posts::PostMoves.

} // namespace mata::posts.

#endif // MATA_CORE_MOVES_HH
