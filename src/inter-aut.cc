/*
 * inter-aut.hh -- intermediate representation of automata.
 *
 * It represents automaton after parsing and before translation to particular automaton.
 */

#include "mata/parser/inter-aut.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

#include <format>
#include <span>

using namespace std::string_view_literals;

namespace {

bool has_at_most_one_auto_naming(const mata::IntermediateAut& aut) {
	return (aut.node_naming == mata::IntermediateAut::Naming::Auto &&
			aut.symbol_naming == mata::IntermediateAut::Naming::Auto) ||
		   (aut.state_naming != mata::IntermediateAut::Naming::Auto);
}

/// Determines whether the character is one of the logical operators of a transition formula.
bool is_logical_operator(const char ch) { return ch == '&' || ch == '|' || ch == '!'; }

/// Throws unless @p tokens is an explicit `state symbol state` transition of an @p automaton_type ("NFA" or "NFT").
/// The source state and the symbol bypass the formula parser, which would otherwise reject an operator or a
/// parenthesis (as it does for the target state), so they are checked here.
void check_explicit_transition(const mata::parser::BodyLine& tokens, const std::string& automaton_type) {
	if (tokens.size() != 3) {
		throw std::runtime_error(
			"Malformed explicit " + automaton_type + " transition " + std::to_string(tokens) +
			": expected exactly 3 tokens (state, symbol, state), got " + std::to_string(tokens.size())
		);
	}
	for (size_t i = 0; i < 2; ++i) { // The source state and the symbol.
		// A quoted token names a symbol, even when the name is that of an operator: the reader quoted it exactly
		//  so that it is not read for an operator.
		if (tokens[i].quoted) { continue; }
		if (const std::string& token = tokens[i].token;
			token == "&" || token == "|" || token == "!" || token == "(" || token == ")") {
			throw std::runtime_error(
				std::format(
					"Malformed explicit {} transition {}: '{}' is an operator, not a state or a symbol", automaton_type,
					std::to_string(tokens), token
				)
			);
		}
	}
}

mata::IntermediateAut::Naming get_naming_type(const std::string& key) {
	const size_t found = key.find('-');
	if (found == std::string::npos) {
		// Bare @NFA or %Alphabet without suffix; accept for compatibility
		return mata::IntermediateAut::Naming::Auto;
	}

	const std::string& type = key.substr(found + 1, std::string::npos);
	if (type == "auto") { return mata::IntermediateAut::Naming::Auto; }
	if (type == "enum") { return mata::IntermediateAut::Naming::Enum; }
	if (type == "marked") { return mata::IntermediateAut::Naming::Marked; }
	if (type == "chars") { return mata::IntermediateAut::Naming::Chars; }
	if (type == "utf") { return mata::IntermediateAut::Naming::Utf; }

	throw std::runtime_error("Unknown naming type '" + type + "'; expected one of: auto, enum, marked, chars, utf");
	return {};
}

mata::IntermediateAut::AlphabetType get_alphabet_type(const std::string& type) {
	const size_t found = type.find('-');
	if (found == std::string::npos) {
		// Bare %Alphabet without suffix; accept for compatibility
		return mata::IntermediateAut::AlphabetType::Explicit;
	}

	const std::string alph_type = type.substr(found + 1, std::string::npos);
	if (alph_type == "bits") { return mata::IntermediateAut::AlphabetType::Bitvector; }
	if (alph_type == "explicit") { return mata::IntermediateAut::AlphabetType::Explicit; }
	if (alph_type == "intervals") { return mata::IntermediateAut::AlphabetType::Intervals; }

	throw std::runtime_error("Unknown alphabet type '" + alph_type + "'; expected one of: bits, explicit, intervals");
	return {};
}

bool is_naming_marker(const mata::IntermediateAut::Naming naming) {
	return naming == mata::IntermediateAut::Naming::Marked;
}

bool is_naming_enum(const mata::IntermediateAut::Naming naming) {
	return naming == mata::IntermediateAut::Naming::Enum;
}

bool is_naming_auto(const mata::IntermediateAut::Naming naming) {
	return naming == mata::IntermediateAut::Naming::Auto;
}

bool no_operators(const std::vector<mata::FormulaNode>& nodes) {
	return std::ranges::all_of(nodes, [](const mata::FormulaNode& node) { return !node.is_operator(); });
}

std::string serialize_graph(const mata::FormulaGraph& graph) {
	if (graph.node.is_operand()) { return graph.node.raw; }

	if (graph.children.size() == 1) { // unary operator
		// Recurse on the child graph itself, keeping its children; wrapping only the node would drop them and the
		//  binary branch below would then read children that do not exist.
		const auto& child = graph.children.front();
		const std::string child_name = child.node.is_operand() ? child.node.raw : "(" + serialize_graph(child) + ")";
		return graph.node.raw + child_name;
	}

	if (!graph.node.is_operator() || graph.children.size() != 2) {
		throw std::runtime_error(
			"Cannot serialize a formula graph with a non-binary operator node and " +
			std::to_string(graph.children.size()) + " children."
		);
	}
	const auto& left_child = graph.children.front();
	std::string lhs = (left_child.node.is_operand()) ? left_child.node.raw : serialize_graph(left_child);
	if (left_child.children.size() == 2) { lhs = "(" + lhs + ")"; }
	const auto& right_child = graph.children[1];
	std::string rhs = (right_child.node.is_operand()) ? right_child.node.raw : serialize_graph(right_child);
	if (right_child.children.size() == 2) { rhs = "(" + rhs + ")"; }

	return lhs + " " + graph.node.raw + " " + rhs;
}

mata::FormulaNode create_node(const mata::IntermediateAut& mata, const std::string& token, const bool quoted = false) {
	// A quoted token stands for a name, even when the name is that of an operator: the reader quoted it exactly
	//  so that it is not read for an operator of the transition formula.
	if (!quoted) {
		if (is_logical_operator(token[0])) {
			switch (token[0]) {
				case '&':
					return {mata::FormulaNode::Type::Operator, token, token, mata::FormulaNode::OperatorType::And};
				case '|':
					return {mata::FormulaNode::Type::Operator, token, token, mata::FormulaNode::OperatorType::Or};
				case '!':
					return {mata::FormulaNode::Type::Operator, token, token, mata::FormulaNode::OperatorType::Neg};
				default:
					MATA_ASSERT(false);
			}
		} else if (token == "("sv) {
			return {mata::FormulaNode::Type::LeftParenthesis, token};
		} else if (token == ")"sv) {
			return {mata::FormulaNode::Type::RightParenthesis, token};
		}
	}

	if (token == "\\true"sv) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::True};
	} else if (token == "\\false"sv) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::False};
	} else if (is_naming_enum(mata.state_naming) && mata.states_names.contains(token)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::State};
	} else if (is_naming_enum(mata.node_naming) && mata.nodes_names.contains(token)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::Node};
	} else if (is_naming_enum(mata.symbol_naming) && mata.symbols_names.contains(token)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::Symbol};
	} else if (is_naming_marker(mata.state_naming) && token[0] == 'q') {
		return {mata::FormulaNode::Type::Operand, token, token.substr(1), mata::FormulaNode::OperandType::State};
	} else if (is_naming_marker(mata.node_naming) && token[0] == 'n') {
		return {mata::FormulaNode::Type::Operand, token, token.substr(1), mata::FormulaNode::OperandType::Node};
	} else if (is_naming_marker(mata.symbol_naming) && token[0] == 'a') {
		return {mata::FormulaNode::Type::Operand, token, token.substr(1), mata::FormulaNode::OperandType::Symbol};
	} else if (is_naming_auto(mata.state_naming)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::State};
	} else if (is_naming_auto(mata.node_naming)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::Node};
	} else if (is_naming_auto(mata.symbol_naming)) {
		return {mata::FormulaNode::Type::Operand, token, token, mata::FormulaNode::OperandType::Symbol};
	}

	throw std::runtime_error("Unknown token " + token);
}

/**
 * Checks if @p op1 has lower precedence than @p op2. Precedence from the lowest to highest is: | < & < !
 */
bool lower_precedence(const mata::FormulaNode::OperatorType op1, const mata::FormulaNode::OperatorType op2) {
	return op1 != mata::FormulaNode::OperatorType::Neg &&
		   (op1 != mata::FormulaNode::OperatorType::And || op2 == mata::FormulaNode::OperatorType::Neg);
}

/**
 * Transforms a series of tokes to postfix notation.
 * @param aut Automaton for which transition formula is parsed
 * @param tokens Series of tokens representing transition formula parsed from the input text
 * @return A postfix notation for input
 */
std::vector<mata::FormulaNode>
	infix_to_postfix(const mata::IntermediateAut& aut, const std::span<const mata::parser::BodyToken> tokens) {
	std::vector<mata::FormulaNode> opstack;
	std::vector<mata::FormulaNode> output;
	output.reserve(tokens.size());
	opstack.reserve(32);

	for (const mata::parser::BodyToken& token : tokens) {
		switch (mata::FormulaNode node = create_node(aut, token.token, token.quoted); node.type) {
			case mata::FormulaNode::Type::Operand:
				output.emplace_back(std::move(node));
				break;
			case mata::FormulaNode::Type::LeftParenthesis:
				opstack.emplace_back(std::move(node));
				break;
			case mata::FormulaNode::Type::RightParenthesis:
				while (!opstack.empty() && !opstack.back().is_left_parenthesis()) {
					output.emplace_back(std::move(opstack.back()));
					opstack.pop_back();
				}
				if (opstack.empty()) { throw std::runtime_error("Unbalanced parentheses: ')' without a matching '('"); }
				opstack.pop_back();
				break;
			case mata::FormulaNode::Type::Operator:
				// The operators an operator pops are a run on top of the stack ('|' pops all down to '(', '&' pops the
				//  '!'s, which are always on top), so the scan stops at the first one kept: a long chain stays linear.
				while (!opstack.empty() && !opstack.back().is_left_parenthesis() &&
					   lower_precedence(node.operator_type, opstack.back().operator_type)) {
					MATA_ASSERT(!opstack.back().is_operand());
					output.emplace_back(std::move(opstack.back()));
					opstack.pop_back();
				}
				opstack.emplace_back(std::move(node));
				break;
			default:
				MATA_ASSERT(false);
		}
	}

	while (!opstack.empty()) {
		if (opstack.back().is_left_parenthesis()) {
			throw std::runtime_error("Unbalanced parentheses: '(' without a matching ')'");
		}
		output.push_back(std::move(opstack.back()));
		opstack.pop_back();
	}

	return output;
}

/**
 * Create a graph for a postfix representation of transition formula
 * @param postfix A postfix representation of transition formula
 * @return A parsed graph
 */
mata::FormulaGraph postfix_to_graph(std::vector<mata::FormulaNode> postfix) {
	std::vector<mata::FormulaGraph> opstack{};
	opstack.reserve(4);
	for (mata::FormulaNode& node : postfix) {
		switch (node.type) {
			case mata::FormulaNode::Type::Operand:
				opstack.emplace_back(std::move(node));
				break;
			case mata::FormulaNode::Type::Operator: {
				switch (node.operator_type) {
					case mata::FormulaNode::OperatorType::Neg: { // 1 child: graph will be a NEG node.
						if (opstack.empty()) { throw std::runtime_error("Malformed formula: '!' without an operand"); }
						mata::FormulaGraph child{std::move(opstack.back())};
						opstack.pop_back();
						mata::FormulaGraph& gr{opstack.emplace_back(std::move(node))};
						gr.children.emplace_back(std::move(child));
						break;
					}
					default: { // 2 children: Graph will be either an AND node, or an OR node.
						if (opstack.size() < 2) {
							throw std::runtime_error("Malformed formula: '" + node.raw + "' needs two operands");
						}
						mata::FormulaGraph second_child{std::move(opstack.back())};
						opstack.pop_back();
						mata::FormulaGraph first_child{std::move(opstack.back())};
						opstack.pop_back();
						mata::FormulaGraph& gr{opstack.emplace_back(std::move(node))};
						gr.children.emplace_back(std::move(first_child));
						gr.children.emplace_back(std::move(second_child));
						break;
					}
				}
				break;
			}
			default:
				throw std::runtime_error("Malformed formula: unexpected '" + node.raw + "'");
		}
	}

	// In case of no transition
	if (opstack.empty()) { return {}; }
	if (opstack.size() > 1) {
		throw std::runtime_error("Malformed formula: operands without an operator between them");
	}

	MATA_ASSERT(opstack.size() == 1);
	return std::move(*opstack.begin());
}

/**
 * Function adds disjunction operators to a postfix form when there are no operators at all.
 * This is currently case of initial and final states of NFA where a user usually doesn't want to write
 * initial or final states as a formula
 * @param postfix Postfix to which operators are eventually added
 * @return A postfix with eventually added operators
 */
std::vector<mata::FormulaNode> add_disjunction_implicitly(std::vector<mata::FormulaNode> postfix) {
	const size_t postfix_size{postfix.size()};
	if (postfix_size == 1) { // no need to add operators
		return postfix;
	}

	for (const auto& op : postfix) {
		if (op.is_operator()) { // operators provided by user, return the original postfix
			return postfix;
		}
	}

	std::vector<mata::FormulaNode> res;
	if (postfix_size >= 2) {
		res.push_back(std::move(postfix[0]));
		res.push_back(std::move(postfix[1]));
		res.emplace_back(mata::FormulaNode::Type::Operator, "|", "|", mata::FormulaNode::OperatorType::Or);
	}

	for (size_t i{2}; i < postfix_size; ++i) {
		res.push_back(std::move(postfix[i]));
		res.emplace_back(mata::FormulaNode::Type::Operator, "|", "|", mata::FormulaNode::OperatorType::Or);
	}

	return res;
}

/**
 * The wrapping function for parsing one section of input to IntermediateAut.
 * It parses basic information about type of automaton and naming of its component.
 * Then it parses initial and final formula, and finally it creates graphs for transition formula.
 * @param section A section of input MATA format
 * @return Parsed InterAutomata representing an automaton from input.
 */
mata::IntermediateAut mf_to_aut(const mata::parser::ParsedSection& section) {
	mata::IntermediateAut aut;

	if (section.type.find("NFA") != std::string::npos) {
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Nfa;
	} else if (section.type.find("AFA") != std::string::npos) {
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Afa;
	} else if (section.type.find("NFT") != std::string::npos) {
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Nft;
	} else {
		throw std::runtime_error(
			"Unknown automaton type '" + section.type + "'; expected type to contain one of: NFA, AFA, NFT"
		);
	}
	aut.alphabet_type = get_alphabet_type(section.type);

	for (const auto& [key, symbol_names] : section.dict) {
		if (key.find("Alphabet") != std::string::npos) {
			aut.symbol_naming = get_naming_type(key);
			if (aut.are_symbols_enum_type()) { aut.symbols_names.insert(symbol_names.begin(), symbol_names.end()); }
		} else if (key.find("States") != std::string::npos) {
			aut.state_naming = get_naming_type(key);
			if (aut.are_states_enum_type()) { aut.states_names.insert(symbol_names.begin(), symbol_names.end()); }
		} else if (key.find("Nodes") != std::string::npos) {
			aut.node_naming = get_naming_type(key);
			if (aut.are_nodes_enum_type()) { aut.nodes_names.insert(symbol_names.begin(), symbol_names.end()); }
		}
	}

	// Once we know what states and nodes are we can parse initial and final formula
	const auto dict_tokens = [](const std::vector<std::string>& names) {
		std::vector<mata::parser::BodyToken> tokens{};
		tokens.reserve(names.size());
		for (const std::string& name : names) { tokens.emplace_back(name, false); }
		return tokens;
	};
	for (const auto& [key, symbol_names] : section.dict) {
		if (key.starts_with("Initial")) {
			auto postfix = infix_to_postfix(aut, dict_tokens(symbol_names));
			if (no_operators(postfix)) {
				aut.initial_enumerated = true;
				postfix = add_disjunction_implicitly(std::move(postfix));
			}
			aut.initial_formula = postfix_to_graph(std::move(postfix));
		} else if (key.starts_with("Final")) {
			auto postfix = infix_to_postfix(aut, dict_tokens(symbol_names));
			if (no_operators(postfix)) {
				postfix = add_disjunction_implicitly(std::move(postfix));
				aut.final_enumerated = true;
			}
			aut.final_formula = postfix_to_graph(std::move(postfix));
		}
	}

	if (!has_at_most_one_auto_naming(aut)) {
		throw std::runtime_error("Only one of nodes, symbols and states could be specified automatically");
	}

	for (const auto& trans : section.body) { mata::IntermediateAut::parse_transition(aut, trans); }

	return aut;
}
} // Anonymous namespace.

size_t mata::IntermediateAut::get_number_of_disjuncts() const {
	size_t res = 0;

	for (const auto& formula_graph : this->transitions | std::views::values) {
		size_t trans_disjuncts = 0;
		std::vector<const FormulaGraph*> stack;
		stack.push_back(&formula_graph);

		while (!stack.empty()) {
			const FormulaGraph* gr = stack.back();
			stack.pop_back();
			if (gr->node.is_operator() && gr->node.operator_type == FormulaNode::OperatorType::Or) {
				trans_disjuncts++;
			}
			for (const auto& ch : gr->children) { stack.push_back(&ch); }
		}
		res += std::max(trans_disjuncts, size_t{1});
	}

	return res;
}

/**
 * Parses a transition by firstly transforming transition formula to postfix form and then creating
 * a tree representing the formula from postfix.
 * @param aut Automaton to which transition will be added.
 * @param tokens Series of tokens representing transition formula
 */
void mata::IntermediateAut::parse_transition(mata::IntermediateAut& aut, const mata::parser::BodyLine& tokens) {
	if (tokens.size() < 2) {
		throw std::runtime_error(
			"Malformed transition " + std::to_string(tokens) + ": expected at least 2 tokens, got " +
			std::to_string(tokens.size())
		);
	}
	mata::FormulaNode lhs = create_node(aut, tokens[0].token, tokens[0].quoted);
	const std::span<const mata::parser::BodyToken> rhs{std::span{tokens}.subspan(1)};

	std::vector<mata::FormulaNode> postfix;

	// add implicit conjunction to NFA explicit states, i.e. p a q -> p a & q
	// A quoted "&" names the symbol &; it is not the conjunction of an explicit transition.
	const bool explicit_transition = tokens[tokens.size() - 2].quoted || tokens[tokens.size() - 2].token != "&";
	if (aut.automaton_type == mata::IntermediateAut::AutomatonType::Nfa && explicit_transition) {
		// we need to take care of this case manually since user does not need to determine
		// symbol and state naming and put conjunction to transition
		if (aut.alphabet_type != mata::IntermediateAut::AlphabetType::Bitvector) {
			check_explicit_transition(tokens, "NFA");
			// The symbol slot is identified by position, not by naming: keep its full name, also when quoted.
			postfix.emplace_back(
				mata::FormulaNode::Type::Operand, rhs[0].token, rhs[0].token, mata::FormulaNode::OperandType::Symbol
			);
			postfix.emplace_back(create_node(aut, rhs[1].token, rhs[1].quoted));
		} else if (aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector) {
			// This is a case where rhs state is not separated by a conjunction from the rest of the transitions.
			const mata::parser::BodyToken& last_token{rhs.back()};
			postfix = infix_to_postfix(aut, rhs.first(rhs.size() - 1));
			postfix.emplace_back(create_node(aut, last_token.token, last_token.quoted));
		} else {
			MATA_ASSERT(false, "Unknown NFA type");
		}

		postfix.emplace_back(mata::FormulaNode::Type::Operator, "&", "&", mata::FormulaNode::OperatorType::And);
	} else if (aut.automaton_type == mata::IntermediateAut::AutomatonType::Nft && explicit_transition) {
		// we need to take care of this case manually since user does not need to determine
		// symbol and state naming and put conjunction to transition
		if (aut.alphabet_type != mata::IntermediateAut::AlphabetType::Bitvector) {
			check_explicit_transition(tokens, "NFT");
			postfix.emplace_back(
				mata::FormulaNode::Type::Operand, rhs[0].token, rhs[0].token, mata::FormulaNode::OperandType::Symbol
			);
			postfix.emplace_back(create_node(aut, rhs[1].token, rhs[1].quoted));
		} else if (aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector) {
			// This is a case where rhs state is not separated by a conjunction from the rest of the transitions.
			const mata::parser::BodyToken& last_token{rhs.back()};
			postfix = infix_to_postfix(aut, rhs.first(rhs.size() - 1));
			postfix.emplace_back(create_node(aut, last_token.token, last_token.quoted));
		} else {
			MATA_ASSERT(false, "Unknown NFT type");
		}

		postfix.emplace_back(mata::FormulaNode::Type::Operator, "&", "&", mata::FormulaNode::OperatorType::And);
	} else {
		postfix = infix_to_postfix(aut, rhs);
	}

	aut.transitions.emplace_back(std::move(lhs), postfix_to_graph(std::move(postfix)));
}

std::unordered_set<std::string> mata::FormulaGraph::collect_node_names() const {
	std::unordered_set<std::string> res;
	std::vector<const mata::FormulaGraph*> stack;

	// FormulaGraph contains a node member, but we alias it as a FormulaGraph for tree traversal.
	// This is intentional type punning required by the algorithm.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	stack.emplace_back(reinterpret_cast<const FormulaGraph*>(&(this->node)));
	while (!stack.empty()) {
		const FormulaGraph* g = stack.back();
		MATA_ASSERT(g != nullptr);
		stack.pop_back();

		if (g->node.type == FormulaNode::Type::Unknown) {
			continue; // skip undefined nodes
		}

		if (g->node.is_operand()) { res.insert(g->node.name); }

		for (const auto& child : g->children) { stack.push_back(&child); }
	}

	return res;
}

void mata::FormulaGraph::print_tree(std::ostream& os) const {
	std::vector<const FormulaGraph*> next_level;

	next_level.push_back(this);
	while (!next_level.empty()) {
		const std::vector<const FormulaGraph*> this_level = next_level;
		next_level.clear();
		for (const auto& graph : this_level) {
			for (const auto& child : graph->children) { next_level.push_back(&child); }
			os << graph->node.raw << "    ";
		}
		os << "\n";
	}
}

std::vector<mata::IntermediateAut> mata::IntermediateAut::parse_from_mf(const mata::parser::Parsed& parsed) {
	std::vector<mata::IntermediateAut> result;
	result.reserve(parsed.size());

	for (const parser::ParsedSection& parsed_section : parsed) {
		if (parsed_section.type.find("FA") == std::string::npos &&
			parsed_section.type.find("FT") == std::string::npos) {
			continue;
		}
		result.push_back(mf_to_aut(parsed_section));
	}

	return result;
}

const mata::FormulaGraph&
	mata::IntermediateAut::get_symbol_part_of_transition(const std::pair<FormulaNode, FormulaGraph>& transition) const {
	if (!this->is_nfa()) { throw std::runtime_error("We currently support symbol extraction only for NFA"); }
	const FormulaGraph& rhs = transition.second;
	// A conjunction of the symbol part with the target state.
	if (!transition.first.is_operand() || !rhs.node.is_operator() || rhs.children.size() != 2 ||
		!rhs.children[1].node.is_operand()) {
		throw std::runtime_error(
			"Malformed transition from '" + transition.first.raw + "': expected a symbol formula and a target state"
		);
	}
	return rhs.children[0];
}

void mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& symbol, const FormulaGraph& rhs) {
	const FormulaNode conjunction(FormulaNode::Type::Operator, "&", "&", FormulaNode::OperatorType::And);
	FormulaGraph graph(conjunction);
	graph.children.emplace_back(symbol);
	graph.children.push_back(rhs);
	this->transitions.emplace_back(lhs, std::move(graph));
}

void mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& rhs) {
	MATA_ASSERT(rhs.is_operand());
	FormulaGraph graph(rhs);
	this->transitions.emplace_back(lhs, std::move(graph));
}

void mata::IntermediateAut::print_transitions_trees(std::ostream& os) const {
	for (const auto& [formula_node, formula_graph] : transitions) {
		os << formula_node.raw << " -> ";
		formula_graph.print_tree(os);
	}
}

std::unordered_set<std::string> mata::IntermediateAut::get_positive_finals() const {
	if (!is_graph_conjunction_of_negations(this->final_formula)) {
		throw(std::runtime_error("Final formula is not a conjunction of negations"));
	}

	std::unordered_set<std::string> all = initial_formula.collect_node_names();
	for (const auto& [formula_node, formula_graph] : this->transitions) {
		all.insert(formula_node.name);
		// get names from state part of transition
		const auto node_names = formula_graph.children[1].collect_node_names();
		all.insert(node_names.begin(), node_names.end());
	}

	for (const std::string& nonfinal : final_formula.collect_node_names()) { all.erase(nonfinal); }

	return all;
}

bool mata::IntermediateAut::is_graph_conjunction_of_negations(const mata::FormulaGraph& graph) {
	const FormulaGraph* act_graph = &graph;

	while (act_graph->children.size() == 2) {
		// this node is conjunction and the left son is negation, otherwise returns false
		if (act_graph->node.is_operator() && act_graph->node.is_and() && act_graph->children[0].node.is_operator() &&
			act_graph->children[0].node.is_neg()) {
			act_graph = &act_graph->children[1];
		} else {
			return false;
		}
	}

	// the last child needs to be negation
	return act_graph->node.is_operator() && act_graph->node.is_neg();
}

std::ostream& std::operator<<(std::ostream& os, const mata::IntermediateAut& inter_aut) {
	std::string type;
	if (inter_aut.is_nfa()) {
		type = "NFA";
	} else if (inter_aut.is_afa()) {
		type = "AFA";
	} else {
		type = "Unknown";
	}
	os << "Intermediate automaton type " << type << '\n';
	os << "Naming - state: " << static_cast<size_t>(inter_aut.state_naming)
	   << " symbol: " << static_cast<size_t>(inter_aut.symbol_naming)
	   << " node: " << static_cast<size_t>(inter_aut.node_naming) << '\n';
	os << "Alphabet " << static_cast<size_t>(inter_aut.alphabet_type) << '\n';

	os << "Initial states: ";
	for (const auto& state : inter_aut.initial_formula.collect_node_names()) { os << state << ' '; }
	os << '\n';

	os << "Final states: ";
	for (const auto& state : inter_aut.final_formula.collect_node_names()) { os << state << ' '; }
	os << '\n';

	os << "Transitions: \n";
	for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
		os << formula_node.raw << " -> ";
		os << serialize_graph(formula_graph);
		/*
		for (const auto& rhs : trans.second.collect_node_names()) {
			os << rhs << ' ';
		}
		 */
		os << '\n';
	}
	os << "\n";

	return os;
}
