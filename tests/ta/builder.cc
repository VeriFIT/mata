#include "mata/ta/builder.hh"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

using mata::IntAlphabet;
using mata::OnTheFlyAlphabet;
using namespace mata::ta;

TEST_CASE("mata::ta::builder parses @TA-explicit protocol hyperedges") {
	const std::string input{R"mata(@TA-explicit
%Alphabet-auto
%Final qok
# Nullary rules and hyperedges are data, not formulas.
a -> qa
b->qb
f(qa, qb,qa) -> qok
g( qok , qok ) -> qok
)mata"};
	OnTheFlyAlphabet alphabet{};
	builder::NameStateMap state_map{};
	const Ta aut{builder::parse_from_mata(input, &alphabet, &state_map)};

	const Symbol a{alphabet.translate_symb("a")};
	const Symbol b{alphabet.translate_symb("b")};
	const Symbol f{alphabet.translate_symb("f")};
	const Symbol g{alphabet.translate_symb("g")};
	const Tree accepted{f, {Tree{a}, Tree{b}, Tree{a}}};
	CHECK(aut.accepts(accepted));
	CHECK(aut.accepts(Tree{g, {accepted, accepted}}));
	CHECK_FALSE(aut.accepts(Tree{f, {Tree{b}, Tree{a}, Tree{a}}}));
	CHECK(state_map.at("qok") != state_map.at("qa"));
	CHECK(state_map.at("qa") != state_map.at("qb"));
	CHECK(aut.final.contains(state_map.at("qok")));
	CHECK(aut.is_deterministic());
}

TEST_CASE("mata::ta::builder maps numeric symbols and repeated numeric state names consistently") {
	const std::string input{R"mata(@TA-explicit
%Final 10
0 -> 2
1(2,2)->10
)mata"};
	IntAlphabet alphabet{};
	const Ta aut{builder::parse_from_mata(input, &alphabet)};
	CHECK(aut.accepts(Tree{1, {Tree{0}, Tree{0}}}));
	CHECK_FALSE(aut.accepts(Tree{0}));
	CHECK_FALSE(aut.accepts(Tree{1, {Tree{0}}}));
}

TEST_CASE("mata::ta::builder accepts streams and ParsedSection input") {
	const std::string input{"@TA-explicit\n%Final r\na -> r\n"};
	std::istringstream stream{input};
	OnTheFlyAlphabet alphabet{};
	const Ta from_stream{builder::parse_from_mata(stream, &alphabet)};
	const Ta from_section{builder::construct(mata::parser::parse_mf(input).front(), &alphabet)};
	const Tree tree{alphabet.translate_symb("a")};
	CHECK(from_stream.accepts(tree));
	CHECK(from_section.accepts(tree));
}

TEST_CASE("mata::ta::builder rejects inconsistent ranked symbols") {
	CHECK_THROWS_AS(
		builder::parse_from_mata(std::string{"@TA-explicit\n%Final q\nf(a)->q\nf(a,a)->q\na->a\n"}),
		std::invalid_argument
	);
	CHECK_THROWS_AS(
		builder::parse_from_mata(std::string{"@TA-explicit\n%Final q\nf->q\nf(a)->q\na->a\n"}), std::invalid_argument
	);
}

TEST_CASE("mata::ta::builder rejects malformed protocol inputs") {
	const std::vector<std::string> malformed{
		"",
		"@NFA-explicit\n%Final q\na -> q\n",
		"@TA-explicit\n%Initial q\na -> q\n",
		"@TA-explicit\n%Alphabet-auto a\na -> q\n",
		"@TA-explicit\n%Unknown q\na -> q\n",
		"@TA-explicit\n%Final q\na q\n",
		"@TA-explicit\n%Final q\na -> q r\n",
		"@TA-explicit\n%Final q\nf(q q) -> q\n",
		"@TA-explicit\n%Final q\nf(q,,q) -> q\n",
		"@TA-explicit\n%Final q\nf(q,) -> q\n",
		"@TA-explicit\n%Final q\nf() -> q\n",
		"@TA-explicit\n%Final q\na -> q -> q\n",
		"@TA-explicit\n%Final q\na -> q\n@TA-explicit\nb -> q\n",
	};
	for (const std::string& input : malformed) {
		INFO(input);
		CHECK_THROWS_AS(builder::parse_from_mata(input), std::runtime_error);
	}
}

TEST_CASE("mata::ta::builder keeps exact duplicate rules deterministic") {
	const Ta aut{
		builder::parse_from_mata(std::string{"@TA-explicit\n%Final q\na -> q\na -> q\nf(q,q)->q\nf(q,q)->q\n"})
	};
	CHECK(aut.is_deterministic());
	CHECK(aut.determinize().is_deterministic());
}

TEST_CASE("mata::ta::builder consumes token-passing ARTmc initial-tree automata") {
	const std::string input{R"mata(@TA-explicit
%Alphabet-auto
%Final marked
nil -> idle
token -> marked
node(idle,idle) -> idle
node(marked,idle) -> marked
node(idle,marked) -> marked
)mata"};
	OnTheFlyAlphabet alphabet{};
	const Ta aut{builder::parse_from_mata(input, &alphabet)};
	const Symbol nil{alphabet.translate_symb("nil")};
	const Symbol token{alphabet.translate_symb("token")};
	const Symbol node_symbol{alphabet.translate_symb("node")};
	const auto node{[&](Tree left, Tree right) { return Tree{node_symbol, {std::move(left), std::move(right)}}; }};

	CHECK(aut.accepts(node(Tree{token}, Tree{nil})));
	CHECK(aut.accepts(node(node(Tree{nil}, Tree{token}), node(Tree{nil}, Tree{nil}))));
	CHECK_FALSE(aut.accepts(node(Tree{nil}, node(Tree{nil}, Tree{nil}))));
	CHECK_FALSE(aut.accepts(node(Tree{token}, Tree{token})));
	Tree witness{nil};
	REQUIRE_FALSE(aut.is_lang_empty(&witness));
	CHECK(aut.accepts(witness));

	const Ta deterministic{aut.determinize()};
	CHECK(deterministic.accepts(node(Tree{token}, Tree{nil})));
	CHECK_FALSE(deterministic.accepts(node(Tree{token}, Tree{token})));
}
