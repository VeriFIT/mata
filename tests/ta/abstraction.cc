/** @file
 * @brief Tests for ranked residual and accepting-context abstractions.
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/ta/abstraction.hh"

#include <stdexcept>
#include <vector>

using namespace mata::ta;
using mata::Symbol;

namespace {

Tree unary(const Symbol symbol, const Tree& child) { return Tree{symbol, {child}}; }
Tree binary(const Symbol symbol, const Tree& left, const Tree& right) { return Tree{symbol, {left, right}}; }

Ta ordered_subject() {
	Ta aut{};
	const State a{aut.add_state()};
	const State b{aut.add_state()};
	const State ab{aut.add_state()};
	const State ba{aut.add_state()};
	aut.final = {ab, ba};
	aut.add_transition('a', a);
	aut.add_transition('b', b);
	aut.add_transition('f', {a, b}, ab);
	aut.add_transition('f', {b, a}, ba);
	return aut;
}

Ta ordered_predicate() {
	Ta predicate{};
	const State a{predicate.add_state()};
	const State b{predicate.add_state()};
	const State ab{predicate.add_state()};
	predicate.final = {ab};
	predicate.add_transition('a', a);
	predicate.add_transition('b', b);
	predicate.add_transition('f', {a, b}, ab);
	return predicate;
}

Ta unary_chain() {
	Ta aut{};
	const State leaf{aut.add_state()};
	const State root{aut.add_state()};
	aut.final = {root};
	aut.add_transition('a', leaf);
	aut.add_transition('f', {leaf}, root);
	return aut;
}

} // namespace

TEST_CASE("mata::ta::intersection computes ordered synchronized languages") {
	Ta lhs{};
	const State la{lhs.add_state()};
	const State lb{lhs.add_state()};
	const State laa{lhs.add_state()};
	const State lab{lhs.add_state()};
	lhs.final = {la, laa, lab};
	lhs.add_transition('a', la);
	lhs.add_transition('b', lb);
	lhs.add_transition('f', {la, la}, laa);
	lhs.add_transition('f', {la, lb}, lab);

	Ta rhs{};
	const State ra{rhs.add_state()};
	const State rb{rhs.add_state()};
	const State raa{rhs.add_state()};
	const State rab{rhs.add_state()};
	rhs.final = {raa, rab};
	rhs.add_transition('a', ra);
	rhs.add_transition('b', rb);
	rhs.add_transition('f', {ra, ra}, raa);
	rhs.add_transition('f', {ra, rb}, rab);

	const Ta both{intersection(lhs, rhs)};
	CHECK(both.accepts(binary('f', Tree{'a'}, Tree{'a'})));
	CHECK(both.accepts(binary('f', Tree{'a'}, Tree{'b'})));
	CHECK(!both.accepts(Tree{'a'})); // The left leaf is final, but the right leaf is not.
	CHECK(!both.accepts(Tree{'b'}));
	CHECK(!both.accepts(binary('f', Tree{'b'}, Tree{'a'})));
	CHECK(!both.accepts(binary('f', Tree{'b'}, Tree{'b'})));
	CHECK_NOTHROW(both.validate());

	Ta reversed{};
	const State xa{reversed.add_state()};
	const State xb{reversed.add_state()};
	const State root{reversed.add_state()};
	reversed.final = {root};
	reversed.add_transition('a', xa);
	reversed.add_transition('b', xb);
	reversed.add_transition('f', {xb, xa}, root);
	CHECK(intersection(lhs, reversed).is_lang_empty());
}

TEST_CASE("mata::ta::collapse_by_predicates uses exact residual labels") {
	const Ta subject{ordered_subject()};
	const Ta predicate{ordered_predicate()};
	const Ta states{collapse_by_predicates(subject, {predicate})};
	const Ta borrowed{collapse_by_predicates(subject, {&predicate})};
	const Ta languages{collapse_by_predicates(subject, {predicate}, true)};
	const Tree ab{binary('f', Tree{'a'}, Tree{'b'})};
	const Tree ba{binary('f', Tree{'b'}, Tree{'a'})};
	const Tree aa{binary('f', Tree{'a'}, Tree{'a'})};
	const Tree bb{binary('f', Tree{'b'}, Tree{'b'})};

	// The inductive target pair gives ab the root label, while a and b have distinct nonfinal predicate labels.
	CHECK(states.num_of_states() == subject.num_of_states());
	CHECK(borrowed.num_of_states() == states.num_of_states());
	CHECK(borrowed.accepts(ab));
	CHECK(!borrowed.accepts(aa));
	for (const Tree* tree : {&ab, &ba}) { CHECK(states.accepts(*tree)); }
	for (const Tree* tree : {&aa, &bb}) { CHECK(!states.accepts(*tree)); }
	CHECK(languages.num_of_states() < states.num_of_states());
	CHECK(languages.accepts(ab));
	CHECK(languages.accepts(ba));
	CHECK(languages.accepts(aa));
	CHECK(languages.accepts(bb));
	CHECK_NOTHROW(states.validate());
	CHECK_NOTHROW(languages.validate());
}

TEST_CASE("mata::ta::collapse_by_predicates filters labels by finality only in whole-language mode") {
	const Ta subject{unary_chain()};
	Ta predicate{unary_chain()};
	predicate.final.clear();

	const Ta states{collapse_by_predicates(subject, {predicate})};
	const Ta languages{collapse_by_predicates(subject, {predicate}, true)};
	const Tree once{unary('f', Tree{'a'})};
	const Tree twice{unary('f', once)};
	CHECK(states.accepts(once));
	CHECK(!states.accepts(twice));
	CHECK(languages.accepts(once));
	CHECK(languages.accepts(twice));
	CHECK(languages.num_of_states() < states.num_of_states());

	// The same empty fingerprints arise with no predicate or with a predicate having no states.
	for (const Ta& empty_fingerprints :
		 {collapse_by_predicates(subject, {}), collapse_by_predicates(subject, {Ta{}})}) {
		CHECK(empty_fingerprints.accepts(once));
		CHECK(empty_fingerprints.accepts(twice));
		CHECK_NOTHROW(empty_fingerprints.validate());
	}

	const Ta no_subject_language{collapse_by_predicates(Ta{RankedAlphabet{{'a', 0}}}, {predicate})};
	CHECK(no_subject_language.is_lang_empty());
	CHECK(no_subject_language.delta.alphabet().arity('a') == 0);
	const Ta* null_predicate[]{nullptr};
	CHECK_THROWS_AS(collapse_by_predicates(subject, std::span<const Ta* const>{null_predicate}), std::invalid_argument);
}

TEST_CASE("mata::ta::collapse_by_height compares bounded accepting contexts") {
	Ta aut{};
	const State a{aut.add_state()};
	const State b{aut.add_state()};
	const State fa{aut.add_state()};
	const State fb{aut.add_state()};
	const State ga{aut.add_state()};
	const State hb{aut.add_state()};
	aut.final = {ga, hb};
	aut.add_transition('a', a);
	aut.add_transition('b', b);
	aut.add_transition('f', {a}, fa);
	aut.add_transition('f', {b}, fb);
	aut.add_transition('g', {fa}, ga);
	aut.add_transition('h', {fb}, hb);

	const Tree ga_tree{unary('g', unary('f', Tree{'a'}))};
	const Tree hb_tree{unary('h', unary('f', Tree{'b'}))};
	const Tree gb_tree{unary('g', unary('f', Tree{'b'}))};
	const Tree ha_tree{unary('h', unary('f', Tree{'a'}))};
	const Ta zero{collapse_by_height(aut, 0)};
	const Ta one{collapse_by_height(aut, 1)};
	const Ta two{collapse_by_height(aut, 2)};
	const Ta stable{collapse_by_height(aut, 20)};

	for (const Ta* abstraction : {&zero, &one, &two, &stable}) {
		CHECK(abstraction->accepts(ga_tree));
		CHECK(abstraction->accepts(hb_tree));
		CHECK_NOTHROW(abstraction->validate());
	}
	// At heights zero and one the leaf states still have equal bounded accepting contexts, unlike subtree languages.
	for (const Ta* abstraction : {&zero, &one}) {
		CHECK(abstraction->accepts(gb_tree));
		CHECK(abstraction->accepts(ha_tree));
	}
	// The second round propagates the distinct g and h parent contexts through f.
	CHECK(!two.accepts(gb_tree));
	CHECK(!two.accepts(ha_tree));
	CHECK(!stable.accepts(gb_tree));
	CHECK(!stable.accepts(ha_tree));
	CHECK(stable.num_of_states() == two.num_of_states());
	CHECK(stable.delta.alphabet().arity('f') == 1);
}
