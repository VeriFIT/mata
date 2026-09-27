/** @file
 * @brief The walks over a chain of posts: reading, writing and rebuilding at any nesting depth.
 *
 * Free functions over a post @p P, each recursing on @c mata::arity_of until it reaches the targets.
 *  They are what @c mata::posts::Post, @c mata::posts::DeltaBase and @c mata::AutomatonBase share, so
 *  the depth is handled once, here, and not again in every class that holds a post.
 */

#ifndef MATA_CORE_WALKS_HH
#define MATA_CORE_WALKS_HH

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "mata/core/traits.hh"
#include "mata/utils/utils.hh"

namespace mata::posts {

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
 *  naming it would take @c mata::utils::format_or_unprintable at every post, for an error path.
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

} // namespace mata::posts.

#endif // MATA_CORE_WALKS_HH
