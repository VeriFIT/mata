/** @file
 * @brief core/ on its own: none of the shipped relation is included, and a relation over keys and states it never
 *  names is built from it.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"
#include "mata/core/cursor.hh"
#include "mata/core/delta.hh"
#include "mata/core/moves.hh"
#include "mata/core/post.hh"
#include "mata/core/tarjan.hh"
#include "mata/core/traits.hh"
#include "mata/core/transition.hh"
#include "mata/core/visits.hh"
#include "mata/utils/ord-vector.hh"

#if defined(MATA_RELATION_HH) || defined(MATA_TYPES_HH) || defined(MATA_ALPHABET_HH)
#error "a core/ header includes the shipped relation, its types or the alphabet"
#endif

// Neither the key nor the state is the shipped one: char is not mata::Symbol, std::uint16_t is not mata::State.
using CoreOnlyDelta = mata::posts::RelationOf<char, mata::utils::OrdVector<std::uint16_t>>;

static_assert(mata::DeltaLike<CoreOnlyDelta>);
static_assert(mata::posts::relation_contract_holds<CoreOnlyDelta>());

// Not in an anonymous namespace: explicitly instantiating over it would warn about every unused member.
struct CoreOnlyAutomaton : mata::AutomatonBase<CoreOnlyDelta> {
	CoreOnlyAutomaton() = default; // So that `aut{}` is not aggregate initialisation.
};

template class mata::AutomatonBase<CoreOnlyDelta>;

TEST_CASE("mata::AutomatonBase from core/ alone, over char keys and 16-bit states") {
	// 0 -a-> 1 -b-> 2, and 3 -c-> 0 reaching in from the side.
	CoreOnlyAutomaton aut{};
	aut.delta.add(0, 'a', 1);
	aut.delta.add(1, 'b', 2);
	aut.delta.add(3, 'c', 0);
	aut.initial.insert(0);
	aut.final.insert(2);

	CHECK(aut.delta.num_of_transitions() == 3);
	CHECK(aut.delta.contains(1, 'b', 2));
	CHECK_FALSE(aut.is_lang_empty());

	aut.trim();
	CHECK(aut.num_of_states() == 3);
	CHECK(aut.delta.num_of_transitions() == 2);
	CHECK_FALSE(aut.delta.contains(3, 'c', 0));
}
