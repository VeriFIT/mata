/**
 * @file closed_set.hh
 * @brief Definition of a downward and upward closed set.
 *
 * This file contains definition of a closed set and function which
 *  allow us to work with them (membership, inclusion, union, intersection)
 *
 * Description:
 * In this context, an upward-closed set is a set of sets of elements of type T,
 * (set of nodes of type T, where a node is a set of elements of type T)
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
 * -> compute a complement of a closed set
 *
 * It is not possible to:
 *
 * -> choose a custom carrier which is not a discrete range <min_val; max_val>
 * -> <=- and >=-compare two closed sets of the different types (nonsense)
 * -> remove a node/more nodes from the closed set (nonsense)
 * -> perform an union over two closed sets of different types or different carriers (nonsense)
 * -> perform an intersection over two closed sets of different types or different carriers (nonsense)
 *
 * Examples:
 *
 * Let us have the carrier {0, 1, 2, 3} and the upward-closed set with the antichain
 * {{0}, {1, 2}}. We can write↑{{0}, {1, 2}} = {{0}, {0, 1}, {0, 2}, {0, 3}, {0, 1, 2},
 * {0, 1, 3}, {0, 2, 3}, {1, 2}, {1, 2, 3}, {0, 1, 2, 3}}.
 *
 * Let us have the carrier {0, 1, 2, 3} and the downward-closed set with the antichain
 * {{0}, {1, 2}}. We can write ↓{{0}, {1, 2}} = {{0}, {1, 2}, {1}, {2}, {}}.
 *
 *  @author Tomáš Kocourek
 */

#ifndef MATA_CLOSED_SET_HH_
#define MATA_CLOSED_SET_HH_

#include <cassert>

#include "utils.hh"
#include "ord-vector.hh"

namespace mata {

// Ordered vector.
template<typename T> using OrdVec = mata::utils::OrdVector<T>;

// A closed set could be upward-closed or downward-closed.
enum class ClosedSetType { upward_closed_set, downward_closed_set };

template <typename T> struct ClosedSet;
template <typename T> std::ostream& operator<<(std::ostream& os, const ClosedSet<T>& cs);

/**
 * @brief Closed set.
 *
 * Contains discrete range borders, its type and the corresponding anti-chain. It is necessary to be able to evaluate
 *  relation operators <, <=, >, >=, ==, != over instances of this datatype T.
 * @tparam T Datatype the closed set contains.
 */
template <typename T>
struct ClosedSet {
    using Node = OrdVec<T>;
    using Nodes = OrdVec<Node>;

    private:

       ClosedSetType type_{ ClosedSetType::upward_closed_set }; // upward_closed or downward_closed sets.
       T min_val_;
       T max_val_;
       Nodes antichain_{};

    public:

       // constructors
       ClosedSet() : type_(), min_val_(), max_val_(), antichain_() { }

       // inserting a single value of the datatype T
       ClosedSet(const ClosedSetType type, const T& min_val, const T& max_val, const T& value)
           : type_(type), min_val_(min_val), max_val_(max_val), antichain_(Nodes(value)) {
           assert(min_val <= max_val);
           assert(min_val <= value && value <= max_val);
       }

       // inserting a single vector of the datatype T
       ClosedSet(ClosedSetType type, const T& min_val, const T& max_val, const Node& node)
           : type_(type), min_val_(min_val), max_val_(max_val), antichain_(Node(node)) {
           assert(min_val <= max_val);
           assert(in_interval(node));
       }

       // inserting a whole antichain
       ClosedSet(const ClosedSetType type, const T& min_val, const T& max_val, const Nodes& antichain = Nodes())
           : type_(type), min_val_(min_val), max_val_(max_val) {
           assert(min_val <= max_val);
           insert(antichain);
       }

       /// Two closed sets are equivalent iff their type, borders and corresponding antichains are the same. They are
       ///  not equivalent otherwise.
       bool operator==(const ClosedSet<T>& rhs) const = default;

        // A closed set is considered to be smaller than the other one iff
        // it is a subset of the other one
        // It is not possible to perform <=-comparisons accros upward- and downward-closed
        // sets, each argument has to be upward- or downward-closed set
        bool operator<=(const ClosedSet<T>& rhs) const
        { // {{{
            assert(type_ == rhs.type_ && min_val_ == rhs.min_val_ && max_val_ == rhs.max_val_ &&
            "Types and borders of given closed sets must be the same to perform their <=-comparison.");
            return rhs.contains(antichain_);
        } // operator<= }}}

        // A closed set is considered to be bigger than the other one iff
        // it is a superset of the other one
        // It is not possible to perform <=-comparisons of accros upward-
        // and downward-closed sets, each argument
        // has to be upward- or downward-closed set
        bool operator>=(const ClosedSet<T>& rhs) const
        { // {{{
            assert(type_ == rhs.type_ && min_val_ == rhs.min_val_ && max_val_ == rhs.max_val_ &&
            "Types and borders of given closed sets must be the same to perform their >=-comparison.");
            return contains(rhs.antichain);
        } // operator<= }}}

        // Text representation of a closed set
        friend std::ostream& operator<< <> (std::ostream& os, const ClosedSet<T>& cs);

        bool is_upward_closed() const { return type_ == ClosedSetType::upward_closed_set; };
        bool is_downward_closed() const { return type_ == ClosedSetType::downward_closed_set; };

        ClosedSetType type() { return type_; }
        ClosedSetType type() const { return type_; }

        Nodes antichain() { return antichain_;}
        Nodes antichain() const { return antichain_; }

        T get_min() { return min_val_; }
        T get_min() const { return min_val_; }

        T get_max() { return max_val_; }
        T get_max() const { return max_val_; }

        bool contains (const Node& node) const;
        bool contains (const Nodes& nodes) const;

        bool in_interval (const Node& node) const;

        void insert(const T& el) { insert(Node(el)); }
        void insert(const Node& node);
        void insert(const Nodes& nodes) {for (const auto& node: nodes) insert(node); }

        ClosedSet set_union (const ClosedSet& rhs) const;
        ClosedSet intersection (const ClosedSet& rhs) const;
        ClosedSet complement () const;
}; // class ClosedSet.

template<typename T>
std::ostream& operator<<(std::ostream& os, const ClosedSet<T>& cs) {
    std::string strType = "TYPE: ";
    strType += cs.type() == ClosedSetType::upward_closed_set ? "UPWARD CLOSED" : "DOWNWARD CLOSED";
    strType += "\n";
    std::string strInterval = "INTERVAL: " + std::to_string(cs.get_min()) + " - " + std::to_string(cs.get_max()) + "\n";
    std::string strValues = "ANTICHAIN: {";
    for(auto node : cs.antichain()) {
        strValues += "{";
        for(auto state : node) {
            strValues += " " + std::to_string(state);
        }
        strValues += "}";
    }
    strValues += "}";
    return os << strType + strInterval + strValues + "\n";
}

/** This function decides whether a set of elements is part of the closed set
* by subset-compraring the input with all elements of the antichain
* @brief decides whether the closed set contains a given element
* @param node a given ordered vector of elements of the type T
* @return true iff the given ordered vector belongs to the current closed set
*/
template <typename T>
bool ClosedSet<T>::contains(const Node& node) const {
    if(type_ == ClosedSetType::upward_closed_set) {
        for(auto element : antichain_) {
            if(element.is_subset_of(node)) {
                return true;
            }
        }
    }
    else if(type_ == ClosedSetType::downward_closed_set) {
        for(auto element : antichain_) {
            if(node.is_subset_of(element)) {
                return true;
            }
        }
    }
    return false;
}

/** This function decides whether a set of sets of elements is a part of the closed set
* by subset-compraring the input with all elements of the antichain
* @brief decides whether the closed set contains a given set of sets of elements
* @param nodes a given ordered vector of ordered vectors of elements of the type T
* @return true iff the given ordered vector of ordered vectors belongs to the current closed set
*/
template <typename T>
bool ClosedSet<T>::contains(const Nodes& nodes) const {
    for(auto node : nodes) {
        if(!contains(node)) {
            return false;
        }
    }
    return true;
}

/** This function decides whether a given ordered vector of elements of the datatype T
* belongs to the discrete interval of the current closed set
* @brief decides whether the given vector respects the borders
* @param node a given ordered vector of elements of the type T which will be inspected
* @return true iff the given ordered vector respects the borders
*/
template <typename T>
bool ClosedSet<T>::in_interval(const Node& node) const {
    for(auto value : node) {
        if(value < min_val_ || value > max_val_) {
            return false;
        }
    }
    return true;
}

/** Adds a new element to the closed set. If the element is already contained in the closed
* set, the closed set remains unchanged. Otherwise, the antichain will be recomputed.
* @brief inserts a new element to the closed set
* @param node a given node which will be added to the closed set
*/
template <typename T>
void ClosedSet<T>::insert(const Node& node) {
    assert(in_interval(node) && "Each element of the given node has to respect " &&
    "the carrier of the closed set.");
    // If the closed set is empty, the antichain could be simply changed
    // by adding the given node as the only node to the antichain
    if(antichain_.empty()) {
        antichain_.insert(node);
        return;
    }
    // If the closed set already contains the given node, there is no
    // need to change the closed set
    if(contains(node)) {
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
    if(type_ == ClosedSetType::upward_closed_set) {
        for(auto element : antichain_) {
            if(node.is_subset_of(element)) {
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
    else if(type_ == ClosedSetType::downward_closed_set) {
        for(auto element : antichain_) {
            if(element.is_subset_of(node)) {
                to_erase.insert(element);
            }
        }
    }

    for(auto element : to_erase) {
        antichain_.erase(element);
    }
    antichain_.insert(node);
} // insert }}}

/** Performs an union over two closed sets with the same type and carrier.
* This function simply adds all the
* elements of the antichain of the first closed set to the second closed set
* @brief performs an union over closed sets
* @param rhs a given closed set which will be unioned with the current one
* @return an union of the given closed sets
*/
template <typename T>
ClosedSet<T> ClosedSet<T>::set_union(const ClosedSet<T>& rhs) const {
    assert(type_ == rhs.type_ && min_val_ == rhs.min_val_ && max_val_ == rhs.max_val_ &&
    "Types and borders of given closed sets must be the same to compute their union.");
    ClosedSet<T> result(type_, min_val_, max_val_, antichain_);
    result.insert(rhs.antichain());
    return result;
}

/** Performs an intersection over two closed sets with the same type and carrier.
* @brief performs an intersection over closed sets
* @param rhs a given closed set which will be intersectioned with the current one
* @return an intersection of the given closed sets
*/
template <typename T>
ClosedSet<T> ClosedSet<T>::intersection(const ClosedSet<T>& rhs) const {
    assert(type_ == rhs.type_ && min_val_ == rhs.min_val_ && max_val_ == rhs.max_val_ &&
    "Types and borders of given closed sets must be the same to compute their union.");
    ClosedSet<T> result(type_, min_val_, max_val_);

    // Iterates through all the tuples from Antichan1 X Antichan2
    // and creates an union of them
    if(type_ == ClosedSetType::upward_closed_set) {
        for(const Node& element1 : antichain_) {
            for(const Node& element2 : rhs.antichain()) {
                Node tmp = element1;
                tmp.insert(element2);
                result.insert(tmp);
            }
        }
    }

    // Iterates through all the tuples from Antichan1 X Antichan2
    // and creates an intersection of them
    if(type_ == ClosedSetType::downward_closed_set) {
        for(auto element1 : antichain_) {
            for(auto element2 : rhs.antichain()) {
               result.insert(element1.intersection(element2));
            }
        }
    }
    return result;
} // intersection }}}

/** Performs a complementation over a closed set. The result will
* contain nodes which are not elements of the former closed set.
* The complement of an upward-closed set is always downward-closed and vice versa.
* @brief performs a complementation over a closed set
* @return a complement of a closed set
*/
template <typename T>
ClosedSet<T> ClosedSet<T>::complement() const {
    // The complement of an upward-closed set is
    // always downward-closed and vice versa.
    ClosedSetType new_type = ClosedSetType::upward_closed_set;
    if(type_ == ClosedSetType::upward_closed_set) {
        new_type = ClosedSetType::downward_closed_set;
    }
    ClosedSet<T> result = ClosedSet(new_type, get_min(), get_max(), antichain());

    if(type_ == ClosedSetType::upward_closed_set) {
        // Initially, a complement contains all possible elements
        // which will be then (possibly) removed.
        Node initialValues{};
        for(long unsigned i = 0; i <= max_val_; ++i) {
            initialValues.insert(i);
        }
        result.insert(initialValues);

        // For each element of the closed set {xa, xb, xc, ...}, we
        // create a set of all sets Xa, Xb, Xc,... such that Xn contains
        // all elements of the carrier except of xn
        // For example, for a node {0, 2, 3} (the maximal element is 4),
        // we create a set {{1, 2, 3, 4}, {0, 1, 3, 4}, {0, 1, 2, 4}}.
        // This set corresponds to an antichain of a new downward-closed set.
        // The result will be an intersection of all downward-closed sets
        // created using this procedure.
        for(const auto& element : antichain()) {
            ClosedSet preparingAntichain(ClosedSetType::downward_closed_set, get_min(), get_max());
            for(T i = 0; i <= max_val_; ++i) {
                if(!element.count(i)) {
                    continue;
                }
                Node candidates{};
                for(T j = 0; j <= max_val_; ++j) {
                    if(i != j) {
                        candidates.insert(j);
                    }
                }
                preparingAntichain.insert(candidates);
            }
            result = result.intersection(preparingAntichain);
        }
    }

    else if(type_ == ClosedSetType::downward_closed_set) {
        // Initially, a complement contains all possible elements
        // which will be then (possibly) removed.
        result.insert(Node());

        // For each element of the closed set {xa, xb, xc, ...}, we
        // create a set of all sets Xa, Xb, Xc,... such that Xn contains
        // only xn. For example, for a node {0, 2, 3}, we create a set
        // {{0}, {2}, {3}}.
        // This set corresponds to an antichain of a new upward-closed set.
        // The result will be an intersection of all upward-closed sets
        // created using this procedure.
        for(Node element : antichain()) {
            ClosedSet preparingAntichain(ClosedSetType::upward_closed_set, min_val_, max_val_);
            for(T i = min_val_; i <= max_val_; ++i) {
                Node candidates{i};
                if(!element.count(i)) {
                    preparingAntichain.insert({i});
                }
            }
            result = result.intersection(preparingAntichain);
        }
    }

    return result;
} // complement }}}

} // namespace mata.

#endif /* MATA_CLOSED_SET_HH_ */
