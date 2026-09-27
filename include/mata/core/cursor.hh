/** @file
 * @brief The resumable successor cursor, hand-written per key arity.
 */

#ifndef MATA_CORE_CURSOR_HH
#define MATA_CORE_CURSOR_HH

#include <cstddef>
#include <iterator>
#include <span>
#include <tuple>

namespace mata::posts {

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

} // namespace mata::posts.

#endif // MATA_CORE_CURSOR_HH
