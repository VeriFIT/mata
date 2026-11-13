// TODO: rename to intermediate_afa / afa-intermediate, or something like that.
/*
 * inter-aut.hh -- intermediate representation of automata.
 *
 * It represents automaton after parsing and before translation to particular automaton.
 */

#ifndef MATA_INTER_AUT_HH
#define MATA_INTER_AUT_HH

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser.hh"

namespace mata {

/**
 * An append-only storage of the names of formula nodes: a node keeps a view into it instead of a string of its own.
 * The bytes handed out are never moved, so a view stays valid for as long as the storage lives. Copies of
 *  @c IntermediateAut share the storage, which keeps the names of their nodes valid.
 */
class NameStore {
  public:
	/**
	 * @brief Copies @p name into the storage.
	 * @return A view of the copy, valid for as long as the storage lives.
	 */
	std::string_view store(const std::string_view name) {
		if (chunks_.empty() || chunks_.back().capacity - used_ < name.size()) {
			const std::size_t capacity{name.size() < chunk_capacity_ ? chunk_capacity_ : name.size()};
			chunks_.push_back(Chunk{.data = std::make_unique_for_overwrite<char[]>(capacity), .capacity = capacity});
			used_ = 0;
		}
		char* const stored{chunks_.back().data.get() + used_};
		std::memcpy(stored, name.data(), name.size());
		used_ += name.size();
		return {stored, name.size()};
	}

  private:
	/// One block of the storage; the bytes of the blocks handed out earlier stay where they are.
	struct Chunk {
		std::unique_ptr<char[]> data{};
		std::size_t capacity{};
	};

	static constexpr std::size_t chunk_capacity_ = 16 * 1'024;

	std::vector<Chunk> chunks_{};
	/// The number of bytes used in the last chunk.
	std::size_t used_{};
};

/**
 * A node of graph representing transition formula. A node could be operator (!,&,|) or operand (symbol, state, node).
 * Each node has its raw name as written in the input, its name (the raw name without the type marker, the initial
 *  character, under the marked naming), and information about its type.
 */
class FormulaNode {
  public:
	enum class OperandType : std::uint8_t { Symbol, State, Node, True, False, NotOperand };

	enum class OperatorType : std::uint8_t { Neg, And, Or, NotOperator };

	enum class Type : std::uint8_t { Operand, Operator, LeftParenthesis, RightParenthesis, Unknown };

	/// Define whether a node is operand or operator
	Type type{};
	/**
	 * Raw name of node as it was specified in input text, i.e., including type marker.
	 * The name is not owned: it has to point into the name storage of the automaton the node belongs to
	 *  (@c IntermediateAut::name_store) or into a literal, so that it lives for as long as the node does.
	 */
	std::string_view raw{};
	/// if a node is operator, it defines which one
	OperatorType operator_type{};
	/// if a node is operand, it defines which one
	OperandType operand_type{};

	/// The parsed name: @c raw without the type marker, if the node has one. Valid while @c raw is unchanged.
	std::string_view name() const { return std::string_view{raw}.substr(marked_ ? 1 : 0); }

	bool is_operand() const { return type == Type::Operand; }

	bool is_operator() const { return type == Type::Operator; }

	bool is_right_parenthesis() const { return type == Type::RightParenthesis; }

	bool is_left_parenthesis() const { return type == Type::LeftParenthesis; }

	bool is_state() const { return operand_type == OperandType::State; }

	bool is_symbol() const { return operand_type == OperandType::Symbol; }

	bool is_and() const { return type == Type::Operator && operator_type == OperatorType::And; }

	bool is_neg() const { return type == Type::Operator && operator_type == OperatorType::Neg; }

	// TODO: should constant be its own operand type?
	bool is_constant() const { return is_true() || is_false(); }
	bool is_true() const { return is_operand() && operand_type == OperandType::True; }
	bool is_false() const { return is_operand() && operand_type == OperandType::False; }

	constexpr FormulaNode()
		: type(Type::Unknown),
		  operator_type(OperatorType::NotOperator),
		  operand_type(OperandType::NotOperand) {}

	FormulaNode(const Type t, const std::string_view raw, const OperatorType operator_t)
		: type(t),
		  raw(raw),
		  operator_type(operator_t),
		  operand_type(OperandType::NotOperand) {}

	/// @param marked Whether the first character of @p raw is a type marker, which the name leaves out.
	FormulaNode(const Type t, const std::string_view raw, const OperandType operand, const bool marked = false)
		: type(t),
		  raw(raw),
		  operator_type(OperatorType::NotOperator),
		  operand_type(operand),
		  marked_(marked && !raw.empty()) {}

	FormulaNode(const Type t, const std::string_view raw)
		: type(t),
		  raw(raw),
		  operator_type(OperatorType::NotOperator),
		  operand_type(OperandType::NotOperand) {}

	// A literal is storage of its own, so a node can point into it directly.
	FormulaNode(const Type t, const char* const raw, const OperatorType operator_t)
		: FormulaNode(t, std::string_view{raw}, operator_t) {}

	FormulaNode(const Type t, const char* const raw, const OperandType operand, const bool marked = false)
		: FormulaNode(t, std::string_view{raw}, operand, marked) {}

	FormulaNode(const Type t, const char* const raw) : FormulaNode(t, std::string_view{raw}) {}

	// A name is a view, so a temporary string would leave it dangling: the name has to be stored first, in the name
	//  storage of the automaton the node belongs to.
	FormulaNode(Type, std::string&&, OperatorType) = delete;
	FormulaNode(Type, std::string&&, OperandType, bool = false) = delete;
	FormulaNode(Type, std::string&&) = delete;

	FormulaNode(const FormulaNode& n) = default;

	FormulaNode(FormulaNode&&) noexcept = default;

	FormulaNode& operator=(const FormulaNode& other) = default;
	FormulaNode& operator=(FormulaNode&& other) noexcept = default;
	~FormulaNode() = default;

  private:
	bool marked_{false};
};

/// A node of a @c FormulaGraph as the graph stores it.
struct FormulaGraphEntry {
	FormulaNode node{};
	/// The number of nodes of the subtree this node roots, this one included.
	uint32_t subtree_size{0};
};

class FormulaGraph;
class FormulaGraphChildren;

/**
 * A subtree of a @c FormulaGraph: a view, valid while the graph it points into lives and is not modified.
 * A view of an empty graph is a single node of the type @c FormulaNode::Type::Unknown without children.
 */
class FormulaGraphView {
  public:
	/// A view of an empty graph.
	constexpr FormulaGraphView() = default;
	/// The view of the whole @p graph.
	FormulaGraphView(const FormulaGraph& graph);

	const FormulaNode& node() const { return entry_->node; }
	FormulaGraphChildren children() const;
	/// The nodes of the subtree in postfix order: the subtrees of the children of a node come right before the node.
	std::span<const FormulaGraphEntry> entries() const {
		return {entry_ + 1 - entry_->subtree_size, entry_->subtree_size};
	}

	std::unordered_set<std::string> collect_node_names() const;
	void print_tree(std::ostream& os) const;

  private:
	friend class FormulaGraph;
	explicit constexpr FormulaGraphView(const FormulaGraphEntry* entry) : entry_{entry} {}

	static constexpr FormulaGraphEntry EMPTY_ENTRY{};
	const FormulaGraphEntry* entry_{&EMPTY_ENTRY};
};

/// The children of a node of a @c FormulaGraph: none for an operand, one for a negation, two for a conjunction or a
///  disjunction.
class FormulaGraphChildren {
  public:
	size_t size() const { return size_; }
	bool empty() const { return size_ == 0; }
	const FormulaGraphView& operator[](const size_t index) const { return views_[index]; }
	const FormulaGraphView& at(const size_t index) const {
		if (index >= size_) { throw std::out_of_range("FormulaGraphChildren::at"); }
		return views_[index];
	}
	const FormulaGraphView& front() const { return views_[0]; }
	const FormulaGraphView& back() const { return views_[size_ - 1]; }
	const FormulaGraphView* begin() const { return views_.data(); }
	const FormulaGraphView* end() const { return views_.data() + size_; }

  private:
	friend class FormulaGraphView;
	std::array<FormulaGraphView, 2> views_{};
	size_t size_{0};
};

inline FormulaGraphChildren FormulaGraphView::children() const {
	FormulaGraphChildren children;
	if (!entry_->node.is_operator()) { return children; }
	const FormulaGraphEntry* const last{entry_ - 1};
	if (entry_->node.is_neg()) {
		children.views_[0] = FormulaGraphView{last};
		children.size_ = 1;
	} else {
		children.views_[0] = FormulaGraphView{last - last->subtree_size};
		children.views_[1] = FormulaGraphView{last};
		children.size_ = 2;
	}
	return children;
}

/**
 * A transition formula as a tree whose nodes are the operators and operands of the formula.
 * E.g., a formula q1 & s1 is a tree with & as the root node and q1 and s1 as the children of the root.
 * The nodes are stored in one vector in postfix order, so a whole formula takes a single allocation. A negation has
 *  one child, a conjunction and a disjunction two, and any other node none.
 */
class FormulaGraph {
  public:
	/// An empty graph, viewed as a single node of the type @c FormulaNode::Type::Unknown.
	FormulaGraph() = default;
	/// A graph of the single node @p node, which has no children, so it must not be an operator.
	explicit FormulaGraph(const FormulaNode& node);
	/// A copy of the subtree @p tree.
	explicit FormulaGraph(FormulaGraphView tree);
	/// A graph of the negation @p op of @p operand.
	FormulaGraph(const FormulaNode& op, FormulaGraphView operand);
	/// A graph of the conjunction or disjunction @p op of @p lhs and @p rhs.
	FormulaGraph(const FormulaNode& op, FormulaGraphView lhs, FormulaGraphView rhs);

	/**
	 * Builds the graph of the formula in postfix notation @p postfix.
	 * @throws std::runtime_error if @p postfix is not a single formula, as when an operator lacks operands.
	 */
	static FormulaGraph from_postfix(std::span<const FormulaNode> postfix);

	bool empty() const { return entries_.empty(); }
	FormulaGraphView root() const { return empty() ? FormulaGraphView{} : FormulaGraphView{&entries_.back()}; }
	const FormulaNode& node() const { return root().node(); }
	FormulaGraphChildren children() const { return root().children(); }
	/// The nodes in postfix order, see @c FormulaGraphView::entries().
	std::span<const FormulaGraphEntry> entries() const { return entries_; }

	/// Stores the names of all nodes in @p names, so that the graph no longer points into the storage it did.
	void store_names(NameStore& names);

	std::unordered_set<std::string> collect_node_names() const { return root().collect_node_names(); }
	void print_tree(std::ostream& os) const { root().print_tree(os); }

  private:
	/// Appends the subtree @p tree, as the next operand of the node appended after it.
	void append(FormulaGraphView tree);
	/// Appends @p node as the root of the subtrees its children are, which end right before it.
	void append_root(const FormulaNode& node);

	std::vector<FormulaGraphEntry> entries_{};
};

inline FormulaGraphView::FormulaGraphView(const FormulaGraph& graph) : FormulaGraphView{graph.root()} {}

/**
 * Structure for a general intermediate representation of parsed automaton.
 * It contains information about type of automata, type of naming of nodes, symbols and states
 * and type of alphabet. It contains also the transitions formula and formula for initial and final
 * states. The formulas are represented as tree where nodes are either operands or operators.
 */
class IntermediateAut {
  public:
	/**
	 * Type of automaton. So far we support nondeterministic finite automata (NFA) and nondeterministic finite
	 *  transducers (NFT).
	 */
	enum class AutomatonType : std::uint8_t {
		Nfa,
	// TODO(afa): Re-add the alternating automata support. The AFA code is kept, but not compiled:
	//  `MATA_AFA_SUPPORT` is never defined.
#ifdef MATA_AFA_SUPPORT
		Afa,
#endif
		Nft,
	};

	/**
	 * The possible kinds of naming of items in sets of states, nodes, or symbols.
	 * It implies how the given set is defined.
	 * Naming could be automatic (all things in formula not belonging to other sets will be assigned to a
	 * set with automatic naming), marker based (everything beginning with `q` is a state, with `s` is a symbol,
	 * with `n` is a node), or enumerated (the given set is defined by enumeration).
	 * There are two special cases used for alphabet - symbols could be any character (CHARS) or anything from utf
	 * (UTF).
	 */
	enum class Naming : std::uint8_t { Auto, Marked, Enum, Chars, Utf };

	/**
	 * Type of alphabet. We can have explicit alphabet (containing explicit symbols), or bitvector, or class of symbols
	 * (e.g., alphabet is everything in utf), or intervals.
	 * So far, only explicit representation is supported.
	 */
	enum class AlphabetType : std::uint8_t { Explicit, Bitvector, Class, Intervals };

	Naming state_naming = Naming::Marked;
	Naming symbol_naming = Naming::Marked;
	Naming node_naming = Naming::Marked;
	AlphabetType alphabet_type{};
	AutomatonType automaton_type{};

	// The sets of names of the given kind when enumeration is used.
	std::unordered_set<std::string> states_names{};
	std::unordered_set<std::string> symbols_names{};
	std::unordered_set<std::string> nodes_names{};

	/**
	 * The storage the names of the formula nodes point into (@c FormulaNode::raw). Copies of the automaton share it,
	 *  which keeps the names of the nodes of a copy alive when the original is gone.
	 */
	std::shared_ptr<NameStore> name_store{std::make_shared<NameStore>()};

	FormulaGraph initial_formula{};
	FormulaGraph final_formula{};

	bool initial_enumerated = false;
	bool final_enumerated = false;

	/**
	 * Transitions are pairs where the first member is left-hand side of transition (i.e., a state)
	 * and the second item is a graph representing transition formula (which can contain symbols, nodes, and states).
	 */
	std::vector<std::pair<FormulaNode, FormulaGraph>> transitions{};

	/**
	 * @brief Returns symbolic part of transition.
	 *
	 * That may be just a symbol or bitvector formula.
	 * This function is supported only for NFA where transitions have a rhs state at the end of right-handed side of
	 *  transition.
	 * @param transition Transition from which symbol is returned
	 * @return Graph representing symbol. It may be just an explicit symbol or graph representing bitvector formula
	 */
	FormulaGraphView get_symbol_part_of_transition(const std::pair<FormulaNode, FormulaGraph>& transition) const;

	/**
	 * @brief A method for building a vector of IntermediateAut for a parsed input.
	 *
	 * For each section in input is created one IntermediateAut.
	 * It parses basic information about type of automata, its naming conventions etc.
	 * Then it parses input and final formula of automaton.
	 * Finally, transition formulas are transformed to graph representation by
	 * turning an input stream of tokens to postfix notation and then
	 * a tree representing the formula is built from it.
	 * @param parsed Parsed input in MATA format.
	 * @return A vector of InterAutomata from each section in parsed input.
	 */
	static std::vector<IntermediateAut> parse_from_mf(const mata::parser::Parsed& parsed);

	bool are_states_enum_type() const { return state_naming == Naming::Enum; }
	bool are_symbols_enum_type() const { return symbol_naming == Naming::Enum; }
	bool are_nodes_enum_type() const { return node_naming == Naming::Enum; }

	bool is_bitvector() const { return alphabet_type == AlphabetType::Bitvector; }
	bool is_nft() const { return automaton_type == AutomatonType::Nft; }
	bool is_nfa() const { return automaton_type == AutomatonType::Nfa; }
#ifdef MATA_AFA_SUPPORT
	bool is_afa() const { return automaton_type == AutomatonType::Afa; }
#endif

	std::unordered_set<std::string> get_enumerated_initials() const { return initial_formula.collect_node_names(); }
	std::unordered_set<std::string> get_enumerated_finals() const { return final_formula.collect_node_names(); }

	bool are_final_states_conjunction_of_negation() const { return is_graph_conjunction_of_negations(final_formula); }

	static bool is_graph_conjunction_of_negations(FormulaGraphView graph);

	/**
	 * Method returns a set of final states in the case that they were entered as a conjunction
	 * of negated states. It collects all negated states and subtracts them from set of all states.
	 */
	std::unordered_set<std::string> get_positive_finals() const;

	size_t get_number_of_disjuncts() const;

	static void parse_transition(IntermediateAut& aut, const mata::parser::BodyLine& tokens);
	void add_transition(const FormulaNode& lhs, const FormulaNode& symbol, FormulaGraphView rhs);
	void add_transition(const FormulaNode& lhs, const FormulaNode& rhs);
	void print_transitions_trees(std::ostream& os) const;
}; // class IntermediateAut.

} // namespace mata.

namespace std {
std::ostream& operator<<(std::ostream& os, const mata::IntermediateAut& inter_aut);
}

#endif // MATA_INTER_AUT_HH
