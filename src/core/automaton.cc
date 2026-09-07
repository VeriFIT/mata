/** @file
 * @brief The single in-tree instantiation of @c mata::AutomatonBase.
 *
 * The definitions themselves live in `include/mata/core/automaton.tpp`, because a third party
 *  instantiating @c mata::AutomatonBase over its own relation needs them. This translation unit
 *  exists to emit the depth-2 specialization once, matching the @c extern template declaration in
 *  `include/mata/core/automaton.hh`.
 */

#include "mata/core/automaton.hh"

template class mata::AutomatonBase<mata::Delta>;
