/** @file
 * @brief Tests for ranked tree transducers and their explicit reader.
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/ta/abstraction.hh"
#include "mata/ta/transducer-builder.hh"
#include "mata/ta/transducer.hh"

#include <sstream>
#include <stdexcept>
#include <string>

using namespace mata::ta;
using mata::OnTheFlyAlphabet;
using mata::Symbol;

namespace {

constexpr Symbol n{'n'};
constexpr Symbol t{'t'};
constexpr Symbol nn{'N'};
constexpr Symbol tt{'T'};

Tree unary(const Symbol symbol, const Tree& child) { return Tree{symbol, {child}}; }
Tree binary(const Symbol symbol, const Tree& left, const Tree& right) { return Tree{symbol, {left, right}}; }

Ta token_initial() {
	Ta aut{};
	const State zero{aut.add_state()};
	const State one{aut.add_state()};
	aut.final = {one};
	aut.add_transition(n, zero);
	aut.add_transition(t, one);
	aut.add_transition(nn, {zero, zero}, zero);
	aut.add_transition(nn, {one, zero}, one);
	aut.add_transition(nn, {zero, one}, one);
	return aut;
}

Ta token_bad() {
	Ta aut{};
	const State zero{aut.add_state()};
	const State one{aut.add_state()};
	const State bad{aut.add_state()};
	aut.final = {bad};
	aut.add_transition(n, zero);
	aut.add_transition(t, one);
	aut.add_transition(tt, {zero, zero}, one);
	aut.add_transition(nn, {zero, zero}, zero);
	aut.add_transition(nn, {one, zero}, one);
	aut.add_transition(nn, {zero, one}, one);
	for (const State child : {zero, one, bad}) {
		aut.add_transition(nn, {bad, child}, bad);
		aut.add_transition(nn, {child, bad}, bad);
		aut.add_transition(tt, {bad, child}, bad);
		aut.add_transition(tt, {child, bad}, bad);
	}
	aut.add_transition(tt, {one, zero}, bad);
	aut.add_transition(tt, {zero, one}, bad);
	aut.add_transition(tt, {one, one}, bad);
	aut.add_transition(nn, {one, one}, bad);
	return aut;
}

const std::string& token_transducer_source() {
	static const std::string source{"@TT-explicit\n"
									"%Alphabet-auto\n"
									"%Initial q0\n"
									"%Final q0 q2\n"
									"n -> n : q0\n"
									"t -> n : q1\n"
									"tt(q0,q0) -> nn : q1\n"
									"nn(q1,q0) -> tt : q2\n"
									"nn(q0,q1) -> tt : q2\n"
									"nn(q0,q0) -> nn : q0\n"
									"nn(q2,q0) -> nn : q2\n"
									"nn(q0,q2) -> nn : q2\n"};
	return source;
}

TreeTransducer safe_token_transducer() {
	OnTheFlyAlphabet alphabet{{{"n", n}, {"t", t}, {"nn", nn}, {"tt", tt}}};
	return transducer_builder::parse_from_mata(token_transducer_source(), &alphabet);
}

} // namespace

TEST_CASE("mata::ta::TreeTransducer applies and inverts structure-preserving relations") {
	SECTION("intermediate transducer states are part of the product") {
		Ta input{};
		const State leaf{input.add_state()};
		const State root{input.add_state()};
		input.final = {root};
		input.add_transition('a', leaf);
		input.add_transition('f', {leaf}, root);

		TreeTransducer transducer{};
		const State q0{transducer.add_state()};
		const State q1{transducer.add_state()};
		transducer.final = {q1};
		transducer.add_transition('a', 'x', q0);
		transducer.add_transition('f', {q0}, 'g', q1);

		const Ta output{transducer.apply(input)};
		CHECK(output.accepts(unary('g', Tree{'x'})));
		CHECK(!output.accepts(Tree{'x'}));
		CHECK(!output.accepts(unary('f', Tree{'a'})));
		CHECK_NOTHROW(output.validate());

		const TreeTransducer inverse{transducer.invert()};
		CHECK(inverse.final == transducer.final);
		CHECK(inverse.num_of_states() == transducer.num_of_states());
		CHECK(inverse.delta.input_alphabet().arity('x') == 0);
		CHECK(inverse.delta.output_alphabet().arity('a') == 0);
		const Ta round_trip{inverse.apply(output)};
		CHECK(round_trip.accepts(unary('f', Tree{'a'})));
		CHECK(!round_trip.accepts(unary('g', Tree{'x'})));
	}

	SECTION("ordered children and the safe token step") {
		const Ta initial{token_initial()};
		const TreeTransducer step{safe_token_transducer()};
		const Ta reached{step.apply(initial)};
		const Tree token_left{binary(tt, Tree{n}, Tree{n})};
		const Tree token_under_node{binary(nn, token_left, Tree{n})};
		CHECK(reached.accepts(token_left));
		CHECK(reached.accepts(token_under_node));
		CHECK(!reached.accepts(Tree{n}));
		CHECK(!reached.accepts(Tree{t}));
		CHECK(intersection(reached, token_bad()).is_lang_empty());

		TreeTransducer copying{};
		const State q0{copying.add_state()};
		copying.final = {q0};
		copying.add_transition(n, t, q0);
		copying.add_transition(t, t, q0);
		copying.add_transition(nn, {q0, q0}, nn, q0);
		const Ta copied{copying.apply(initial)};
		const Ta bad_copies{intersection(copied, token_bad())};
		CHECK(!bad_copies.is_lang_empty());
		CHECK(bad_copies.accepts(binary(nn, Tree{t}, Tree{t})));
	}
}

TEST_CASE("mata::ta::TreeTransducer enforces ranked and state invariants") {
	TreeTransducer transducer{1, RankedAlphabet{{'f', 2}}, RankedAlphabet{{'g', 1}}};
	CHECK_THROWS_AS(transducer.add_transition('f', {0}, 'x', 0), std::invalid_argument);
	CHECK(!transducer.delta.contains(TransducerTransition{'f', {0}, 'x', 0}));
	CHECK(transducer.delta.input_alphabet().arity('f') == 2);
	CHECK(transducer.delta.output_alphabet().arity('g') == 1);
	CHECK_THROWS_AS(transducer.add_transition('h', {0, 0}, 'g', 0), std::invalid_argument);
	CHECK(transducer.delta.input_alphabet().arity('h') == std::nullopt);
	CHECK(transducer.delta.size() == 0);
	transducer.add_transition('f', {0, 0}, 'u', 0);
	transducer.final.insert(0);
	CHECK_NOTHROW(transducer.validate());
	CHECK(transducer.delta.input_alphabet().arity('f') == 2);
	CHECK(transducer.delta.output_alphabet().arity('u') == 2);
	const TreeTransducer inverse{transducer.invert()};
	CHECK(inverse.delta.input_alphabet().arity('u') == 2);
	CHECK(inverse.delta.output_alphabet().arity('f') == 2);
}

TEST_CASE("mata::ta::transducer_builder parses the shipped TT-explicit token fixture") {
	OnTheFlyAlphabet alphabet{};
	transducer_builder::NameStateMap states{};
	const TreeTransducer transducer{transducer_builder::parse_from_mata(token_transducer_source(), &alphabet, &states)};
	REQUIRE(states.size() == 3);
	CHECK(transducer.num_of_states() == 3);
	CHECK(transducer.final.contains(states.at("q0")));
	CHECK(!transducer.final.contains(states.at("q1")));
	CHECK(transducer.final.contains(states.at("q2")));
	CHECK(transducer.delta.size() == 8);
	CHECK(transducer.delta.input_alphabet().arity(alphabet["tt"]) == 2);
	CHECK(transducer.delta.output_alphabet().arity(alphabet["tt"]) == 2);
	CHECK(transducer.delta.input_alphabet().arity(alphabet["t"]) == 0);
	CHECK_NOTHROW(transducer.validate());

	std::istringstream stream{token_transducer_source()};
	CHECK_NOTHROW(transducer_builder::parse_from_mata(stream));
	CHECK_THROWS_AS(
		transducer_builder::parse_from_mata(std::string{"@TT-explicit\nf(q)->g:p\nf(p,p)->g:p\n"}),
		std::invalid_argument
	);
	CHECK_THROWS_AS(transducer_builder::parse_from_mata(std::string{"@TT-explicit\nf(q)->g p\n"}), std::runtime_error);
}
