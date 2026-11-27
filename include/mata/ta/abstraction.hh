/** @file
 * @brief Predicate and bounded-height abstractions of ranked tree automata.
 */

#ifndef MATA_TA_ABSTRACTION_HH
#define MATA_TA_ABSTRACTION_HH

#include "mata/ta/ta.hh"

#include <initializer_list>
#include <span>
#include <vector>

namespace mata::ta {

/// Returns the language intersection of two automata, trimmed to useful product states.
Ta intersection(const Ta& lhs, const Ta& rhs);

/**
 * Computes the residual predicate-state abstraction of abstract regular model checking.
 *
 * The subject is trimmed, determinized, and trimmed again. A state receives one label for every predicate state whose
 * residual tree language intersects its own; equal complete label vectors are merged. With @p whole_languages, only
 * final predicate states receive labels, so fingerprints compare whole predicate languages. The quotient accepts a
 * superset of the subject language and keeps its ranked alphabet. Null predicate pointers are rejected.
 */
Ta collapse_by_predicates(const Ta& subject, std::initializer_list<const Ta*> predicates, bool whole_languages = false);
Ta collapse_by_predicates(const Ta& subject, const std::vector<Ta>& predicates, bool whole_languages = false);
Ta collapse_by_predicates(const Ta& subject, std::span<const Ta* const> predicates, bool whole_languages = false);

/**
 * Merges states with equal accepting contexts of depth at most @p height.
 *
 * The subject is determinized and trimmed first. Height zero distinguishes only final and nonfinal states. Every later
 * Moore round compares one-hole parent contexts, including symbol, hole position, ordered sibling classes, and target
 * class, stopping early on stability. The quotient is trimmed and keeps the subject's ranked alphabet.
 */
Ta collapse_by_height(const Ta& subject, size_t height);

} // namespace mata::ta

#endif // MATA_TA_ABSTRACTION_HH
