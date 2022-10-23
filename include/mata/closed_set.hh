/* closed_set.hh --- downward and upward closed sets
 *
 * Copyright (c) TODO
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

/** @file closed_set.hh
 *  @brief Definition of a closed set
 *
 *  This file contains definition of a closed set and function which
 *  allow us to work with them (membership, inclusion, union, intersection)
 *
 *
 * Description:
 * In this context, an upward-closed set is a set of sets of elements of type T,
 * where the elements come from the discrete range <min_val; max_val>. If the upward
 * closed set contains a subset A of the range <min_val; max_val>, it also contains
 * all the supersets of A (in context of the discrete range <min_val; max_val>).
 * Thus, the upward closed set could be easily represented by its 1) type (upward closed),
 * 2) discrete range borders (min_val, max_val), 3) its antichain.
 * Analogously, a downward closed set contains all the subsets of the antichain elements.
 *
 * It is possible to:
 *
 * -> ==- and !=-compare two closed sets
 * -> <=- and >=-compare two closed sets of the same type
 * -> get a text representation of the closed set
 * -> chceck whether the closed set contains a given node/nodes
 * -> insert a node/more nodes to the closed set
 * -> perform an union over two closed sets of the same type and with the same carrier
 * -> perform an intersection over two closed sets of the same type and with the same carrier
 *
 * It is not possible to:
 *
 * -> choose a custom carrier which is not a discrete range <min_val; max_val> (???)
 * -> <=- and >=-compare two closed sets of the different types (nonsense)
 * -> remove a node/more nodes from the closed set (nonsense (???))
 * -> perform an union over two closed sets of different types or different carriers (nonsense)
 * -> perform an intersection over two closed sets of different types or different carriers (nonsense) 
 * -> compute a complement of a closed set (TODO)
 *
 * Examples:
 *
 * Let us have the carrier {0, 1, 2, 3} and the upward-closed set with the antichain {{0}, {1, 2}}. We can write
 * ↑{{0}, {1, 2}} = {{0}, {0, 1}, {0, 2}, {0, 3}, {0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2}, {1, 2, 3}, {0, 1, 2, 3}}.
 *
 * Let us have the carrier {0, 1, 2, 3} and the downward-closed set with the antichain {{0}, {1, 2}}. We can write
 * ↓{{0}, {1, 2}} = {{0}, {1, 2}, {1}, {2}, {}}.
 *
 *  @author Tomáš Kocourek
 */

#ifndef _MATA_CLOSED_SET_HH_
#define _MATA_CLOSED_SET_HH_

#include <cassert>

#include <mata/util.hh>
#include <mata/ord_vector.hh>

namespace Mata
{

// Ordered vector
template<typename T> using OrdVec = Mata::Util::OrdVector<T>;

// Closed set
// contains discrete range borders, its type
// and the corresponding antichain
// It is neccessary to be able to evaluate relations operators
// <, <=, >, >=, ==, != over instances of this datatype T
template <typename T> 
struct ClosedSet
{
    // static constants describing the type of closed sets
    static const bool upward_closed = true;
    static const bool downward_closed = false;
    using Node = OrdVec<T>;
    using Nodes = OrdVec<Node>;

    private:

       bool type{upward_closed}; // upward_closed or downward_closed sets
       T min_val;
       T max_val;
       Nodes antichain{}; 

    public:
    
       // constructors
       /////////////////////////////////////////////

       ClosedSet() : type(), min_val(), max_val(), antichain() { }

       // inserting a single value of the datatype T           
       ClosedSet(bool type, T min_val, T max_val, T value) : 
       type(type), min_val(min_val), max_val(max_val), antichain(OrdVec<T>(OrdVec<T>(value))) 
       { 
            assert(min_val <= max_val); 
            assert(min_val <= value && value <= max_val);
       }

       // inserting a single vector of the datatype T
       ClosedSet(bool type, T min_val, T max_val, Node node) : 
       type(type), min_val(min_val), max_val(max_val), antichain(OrdVec<T>(node)) 
       { 
            assert(min_val <= max_val);
            assert(in_interval(node));
       }

       // inserting a whole antichain
       ClosedSet(bool type, T min_val, T max_val, Nodes antichain = Nodes()) : 
       type(type), min_val(min_val), max_val(max_val)
       { 
            assert(min_val <= max_val);
            insert(antichain);
       }

       /////////////////////////////////////////////

       // operators
       /////////////////////////////////////////////

       // Two closed sets are equivalent iff their type, borders and corresponding antichains are the same
       bool operator==(ClosedSet<T> rhs) const
	    { // {{{
		    return type == rhs.type && min_val == rhs.min_val && max_val == rhs.max_val && antichain == rhs.antichain;
	    } // operator== }}}

       // Two closed sets are not equivalent iff their type, borders or corresponding antichains differ
       bool operator!=(ClosedSet<T> rhs) const
	    { // {{{
		    return type != rhs.type || min_val != rhs.min_val || max_val != rhs.max_val || antichain != rhs.antichain;
	    } // operator!= }}}

        // A closed set is considered to be smaller than the other one iff it is a subset of the other one
        // It is not possible to perform <=-comparisons of accros upward- and downward-closed sets, each argument
        // has to be upward- or downward-closed set
        bool operator<=(ClosedSet<T> rhs) const
	    { // {{{
            assert(type == rhs.type && min_val == rhs.min_val && max_val == rhs.max_val);
            return rhs.contains(antichain);
	    } // operator<= }}}

        // A closed set is considered to be bigger than the other one iff it is a superset of the other one
        // It is not possible to perform <=-comparisons of accros upward- and downward-closed sets, each argument
        // has to be upward- or downward-closed set
        bool operator>=(ClosedSet<T> rhs) const
	    { // {{{
            assert(type == rhs.type && min_val == rhs.min_val && max_val == rhs.max_val);
            return contains(rhs.antichain);
	    } // operator<= }}}

        // Text representation of a closed set
        friend std::ostream& operator<<(std::ostream& os, const ClosedSet<T> cs)
        {
            std::string strType = "TYPE: ";
            strType += cs.get_type() ? "UPWARD CLOSED" : "DOWNWARD CLOSED";
            strType += "\n";
            std::string strInterval = "INTERVAL: " + std::to_string(cs.get_min()) + " - " + std::to_string(cs.get_max()) + "\n";
            std::string strValues = "ANTICHAIN: {";
            for(auto node : cs.get_antichain())
            {
                strValues += "{";
                for(auto state : node)
                {
                    strValues += " " + std::to_string(state);
                }
                strValues += "}";
            }
            strValues += "}";
            return os << strType + strInterval + strValues + "\n";
        }

       /////////////////////////////////////////////

       // other methods
       /////////////////////////////////////////////

       bool is_upward_closed(void) const {return type == upward_closed;};
       bool is_downward_closed(void) const {return type == downward_closed;};
       const bool get_type(void) const {return type;};
       const Nodes get_antichain(void) const {return antichain;};
       const T get_min(void) const {return min_val;};
       const T get_max(void) const {return max_val;};

       bool contains (Node node) const;
       bool contains (Nodes nodes) const;

       bool in_interval (Node node) const;      

       void insert(T el) {insert(Node(el));};
       void insert(Node node);
       void insert(Nodes nodes) {for(auto node : nodes) insert(node);};

       ClosedSet Union (const ClosedSet rhs) const;
       ClosedSet intersection (const ClosedSet rhs) const;

       //TODO: complement!!!

       /////////////////////////////////////////////
}; // ClosedSet }}}

/** This function decides whether a set of elements is part of the closed set
* by subset-compraring the input with all elements of the antichain 
* @brief decides whether the closed set contains a given element
* @param node a given ordered vector of elements of the type T
* @return true iff the given ordered vector belongs to the current closed set
*/
template <typename T>
bool ClosedSet<T>::contains(Node node) const
{
    if(type == upward_closed)
    {         
        for(auto element : antichain)
        {
            if(element.IsSubsetOf(node))
            {
                return true;
            }
        }
    }
    else if(type == downward_closed)
    {         
        for(auto element : antichain)
        {
            if(node.IsSubsetOf(element))
            {
                return true;
            }
        }
    }
    return false;
} // contains }}}

/** This function decides whether a set of sets of elements is a part of the closed set
* by subset-compraring the input with all elements of the antichain 
* @brief decides whether the closed set contains a given set of sets of elements
* @param node a given ordered vector of ordered vectors of elements of the type T
* @return true iff the given ordered vector of ordered vectors belongs to the current closed set
*/
template <typename T>
bool ClosedSet<T>::contains(Nodes nodes) const
{
    for(auto node : nodes)
    {
        if(!contains(node))
        {
            return false;
        }
    }
    return true;
} // contains }}}

/** This function decides whether a given ordered vector of elements of the datatype T
* belongs to the discrete interval of the current closed set
* @brief decides whether the given vector respects the borders
* @param node a given ordered vector elements of the type T which will be inspected
* @return true iff the given ordered vector respects the borders
*/
template <typename T>
bool ClosedSet<T>::in_interval(Node node) const
{
    for(auto value : node)
    {
        if(value < min_val || value > max_val)
        {
            return false;
        }
    }
    return true;
} // in_interval }}}

/** Adds a new element to the closed set. If the element is already contained in the closed
* set, the closed set remains unchanged. Otherwise, the antichain will be recomputed.
* @brief decides whether the given vector respects the borders
* @param node a given ordered vector elements of the type T which will be inspected
*/
template <typename T>
void ClosedSet<T>::insert(Node node)
{
    assert(in_interval(node));
    // If the closed set is empty, the antichain could be simply changed
    if(antichain.empty())
    {
        antichain.insert(node);
        return;
    }
    // If the closed set already contains the given node, there is no
    // need to change the closed set
    if(contains(node))
    {
        return;
    }
    // We need to collect all the elements off the antichain which could be removed
    // as soon as the new element is added to keep the antichain <=-uncomparable
    OrdVec<OrdVec<T>> to_erase{};

    // If the closed set is upward-closed, we have to erase all the elements of
    // the antichain which are supersets of the inserted node
    // Example: Let us have an upward-closed set ↑{{0, 1}, {2}} with a corresponding 
    // antichain {{0, 1}, {2}}. If we add {0} to the closed set, the element {0, 1}
    // needs to be erased from the antichain since the set {{0}, {0, 1}, {2}} contains
    // <=-comparable elements and the result should be upward-closed.
    if(type == upward_closed)
    {
        for(auto element : antichain)
        {
            if(node.IsSubsetOf(element))
            {
                to_erase.insert(element);
            }
        }
    }

    // If the closed set is downward-closed, we have to erase all the elements of
    // the antichain which are subsets of the inserted node
    // Example: Let us have an upward-closed set ↓{{0, 1}, {2}} with a corresponding 
    // antichain {{0, 1}, {2}}. If we add {1, 2} to the closed set, the element {2}
    // needs to be erased from the antichain since the set {{0}, {1, 2}, {2}} contains
    // <=-comparable elements and the result should be downward-closed.
    else if(type == downward_closed)
    {
        for(auto element : antichain)
        {
            if(element.IsSubsetOf(node))
            {
                to_erase.insert(element);
            }
        }
    }

    for(auto element : to_erase)
    {
        antichain.remove(element);
    }
    antichain.insert(node);
} // insert }}}

/** Performs an union over two closed sets with the same type and carrier. 
* This function simply adds all the
* elements of the antichain of the first closed set to the second closed set
* @brief performs an union over closed sets
* @param rhs a given closed set which will be unioned with the current one
* @return an union of the given closed sets
*/
template <typename T>
ClosedSet<T> ClosedSet<T>::Union(const ClosedSet<T> rhs) const
{
    assert(type == rhs.get_type() && min_val == rhs.get_min() && max_val == rhs.get_max());
    ClosedSet<T> result(type, min_val, max_val, antichain);
    result.insert(rhs.get_antichain());
    return result;
} // Union }}}

/** Performs an intersection over two closed sets with the same type and carrier.
* @brief performs an intersection over closed sets
* @param rhs a given closed set which will be intersectioned with the current one
* @return an intersection of the given closed sets
*/
template <typename T>
ClosedSet<T> ClosedSet<T>::intersection(const ClosedSet<T> rhs) const
{
    assert(type == rhs.get_type() && min_val == rhs.get_min() && max_val == rhs.get_max());
    ClosedSet<T> result(type, min_val, max_val);

    // Iterates through all the tuples from Antichan1 X Antichan2 and creates an union of them
    if(type == upward_closed)
    {
        for(auto element1 : antichain)
        {
            for(auto element2 : rhs.get_antichain())
            {
               result.insert(element1.Union(element2));    
            }
        }
    }

    // Iterates through all the tuples from Antichan1 X Antichan2 and creates an intersection of them
    if(type == downward_closed)
    {
        for(auto element1 : antichain)
        {
            for(auto element2 : rhs.get_antichain())
            {
               result.insert(element1.intersection(element2));    
            }
        }
    }    
    return result;
} // intersection }}}


} // std }}}


#endif /* _MATA_CLOSED_SET_HH_ */
