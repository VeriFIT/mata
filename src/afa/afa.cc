/* afa.cc -- operations for AFA
 *
 * Copyright (c) TODO TODO
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <algorithm>
#include <list>
#include <unordered_set>
#include <memory>
#include <queue>

// MATA headers
#include <mata/afa.hh>
#include <mata/nfa.hh>
#include <mata/util.hh>
#include <mata/closed-set.hh>

using std::tie;

using namespace Mata::util;
using namespace Mata::Afa;

const std::string Mata::Afa::TYPE_AFA = "AFA";

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::Trans& trans)
{ // {{{
	std::string result = "(" + std::to_string(trans.src) + ", " 
                       + std::to_string(trans.symb) + ", " 
                       + std::to_string(trans.dst) + ")";
	return os << result;
} // operator<<(ostream, Trans) }}}

/** This function adds a new transition to the automaton. It changes a transition
* relation. The transition is added to the transition relation it its current form.
* @brief adds a new transition
* @param trans a given transition
*/
void Afa::add_trans(const Trans& trans)
{ // {{{
	assert(trans.src < this->transitionrelation.size() && "It is not possible to perform a transition " &&
	"from non-existing state.");

	// If the corresponding transition already exists, given 'dst' will be added to
	// the transition
	for(auto & transVec : transitionrelation[trans.src])
	{
		if(transVec.symb == trans.symb)
		{
			// Before the dst nodes are added to the transition, we want to get rid
			// of redundant clauses. For example, in context of the formula
			// (1 || (1 && 2)), the clause (1 && 2) could be deleted
			auto cl = StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1, transVec.dst);
			cl.insert(trans.dst);
			transVec.dst = cl.antichain();	
			return;
		}
	}    

	// If there is no result, a new transition will be created
	transitionrelation[trans.src].push_back(trans);

} // add_trans }}}

/** This function gets a vector of all transitions which are possible to
* be performed from the given state.
* @brief gets all the transitions from the given state
* @param state a state of the automaton
* @return a vector of transitions
*/
std::vector<Trans> Afa::get_trans_from_state(State state) const
{
	assert(state < transitionrelation.size() &&
	"It is not possible to perform transitions from an non-existing state.");
	std::vector<Trans> result = std::vector<Trans>();
	for(auto transition : transitionrelation[state])
	{
		result.push_back(transition);
	}
	return result;
}

/** This function gets a vector of all transitions which are possible to
* be performed from the given state using the given symbol.
* If there is no such transition, the result will be an empty transition.
* @brief gets all the transitions from the given state
* @param state a state of the automaton
* @param symbol a symbol which could be used to perform a transition
* @return a corresponding transition
*/
Trans Afa::get_trans_from_state(State state, Symbol symbol) const
{
	assert(state < transitionrelation.size() &&
	"It is not possible to perform transitions from an non-existing state.");
	for(auto transition : transitionrelation[state])
	{
		if(transition.symb == symbol)
		{
			return transition;
		}
	}
	return Trans(state, symbol, Nodes());
}


/** This function adds a new inverse transition to the automaton
* It changes an inverse transition relation.
* @brief adds a new inverse transition
* @param trans a given TRANSITION (it will be inverted within this function)
*/
void Afa::add_inverse_trans(const Trans& trans)
{// {{{

	// Iterating through all the nodes which were given as a destination of the current transition
	for(auto node : trans.dst)
	{

		// If there is already a memory cell which corresponds to the current dst node and a
		// transition symbol, we have to find it and add the 'src' state to the corresponding
		// result_node vector. Let us recall that the inverse transition datatype is a vector
		// of vectors of tuples (symbol, vector_of_inverse_results). Each InverseResult is 
		// a tuple (result_node, precondition). If there exists such a tuple, where
		// the precondition corresponds to the current dst node, we can simply add the current
		// 'src' state to the result_node. Since the corresponding tuple (result_node,
		// precondition) would be identical for all states of the current node, we can store it
		// to the memory only once. For this purpose, we choose the minimal element from 
		// the current node. Then, it is sufficient to choose the minimal element of the dst
		// node and look if there exists such a tuple (result_node, precondition), where 
		// precondition == dst node in context of the current symbol.
		//
		// Example: We have transitions (0, a, {0, 1}), (0, b, {1}). 
		// The inverse transition data structure looks like this:
		//
		// 0 -> {('a', {(result_node:{0}, precondition:{0, 1})})}
		// 1 -> {('a', {}),
		//      ('b', {(result_node:{0}, precondition:{1})})}
		//
		// Now, we want to add the transition (1, a, {0, 1}), so we choose the minimal element
		// of the dst node {0, 1}, access the corresponding element of the inverse transition
		// relation and look if there exists a tuple (result_node, precondition),
		// where precondition == {0, 1}.  If so, we add 1 to the result_node. The result:
		//	
		// 0 -> {('a', {(result_node:{0, 1}, precondition:{0, 1})})}
		// 1 -> {('a', {}),
		//      ('b', {(result_node:{0}, precondition:{1})})}
		bool found = false;

		State storeTo = *(node.begin());

		auto inverseTransResults = perform_inverse_trans(storeTo, trans.symb);
		for(auto & result : inverseTransResults)
		{
			if(result.precondition == node)
			{
				result.result_node.insert(trans.src);
				found = true;
				break;
			}
		}

		if(found)
		{
			continue;
		}

		// Otherwise, we need to create a new tuple (result_node, precondition)
		// == ({src state}, node) and store it to the vector given by the minimal
		// state of the current node and the current symbol

		// If there is no tuple (symbol, vector_of_pointers) in context of the current
		// state and symbol because the symbol has not been used yet, we need to create it
		if(perform_inverse_trans(storeTo, trans.symb).empty())
		{
			inverseTransRelation[storeTo].push_back
			(InverseTrans(trans.src, trans.symb, InverseResults(trans.src, node)));
			continue;
		} 

		// Otherwise, we need to find the appropriate tuple (symbol, inverse_results)
		// and store the inverse result to the vector of inverse results
		for(auto & transVec : inverseTransRelation[storeTo])
		{
			if(transVec.symb == trans.symb)
			{
			    transVec.inverseResults.push_back(InverseResults(trans.src, node));
			    break;
			}
		}
	}

} // add_inverse_trans }}}


/** This function adds new state to the automaton.
* @brief adds new state
* @return id of the created state
*/
State Afa::add_new_state() {
    transitionrelation.emplace_back();
    inverseTransRelation.emplace_back();
    return transitionrelation.size() - 1;
}

//***************************************************
//
// POST
//
// Inspection of the automaton in the forward fashion
//
//***************************************************

/** This function takes a single state and a symbol and returns all the nodes
* which are accessible from the node {state} in one step using the given symbol. 
* The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes. The result is always an upward closed set!
* @param state a source state
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(State state, Symbol symb) const
{
	return StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1, 
	get_trans_from_state(state, symb).dst);
} // post }}}

/** This function takes a single node and a symbol and returns all the nodes
* which are accessible from the node in one step using the given symbol. 
* The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes. We will perform a transition
* for each state of the given node and then, we perform an intersection
* over these subresults. The result is always an upward closed set!
* @param node a source set of states
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(Node node, Symbol symb) const
{
	// initially, the result is empty
	StateClosedSet result = StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1);
	if(node.empty())
	{
		result.insert(node);
		return result;
	}

	// we get the first result without any changes and then, we will 
	// perform several intersections over it
	bool used = false;
	for(auto state : node)
	{
		if(!used)
		{
			result.insert(post(state, symb).antichain());
			used = true;
			continue;
		}
		result = result.intersection(post(state, symb));
	}
	return result;
} // post }}}

/** This function takes a whole set of nodes and a symbol and returns all the nodes
* which are accessible from the given nodes in one step using the given symbol.
* The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes. We will perform a transition for each node
* of the given set of nodes and then, we perform an union over these subresults.
* The result is always an upward closed set!
* @param nodes a source set of sets of states
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(Nodes nodes, Symbol symb) const
{
	// initially, the result is empty
	StateClosedSet result = StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1);
	for(auto node : nodes)
	{
		result.insert(post(node, symb).antichain());
	}
	return result;
} // post }}}

/** This function allows us to perform post directly over the closed set. 
* The output will be represented as a ClosedSet to omit the neccessity to 
* explicitly return all the proper nodes. We will perform a transition for
* each node of the antichain of the given set of nodes and then we perform
* an union over these subresults. The result is always an upward closed set!
* @param closed_set an upward closed set which will be used to perform a transition
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(StateClosedSet closed_set, Symbol symb) const
{
	assert(closed_set.type() == Mata::upward_closed_set && "The predicate transformer " && 
	" post can be computed only over upward-closed sets.");
	return post(closed_set.antichain(), symb);
} // post }}}


/** This function takes a single node and and returns all the nodes which are 
* accessible from the node in one step using any symbol. The output will be
* represented as a ClosedSet to omit the neccessity to explicitly return all 
* the proper nodes. We will perform a transition for each state of the given 
* node and then, we perform an intersection over these subresults.
* The result is always an upward closed set!
* @param node a source set of states
* @return closed set of nodes
*/
StateClosedSet Afa::post(Node node) const
{
	if(node.empty())
	{
		return StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1, Nodes{Node{}});
	}
	StateClosedSet result = StateClosedSet(upward_closed_set, 0, transitionrelation.size()-1);

	// It is sufficient to access the first element of the node
	// to collect all required symbols of the alphabet. If there is another symbol used
	// in context of another state of the node, it won't affect the result. The result for
	// such symbol will be empty since it is not used in context of all states
	// stored in the node
	for(auto transVec : transitionrelation[*(node.begin())])
	{
		result.insert(post(node, transVec.symb).antichain());
	}
	return result;
} // post }}}

/** This function allows us to perfom "post" and use the whole alphabet 
* to perform individual transitions. The output will be represented as 
* a ClosedSet to omit the neccessity to explicitly return all the proper nodes.
* We will perform a transition for each node of the antichain of the given set
* of nodes and then we perform an union over these subresults.
* The result is always an upward closed set!
* @param nodes a set of sets of nodes used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(Nodes nodes) const
{
	StateClosedSet result(upward_closed_set, 0, transitionrelation.size()-1);
	for(auto node : nodes)
	{
		result.insert(post(node).antichain());
	}
	return result;
} // post }}}


/** This function allows us to perfom "post" and use the whole alphabet 
* to perform individual transitions directly over the closed set. 
* The output will be represented as a ClosedSet to omit the neccessity to
* explicitly return all the proper nodes. We will perform a transition 
* for each node of the antichain of the given set
* of nodes and then we perform an union over these subresults.
* The result is always an upward closed set!
* @param nodes a set of sets of nodes used to perform a transition
* @return closed set of nodes
*/
StateClosedSet Afa::post(StateClosedSet closed_set) const 
{
	assert(closed_set.type() == Mata::upward_closed_set && "The predicate transformer " && 
	" post can be computed only over upward-closed sets.");
	return post(closed_set.antichain());
};

// The end of the definitions of the "post" functions

//***************************************************
//
// PRE
//
// Inspection of the automaton in the backward fashion
//
//***************************************************

/** This function inspects an inverse transition relation and returns a vector
* of inverse results according to the given source state and symbol.
* Otherwise, it gives us an empty vector which means that the result does not exist.
* @brief performs an inverse transition using a single state and symbol
* @param src a source state
* @param symb a symbol used to perform an inverse transition
* @return a vector of the inverse results
*/
std::vector<InverseResults> Afa::perform_inverse_trans(State src, Symbol symb) const
{
	for(auto element : this->inverseTransRelation[src])
	{
		if(element.symb == symb)
		{
		    return element.inverseResults;
		}
	}
	return std::vector<InverseResults>();
}  // perform_inverse_trans }}}

/** This function inspects an inverse transition relation and returns a vector
* of inverse results according to the given source states and symbol.
* Otherwise, it gives us an empty vector which means that the result does not exist.
* @brief performs an inverse transition using a single state and symbol
* @param node source states
* @param symb a symbol used to perform an inverse transition
* @return a vector of the inverse results
*/
std::vector<InverseResults> Afa::perform_inverse_trans(Node node, Symbol symb) const
{
	std::vector<InverseResults> result{};
	for(auto state : node)
	{
		auto subresult = perform_inverse_trans(state, symb);
		result.insert(result.end(), subresult.begin(), subresult.end());
	}
	return result;
} // perform_inverse_trans }}}

/** This function takes a single node and a symbol and returns all the nodes
* which are able to access the given node in one step using the given symbol.
* The output will be represented as ClosedSet to omit the neccessity
* to explicitly return all the proper nodes.
* The result is always a downward closed set!
* @param node source nodes
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
StateClosedSet Afa::pre(Node node, Symbol symb) const
{
	std::vector<InverseResults> candidates = perform_inverse_trans(node, symb);
	Node result{};
	for(auto candidate : candidates)
	{
		if(candidate.precondition.IsSubsetOf(node))
		{
			for(auto el : candidate.result_node)
			{
				result.insert(el);
			}
		}
	}
	return StateClosedSet(downward_closed_set, 0, transitionrelation.size()-1, result);
} // pre }}}

/** This function takes a set of nodes and a symbol and returns all the nodes
* which are able to access the given nodes in one step using the given symbol.
* The output will be represented as ClosedSet to omit the neccessity
* to explicitly return all the proper nodes. The result is always a downward closed set!
* @param nodes source nodes
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
StateClosedSet Afa::pre(Nodes nodes, Symbol symb) const
{
	StateClosedSet result(downward_closed_set, 0, transitionrelation.size()-1);
	for(auto node : nodes)
	{
		result = result.Union(pre(node, symb));
	}
	return result;
} // pre }}}

/** This function performs 'pre' directly over the given closed set
* and the given symbol. The result is always a downward closed set!
* @param closed_set a closed set
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
StateClosedSet Afa::pre(StateClosedSet closed_set, Symbol symb) const
{
	assert(closed_set.type() == downward_closed_set && "The predicate transformer " &&
	"pre can be computed only over downward-closed sets.");
	return pre(closed_set.antichain(), symb);
} // pre }}}

/** This function allows us to perfom pre over a node and use the whole
* alphabet to perform individual transitions. The result is always 
* a downward closed set!
* @param node a set of states
* @return closed set of nodes
*/
StateClosedSet Afa::pre(Node node) const
{
	if(node.empty())
	{
		return StateClosedSet(downward_closed_set, 0, transitionrelation.size()-1, Nodes{Node{}});
	}
	StateClosedSet result(downward_closed_set, 0, transitionrelation.size()-1);

	// It is sufficient to access the first element of the node
	// to collect all required symbols of the alphabet. If there is another symbol used
	// in context of another state of the node, it won't affect the result. The result for
	// such symbol will be empty since it is not used in context of all states
	// stored in the node
	for(auto transVec : inverseTransRelation[*(node.begin())])
	{
		result.insert(pre(node, transVec.symb).antichain());
	}
	return result;
} // pre }}}

/** This function allows us to perfom pre over a set of nodes and use
* the whole alphabet to perform individual transitions 
* The result is always a downward closed set!
* @param nodes a set of sets of states
* @return closed set of nodes
*/
StateClosedSet Afa::pre(Nodes nodes) const
{
	StateClosedSet result(downward_closed_set, 0, transitionrelation.size()-1);
	for(auto node : nodes)
	{
		result.insert(pre(node).antichain());
	}
	return result;
} // pre }}}

// The end of the definitions of the "pre" functions
////////////////////////////////////////////////////

bool Afa::has_trans(const Trans& trans) const
{ // {{{
	Nodes res = get_trans_from_state(trans.src, trans.symb).dst;
	if(res.size() && res.IsSubsetOf(trans.dst))
	{
		return true;
	}
	return false;
} // has_trans }}}


size_t Afa::trans_size() const
{ // {{{
	size_t result = 0;
	for(auto state : transitionrelation)
	{
		result += state.size();
	}
	return result;
} // trans_size() }}}

/** This function returns an upward-closed set of all
* the nodes which are non-final
* @return closed set of non-final nodes
*/
StateClosedSet Afa::get_non_final_nodes(void) const
{
	StateClosedSet result(upward_closed_set, 0, transitionrelation.size()-1);
	auto transSize = transitionrelation.size();
	for(State state = 0; state < transSize; ++state)
	{
		if(!has_final(state))
		{
			result.insert(state);
		}    
	}
	return result;
} // get_non_final_nodes() }}}


std::ostream& Mata::Afa::operator<<(std::ostream& os, const Afa& afa)
{ // {{{
	return os << std::to_string(serialize(afa));
} // Nfa::operator<<(ostream) }}}


bool Mata::Afa::are_state_disjoint(const Afa& lhs, const Afa& rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // are_disjoint }}}


void Mata::Afa::union_norename(
	Afa*        result,
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
	assert(nullptr != result);

  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // union_norename }}}


Afa Mata::Afa::union_rename(
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // union_rename }}}


bool Mata::Afa::is_lang_empty(const Afa& aut, Path* cex)
{ // {{{
  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty }}}


bool Mata::Afa::is_lang_empty_cex(const Afa& aut, Word* cex)
{ // {{{
	assert(nullptr != cex);

  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty_cex }}}

/** This function decides whether the given automaton is empty using 
* an antichain-based emptiness test working in the concrete domain
* in the forward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_forward_emptiness_test_old(const Afa& aut)
{
    // We will iteratively build a set of reachable nodes (next) until
	// we reach a fixed point or until we find out that there exists
	// a final node which is reachable (is not part of goal).
    // We will perform each operation directly over antichains.
    // Note that the fixed point always exists so the while loop always terminates.
    StateClosedSet goal = aut.get_non_final_nodes();
    StateClosedSet current = StateClosedSet(upward_closed_set, 0, aut.get_num_of_states()-1);
    StateClosedSet next = aut.get_initial_nodes();

    while(current != next)
    {
		current = next;
		next = current.Union(aut.post(current));
		if(!(next <= goal)) // inclusion test
		{
		    return false;
		}
    }
    return true;
}

/** This function decides whether the given automaton is empty using 
* an antichain-based emptiness test working in the concrete domain
* in the forward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_forward_emptiness_test_new(const Afa& aut)
{
	StateClosedSet goal = aut.get_non_final_nodes();
	StateClosedSet result = aut.get_initial_nodes();
	std::set<Node> processed = std::set<Node>();
	std::vector<Node> worklist = std::vector<Node>();
	for(Node node : aut.get_initial_nodes().antichain())
	{
		worklist.push_back(node);
	}

	if(!(result <= goal))
	{
		return false;
	}

	while(!worklist.empty()) 
	{
		Node current = worklist.back();
		worklist.pop_back();
		auto post_current = aut.post(current);
		result = result.Union(post_current);
		for(Node node : post_current.antichain())
		{
			if(!goal.contains(node))
			{
			    return false;
			}
			if(!processed.count(node))
			{
			    worklist.push_back(node);
			}
		}
		processed.insert(current);
	}
	return true;
}

/** This function decides whether the given automaton is empty using 
* an antichain-based emptiness test working in the concrete domain
* in the backward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_backward_emptiness_test_old(const Afa& aut)
{
	// We will iteratively build a set of terminating nodes (next)
	// until we reach a fixed point or until we find out that there exists
	// an initial node which is terminating (is not part of goal).
	// We will perform each operation directly over antichains
	// Note that the fixed point always exists so the while loop always terminates
	StateClosedSet goal = aut.get_non_initial_nodes();
	StateClosedSet current = StateClosedSet(downward_closed_set, 0, aut.get_num_of_states()-1);
	StateClosedSet next = aut.get_final_nodes();

	while(current != next)
	{
		current = next;
		next = current.Union(aut.pre(current));
		if(!(next <= goal))
		{
			return false;
		}
	}
	return true;
}


/** This function decides whether the given automaton is empty using 
* an antichain-based emptiness test working in the concrete domain
* in the backward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_backward_emptiness_test_new(const Afa& aut)
{
	StateClosedSet goal = aut.get_non_initial_nodes();
	StateClosedSet result = aut.get_final_nodes();
	std::set<Node> processed = std::set<Node>();
	std::vector<Node> worklist = std::vector<Node>();
	for(Node node : aut.get_final_nodes().antichain())
	{
		worklist.push_back(node);
	}

	if(!(result <= goal))
	{
		return false;
	}

	while(!worklist.empty()) 
	{
		Node current = worklist.back();
		worklist.pop_back();
		auto pre_current = aut.pre(current);
		result = result.Union(pre_current);
		for(Node node : pre_current.antichain())
		{
			if(!goal.contains(node))
			{
				return false;
			}
			if(!processed.count(node))
			{
				worklist.push_back(node);
			}
		}
		processed.insert(current);
	}
	return true;
}


void Mata::Afa::make_complete(
	Afa*             aut,
	const Alphabet&  alphabet,
	State            sink_state)
{ // {{{
	assert(nullptr != aut);

  assert(&alphabet);
  assert(&sink_state);

  // TODO
  assert(false);
} // make_complete }}}


Mata::Parser::ParsedSection Mata::Afa::serialize(
	const Afa&                aut,
	const SymbolToStringMap*  symbol_map,
	const StateToStringMap*   state_map)
{ // {{{
	Mata::Parser::ParsedSection parsec;
	parsec.type = Mata::Afa::TYPE_AFA;

	using bool_str_pair = std::pair<bool, std::string>;

	std::function<bool_str_pair(State)> state_namer = nullptr;
	if (nullptr == state_map)
	{
		state_namer = [](State st) -> auto {
			return bool_str_pair(true, "q" + std::to_string(st));
		};
	}
	else
	{
		state_namer = [&state_map](State st) -> auto {
			auto it = state_map->find(st);
			if (it != state_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	std::function<bool_str_pair(Symbol)> symbol_namer = nullptr;
	if (nullptr == symbol_map) {
		symbol_namer = [](Symbol sym) -> auto {
			return bool_str_pair(true, "a" + std::to_string(sym));
		};
	} else {
		symbol_namer = [&symbol_map](Symbol sym) -> auto {
			auto it = symbol_map->find(sym);
			if (it != symbol_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	// construct initial states
	std::vector<std::string> init_states;
	for (const Node& node : aut.initialstates) {
		std::string init_res = "( ";
		bool first = true;
		for (State s : node) {
			bool_str_pair bsp = state_namer(s);
			if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
			if(!first) {init_res += " & ";}
			init_res += bsp.second;
			first = false;		
		}
		init_res += " )";
		init_states.push_back(init_res);
	}
	parsec.dict["Initial"] = init_states;

	// construct final states
	std::vector<std::string> fin_states;
	for (State s : aut.finalstates) {
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		fin_states.push_back(bsp.second);
	}
	parsec.dict["Final"] = fin_states;

    // TODO: transitions serialization

	return parsec;
} // serialize }}}


void Mata::Afa::revert(Afa* result, const Afa& aut)
{ // {{{
	assert(nullptr != result);

  assert(&aut);

  // TODO
  assert(false);
} // revert }}}


void Mata::Afa::remove_epsilon(Afa* result, const Afa& aut, Symbol epsilon)
{ // {{{
	assert(nullptr != result);

  assert(&aut);
  assert(&epsilon);

  // TODO
  assert(false);
} // remove_epsilon }}}


void Mata::Afa::minimize(
	Afa*               result,
	const Afa&         aut,
	const StringDict&  params)
{ // {{{
	assert(nullptr != result);

  assert(&aut);
	assert(&params);

  // TODO
  assert(false);
} // minimize }}}

// TODO this function should the same thing as the one taking IntermediateAut or be deleted
Afa Mata::Afa::construct(
	const Mata::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != alphabet);
	Afa aut;

	if (parsec.type != Mata::Afa::TYPE_AFA) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 Mata::Afa::TYPE_AFA + "\"");
	}

	bool remove_state_map = false;
	if (nullptr == state_map) {
		state_map = new StringToStateMap();
		remove_state_map = true;
	}

	State cnt_state = 0;

	// a lambda for translating state names to identifiers
	auto get_state_name = [state_map, &cnt_state](const std::string& str) {
		auto it_insert_pair = state_map->insert({str, cnt_state});
		if (it_insert_pair.second) { return cnt_state++; }
		else { return it_insert_pair.first->second; }
	};

	// a lambda for cleanup
	auto clean_up = [&]() {
		if (remove_state_map) { delete state_map; }
	};


	auto it = parsec.dict.find("Initial");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			State state = get_state_name(str);
			aut.initialstates.insert(Node{state});
		}
	}


	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			State state = get_state_name(str);
			aut.finalstates.insert(state);
		}
	}

	for (const auto& body_line : parsec.body) {
		if (body_line.size() < 2) {
			// clean up
      clean_up();

      throw std::runtime_error("Invalid transition: " +
        std::to_string(body_line));
		}

		State src_state = get_state_name(body_line[0]);
    std::string formula;
    for (size_t i = 1; i < body_line.size(); ++i) {
      formula += body_line[i] + " ";
    }

	// TODO: Transform a positive Boolean formula from the string format
	// to the inner representation (src state, symbol on transition, ordered
	// vector of ordered vectors of states which corresponds to the formula in DNF) 
	// Call the "add_trans" and "add_inverse_trans" functions over the
	// parsed formula to add it do the automaton

	}

	// do the dishes and take out garbage
	clean_up();

    return aut;
} // construct }}}

Afa Mata::Afa::construct(
        const Mata::IntermediateAut&         inter_aut,
        Alphabet*                            alphabet,
        StringToStateMap*                    state_map)
{ // {{{
    Afa aut;
    assert(nullptr != alphabet);

    if (!inter_aut.is_afa()) {
        throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 Mata::Afa::TYPE_AFA + "\"");
    }

    StringToStateMap tmp_state_map;
    if (nullptr == state_map) {
        state_map = &tmp_state_map;
    }

    // a lambda for translating state names to identifiers
    auto get_state_name = [&state_map, &aut](const std::string& str) {
        if (!state_map->count(str)) {
            State state = aut.add_new_state();
            state_map->insert({str, state});
            return state;
        } else {
            return (*state_map)[str];
        }
    };

    // lambda returning true if node is operator of given type
    auto is_node_operator =
            [](const FormulaNode& node, FormulaNode::OperatorType type) -> bool {
        return node.is_operator() && node.operator_type == type;
    };

    // lambda creates Node from set of strings which are state names
    auto create_node = [get_state_name](
            const std::unordered_set<std::string>& states_names) -> Node {
        Node tgt_node;
        for (const std::string& s : states_names)
        {
            tgt_node.insert(get_state_name(s));
        }

        return tgt_node;
    };


    const FormulaGraph* init_graph = &inter_aut.initial_formula;
    if (is_node_operator(init_graph->node, FormulaNode::AND)) { // initial formula is just conjunction
        for (const auto& str : init_graph->collect_node_names())
        {
            State state = get_state_name(str);
            aut.add_initial(state);
        }
    } else { // initial formula is dnf
        while (is_node_operator(init_graph->node, FormulaNode::OR))
        {  // Processes each clause separately
            assert(init_graph->children[1].node.is_operand() ||
                   is_node_operator(init_graph->children[1].node, FormulaNode::AND) ||
                   "Clause should be conjunction or single state");
            // Conjunction is the right son of initent node
            Node initial_node;
            for (const auto s : init_graph->children[1].collect_node_names())
                initial_node.insert(get_state_name(s));
            aut.add_initial(initial_node);

            // jump to another clause which is the left son of initent node
            init_graph = &init_graph->children.front();
        }
        assert(init_graph->node.is_operand() ||
               is_node_operator(init_graph->node, FormulaNode::AND) ||
                       "Remaining clause should be conjunction or single element");
        Node initial_node;
        for (const auto s : init_graph->collect_node_names())
            initial_node.insert(get_state_name(s));
        aut.add_initial(initial_node);
    }

    for (const auto& trans : inter_aut.transitions)
    {
        State src_state = get_state_name(trans.first.name);
        if (trans.second.node.is_operand() && trans.second.node.operand_type == FormulaNode::SYMBOL)
        {
            Symbol symbol = alphabet->translate_symb(trans.second.node.name);
            aut.add_trans(src_state, symbol, Node());
            continue;
        }
        else if (trans.second.children.size() != 2)
        {
            if (trans.second.children.size() == 1)
            {
                throw std::runtime_error("Epsilon transitions not supported");
            }
            else
            {
                throw std::runtime_error("Invalid transition");
            }
        }

        assert(is_node_operator(trans.second.node, FormulaNode::AND) ||
            "Clause of DNF should be conjunction");
        assert(trans.second.children.front().node.is_operand() || "Node in conjunction should be operand");
        Symbol symbol = alphabet->translate_symb(trans.second.children.front().node.name);

        const FormulaGraph* curr_graph = &trans.second.children[1];

        while (is_node_operator(curr_graph->node, FormulaNode::OR))
        {  // Processes each clause separately
            assert(curr_graph->children[1].node.is_operand() ||
                   is_node_operator(curr_graph->children[1].node, FormulaNode::AND) ||
                   "Clause should be conjunction");
            // Conjunction is the right son of current node
            aut.add_trans(src_state, symbol,
                          create_node(curr_graph->children[1].collect_node_names()));

            // jump to another clause which is the left son of current node
            curr_graph = &curr_graph->children.front();
        }

        // process remaining conjunction
        assert(curr_graph->node.is_operand() ||
               is_node_operator(curr_graph->node, FormulaNode::AND) ||
               "Remaining clause should be conjunction");
        aut.add_trans(src_state, symbol,
                      create_node(curr_graph->collect_node_names()));
    }

	// TODO final states can be also given as true/false
	if (inter_aut.are_final_states_conjunction_of_negation()) {
		// final states are given as a conjunction of non-final states
		auto non_final_states = inter_aut.final_formula.collect_node_names();
		for (const auto &state_name_and_number : *state_map) {
			if (!non_final_states.count(state_name_and_number.first)) {
				aut.finalstates.insert(state_name_and_number.second);
			}
		}
	} else {
		// final states are given normally
		for (const auto& str : inter_aut.final_formula.collect_node_names())
		{
			State state = get_state_name(str);
			aut.finalstates.insert(state);
		}
	}
    return aut;
} // construct }}}

bool Mata::Afa::is_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_in_lang }}}


bool Mata::Afa::is_prfx_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_prfx_in_lang }}}


bool Mata::Afa::is_deterministic(const Afa& aut)
{ // {{{
  assert(&aut);

  // TODO
  assert(false);
} // is_deterministic }}}


bool Mata::Afa::is_complete(const Afa& aut, const Alphabet& alphabet)
{ // {{{
  assert(&aut);
  assert(&alphabet);

  // TODO
  assert(false);
} // is_complete }}}

bool Mata::Afa::accepts_epsilon(const Afa& aut)
{ // {{{
	for (const Node &node : aut.initialstates) {
		if(node.IsSubsetOf(aut.finalstates))
		{
			return true;
		}
	}

	return false;
} // accepts_epsilon }}}

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::AfaWrapper& afa_wrap)
{ // {{{
	os << "{AFA wrapper|AFA: " << afa_wrap.afa << "|alphabet: " << afa_wrap.alphabet <<
		"|state_dict: " << std::to_string(afa_wrap.state_dict) << "}";
	return os;
} // operator<<(AfaWrapper) }}}

