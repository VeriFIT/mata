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

// MATA headers
#include <mata/afa.hh>
#include <mata/nfa.hh>
#include <mata/util.hh>
#include <mata/closed_set.hh>

using std::tie;

using namespace Mata::util;
using namespace Mata::Afa;
using Mata::Afa::Symbol;

const std::string Mata::Afa::TYPE_AFA = "AFA";

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::Trans& trans)
{ // {{{
	std::string result = "(" + std::to_string(trans.src) + ", " 
                       + std::to_string(trans.symb) + ", " 
                       + std::to_string(trans.dst) + ")";
	return os << result;
} // operator<<(ostream, Trans) }}}

/** This function adds a new transition to the automaton. It changes a transition
* relation. 
* @brief adds a new transition
* @param trans a given transition
*/
void Afa::add_trans(const Trans& trans)
{ // {{{
    assert(!this->transRelation.empty());

    // Performs a transition using given src state and symbol
    // in context of transitions which have been already added to the automaton
    // If there is no result, a new transition will be created
    // If the corresponding transition already exists, given 'dst' will be added to
    // the transition
    auto nodesPtr = perform_trans(trans.src, trans.symb);
    if(!nodesPtr->empty())
    {
        // TODO: It would be nice to remove redundant clauses (like (1 || (1 && 2)))
        // The clause (1 && 2) could be deleted. Solution:
        // create closed set -> add the new transition -> extract the antichain ->
        // store it to the transition (???). Working solution:
        //        
        // auto cl = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, max_state_used, *nodesPtr);
        // cl.insert(trans.dst);
        // *nodesPtr = cl.get_antichain();
        //        
        // Is it worth it?         

        *nodesPtr = nodesPtr->Union(trans.dst);
        return;
    }
    transRelation[trans.src].push_back(trans);

} // add_trans }}}


/** This function adds a new inverse transition to the automaton
* It changes an inverse transition relation.
* @brief adds a new inverse transition
* @param trans a given TRANSITION (it will be inverted within this function)
*/
void Afa::add_inv_trans(const Trans& trans)
{// {{{

    // Iterating through all the nodes which were given as a destination of the current transition
    for(auto node : trans.dst)
    {
        // TODO: Could this ever happen?
        if(node.empty())
        {
            continue;
        }

        // If there is already a memory cell which corresponds to the current dst node and a transition symbol,
        // we have to find it and add the 'src' state to the corresponding result_nodes vector. 
        // Let us recall that the inverse transition datatype is a vector of vectors of tuples (symbol, vector_of_pointers)
        // Each mentioned pointer points to the tuple (result_nodes, sharing_list). If there exists such a tuple, where
        // the sharing_list corresponds to the current dst node, we can simply add the current 'src' state to the result_nodes.
        // Since the corresponding tuple (result_nodes, sharing_list) must be referred from all the vector_of_pointers, where
        // symbol == trans.symbol, it is sufficient to choose the first element of the dst node and look if there exists such a
        // tuple (result_nodes, sharing_list), where sharing_list == dst node in context of the current symbol
        //
        // Example: We have transitions (0, a, {0, 1}), (0, b, {1}). The inverse transition data structure looks like this:
        //
        // 0 -> {('a', {&(result_nodes:{0}, sharing_list:{0, 1})})}
        // 1 -> {('a', {&(result_nodes:{0}, sharing_list:{0, 1})}),
        //      ('b', {&(result_nodes:{0}, sharing_list:{1})})}
        //
        // Now, we want to add the transition (1, a, {0, 1}), so we choose only one element of the dst node (0 or 1) and look if there exists
        // a tuple (result_nodes, sharing_list), where sharing_list == {0, 1}. If so, we add 0 to the result_nodes. 
        bool found = false;

        auto invTransPtrs = perform_inv_trans(*(node.begin()), trans.symb);
        for(auto result : invTransPtrs)
        {
            if(result->sharing_list == node)
            {
                result->result_nodes.insert(trans.src);
                found = true;
                break;
            }
        }

        if(found)
        {
            continue;
        }

        // Otherwise, we need to create a new tuple (result_nodes, sharing_list) == ({src state}, node) and ensure us
        // that the pointer to this tuple will be part of all the vectors_of_pointers which correspond to the (dst_state, symbol)
        // where dst_state is an element of the current node
        auto result = std::make_shared<InvResults>(InvResults(trans.src, node));

        for(auto state : node)
        {
            // If there is no tuple (symbol, vector_of_pointers) in context of the current state and symbol because the
            // symbol has not been used yet, we need to create it
            if(perform_inv_trans(state, trans.symb).empty())
            {
                invTransRelation[state].push_back(InvTrans(trans.symb, InvTrans::InvResultPtrs{result}));
                continue;
            } 

            // Otherwise, we need to find the appropriate tuple (symbol vector_of_pointers) and store the pointer
            // to the vector of pointers
            for(auto & transVec : invTransRelation[state])
            {
                if(transVec.symb == trans.symb)
                {
                    transVec.invResultPtrs.push_back(std::make_shared<InvResults>(trans.src, node));
                    break;
                }
            }
        }
    }

} // add_inv_trans }}}

//***************************************************
//
// POST
//
// Inspection of the automaton in the forward fashion
//
//***************************************************

/** This function inspects a transition relation and returns a pointer to the proper result for delta(src, symb)
* Otherwise, it gives us a pointer to the empty Nodes which means that the result does not exist. We return the pointer
* to be able to directly change the transition relation if there is a neccessity to add a new transition 
* @brief performs a transition using a single state and symbol
* @param src a source state
* @param symb a symbol used to perform a transition
* @return a pointer to the set of nodes
*/
std::unique_ptr<Nodes> Afa::perform_trans(State src, Symbol symb) const
{
    assert(src < this->transRelation.size());
    for(auto transVec : transRelation[src])
    {
        if(transVec.symb == symb)
        {
            return std::make_unique<Nodes>(transVec.dst);
        }
    }    
    return std::make_unique<Nodes>(Nodes());
} // perform_trans }}}

/** This function takes a single state and a symbol and returns all the nodes which are accessible from the node {state}
* in one step using the given symbol. The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* The result is always an upward closed set!
* @param state a source state
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(State state, Symbol symb) const
{
    assert(state < this->transRelation.size());
    Nodes result = *perform_trans(state, symb);
    if(result.size())
    {
        return ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1, result);
    }   
    // if there is no proper result, we will return an empty closed set
    return ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
} // post }}}

/** This function takes a single node and a symbol and returns all the nodes which are accessible from the node
* in one step using the given symbol. The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* We will perform a transition for each state of the given node and then, we perform an intersection
* over these subresults
* The result is always an upward closed set!
* @param node a source set of states
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(Node node, Symbol symb) const
{
    // initially, the result is empty
    ClosedSet<State> result = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    if(node.empty())
    {
        result.insert(node);
        return result;
    }

    // we get the first result without any changes and then, we will perform several intersections
    // over it
    bool used = false;
    for(auto state : node)
    {
        assert(state < this->transRelation.size());
        if(!used)
        {
            result.insert(post(state, symb).get_antichain());
            used = true;
            continue;
        }
        result = result.intersection(post(state, symb));
    }
    return result;
} // post }}}

/** This function takes a whole set of nodes and a symbol and returns all the nodes which are accessible from the
* given nodes in one step using the given symbol. The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* We will perform a transition for each node of the given set of nodes and then, we perform an union
* over these subresults
* The result is always an upward closed set!
* @param nodes a source set of sets of states
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(Nodes nodes, Symbol symb) const
{
    // initially, the result is empty
    ClosedSet<State> result = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    for(auto node : nodes)
    {
        result.insert(post(node, symb).get_antichain());
    }
    return result;
} // post }}}

/** This function allows us to perform post directly over the closed set. The output will be represented
* as a ClosedSet to omit the neccessity to explicitly return all the proper nodes
* We will perform a transition for each node of the antichain of the given set of nodes and then
* we perform an union over these subresults
* The result is always an upward closed set!
* @param closed_set an upward closed set which will be used to perform a transition
* @param symb a symbol used to perform a transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(ClosedSet<State> closed_set, Symbol symb) const
{
    assert(closed_set.get_type() == ClosedSet<State>::upward_closed);
    return post(closed_set.get_antichain(), symb);
} // post }}}


/** This function takes a single node and and returns all the nodes which are accessible from the node
* in one step using any symbol. The output will be represented as a ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* We will perform a transition for each state of the given node and then, we perform an intersection
* over these subresults
* The result is always an upward closed set!
* @param node a source set of states
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(Node node) const
{
    if(node.empty())
    {
        return ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1, Nodes{Node{}});
    }
    ClosedSet<State> result = ClosedSet<State>(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    for(auto transVec : transRelation[*(node.begin())])
    {
        result.insert(post(node, transVec.symb).get_antichain());
    }
    return result;
} // post }}}

/** This function allows us to perfom "post" and use the whole alphabet to perform individual transitions 
* The output will be represented as a ClosedSet to omit the neccessity to explicitly return all the proper nodes
* We will perform a transition for each node of the antichain of the given set of nodes and then
* we perform an union over these subresults
* The result is always an upward closed set!
* @param nodes a set of sets of nodes used to perform a transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::post(Nodes nodes) const
{
    ClosedSet<State> result(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    for(auto node : nodes)
    {
        result.insert(post(node).get_antichain());
    }
    return result;
} // post }}}

// The end of the definitions of the "post" functions

//***************************************************
//
// PRE
//
// Inspection of the automaton in the backward fashion
//
//***************************************************

/** This function inspects an inverse transition relation and returns a vector of pointers
* according to the given source state and symbol
* Otherwise, it gives us an empty list which means that the result does not exist. We return pointers
* to be able to directly change the inverse transition relation if there is a neccessity to add a new transition 
* @brief performs a transition using a single state and symbol
* @param src a source state
* @param symb a symbol used to perform an inverse transition
* @return a vector of pointers to the inverse results
*/
InvTrans::InvResultPtrs Afa::perform_inv_trans(State src, Symbol symb) const
{
    for(auto element : this->invTransRelation[src])
    {
        if(element.symb == symb)
        {
            return element.invResultPtrs;
        }
    }
    return InvTrans::InvResultPtrs();
}  // perform_inv_trans }}}

/** This function inspects an inverse transition relation and returns a vector of pointers
* according to the given source states and symbol
* Otherwise, it gives us an empty list which means that the result does not exist. We return pointers
* to be able to directly change the inverse transition relation if there is a neccessity to add a new transition 
* @brief performs an inverse transition using a single state and symbol
* @param node source states
* @param symb a symbol used to perform an inverse transition
* @return a vector of pointers to the inverse results
*/
InvTrans::InvResultPtrs Afa::perform_inv_trans(Node node, Symbol symb) const
{
    InvTrans::InvResultPtrs result{};
    bool used = false;
    for(auto state : node)
    {
        if(!used)
        {
            result = perform_inv_trans(state, symb);
            used = true;
        }
        else
        {
            auto subresult = perform_inv_trans(state, symb);
            result.insert(result.end(), subresult.begin(), subresult.end());
        }
    }
    return result;
} // perform_inv_trans }}}

/** This function takes a single node and a symbol and returns all the nodes which are able to access the given node
* in one step using the given symbol. The output will be represented as ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* The result is always a downward closed set!
* @param node source nodes
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::pre(Node node, Symbol symb) const
{
    InvTrans::InvResultPtrs ptrs = perform_inv_trans(node, symb);
    Node result{};
    for(auto ptr : ptrs)
    {
        if(ptr->sharing_list.IsSubsetOf(node))
        {
            for(auto el : ptr->result_nodes)
            {
                result.insert(el);
            }
        }
    }
    return ClosedSet<State>(ClosedSet<State>::downward_closed, 0, transRelation.size()-1, result);
} // pre }}}

/** This function takes set of nodes and a symbol and returns all the nodes which are able to access the given nodes
* in one step using the given symbol. The output will be represented as ClosedSet to omit the neccessity
* to explicitly return all the proper nodes
* The result is always a downward closed set!
* @param nodes source nodes
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::pre(Nodes nodes, Symbol symb) const
{
    ClosedSet<State> result(ClosedSet<State>::downward_closed, 0, transRelation.size()-1);
    for(auto node : nodes)
    {
        result = result.Union(pre(node, symb));
    }
    return result;
} // pre }}}

/** This function performs pre directly over the given closed set and the given symbol
* The result is always a downward closed set!
* @param closed_set a closed set
* @param symb a symbol used to perform an inverse transition
* @return closed set of nodes
*/
ClosedSet<State> Afa::pre(ClosedSet<State> closed_set, Symbol symb) const
{
    assert(closed_set.get_type() == ClosedSet<State>::downward_closed);
    return pre(closed_set.get_antichain(), symb);
} // pre }}}

/** This function allows us to perfom pre over a node and use the whole alphabet to perform individual transitions 
* The result is always a downward closed set!
* @param node a set of states
* @return closed set of nodes
*/
ClosedSet<State> Afa::pre(Node node) const
{
    if(node.empty())
    {
        return ClosedSet<State>(ClosedSet<State>::downward_closed, 0, transRelation.size()-1, Nodes{Node{}});
    }
    ClosedSet<State> result(ClosedSet<State>::downward_closed, 0, transRelation.size()-1);
    for(auto transVec : invTransRelation[*(node.begin())])
    {
        result.insert(pre(node, transVec.symb).get_antichain());
    }
    return result;
} // pre }}}

/** This function allows us to perfom pre over a set of nodes and use the whole alphabet to perform individual transitions 
* The result is always a downward closed set!
* @param nodes a set of sets of states
* @return closed set of nodes
*/
ClosedSet<State> Afa::pre(Nodes nodes) const
{
    ClosedSet<State> result(ClosedSet<State>::downward_closed, 0, transRelation.size()-1);
    for(auto node : nodes)
    {
        result.insert(pre(node).get_antichain());
    }
    return result;
} // pre }}}

// The end of the definitions of the "pre" functions
////////////////////////////////////////////////////

bool Afa::has_trans(const Trans& trans) const
{ // {{{
    Nodes res = *perform_trans(trans.src, trans.symb);
    if(res.size() && res.IsSubsetOf(trans.dst))
    {
        return true;
    }
    return false;
} // has_trans }}}


size_t Afa::trans_size() const
{ // {{{
    size_t result = 0;
    for(auto state : transRelation)
    {
        result += state.size();
    }
    return result;
} // trans_size() }}}

/** This function takes all the initial states and creates a corresponding upward-closed set
* @return closed set of initial nodes
*/
ClosedSet<State> Afa::get_initial_nodes(void) const
{
    ClosedSet<State> result(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    for(State state = 0; state < transRelation.size(); ++state)
    {
        if(has_initial(state))
        {
            result.insert(state);
        }    
    }
    return result;
} // get_initial_nodes() }}}

/** This function returns a downward-closed set of all the nodes which are not initial
* @return closed set of non-initial nodes
*/
ClosedSet<State> Afa::get_non_initial_nodes(void) const
{
    OrdVec<State> subresult{};
    for(State state = 0; state < transRelation.size(); ++state)
    {
        if(!has_initial(state))
        {
            subresult.insert(state);
        }  
    }
    return ClosedSet<State>(ClosedSet<State>::downward_closed, 0, transRelation.size()-1, subresult);
} // get_non_initial_nodes() }}}

/** This function returns a downward-closed set of all the nodes which are final
* @return closed set of final nodes
*/
ClosedSet<State> Afa::get_final_nodes(void) const
{
    OrdVec<State> subresult{};
    for(State state = 0; state < transRelation.size(); ++state)
    {
        if(has_final(state))
        {
            subresult.insert(state);
        }  
    }
    return ClosedSet<State>(ClosedSet<State>::downward_closed, 0, transRelation.size()-1, subresult);
} // get_final_nodes() }}}

/** This function returns an upward-closed set of all the nodes which are non-final
* @return closed set of non-final nodes
*/
ClosedSet<State> Afa::get_non_final_nodes(void) const
{
    ClosedSet<State> result(ClosedSet<State>::upward_closed, 0, transRelation.size()-1);
    for(State state = 0; state < transRelation.size(); ++state)
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
* a antichain-based emptiness test working in the concrete domain
* in the forward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_forward_emptiness_test(const Afa& aut)
{
    // We will iteratively build a set of reachable nodes (next) until we reach a fixed
    // point or until we find out that there exists a final node which is reachable (is not part of goal).
    // We will perform each operation directly over antichains
    // Note that the fixed point always exists so the while loop always terminates
    ClosedSet<Mata::Afa::State> goal = aut.get_non_final_nodes();
    ClosedSet<Mata::Afa::State> current = ClosedSet<Mata::Afa::State>();
    ClosedSet<Mata::Afa::State> next = aut.get_initial_nodes();

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
* a antichain-based emptiness test working in the concrete domain
* in the backward fashion
* @param aut a given automaton
* @return true iff the automaton is empty
*/
bool Mata::Afa::antichain_concrete_backward_emptiness_test(const Afa& aut)
{
    // We will iteratively build a set of terminating nodes (next) until we reach a fixed
    // point or until we find out that there exists a initial node which is terminating (is not part of goal).
    // We will perform each operation directly over antichains
    // Note that the fixed point always exists so the while loop always terminates
    ClosedSet<Mata::Afa::State> goal = aut.get_non_initial_nodes();
    ClosedSet<Mata::Afa::State> current = ClosedSet<Mata::Afa::State>();
    ClosedSet<Mata::Afa::State> next = aut.get_final_nodes();

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
	for (State s : aut.initialstates) {
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		init_states.push_back(bsp.second);
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

	/*for (const auto& trans : aut.transitions) {
		bool_str_pair src_bsp = state_namer(trans.src);
		bool_str_pair symb_bsp = symbol_namer(trans.src);
		if (!src_bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(trans.src)); }

		parsec.body.push_back({ src_bsp.second, symb_bsp.second, trans.dst });
	}*/ // TODO

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


void Mata::Afa::construct(
	Afa*                                 aut,
	const Mata::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);
	assert(nullptr != alphabet);

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
			aut->initialstates.insert(state);
		}
	}


	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			State state = get_state_name(str);
			aut->finalstates.insert(state);
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

    //aut->add_trans(src_state, formula);
    //TODO
	}

	// do the dishes and take out garbage
	clean_up();
} // construct }}}


void Mata::Afa::construct(
	Afa*                                 aut,
	const Mata::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);

	bool remove_symbol_map = false;
	if (nullptr == symbol_map)
	{
		symbol_map = new StringToSymbolMap();
		remove_symbol_map = true;
	}

	auto release_res = [&](){ if (remove_symbol_map) delete symbol_map; };

  Mata::Nfa::OnTheFlyAlphabet alphabet(*symbol_map);

	try
	{
		construct(aut, parsec, &alphabet, state_map);
	}
	catch (std::exception&)
	{
		release_res();
		throw;
	}

	release_res();
} // construct(StringToSymbolMap) }}}


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
	for (State st : aut.initialstates) {
		if (haskey(aut.finalstates, st)) return true;
	}

	return false;
} // accepts_epsilon }}}

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::AfaWrapper& afa_wrap)
{ // {{{
	os << "{AFA wrapper|AFA: " << afa_wrap.afa << "|alphabet: " << afa_wrap.alphabet <<
		"|state_dict: " << std::to_string(afa_wrap.state_dict) << "}";
	return os;
} // operator<<(AfaWrapper) }}}

