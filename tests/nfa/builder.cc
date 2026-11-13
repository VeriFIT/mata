// TODO: some header

#include <cmath>
#include <fstream>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/parser/mintermization.hh"
#include "mata/parser/parser.hh"

using namespace mata::nfa;
using Symbol = mata::Symbol;
using IntAlphabet = mata::IntAlphabet;
using OnTheFlyAlphabet = mata::OnTheFlyAlphabet;

using Word = std::vector<Symbol>;

TEST_CASE("parse_from_mata()") {
	Delta delta;

	SECTION("Empty automaton - No initial and final") {
		Nfa nfa{delta, {}, {}};
		SECTION("from String") {
			std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n";
			Nfa empty_nfa{mata::nfa::builder::parse_from_mata(empty_nfa_str)};
			CHECK(are_equivalent(empty_nfa, nfa));
		}

		SECTION("from file") {
			std::filesystem::path nfa_file{"./temp-test-parse_from_mata-empty_nfa.mata"};
			std::fstream file{nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			file << "@NFA-explicit\n%Alphabet-auto\n";
			file.close();
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_file)};
			std::filesystem::remove(nfa_file);
			CHECK(are_equivalent(parsed, nfa));
		}
	}

	SECTION("Empty automaton with empty final and initial") {
		Nfa nfa{delta, {}, {}};
		SECTION("from String") {
			std::string empty_nfa_str = "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
			Nfa empty_nfa{mata::nfa::builder::parse_from_mata(empty_nfa_str)};
			CHECK(are_equivalent(empty_nfa, nfa));
		}
		SECTION("from file") {
			std::filesystem::path nfa_file{"./temp-test-parse_from_mata-empty_nfa-empty_initial_final.mata"};
			std::fstream file{nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			file << "@NFA-explicit\n%Alphabet-auto\n%Initial\n%Final\n";
			file.close();
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_file)};
			std::filesystem::remove(nfa_file);
			CHECK(are_equivalent(parsed, nfa));
		}
	}

	SECTION("Simple automaton") {
		delta.add(0, 0, 0);
		delta.add(0, 1, 1);
		delta.add(1, 2, 0);
		Nfa nfa{delta, {0}, {1}};

		SECTION("from string") {
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa.print_to_mata())};
			CHECK(are_equivalent(parsed, nfa));
		}

		SECTION("from stream") {
			std::stringstream nfa_stream;
			nfa.print_to_mata(nfa_stream);
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_stream)};
			CHECK(are_equivalent(parsed, nfa));
		}

		SECTION("from file") {
			std::filesystem::path nfa_file{"./temp-test-parse_from_mata-simple_nfa.mata"};
			std::fstream file{nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			nfa.print_to_mata(file);
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_file)};
			file.close();
			std::filesystem::remove(nfa_file);

			CHECK(are_equivalent(parsed, nfa));
		}
	}

	SECTION("automaton with epsilon transitions") {
		Nfa nfa{3, {0}, {2}};
		nfa.delta.add(0, 'a', 1);
		nfa.delta.add(1, EPSILON, 2);
		nfa.delta.add(2, EPSILON, 0);

		SECTION("from string") {
			const std::string printed{nfa.print_to_mata()};
			CHECK(printed.find(mata::EPSILON_NAME) != std::string::npos);
			Nfa parsed{mata::nfa::builder::parse_from_mata(printed)};
			CHECK(parsed.delta.contains(1, EPSILON, 2));
			CHECK(parsed.delta.contains(2, EPSILON, 0));
			CHECK(parsed.delta.contains(0, 'a', 1));
			CHECK(parsed.delta.num_of_transitions() == nfa.delta.num_of_transitions());
		}

		SECTION("an attached enumeration alphabet does not see the special name") {
			auto alphabet{std::make_shared<mata::EnumAlphabet>(mata::EnumAlphabet{'a'})};
			nfa.alphabet = alphabet;
			std::string printed;
			REQUIRE_NOTHROW(printed = nfa.print_to_mata());
			Nfa parsed{mata::nfa::builder::parse_from_mata(printed)};
			CHECK(parsed.delta.contains(1, EPSILON, 2));
			CHECK(alphabet->get_number_of_symbols() == 1);
		}

		SECTION("an attached on-the-fly alphabet cannot register the special name (#794)") {
			// "<eps>" is reserved: the alphabet translates it straight to EPSILON and refuses to
			//  register it as an ordinary symbol, so a printed name never changes its meaning.
			Nfa epsilon_nfa{3, {0}, {2}};
			epsilon_nfa.delta.add(0, EPSILON, 1);
			epsilon_nfa.delta.add(1, EPSILON, 2);
			auto alphabet{std::make_shared<mata::OnTheFlyAlphabet>()};
			epsilon_nfa.alphabet = alphabet;
			const std::string printed{epsilon_nfa.print_to_mata()};
			CHECK(alphabet->translate_symb(std::string{mata::EPSILON_NAME}) == EPSILON);
			CHECK_THROWS(alphabet->add_new_symbol(std::string{mata::EPSILON_NAME}));
			CHECK(alphabet->get_number_of_symbols() == 0);
			Nfa parsed{mata::nfa::builder::parse_from_mata(printed)};
			CHECK(parsed.delta.contains(0, EPSILON, 1));
			CHECK(parsed.delta.contains(1, EPSILON, 2));
			CHECK(parsed.delta.num_of_transitions() == epsilon_nfa.delta.num_of_transitions());
		}
	}

	SECTION("larger automaton") {
		Nfa nfa;
		nfa.initial = {1, 2, 50};
		nfa.delta.add(1, 'a', 2);
		nfa.delta.add(1, 'a', 3);
		nfa.delta.add(1, 'b', 4);
		nfa.delta.add(2, 'a', 2);
		nfa.delta.add(2, 'b', 2);
		nfa.delta.add(2, 'a', 3);
		nfa.delta.add(2, 'b', 4);
		nfa.delta.add(3, 'b', 4);
		nfa.delta.add(3, 'c', 7);
		nfa.delta.add(3, 'b', 2);
		nfa.delta.add(5, 'c', 3);
		nfa.delta.add(7, 'a', 8);
		nfa.delta.add(12, 'b', 15);
		nfa.delta.add(1, 'b', 40);
		nfa.delta.add(51, 'z', 42);
		nfa.final = {3, 103};

		SECTION("from string") {
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa.print_to_mata())};
			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(are_equivalent(parsed, nfa));
		}

		SECTION("from stream") {
			std::stringstream nfa_stream;
			nfa.print_to_mata(nfa_stream);
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_stream)};
			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(are_equivalent(parsed, nfa));
		}

		SECTION("from file") {
			std::filesystem::path nfa_file{"./temp-test-parse_from_mata-larger_nfa.mata"};
			std::fstream file{nfa_file, std::fstream::in | std::fstream::out | std::fstream::trunc};
			nfa.print_to_mata(file);
			Nfa parsed{mata::nfa::builder::parse_from_mata(nfa_file)};
			file.close();
			std::filesystem::remove(nfa_file);

			parsed.final.contains(103);
			parsed.initial.contains(50);
			parsed.delta.contains(51, 'z', 42);
			CHECK(are_equivalent(parsed, nfa));
		}
	}
}

namespace {
/// Reads @p input with the general reader, mata::parser::parse_mf() and IntermediateAut, as parse_from_mata() did.
Nfa read_with_general_reader(
	const std::string& input, mata::Alphabet* alphabet, builder::NameStateMap* state_map, const bool keep_quotes
) {
	const mata::parser::Parsed parsed{mata::parser::parse_mf(input, keep_quotes)};
	if (parsed.size() != 1 || !parsed[0].type.starts_with("NFA")) { throw std::runtime_error("not a single NFA"); }
	mata::IntermediateAut inter_aut{mata::IntermediateAut::parse_from_mf(parsed)[0]};
	if (inter_aut.is_bitvector()) { inter_aut = mata::Mintermization{}.mintermize(inter_aut); }
	return builder::construct(inter_aut, alphabet, state_map);
}

/// Checks that @p lhs and @p rhs are the same NFA up to the numbering of states, which the state names fix.
void check_same_up_to_state_names(
	const Nfa& lhs, const builder::NameStateMap& lhs_states, const Nfa& rhs, const builder::NameStateMap& rhs_states
) {
	REQUIRE(lhs.num_of_states() == rhs.num_of_states());
	REQUIRE(lhs_states.size() == rhs_states.size());
	std::vector<State> renaming(rhs.num_of_states());
	for (const auto& [name, state] : rhs_states) {
		REQUIRE(lhs_states.contains(name));
		renaming[state] = lhs_states.at(name);
	}
	Nfa renamed{lhs.num_of_states()};
	for (const Transition& transition : rhs.delta.transitions()) {
		renamed.delta.add(renaming[transition.source], transition.symbol, renaming[transition.target]);
	}
	for (const State state : rhs.initial) { renamed.initial.insert(renaming[state]); }
	for (const State state : rhs.final) { renamed.final.insert(renaming[state]); }
	CHECK(lhs.delta == renamed.delta);
	CHECK(
		std::set<State>(lhs.initial.begin(), lhs.initial.end()) ==
		std::set<State>(renamed.initial.begin(), renamed.initial.end())
	);
	CHECK(
		std::set<State>(lhs.final.begin(), lhs.final.end()) ==
		std::set<State>(renamed.final.begin(), renamed.final.end())
	);
}

/// Checks that parse_explicit_from_mata() reads @p input, and reads the NFA and the alphabet the general reader reads.
void check_explicit_reader_reads(const std::string& input, const bool keep_quotes = false) {
	CAPTURE(input, keep_quotes);
	OnTheFlyAlphabet explicit_alphabet;
	builder::NameStateMap explicit_states;
	const std::optional<Nfa> explicit_nfa{
		builder::parse_explicit_from_mata(input, &explicit_alphabet, &explicit_states, keep_quotes)
	};
	REQUIRE(explicit_nfa.has_value());
	OnTheFlyAlphabet general_alphabet;
	builder::NameStateMap general_states;
	const Nfa general_nfa{read_with_general_reader(input, &general_alphabet, &general_states, keep_quotes)};
	CHECK(explicit_alphabet.get_symbol_map() == general_alphabet.get_symbol_map());
	check_same_up_to_state_names(*explicit_nfa, explicit_states, general_nfa, general_states);
}

/// Checks that parse_explicit_from_mata() leaves @p input to the general reader, and that parse_from_mata() then reads
///  what the general reader reads, or fails as it does.
void check_general_reader_reads(const std::string& input, const bool keep_quotes = false) {
	CAPTURE(input, keep_quotes);
	OnTheFlyAlphabet explicit_alphabet;
	builder::NameStateMap explicit_states{{"kept", 0}};
	CHECK_FALSE(
		builder::parse_explicit_from_mata(input, &explicit_alphabet, &explicit_states, keep_quotes).has_value()
	);
	CHECK(explicit_states == builder::NameStateMap{{"kept", 0}});

	OnTheFlyAlphabet general_alphabet;
	builder::NameStateMap general_states;
	std::optional<Nfa> general_nfa;
	try {
		general_nfa = read_with_general_reader(input, &general_alphabet, &general_states, keep_quotes);
	} catch (const std::exception&) {}
	OnTheFlyAlphabet alphabet;
	builder::NameStateMap states;
	if (!general_nfa.has_value()) {
		CHECK_THROWS(builder::parse_from_mata(input, &alphabet, &states, keep_quotes));
		return;
	}
	const Nfa nfa{builder::parse_from_mata(input, &alphabet, &states, keep_quotes)};
	CHECK(alphabet.get_symbol_map() == general_alphabet.get_symbol_map());
	check_same_up_to_state_names(nfa, states, *general_nfa, general_states);
}
} // namespace

TEST_CASE("parse_explicit_from_mata() reads what the general reader reads") {
	SECTION("printed automata") {
		Nfa nfa{6, {0, 4}, {2, 5}};
		nfa.delta.add(0, 'a', 1);
		nfa.delta.add(1, 'b', 2);
		nfa.delta.add(1, EPSILON, 3);
		nfa.delta.add(3, 'a', 0);
		OnTheFlyAlphabet alphabet{{"a", 'a'}, {"b", 'b'}};
		check_explicit_reader_reads(nfa.print_to_mata(&alphabet));
		check_explicit_reader_reads(Nfa{}.print_to_mata());
		check_explicit_reader_reads(Nfa{3, {0}, {2}}.print_to_mata());
	}

	SECTION("layout") {
		check_explicit_reader_reads("@NFA\n%Alphabet-auto\n%Initial q0\nq0 a q1\n");
		check_explicit_reader_reads(
			"# comment\n\n  @NFA-explicit   # comment\n%Alphabet-auto\n\t%Initial q0 q1 # comment\n"
			"%Final q2\n%Final q3\n\nq0 a q1#comment\n   q1\tb  q2\r\n# comment\nq2 c q0 \n"
		);
		// Continued lines, including across lines without tokens.
		check_explicit_reader_reads(
			"@NFA-explicit\n%Alphabet-auto\n%Initial q0 \\\n q1\nq0 a \\\n# comment \\\n\n q1\nq1 \\\n\\\n b q0\n"
		);
		// The namings apply to the whole section; other keys are ignored.
		check_explicit_reader_reads("@NFA-explicit\n%Initial q0\n%Final q1\n%Foo x y\n%Alphabet-auto\nq0 a q1\n");
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a q1\n%Final q1\n");
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q\nq a q\n");
	}

	SECTION("namings") {
		check_explicit_reader_reads(
			"@NFA-explicit\n%Alphabet-auto\n%States-auto\n%Nodes-auto\n%Initial s0\n%Final qs1\ns0 a qs1\n"
		);
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-marked\n%States-marked\n%Initial q0\nq0 b q1\n");
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet\n%Initial q0\nq0 a q1\n");
	}

	SECTION("quoted names") {
		const std::string input{"@NFA-explicit\n%Alphabet-auto\n%Initial \"q0\"\nq0 \"a b\" q1\nq1 \"x\\\"y\" q0\nq0 "
								"\"\\\\\" q1\nq1 \"\" \"q2\"\n"
								"\"q2\" \"#\" q0 # comment\n"};
		check_explicit_reader_reads(input);
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 \"a b\" q1\nq1 \"x\\\"y\" q0\n", true);
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 \"&\" q1\nq1 \"(\" q0\n");
	}

	SECTION("special symbols") {
		check_explicit_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 <eps> q1\nq1 a q0\n");
	}

	SECTION("caller's state names") {
		const std::string input{"@NFA-explicit\n%Alphabet-auto\n%Initial q1\n%Final q5\nq1 a q5\nq5 b q7\n"};
		builder::NameStateMap explicit_states{{"5", 3}, {"9", 0}};
		builder::NameStateMap general_states{explicit_states};
		OnTheFlyAlphabet explicit_alphabet;
		OnTheFlyAlphabet general_alphabet;
		const std::optional<Nfa> explicit_nfa{
			builder::parse_explicit_from_mata(input, &explicit_alphabet, &explicit_states)
		};
		REQUIRE(explicit_nfa.has_value());
		const Nfa general_nfa{read_with_general_reader(input, &general_alphabet, &general_states, false)};
		CHECK(explicit_states.at("5") == 3);
		check_same_up_to_state_names(*explicit_nfa, explicit_states, general_nfa, general_states);
	}
}

TEST_CASE("parse_explicit_from_mata() leaves the general format to the general reader") {
	SECTION("formulas") {
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0 | q1\n%Final q1\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final !q0 & !q1\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final \\true\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final (q1)\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a & q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final \"!q1\"\nq0 a q1\n");
	}

	SECTION("bitvector alphabets") {
		check_general_reader_reads(
			"@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 x & !y q1\nq1 (x | y) & !z q0\nq0 z q0\n"
		);
	}

	SECTION("names that are not states") {
		// IntermediateAut reads these as nodes and symbols, and builder::construct() then takes them for states.
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 a n1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 a x1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial x0\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n\"q0\" a \"q1\"\n", true);
	}

	SECTION("namings") {
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%States-enum p r\n%Initial p\np a r\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%States-auto\n%Initial p\np a r\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Alphabet-marked\n%Initial q0\nq0 a1 q1\n");
		check_general_reader_reads("@NFA-explicit\n%Initial q0\nq0 a q1\n%Alphabet-auto\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial-x q0\nq0 a q1\n");
	}

	SECTION("malformed inputs") {
		check_general_reader_reads("");
		check_general_reader_reads("%Initial q0\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a q1\n@NFA-explicit\n%Alphabet-auto\nq0 a q1\n");
		check_general_reader_reads("@NFA-foo\n%Alphabet-auto\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit trailing\n%Alphabet-auto\nq0 a q1\n");
		check_general_reader_reads("@NFT-explicit\n%Alphabet-auto\nq0 a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a q1 q2\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 \"a q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 \"a\"b q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a%b q1\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\nq0 a %b\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n% q0\n");
		check_general_reader_reads("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 a q1\nq1 b \\");
	}
}

namespace {
/// Reads @p inputs with the general reader and mintermizes them together, as the loaders did before the bitvector
///  reader.
std::vector<Nfa> read_bitvector_with_general_reader(
	const std::vector<std::string>& inputs,
	mata::Alphabet* alphabet,
	std::vector<builder::NameStateMap>& state_maps,
	const bool keep_quotes
) {
	std::vector<mata::IntermediateAut> inter_auts;
	for (const std::string& input : inputs) {
		inter_auts.push_back(mata::IntermediateAut::parse_from_mf(mata::parser::parse_mf(input, keep_quotes))[0]);
	}
	std::vector<mata::IntermediateAut> mintermized{mata::Mintermization{}.mintermize(inter_auts)};
	std::vector<Nfa> nfas;
	state_maps.resize(inputs.size());
	for (size_t i{0}; i < inputs.size(); ++i) {
		nfas.push_back(builder::construct(mintermized[i], alphabet, &state_maps[i]));
	}
	return nfas;
}

/// Checks that @p lhs and @p rhs are the same NFAs up to the numbering of states and of the minterms.
void check_same_up_to_names(
	const std::vector<Nfa>& lhs,
	const std::vector<builder::NameStateMap>& lhs_states,
	const std::vector<Nfa>& rhs,
	const std::vector<builder::NameStateMap>& rhs_states
) {
	REQUIRE(lhs.size() == rhs.size());
	// Each minterm is identified by the transitions over it, in all the automata together.
	using Edges = std::set<std::tuple<size_t, std::string, std::string>>;
	const auto edges_of_symbols{[](const std::vector<Nfa>& nfas, const std::vector<builder::NameStateMap>& states) {
		std::map<Symbol, Edges> edges;
		for (size_t i{0}; i < nfas.size(); ++i) {
			std::map<State, std::string> names;
			for (const auto& [name, state] : states[i]) { names[state] = name; }
			for (const Transition& transition : nfas[i].delta.transitions()) {
				edges[transition.symbol].emplace(i, names.at(transition.source), names.at(transition.target));
			}
		}
		std::multiset<Edges> result;
		for (Edges& symbol_edges : edges | std::views::values) { result.insert(std::move(symbol_edges)); }
		return result;
	}};
	CHECK(edges_of_symbols(lhs, lhs_states) == edges_of_symbols(rhs, rhs_states));
	for (size_t i{0}; i < lhs.size(); ++i) {
		CAPTURE(i);
		REQUIRE(lhs[i].num_of_states() == rhs[i].num_of_states());
		CHECK(lhs_states[i].size() == rhs_states[i].size());
		const auto names_of{[](const auto& states, const builder::NameStateMap& map) {
			std::map<State, std::string> names;
			for (const auto& [name, state] : map) { names[state] = name; }
			std::set<std::string> result;
			for (const State state : states) { result.insert(names.at(state)); }
			return result;
		}};
		CHECK(names_of(lhs[i].initial, lhs_states[i]) == names_of(rhs[i].initial, rhs_states[i]));
		CHECK(names_of(lhs[i].final, lhs_states[i]) == names_of(rhs[i].final, rhs_states[i]));
	}
}

using Read = builder::BitvectorNfaReader::Read;

/// Checks that the bitvector reader reads @p inputs together, each the way @p how says, as the general reader and
///  @c Mintermization read them.
void check_bitvector_reader_reads(
	const std::vector<std::string>& inputs, const Read how = Read::Directly, const bool keep_quotes = false
) {
	CAPTURE(inputs, keep_quotes);
	builder::BitvectorNfaReader reader;
	std::vector<builder::NameStateMap> reader_states(inputs.size());
	for (size_t i{0}; i < inputs.size(); ++i) { CHECK(reader.add(inputs[i], keep_quotes, &reader_states[i]) == how); }
	OnTheFlyAlphabet reader_alphabet;
	const std::vector<Nfa> read{reader.build(&reader_alphabet)};

	OnTheFlyAlphabet general_alphabet;
	std::vector<builder::NameStateMap> general_states;
	const std::vector<Nfa> general{
		read_bitvector_with_general_reader(inputs, &general_alphabet, general_states, keep_quotes)
	};
	CHECK(reader_alphabet.get_symbol_map().size() == general_alphabet.get_symbol_map().size());
	check_same_up_to_names(read, reader_states, general, general_states);

	// A single input read with parse_from_mata() is the same automaton.
	if (inputs.size() == 1) {
		OnTheFlyAlphabet alphabet;
		std::vector<builder::NameStateMap> states(1);
		const std::vector<Nfa> parsed{builder::parse_from_mata(inputs[0], &alphabet, states.data(), keep_quotes)};
		check_same_up_to_names(parsed, states, general, general_states);
	}
}
} // namespace

TEST_CASE("BitvectorNfaReader reads what the general reader and mintermization read") {
	const std::string header{"@NFA-bits\n%Alphabet-auto\n%Initial q0\n"};
	SECTION("formulas") {
		check_bitvector_reader_reads({header + "%Final q1\nq0 x & !y q1\nq1 (x | y) & !z q0\nq0 z q0\n"});
		check_bitvector_reader_reads({header + "%Final q1\nq0 x&!y q1\nq1 !(x|y)&z q0\nq0 !!x q1\nq1 a|b&c q1\n"});
		check_bitvector_reader_reads({header + "%Final q1\nq0 (((x))) q1\nq1 x | y | z q0\nq0 x & y & z q1\n"});
		check_bitvector_reader_reads({header + "%Final q1\nq0 \\true q1\nq1 \\false q0\nq0 x & \\true q1\n"});
		// An unsatisfiable predicate drops the transition, and leaves the state of its target unnamed.
		check_bitvector_reader_reads({header + "%Final q1\nq0 x & !x q2\nq0 x q1\n"});
		// Even when that target is a constant, which only a dropped transition may have.
		check_bitvector_reader_reads({header + "%Final !q1\nq3 \\false \\true\nq0 x q1\n"});
	}

	SECTION("final formulas") {
		check_bitvector_reader_reads({header + "%Final !q0 & !q2\nq0 x q1\nq1 y q2\nq2 z q0\n"});
		check_bitvector_reader_reads({header + "%Final !q5\nq0 x q1\n"});
		check_bitvector_reader_reads({header + "%Final \\true\nq0 x q1\n"});
		check_bitvector_reader_reads({header + "%Final \\false\nq0 x q1\n"});
		check_bitvector_reader_reads({header + "%Final q1 q2\nq0 x q1\n"});
		check_bitvector_reader_reads({header + "q0 x q1\n"});
		check_bitvector_reader_reads({"@NFA-bits\n%Alphabet-auto\nq0 x q1\n"});
		// Long enough that the kept token texts outgrow any initial storage.
		std::string all_but{"%Final !q0"};
		std::string transitions{};
		for (size_t state{1}; state < 2'000; ++state) {
			all_but += " & !q" + std::to_string(state);
			transitions += "q" + std::to_string(state - 1) + " x q" + std::to_string(state) + "\n";
		}
		check_bitvector_reader_reads({header + all_but + "\nq1999 y q2000\n" + transitions});
	}

	SECTION("formulas the general reader reads for the bitvector reader") {
		check_bitvector_reader_reads({header + "%Final q1 | q0\nq0 x q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads({header + "%Final !q1 | !q0\nq0 x q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads({header + "%Final q1\nq0 x & q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads({header + "%Final q1\nq0 \"x y\" q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads({"@NFA-bits\n%Alphabet-auto\n%Initial q0 | q1\nq0 x q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads(
			{"@NFA-bits\n%Alphabet-auto\n%States-enum p r\n%Initial p\np x r\n"}, Read::ByGeneralReader
		);
	}

	SECTION("namings") {
		check_bitvector_reader_reads({"@NFA-bits\n%Alphabet-marked\n%Initial q0\n%Final q1\nq0 a1 & !a2 q1\n"});
		check_bitvector_reader_reads(
			{"@NFA-bits\n%Alphabet-auto\n%Nodes-auto\n%States-auto\n%Initial p\n%Final r\np x & !y r\n"}
		);
	}

	SECTION("automata sharing minterms") {
		check_bitvector_reader_reads({
			header + "%Final q1\nq0 x & !y q1\nq1 y q0\n",
			header + "%Final !q0\nq0 x | z q1\nq1 \\true q0\n",
			header + "%Final q1 q0\nq0 x & y q1\n",
		});
		// One of them read by the general reader still shares the minterms.
		builder::BitvectorNfaReader reader;
		std::vector<builder::NameStateMap> reader_states(2);
		const std::vector<std::string> inputs{
			header + "%Final q1\nq0 x & !y q1\nq1 y q0\n", header + "%Final q1 | q0\nq0 x & y q1\nq1 y & q0\n"
		};
		CHECK(reader.add(inputs[0], false, &reader_states[0]) == Read::Directly);
		CHECK(reader.add(inputs[1], false, &reader_states[1]) == Read::ByGeneralReader);
		OnTheFlyAlphabet reader_alphabet;
		const std::vector<Nfa> read{reader.build(&reader_alphabet)};
		OnTheFlyAlphabet general_alphabet;
		std::vector<builder::NameStateMap> general_states;
		const std::vector<Nfa> general{
			read_bitvector_with_general_reader(inputs, &general_alphabet, general_states, false)
		};
		check_same_up_to_names(read, reader_states, general, general_states);
	}

	SECTION("quoted names") {
		// IntermediateAut takes a quoted state of a body line for a node or a symbol.
		check_bitvector_reader_reads({header + "%Final q1\n\"q0\" x \"q1\"\n"}, Read::ByGeneralReader, true);
		check_bitvector_reader_reads({header + "%Final q1\nq0 \"x\" & y q1\n"}, Read::ByGeneralReader);
		check_bitvector_reader_reads({header + "%Final q1\nq0 (\"x\") q1\n"}, Read::ByGeneralReader);
	}
}

TEST_CASE("BitvectorNfaReader leaves inputs that are not bitvector NFAs") {
	builder::BitvectorNfaReader reader;
	CHECK(reader.add("@NFA-explicit\n%Alphabet-auto\n%Initial q0\nq0 a q1\n") == Read::Rejected);
	CHECK(reader.add("@NFA-bits\n%Alphabet-auto\nq0 x q1\n@NFA-bits\n%Alphabet-auto\nq0 x q1\n") == Read::Rejected);
	CHECK_THROWS(reader.add("@NFA-bits\n%Alphabet-auto\n%Initial q0\nq0 x & q1\n& y\n"));
	// The general reader rejects these, so the bitvector reader cannot read them for it.
	CHECK_THROWS(reader.add("@NFA-bits\n%Alphabet-auto\n%States-auto\n%Initial p\np x r\n"));
	CHECK_THROWS(reader.add("@NFA-bits\n%Alphabet-auto\n%Initial q0\nq0 x&\"y\" q1\n"));
	CHECK(reader.build().empty());
}

TEST_CASE("Create Tabakov-Vardi NFA") {
	size_t num_of_states;
	size_t alphabet_size;
	double states_trans_ratio_per_symbol;
	double final_state_density;

	SECTION("EMPTY") {
		num_of_states = 0;
		alphabet_size = 0;
		states_trans_ratio_per_symbol = 0;
		final_state_density = 0;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == 0);
		CHECK(nfa.initial.size() == 0);
		CHECK(nfa.final.size() == 0);
		CHECK(nfa.delta.empty());
	}

	SECTION("10-5-0.5-0.5") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = 0.5;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == 5);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 25);
	}

	SECTION("Min final") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = 0.0001;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == 1);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 25);
	}

	SECTION("Max final") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = 1;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == num_of_states);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 25);
	}

	SECTION("Min transitions") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0;
		final_state_density = 0.5;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == 5);
		CHECK(nfa.delta.get_used_symbols().size() == 0);
		CHECK(nfa.delta.num_of_transitions() == 0);
	}

	SECTION("Max transitions") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 10;
		final_state_density = 0.5;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == 5);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 500);
	}

	SECTION("BIG") {
		num_of_states = 200;
		alphabet_size = 100;
		states_trans_ratio_per_symbol = 5;
		final_state_density = 1;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.initial.size() == 1);
		CHECK(nfa.final.size() == num_of_states);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 100'000);
	}

	SECTION("Sparse transitions in a large matrix") {
		// 4'000 transitions per symbol out of 4'000'000 possible ones: every one must be distinct.
		num_of_states = 2'000;
		alphabet_size = 3;
		states_trans_ratio_per_symbol = 2;
		final_state_density = 0.5;

		Nfa nfa = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, 750
		);
		CHECK(nfa.num_of_states() == num_of_states);
		CHECK(nfa.final.size() == 1'000);
		CHECK(nfa.delta.get_used_symbols().size() == alphabet_size);
		CHECK(nfa.delta.num_of_transitions() == 12'000);
	}

	SECTION("Throw runtime_error. transition_density < 0") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = static_cast<double>(-0.1);
		final_state_density = 0.5;

		CHECK_THROWS_AS(
			mata::nfa::builder::create_random_nfa_tabakov_vardi(
				num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
			),
			std::runtime_error
		);
	}

	SECTION("Throw runtime_error. transition_density > num_of_states") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 11;
		final_state_density = 0.5;

		CHECK_THROWS_AS(
			mata::nfa::builder::create_random_nfa_tabakov_vardi(
				num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
			),
			std::runtime_error
		);
	}

	SECTION("Throw runtime_error. final_state_density < 0") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = static_cast<double>(-0.1);

		CHECK_THROWS_AS(
			mata::nfa::builder::create_random_nfa_tabakov_vardi(
				num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
			),
			std::runtime_error
		);
	}

	SECTION("Throw runtime_error. final_state_density > 1") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = static_cast<double>(1.1);

		CHECK_THROWS_AS(
			mata::nfa::builder::create_random_nfa_tabakov_vardi(
				num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density
			),
			std::runtime_error
		);
	}

	SECTION("Same seed results in same NFA.") {
		num_of_states = 10;
		alphabet_size = 5;
		states_trans_ratio_per_symbol = 0.5;
		final_state_density = 0.5;
		std::optional<unsigned int> seed1{3'171'643'142};
		std::optional<unsigned int> seed2{4'283'451'011};

		Nfa nfa1_1 = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed1
		);
		Nfa nfa1_2 = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed1
		);
		Nfa nfa2 = mata::nfa::builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio_per_symbol, final_state_density, seed2
		);
		CHECK(nfa1_1.is_identical(nfa1_2));
		CHECK(!nfa1_2.is_identical(nfa2));
	}
	SECTION("random NFA attaches correct alphabet") {
		const size_t num_of_states = 10;
		const size_t alphabet_size = 4;
		const double states_trans_ratio = 0.5;
		const double final_state_density = 0.5;

		Nfa nfa = builder::create_random_nfa_tabakov_vardi(
			num_of_states, alphabet_size, states_trans_ratio, final_state_density, 42
		);

		// The alphabet holds exactly the symbols 0 .. alphabet_size - 1 that the generator uses.
		REQUIRE(nfa.alphabet != nullptr);
		const auto alphabet_symbols = nfa.alphabet->get_alphabet_symbols();
		CHECK(alphabet_symbols.size() == alphabet_size);
		for (size_t i = 0; i < alphabet_size; ++i) { CHECK(alphabet_symbols.contains(static_cast<Symbol>(i))); }
		CHECK(nfa.get_symbols_to_work_with() == alphabet_symbols);

		std::string mata_str;
		REQUIRE_NOTHROW(mata_str = nfa.print_to_mata());
		CHECK(!mata_str.empty());

		bool is_complete_result{true};
		REQUIRE_NOTHROW(is_complete_result = nfa.is_complete());
		CHECK(!is_complete_result); // A sparse random NFA misses most of the alphabet in most states.

		// make_complete() adds the missing transitions to a new sink state.
		Nfa nfa_copy{nfa};
		bool added{false};
		REQUIRE_NOTHROW(added = nfa_copy.make_complete());
		CHECK(added);
		CHECK(nfa_copy.num_of_states() == num_of_states + 1);
		CHECK(nfa_copy.is_complete());
		const State sink{num_of_states};
		for (const Symbol symbol : alphabet_symbols) { CHECK(nfa_copy.delta.contains(sink, symbol, sink)); }
	}
}
