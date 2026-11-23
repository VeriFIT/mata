/** @file
 * @brief The resumable successor cursor: generic in the key arity, hand-written at arity 1.
 */

#ifndef MATA_CORE_CURSOR_HH
#define MATA_CORE_CURSOR_HH

#include <cstddef>
#include <iterator>
#include <span>
#include <tuple>
#include <utility>

#include "mata/core/traits.hh"

namespace mata::posts {

/**
 * @brief A generic resumable cursor over every target reachable from one state post.
 *
 * Flattens the descent down the post chain into a single pointer sweep,
 *  so a position can be stored and continued later.
 *
 * @tparam P The whole post chain, not one post of it.
 *
 * @note 
 */
template <typename P, size_t Arity = P::key_arity> class SuccessorCursor {
	template <size_t I> using PostAt = typename posts::PostAt<P, I>::type;

	/// The position in post @p I. One base per post keeps the iterator flat, outermost post first.
	template <size_t I> struct Position {
		typename PostAt<I>::const_iterator it{}, end{};
		bool operator==(const Position&) const = default;
	};
	template <typename> struct Positions;
	template <size_t... KeyIdx> struct Positions<std::index_sequence<KeyIdx...>> : Position<KeyIdx>... {
		bool operator==(const Positions&) const = default;
	};

  // Note: Having one cursor struct per post, each delegating downwards is slower
  //  by 24.4% at arity 2 and 18.6% at arity 3 against hand-written nested loops on the same shapes.
  public:
	using Target = typename P::Target;

	class const_iterator : Positions<std::make_index_sequence<Arity>> {
	  public:
		const Target *target_it_{nullptr}, *target_end_{nullptr};

		template <size_t I> Position<I>& at() { return *this; }
		template <size_t I> const Position<I>& at() const { return *this; }

		/**
		 * @brief Scan post @p I for the next target, descending into post `I + 1` as needed.
		 * 
		 * @tparam I The post index to scan.
		 * @return true if a target was found, false if post @p I is exhausted.
		 */
		template <size_t I> bool scan() {
			auto& [cur, last]{at<I>()};
			for (; cur != last; ++cur) {
				if constexpr (I + 1 == Arity) {
					const std::span<const Target> targets{cur->target_span()};
					if (!targets.empty()) {
						target_it_ = targets.data();
						target_end_ = target_it_ + targets.size();
						return true;
					}
				} else {
					at<I + 1>().it = cur->nested().begin();
					at<I + 1>().end = cur->nested().end();
					if (scan<I + 1>()) { return true; }
				}
			}
			return false;
		}

		/// Advance post @p I and rescan. When it is exhausted, carry into post `I - 1`.
		template <size_t I> void carry() {
			++at<I>().it;
			if (scan<I>()) { return; }
			if constexpr (I > 0) {
				carry<I - 1>();
			} else {
				exhaust();
			}
		}

		/// Exhaust the cursor: no more targets to yield.
		void exhaust() { target_it_ = target_end_ = nullptr; }

		const Target& operator*() const { return *target_it_; }
		const_iterator& operator++() {
			if (++target_it_ == target_end_) { carry<Arity - 1>(); }
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return at<0>().it == at<0>().end; }
		bool operator==(const const_iterator&) const = default;

		/// Returns the key path to the current target, outermost first.
		auto keys() const { return keys(std::make_index_sequence<Arity>{}); }

	  private:
		template <size_t... KeyIdx> auto keys(std::index_sequence<KeyIdx...>) const {
			return std::tuple<typename PostAt<KeyIdx>::Key...>{at<KeyIdx>().it->key()...};
		}
	};

	explicit SuccessorCursor(const P& post) : post_{&post} {}

	const_iterator begin() const {
		const_iterator it;
		it.template at<0>().it = post_->begin();
		it.template at<0>().end = post_->end();
		if (!it.template scan<0>()) { it.exhaust(); }
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const P* post_;
};

/**
 * @brief Specialization of @c SuccessorCursor for arity 1.
 *
 * @note The generic version is slower at arity 1 (on come cases by up to 24.4%).
 */
template <typename P> class SuccessorCursor<P, 1> {
  public:
	using Target = typename P::Target;

	class const_iterator {
	  public:
		typename P::const_iterator symbol_post_it_{}, symbol_post_end_{};
		const Target *target_it_{nullptr}, *target_end_{nullptr};

		/// Load the targets of the current symbol post into the target iterator.
		void load_targets() {
			const std::span<const Target> targets{symbol_post_it_->target_span()};
			target_it_ = targets.data();
			target_end_ = target_it_ + targets.size();
		}

		/// Seek the next target, loading the next symbol post if needed.
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

		/// Returns the key path to the current target, outermost first.
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

} // namespace mata::posts.

#endif // MATA_CORE_CURSOR_HH
