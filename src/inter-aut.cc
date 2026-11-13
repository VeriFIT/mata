/*
 * inter-aut.hh -- intermediate representation of automata.
 *
 * It represents automaton after parsing and before translation to particular automaton.
 */

#include "mata/parser/inter-aut.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

#include <algorithm>
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
		if (const std::string_view token = tokens[i].token;
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

std::string serialize_graph(const mata::FormulaGraphView graph) {
	if (graph.node().is_operand()) { return std::string{graph.node().raw}; }

	const mata::FormulaGraphChildren children{graph.children()};
	if (children.size() == 1) { // unary operator
		const mata::FormulaGraphView child{children.front()};
		const std::string child_name =
			child.node().is_operand() ? std::string{child.node().raw} : "(" + serialize_graph(child) + ")";
		return std::string{graph.node().raw} + child_name;
	}

	if (children.size() != 2) {
		throw std::runtime_error(
			"Cannot serialize a formula graph with a node '" + std::string{graph.node().raw} +
			"' that is neither an operand nor an operator."
		);
	}
	const mata::FormulaGraphView left_child{children.front()};
	std::string lhs =
		(left_child.node().is_operand()) ? std::string{left_child.node().raw} : serialize_graph(left_child);
	if (left_child.children().size() == 2) { lhs = "(" + lhs + ")"; }
	const mata::FormulaGraphView right_child{children[1]};
	std::string rhs =
		(right_child.node().is_operand()) ? std::string{right_child.node().raw} : serialize_graph(right_child);
	if (right_child.children().size() == 2) { rhs = "(" + rhs + ")"; }

	return lhs + " " + std::string{graph.node().raw} + " " + rhs;
}

/**
 * Copies @p node with its name stored in @p names, so that the copy does not depend on the storage the name of
 *  the original points into.
 */
mata::FormulaNode stored_copy(const mata::FormulaNode& node, mata::NameStore& names) {
	mata::FormulaNode copy{node};
	copy.raw = names.store(node.raw);
	return copy;
}

mata::FormulaNode create_node(mata::IntermediateAut& mata, const std::string_view token, const bool quoted = false) {
	// A node keeps a view of its name, so the token has to be stored in the automaton first.
	const std::string_view raw{mata.name_store->store(token)};

	// A quoted token stands for a name, even when the name is that of an operator: the reader quoted it exactly
	//  so that it is not read for an operator of the transition formula.
	if (!quoted) {
		if (!token.empty() && is_logical_operator(token[0])) {
			switch (token[0]) {
				case '&':
					return {mata::FormulaNode::Type::Operator, raw, mata::FormulaNode::OperatorType::And};
				case '|':
					return {mata::FormulaNode::Type::Operator, raw, mata::FormulaNode::OperatorType::Or};
				case '!':
					return {mata::FormulaNode::Type::Operator, raw, mata::FormulaNode::OperatorType::Neg};
				default:
					MATA_ASSERT(false);
			}
		} else if (token == "("sv) {
			return {mata::FormulaNode::Type::LeftParenthesis, raw};
		} else if (token == ")"sv) {
			return {mata::FormulaNode::Type::RightParenthesis, raw};
		}
	}

	if (token == "\\true"sv) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::True};
	} else if (token == "\\false"sv) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::False};
	} else if (is_naming_enum(mata.state_naming) && mata.states_names.contains(std::string{token})) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::State};
	} else if (is_naming_enum(mata.node_naming) && mata.nodes_names.contains(std::string{token})) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Node};
	} else if (is_naming_enum(mata.symbol_naming) && mata.symbols_names.contains(std::string{token})) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Symbol};
	} else if (is_naming_marker(mata.state_naming) && token.starts_with('q')) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::State, true};
	} else if (is_naming_marker(mata.node_naming) && token.starts_with('n')) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Node, true};
	} else if (is_naming_marker(mata.symbol_naming) && token.starts_with('a')) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Symbol, true};
	} else if (is_naming_auto(mata.state_naming)) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::State};
	} else if (is_naming_auto(mata.node_naming)) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Node};
	} else if (is_naming_auto(mata.symbol_naming)) {
		return {mata::FormulaNode::Type::Operand, raw, mata::FormulaNode::OperandType::Symbol};
	}

	throw std::runtime_error("Unknown token " + std::string{token});
}

/**
 * Checks if @p op1 has lower precedence than @p op2. Precedence from the lowest to highest is: | < & < !
 */
bool lower_precedence(const mata::FormulaNode::OperatorType op1, const mata::FormulaNode::OperatorType op2) {
	return op1 != mata::FormulaNode::OperatorType::Neg &&
		   (op1 != mata::FormulaNode::OperatorType::And || op2 == mata::FormulaNode::OperatorType::Neg);
}

/// The buffers of parsing a transition formula, reused from formula to formula.
struct FormulaBuffers {
	std::vector<mata::FormulaNode> postfix{};
	std::vector<mata::FormulaNode> opstack{};
};

/**
 * Transforms a series of tokes to postfix notation.
 * @param aut Automaton for which transition formula is parsed
 * @param tokens Series of tokens representing transition formula parsed from the input text
 * @param buffers Buffers whose @c postfix the postfix notation is appended to
 */
void infix_to_postfix(
	mata::IntermediateAut& aut, const std::span<const mata::parser::BodyToken> tokens, FormulaBuffers& buffers
) {
	std::vector<mata::FormulaNode>& opstack{buffers.opstack};
	std::vector<mata::FormulaNode>& output{buffers.postfix};
	opstack.clear();
	output.reserve(output.size() + tokens.size());

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
}

/// Transforms @p tokens to postfix notation, see @c infix_to_postfix().
std::vector<mata::FormulaNode>
	infix_to_postfix(mata::IntermediateAut& aut, const std::span<const mata::parser::BodyToken> tokens) {
	FormulaBuffers buffers;
	infix_to_postfix(aut, tokens, buffers);
	return std::move(buffers.postfix);
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
		res.emplace_back(mata::FormulaNode::Type::Operator, "|", mata::FormulaNode::OperatorType::Or);
	}

	for (size_t i{2}; i < postfix_size; ++i) {
		res.push_back(std::move(postfix[i]));
		res.emplace_back(mata::FormulaNode::Type::Operator, "|", mata::FormulaNode::OperatorType::Or);
	}

	return res;
}

/**
 * Parses a transition by firstly transforming transition formula to postfix form and then creating
 * a tree representing the formula from postfix.
 * @param aut Automaton to which transition will be added.
 * @param tokens Series of tokens representing transition formula
 * @param buffers Buffers to reuse
 */
void parse_transition(mata::IntermediateAut& aut, const mata::parser::BodyLine& tokens, FormulaBuffers& buffers) {
	if (tokens.size() < 2) {
		throw std::runtime_error(
			"Malformed transition " + std::to_string(tokens) + ": expected at least 2 tokens, got " +
			std::to_string(tokens.size())
		);
	}
	mata::FormulaNode lhs = create_node(aut, tokens[0].token, tokens[0].quoted);
	const std::span<const mata::parser::BodyToken> rhs{std::span{tokens}.subspan(1)};

	std::vector<mata::FormulaNode>& postfix{buffers.postfix};
	postfix.clear();

	// add implicit conjunction to NFA explicit states, i.e. p a q -> p a & q
	// A quoted "&" names the symbol &; it is not the conjunction of an explicit transition.
	const bool explicit_transition = tokens[tokens.size() - 2].quoted || tokens[tokens.size() - 2].token != "&";
	const bool explicit_automaton{
		aut.automaton_type == mata::IntermediateAut::AutomatonType::Nfa ||
		aut.automaton_type == mata::IntermediateAut::AutomatonType::Nft
	};
	if (explicit_automaton && explicit_transition) {
		// we need to take care of this case manually since user does not need to determine
		// symbol and state naming and put conjunction to transition
		if (aut.alphabet_type != mata::IntermediateAut::AlphabetType::Bitvector) {
			check_explicit_transition(tokens, aut.is_nfa() ? "NFA" : "NFT");
			// The symbol slot is identified by position, not by naming: keep its full name, also when quoted.
			postfix.emplace_back(
				mata::FormulaNode::Type::Operand, aut.name_store->store(rhs[0].token),
				mata::FormulaNode::OperandType::Symbol
			);
			postfix.emplace_back(create_node(aut, rhs[1].token, rhs[1].quoted));
		} else {
			// This is a case where rhs state is not separated by a conjunction from the rest of the transitions.
			const mata::parser::BodyToken& last_token{rhs.back()};
			infix_to_postfix(aut, rhs.first(rhs.size() - 1), buffers);
			postfix.emplace_back(create_node(aut, last_token.token, last_token.quoted));
		}

		postfix.emplace_back(mata::FormulaNode::Type::Operator, "&", mata::FormulaNode::OperatorType::And);
	} else {
		infix_to_postfix(aut, rhs, buffers);
	}

	aut.transitions.emplace_back(std::move(lhs), mata::FormulaGraph::from_postfix(postfix));
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
		// TODO(afa): Re-add the alternating automata support. The AFA code is kept, but not compiled:
		//  `MATA_AFA_SUPPORT` is never defined.
#ifdef MATA_AFA_SUPPORT
	} else if (section.type.find("AFA") != std::string::npos) {
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Afa;
#endif
	} else if (section.type.find("NFT") != std::string::npos) {
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Nft;
	} else {
		throw std::runtime_error(
			"Unknown automaton type '" + section.type + "'; expected type to contain one of: NFA, NFT"
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
			aut.initial_formula = mata::FormulaGraph::from_postfix(postfix);
		} else if (key.starts_with("Final")) {
			auto postfix = infix_to_postfix(aut, dict_tokens(symbol_names));
			if (no_operators(postfix)) {
				postfix = add_disjunction_implicitly(std::move(postfix));
				aut.final_enumerated = true;
			}
			aut.final_formula = mata::FormulaGraph::from_postfix(postfix);
		}
	}

	if (!has_at_most_one_auto_naming(aut)) {
		throw std::runtime_error("Only one of nodes, symbols and states could be specified automatically");
	}

	FormulaBuffers buffers;
	for (const auto& trans : section.body) { parse_transition(aut, trans, buffers); }

	return aut;
}
} // Anonymous namespace.

size_t mata::IntermediateAut::get_number_of_disjuncts() const {
	size_t res = 0;

	for (const auto& formula_graph : this->transitions | std::views::values) {
		const auto trans_disjuncts{
			static_cast<size_t>(std::ranges::count_if(formula_graph.entries(), [](const FormulaGraphEntry& entry) {
				return entry.node.is_operator() && entry.node.operator_type == FormulaNode::OperatorType::Or;
			}))
		};
		res += std::max(trans_disjuncts, size_t{1});
	}

	return res;
}

void mata::IntermediateAut::parse_transition(mata::IntermediateAut& aut, const mata::parser::BodyLine& tokens) {
	FormulaBuffers buffers;
	::parse_transition(aut, tokens, buffers);
}

mata::FormulaGraph mata::FormulaGraph::from_postfix(const std::span<const FormulaNode> postfix) {
	FormulaGraph graph;
	graph.entries_.resize(postfix.size());
	FormulaGraphEntry* const entries{graph.entries_.data()};
	// The number of the subtrees before the current node that are not children of a node yet.
	size_t num_trees{0};
	for (size_t i{0}; i < postfix.size(); ++i) {
		const FormulaNode& node{postfix[i]};
		uint32_t size{1};
		switch (node.type) {
			case FormulaNode::Type::Operand:
				++num_trees;
				break;
			case FormulaNode::Type::Operator:
				if (node.is_neg()) {
					if (num_trees == 0) { throw std::runtime_error("Malformed formula: '!' without an operand"); }
					size += entries[i - 1].subtree_size;
				} else {
					if (num_trees < 2) {
						throw std::runtime_error(
							"Malformed formula: '" + std::string{node.raw} + "' needs two operands"
						);
					}
					--num_trees;
					size += entries[i - 1].subtree_size;
					size += entries[i - size].subtree_size;
				}
				break;
			default:
				throw std::runtime_error("Malformed formula: unexpected '" + std::string{node.raw} + "'");
		}
		entries[i] = {node, size};
	}
	if (num_trees > 1) { throw std::runtime_error("Malformed formula: operands without an operator between them"); }
	return graph;
}

mata::FormulaGraph::FormulaGraph(const FormulaNode& node) {
	if (node.is_operator()) {
		throw std::invalid_argument(
			"Formula graph: the operator '" + std::string{node.raw} + "' needs operands to be a graph"
		);
	}
	entries_.push_back({node, 1});
}

mata::FormulaGraph::FormulaGraph(const FormulaGraphView tree)
	: entries_{tree.entries().begin(), tree.entries().end()} {}

mata::FormulaGraph::FormulaGraph(const FormulaNode& op, const FormulaGraphView operand) {
	if (!op.is_neg() || operand.entries().empty()) {
		throw std::invalid_argument(
			"Formula graph: expected a negation of a formula, got '" + std::string{op.raw} + "'"
		);
	}
	entries_.reserve(operand.entries().size() + 1);
	append(operand);
	append_root(op);
}

mata::FormulaGraph::FormulaGraph(const FormulaNode& op, const FormulaGraphView lhs, const FormulaGraphView rhs) {
	if (!op.is_operator() || op.is_neg() || lhs.entries().empty() || rhs.entries().empty()) {
		throw std::invalid_argument(
			"Formula graph: expected a binary operator of two formulas, got '" + std::string{op.raw} + "'"
		);
	}
	entries_.reserve(lhs.entries().size() + rhs.entries().size() + 1);
	append(lhs);
	append(rhs);
	append_root(op);
}

void mata::FormulaGraph::append(const FormulaGraphView tree) {
	entries_.insert(entries_.end(), tree.entries().begin(), tree.entries().end());
}

void mata::FormulaGraph::append_root(const FormulaNode& node) {
	uint32_t size{1 + entries_.back().subtree_size};
	if (!node.is_neg()) { size += entries_[entries_.size() - size].subtree_size; }
	entries_.push_back({node, size});
}

void mata::FormulaGraph::store_names(NameStore& names) {
	for (FormulaGraphEntry& entry : entries_) { entry.node.raw = names.store(entry.node.raw); }
}

std::unordered_set<std::string> mata::FormulaGraphView::collect_node_names() const {
	std::unordered_set<std::string> res;
	for (const FormulaGraphEntry& entry : entries()) {
		if (entry.node.is_operand()) { res.emplace(entry.node.name()); }
	}
	return res;
}

void mata::FormulaGraphView::print_tree(std::ostream& os) const {
	std::vector<FormulaGraphView> next_level;

	next_level.push_back(*this);
	while (!next_level.empty()) {
		const std::vector<FormulaGraphView> this_level = next_level;
		next_level.clear();
		for (const FormulaGraphView graph : this_level) {
			for (const FormulaGraphView child : graph.children()) { next_level.push_back(child); }
			os << graph.node().raw << "    ";
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

mata::FormulaGraphView
	mata::IntermediateAut::get_symbol_part_of_transition(const std::pair<FormulaNode, FormulaGraph>& transition) const {
	if (!this->is_nfa()) { throw std::runtime_error("We currently support symbol extraction only for NFA"); }
	const FormulaGraphView rhs{transition.second};
	const FormulaGraphChildren children{rhs.children()};
	// A conjunction of the symbol part with the target state.
	if (!transition.first.is_operand() || children.size() != 2 || !children[1].node().is_operand()) {
		throw std::runtime_error(
			"Malformed transition from '" + std::string{transition.first.raw} +
			"': expected a symbol formula and a target state"
		);
	}
	return children[0];
}

void mata::IntermediateAut::add_transition(
	const FormulaNode& lhs, const FormulaNode& symbol, const FormulaGraphView rhs
) {
	const FormulaNode conjunction(FormulaNode::Type::Operator, "&", FormulaNode::OperatorType::And);
	FormulaGraph graph{conjunction, FormulaGraph{symbol}, rhs};
	graph.store_names(*name_store);
	this->transitions.emplace_back(stored_copy(lhs, *name_store), std::move(graph));
}

void mata::IntermediateAut::add_transition(const FormulaNode& lhs, const FormulaNode& rhs) {
	MATA_ASSERT(rhs.is_operand());
	this->transitions.emplace_back(stored_copy(lhs, *name_store), stored_copy(rhs, *name_store));
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
		all.emplace(formula_node.name());
		// get names from state part of transition
		const auto node_names = formula_graph.children()[1].collect_node_names();
		all.insert(node_names.begin(), node_names.end());
	}

	for (const std::string& nonfinal : final_formula.collect_node_names()) { all.erase(nonfinal); }

	return all;
}

bool mata::IntermediateAut::is_graph_conjunction_of_negations(FormulaGraphView graph) {
	for (FormulaGraphChildren children{graph.children()}; children.size() == 2; children = graph.children()) {
		// this node is conjunction and the left son is negation, otherwise returns false
		if (graph.node().is_and() && children[0].node().is_neg()) {
			graph = children[1];
		} else {
			return false;
		}
	}

	// the last child needs to be negation
	return graph.node().is_neg();
}

std::ostream& std::operator<<(std::ostream& os, const mata::IntermediateAut& inter_aut) {
	std::string type;
	if (inter_aut.is_nfa()) {
		type = "NFA";
#ifdef MATA_AFA_SUPPORT
	} else if (inter_aut.is_afa()) {
		type = "AFA";
#endif
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
