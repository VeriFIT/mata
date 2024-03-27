/** @file partition_relation_pair.hh
 *  @brief Definition of a partition, extendable square matrix
 *  and partition-relation pair.
 *
 *  This file contains definitions of partition, extendable square
 *  matrix, partition-relation pair and operations which allow us
 *  to manipulate with them.
 *
 *  Description:
 *
 *  A partition-relation pair is a tuple (P, Rel). It is an efficient
 *  representation of a preorder/quasiorder R, which is a reflexive and
 *  transitive binary relation.
 *  In this context, we consider a carrier set S which contains all
 *  natural numbers from 0 to |S|-1. These numbers are called states.
 *  P is a partition of S which corresponds to an equivalence relation
 *  induced by the preorder R.
 *  Rel is a partial order over P.
 *  Thus, (P, Rel) corresponds to a preorder relation R over states S.
 *  
 *  This file provides implementation of a partition P and defines the
 *  ExtendableSquareMatrix structure which can be used to represent 
 *  the binary relation Rel. 
 *  These structures can be combined to represent the preorder R.
 *
 *  @author Tomáš Kocourek
 */

#ifndef _PARTITION_RELATION_PAIR_HH_
#define _PARTITION_RELATION_PAIR_HH_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>

namespace mata::utils {

/************************************************************************
*
*
*
*                              PARTITION
*                   
*
*
*************************************************************************/


using State = unsigned long;
using StateBlock = std::vector<State>;
using StateBlocks = std::vector<StateBlock>;

using BlockItem = struct BlockItem {
    State state;
    size_t block_idx;
};

using Block = struct Block {
    size_t node_idx;
};

using Node = struct Node {
    size_t first;
    size_t last;
};

using BlockItems = std::vector<BlockItem>;
using Blocks = std::vector<Block>;
using Nodes = std::vector<Node>;

using SplitPair = struct SplitPair {
    size_t former;
    size_t created;
    size_t old_node_idx;
};

/**
 * Partition
 * 
 * @brief Partition of a set of states
 *
 * This data structure provides a partition of a set of states S. In this
 * context, the term 'state' refers to any natural number from the
 * interval <0, |S|-1>.
 *
 * STATES:
 * This representation works with the vector of indices 'states_'
 * with the constant size |S|. Each state is represented by an index
 * to that vector so we can refer to a state in constant time using
 * states_[state].
 *
 * BLOCK ITEMS:
 * The memory cell states_[state] contains a corresponding index to the 
 * 'block_items_' vector. The vector 'block_items_' has the constant size |S|.
 * Each BlockItem contains an index of the corresponding state which means
 * that states and BlockItems are bijectively mapped. In addition, 
 * each BlockItem includes an index to the corresponding partition class 
 * (called block). The ordering of BlockItems satisfies the condition that 
 * the states of the same block should always form a contiguous subvector
 * so one could iterate through states in each block efficiently using 
 * 'block_items_' vector.
 *
 * BLOCKS:
 * The blocks themselves are represented by the vector 'blocks_' with the
 * size |P|, where P is a partition of states. Each block can be accessed by
 * its index 0 <= i < |P|. The block can by accessed by its index using 
 * blocks_[block_idx]. The block contains only an index of its 
 * corresponding node. The total number of blocks can be changed as soon as
 * one block is split. However, the maximal number of blocks is equal to |S|
 * (the case when each block contains only one state). When a block 'B' is
 * split in two pieces 'B1' and 'B2', we create a brand new block 'B2' 
 * and modify the former block 'B' such that it will correspond to its
 * subblock 'B1'. The former block 'B' is thus not represented
 * in the 'blocks_' vector anymore since 'B1' takes over its identity.
 *
 * NODES:
 * Each node represents a current block or a block which has been split before.
 * The node can by accessed by its index using nodes_[node_idx]. If the given
 * node represents an existing block, such block contains an index of that node.
 * In context of nodes which represent former blocks, no block contains their
 * indices. The total number of nodes can be changed as soon as
 * one block is split. In such situation, two new nodes (which represent both
 * new blocks) are created and the former node remains unchanged. Therefore, the
 * maximal number of nodes is equal to 2 * |P| - 1 since once a node is created,
 * it is never changed. Each node contains two indices ('first' and 'last')
 * which could be used to access BlockItems corresponding to the first and last
 * BlockItems which form a contiguous subvector of BlockItem included in such 
 * node. When a block is split, the corresponding BlockItems are swapped 
 * in situ such that the indices 'first' and 'last' still surround the
 * corresponding node and both new nodes also point to the contiguous subvector
 * of its BlockItems.
 * 
 * EXAMPLE:
 * In the example below, we represent a partition {{0, 2}, {1, 3, 4}}.
 * Thus, we have two blocks: 0 ({0, 2}) and 1 ({1, 3, 4}). The block 0 
 * corresponds to the node 1 and the block 1 corresponds to the node 2.
 * The node 1 contains indices 0 (first) and 1 (last) which means that
 * the blockItems 0 and 1 surround a contiguous subvector of elements in the 
 * node 1 (or in the block 0).
 * Likewise, the node 2 contains indices 2 (first) and 4 (last) which means that
 * the blockItems 2 and 4 surround a contiguous subvector of elements in the
 * node 2 (or in the block 1).
 * Moreover, we also represent the former block which does not exist anymore
 * by the node 0 which contains indices 0 (first) and 4 (last) which means that
 * the blockItems 0 and 4 surround a contiguous subvector of elements in the 
 * node 0. Thus, we know that there had been a block {0, 1, 2, 3, 4} before 
 * it has been split to obtain blocks {0, 2} and {1, 3, 4}.
 * 
 *
 *             0       1       2       3       4
 *             ------- ------- ------- ------- -------
 *            |   0   |   2   |   1   |   4   |   3   |    states_
 *             ------- ------- ------- ------- -------
 *                ↑          ↑ ↑             ↑ ↑
 *                |          \ /             \ /
 *                |           X               X
 *                |          / \             / \
 *             0  ↓    1     ↓ ↓    2   3    ↓ ↓     4
 *             ------- ------- ------- ------- -------
 *            |   0   |   2   |   1   |   4   |   3   |    block_items_
 *       --→→→|-------|-------|-------|-------|--------←←←------------
 *       |    |   0   |   0   |   1   |   1   |   1   |               |
 *       |     ------- ------- ------- ------- -------                |
 *       |        |       |       |       |       |                   |
 *       |     0  ↓       ↓    1  ↓       ↓       ↓                   |
 *       |     ----------------------------------------               |
 *       |    |       1       |            2           |   blocks_    |
 *       |     ----------------------------------------               |
 *       |                |       |                                   |
 *       |     0       1  ↓    2  ↓                                   |
 *       |     ------- ------- -------                                |
 *       -----|   0   |   0   |   2   |   nodes_                      |
 *            |-------|-------|-------|                               |
 *            |   4   |   1   |   4   |                               |
 *             ------- ------- -------                                |
 *                |                                                   |
 *                ----------------------------------------------------
 * 
 *  Using this structure, we can:
 *  - find the block which contains given state in O(1)
 *  - find a representative state of the given block in O(1)
 *  - test whether two states share the same block in O(1)
 *  - test whether all states in a vector A share the same block in O(|A|)
 *  - iterate through the block B in O(|B|)
 *  - split the whole partition such that each block
 *    is split in two pieces or remains unchanged in O(|S|)
 *  - remember all ancestors of current blocks and access them
 *
 */
typedef struct Partition {
    private:
        
        /* indices to the block_items_ vector */
        std::vector<size_t> states_{};
        
        /* indices to the states_ and blocks_ vectors */
        BlockItems block_items_{};
        
        /* indices to the nodes_ vector */
        Blocks blocks_{};
        
        /* tuples of indices to the block_items_ vectors */
        Nodes nodes_{};
            
    public:
        
        // constructors
        Partition(size_t num_of_states, 
                  const StateBlocks& partition = StateBlocks());
        Partition(const Partition& other);

        // sizes of the used vectors
        inline size_t num_of_states(void) const { return states_.size(); }
        inline size_t num_of_block_items(void) const {
            return block_items_.size();}
        inline size_t num_of_blocks(void) const { return blocks_.size(); }
        inline size_t num_of_nodes(void) const { return nodes_.size(); }
        
        // blocks splitting        
        std::vector<SplitPair> split_blocks(const std::vector<State>& marked);
        
        // basic information about the partition
        inline bool in_same_block(State first, State second) const;
        bool in_same_block(const std::vector<State>& states) const;
        std::vector<State> states_in_same_block(State state) const;
      
        // accessing blockItems, blocks, nodes through indices       
        inline BlockItem get_block_item(size_t block_item_idx) const;
        inline Block get_block(size_t block_idx) const;
        inline Node get_node(size_t node_idx) const;
        
        // refering between blockItems, blocks, nodes using indices        
        inline size_t get_block_idx_from_state(State state) const;
        inline size_t get_node_idx_from_state(State state) const;
        inline size_t get_block_item_idx_from_state(State state) const;
        inline size_t get_node_idx_from_block_item_idx(
            size_t block_item_idx) const;
        inline size_t get_node_idx_from_block_idx(size_t block_idx) const;
        inline size_t get_repr_idx_from_block_idx(size_t block_idx) const;
        inline size_t get_repr_idx_from_node_idx(size_t node_idx) const;  
        
        // converts the partition to the vector of vectors of states
        StateBlocks partition(void);
        
        // operators
        Partition& operator=(const Partition& other);        
        friend std::ostream& operator<<(std::ostream& os, 
                                        const Partition& p);
        
        
            
            
} Partition; // Partition

/** Constructor of the partition structure. This method reserves memory space
* for the vectors used to represent partition to ensure us that they won't
* ever be moved in the memory when extended.
* The partition can be initialized in linear time (in respect to the carrier
* set of the partition) using initial partition represented as a vector of 
* vectors of states.
* The constructor works as follows:
* - if there is any nonexisting state in the initial partition, the function
*   fails
* - if there are duplicates in the initial partition, the function fails
* - if there is an empty partition class, the function fails
* - if there are states which are not represented in the initial partition,
*   they will be all part of the one additional block
* If there is no initial partition, all states will be assigned 
* to the same block
* @brief constructs the partition
* @param num_of_states cardinality of the carrier set
* @param partition optional initial partition in the form of vectors of
*        vectors of states
*/
Partition::Partition(size_t num_of_states, const StateBlocks& partition) {
    // reserving memory space to avoid moving extended vectors
    states_.reserve(num_of_states);
    block_items_.reserve(num_of_states);
    blocks_.reserve(num_of_states);
    nodes_.reserve(2 * num_of_states - 1);

    // this vector says whether the given state has been already seen
    // in the given initial partition to detect duplicates
    // and to detect unused states
    std::vector<char> used;
    used.insert(used.end(), num_of_states, false);
    
    // initialization of the states_ vector
    states_.insert(states_.end(), num_of_states, 0);
    
    // creating partition using given initial vector of vectors
    size_t num_of_blocks = partition.size();
    // iterating through initial partition blocks
    for(size_t block_idx = 0; block_idx < num_of_blocks; ++block_idx) {
        assert(!partition[block_idx].empty() &&
               "Partition class cannot be empty.");
        
        // iterating through one partition block
        for(auto state : partition[block_idx]) {
            assert(state < num_of_states &&
                   "Invalid state name detected while creating" 
                   "a partition relation pair.");
            assert(!used[state] && 
                   "Partition could not be created."
                   "Duplicate occurence of a state");
            
            used[state] = true;
            
            // creating a corresponding BlockItem
            states_[state] = block_items_.size();
            block_items_.push_back({.state = state, .block_idx = block_idx});
            
        }
        
        // first and last states of the block will be used to create
        // a corresponding node
        State first = partition[block_idx].front();
        State last = partition[block_idx].back();

        // creating a corresponding block and node
        nodes_.push_back({.first = states_[first], .last = states_[last]});
        blocks_.push_back({.node_idx = block_idx});
    }
    
    // we need to detect whether there is a state which has not be used
    // to create an additional partition block
    bool all_states_used = true;
    
    // first and last unused states will surround a contiguous subvector
    // of BlockItems
    State first = 0;
    State last = 0;
    
    // iterating through the vector of flags saying which states has been seen
    for(State state = 0; state < num_of_states; ++state) {
        // if a state has been already seen and processed, 
        // there is no need to add it to the additional block
        if(used[state]) {
            continue;
        }
        
        // if there is at least one unused state, we need to 
        // create an additional block
        if(all_states_used) {
            all_states_used = false;
            first = state;
            ++num_of_blocks;
        }
        
        // creating the new BlockItem
        last = state;
        states_[state] = block_items_.size();
        block_items_.push_back({.state = state, .block_idx = num_of_blocks-1});
    }
    
    // creating a new block and node if there was an unused state
    if(!all_states_used) { 
        nodes_.push_back({.first = states_[first], .last = states_[last]});
        blocks_.push_back({.node_idx = num_of_blocks-1});
    }
}

/**
* Custom copy constructor which preserves reserved memory for the
* partition vectors. This method has to be implemented since the custom
* assignment operator is also implemented. The preservation of the reserved
* memory is provided by the custom assignment operator=.
* @brief copy constructor of the Partition
* @param other partition which will be copied
*/
Partition::Partition(const Partition& other) {
    // using the custom assignment operator
    *this = other;
}


/**
* @brief returns a BlockItem corresponding to the given index
* @param block_item_idx index of the BlockItem
* @return corresponding BlockItem
*/
inline BlockItem Partition::get_block_item(size_t block_item_idx) const {
    assert(block_item_idx < num_of_block_items() &&
           "Nonexisting block item index used.");    
    return block_items_[block_item_idx];
}

/**
* @brief returns a block corresponding to the given index
* @param block_idx index of the block
* @return corresponding block
*/
inline Block Partition::get_block(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");    
    return blocks_[block_idx];
}

/**
* @brief returns a node corresponding to the given index
* @param node_idx index of the node
* @return corresponding node
*/
inline Node Partition::get_node(size_t node_idx) const {
    assert(node_idx < num_of_nodes() && "Nonexisting node index used.");    
    return nodes_[node_idx];
}

/**
* @brief returns a block index corresponding to the given state
* @param state given state
* @return corresponding block index
*/
inline size_t Partition::get_block_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");            
    return block_items_[states_[state]].block_idx;
}

/**
* @brief returns a node index corresponding to the given state
* @param state given state
* @return corresponding node index
*/
inline size_t Partition::get_node_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");           
    return blocks_[block_items_[states_[state]].block_idx].node_idx;
}

/**
* @brief returns a BlockItem index corresponding to the given state
* @param state given state
* @return corresponding BlockItem index
*/
inline size_t Partition::get_block_item_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");           
    return states_[state];
}

/**
* @brief returns a Node index corresponding to the given BlockItem index
* @param block_item_idx BlockItem index
* @return corresponding node index
*/
inline size_t Partition::get_node_idx_from_block_item_idx(
    size_t block_item_idx) const {

    assert(block_item_idx < num_of_block_items() &&
           "Nonexisting BlockItem index used.");           
    return blocks_[block_items_[block_item_idx].block_idx].node_idx;
}

/**
* @brief returns a node index corresponding to the given block index
* @param block_idx given block index
* @return corresponding node index
*/
inline size_t Partition::get_node_idx_from_block_idx(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");
    return blocks_[block_idx].node_idx;
}

/** Get a representant from the block index
* @brief returns the first blockItem index corresponding to the given 
* block index
* @param block_idx given block index
* @return first blockItem index corresponding to the given block index
*/
inline size_t Partition::get_repr_idx_from_block_idx(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");
    return nodes_[blocks_[block_idx].node_idx].first;
}

/** Get a representant from the node index
* @brief returns the first blockItem index corresponding to the given node index
* @param node_idx given node index
* @return first blockItem index corresponding to the given node index
*/
inline size_t Partition::get_repr_idx_from_node_idx(size_t node_idx) const {
    assert(node_idx < num_of_nodes() && "Nonexisting node index used.");
    return nodes_[node_idx].first;
}

/**
* @brief tests whether the two given states correspond 
* to the same partition block
* @param first first state to be checked
* @param second second state to be checked
* @return true iff both given states belong to the same partition block
*/
inline bool Partition::in_same_block(State first, State second) const {
    assert(first < states_.size() && "The given state does not exist");
    assert(second < states_.size() && "The given state does not exist");
    return get_block_idx_from_state(first) == get_block_idx_from_state(second);
}

/**
* @brief tests whether the given n states correspond to the same partition block
* @param states vector of states to be checked
* @return true iff all of the given states belong to the same partition block
*/
bool Partition::in_same_block(const std::vector<State>& states) const {
    if(states.empty()) { return true; }
    size_t block_idx = get_block_idx_from_state(states.front());
    for(size_t state : states) {
        assert(state < states_.size() && "The given state does not exist.");
        if(get_block_idx_from_state(state) != block_idx) { return false; }
    }
    return true;
}

/**
* @brief find all of the states which share the block with the input state
* @param state input state
* @return vector of all the states in the corresponding block
*/
std::vector<State> Partition::states_in_same_block(State state) const {
    assert(state < num_of_states() && "The given state does not exist.");
    std::vector<State> result{};
    
    // first and last states in the block stored in the vector
    // of BlockItems
    size_t first = get_node(get_node_idx_from_state(state)).first;
    size_t last = get_node(get_node_idx_from_state(state)).last;
    
    // iterating through BlockItems
    for(size_t i = first; i <= last; ++i) {
        result.push_back(get_block_item(i).state);
    }
    
    return result; 
}

/**
* @brief transforms inner representation of the partition to the vector of
* vectors of states
* @return vector of vectors of states
*/
StateBlocks Partition::partition(void) {
    StateBlocks result{};
    result.insert(result.end(), blocks_.size(), std::vector<State>());
    for(auto block_item : block_items_) {
        result[block_item.block_idx].push_back(block_item.state);
    }
    return result;
}

/** Splitting the blocks of existing partition. According to the input
* vector of states 'marked', there will be two types of states - marked
* and unmarked ones. The partition will be split as follows:
* - if there is a block whose all elements are marked, the block remains
* unchanged
* - if there is a block whose all elements are unmarked, the block remains
* unchanged
* - if there is a block which contains both marked and unmarked states, it
* will be split in two blocks such that the first one contains marked states
* and the second one contains unmarked states of the original block
* - it means that each block is either unchanged or split in two parts
* - if a block contains states such that corresponding BlockItems form
* contiguous subvector on the interval of natural numbers <a, b>, the split
* nodes will correspond to such BlockItems that they form contiguous subvectors
* on the intervals of natural numbers <a, k> and <k+1, b>, where a <= k < b.
* The BlockItems on the interval <a, b> will be swapped such that the property
* above holds. The representant (first BlockItem on the interval) will always
* keep its position (the strategy of swapping will adapt to the fact whether
* the representant is marked or not). Thus, a representant of any node never
* changes its position.
* Moreover, the node corresponding to the ancestor of the two split blocks
* will still describe a valid contiguous interval of natural numbers <a, b>.
*
* There are several troubles which can possiby occur:
* - if an nonexisting state is used, the function detects it and fails
* - if there is a state which is marked multiple times, the function detects it
* and fails
*
* If a block is split, the function creates a structure SplitPair which
* contains:
* - index of the new block which keeps identity of the former block
* - index of the new block which is newly constructed
* - index of the node which is an ancestor of these two blocks
* The function returns a vector of such SplitPairs.
*
* @brief splits blocks of the partition
* @param marked marked states which influence splitting
* @return vector of SplitPair which contains information about split blocks
*/
std::vector<SplitPair> Partition::split_blocks(
    const std::vector<State>& marked) {
    
    // the vector which will be returned as the result
    std::vector<SplitPair> split{};
    
    // if there is no marked state, no block could be split
    if(marked.empty()) { return split; }
    
    // this vector contains information about states which has been marked
    // and helps to detect states which has been marked multiple times
    std::vector<char> used_states{};
    used_states.insert(used_states.end(), states_.size(), false);

    // this vector contains information about blocks whose states has been
    // marked and keeps number of states of each block which has been marked
    // to ease detecting whether the whole block has been marked
    std::vector<size_t> used_blocks{};
    used_blocks.insert(used_blocks.end(), blocks_.size(), 0);

    // iterating through the marked states to fill used_states and
    // used_blocks vectors
    for(size_t i : marked) {
        assert(i < states_.size() && "The given state does not exist.");
        assert(!used_states[i] && "The given state was marked multiple times");
        used_states[i] = true;
        ++used_blocks[get_block_idx_from_state(i)];
    }    
    
    size_t old_blocks_size, new_block_idx;
    old_blocks_size = new_block_idx = blocks_.size();
    
    // iterating through existing blocks
    for(size_t i = 0; i < old_blocks_size; ++i) {
        // if no state of the given block has been marked, it
        // won't be split
        if(!used_blocks[i]) { continue; }
        
        // looking for the subvector of BlockItems which forms processed
        // block and computing its size
        size_t node_idx = get_node_idx_from_block_idx(i);
        size_t iter_first = get_node(node_idx).first;
        size_t iter_last = get_node(node_idx).last;
        size_t block_size = iter_last - iter_first + 1;
        
        // if all states of the processed block have been marked, the block
        // won't be split
        if(used_blocks[i] >= block_size) { continue; }
        
        // choosing the strategy of swapping BlocksItems such that
        // the representant of split block keeps its position        
        bool repr_marked = used_states[get_block_item(
                            get_repr_idx_from_node_idx(node_idx)).state];

        // We access the first and last element of the subvector of BlockItems
        // which forms processed block. We look for the first unmarked element
        // from left and first marked element from right (or vice versa since
        // the exact strategy is chosen according to the fact whether the first
        // element is marked or not). As soon as such elements are found, they
        // are swapped. This procedure continues until these two indices used
        // to iterate through the BlockItems meet somewhere in the middle
        while(iter_first <= iter_last) {
            // we choose the swapping strategy using XOR operation
            while(repr_marked ^ !used_states[get_block_item(iter_first).state]) {
                // this visited state will be part of the former block
                ++iter_first;
            }
            while(repr_marked ^ used_states[get_block_item(iter_last).state]) {
                // this visited state will be part of the new block
                block_items_[iter_last].block_idx = new_block_idx;
                --iter_last;
            }
            
            // if the used indices meet, we finish swapping         
            if(iter_first > iter_last) {
                break;
            }
            
            // swapping BlockItems
            BlockItem swapped_block_item = get_block_item(iter_first);
            block_items_[iter_first] = get_block_item(iter_last);
            block_items_[iter_last] = swapped_block_item;
            
            // since states_ and block_items_ vectors should be bijectively
            // mapped, we need to update states_ after swapping two BlockItems           
            states_[block_items_[iter_first].state] = iter_first;
            states_[block_items_[iter_last].state] = iter_last;    
            
            // after the blockItems are swapped, one of them should
            // be assigned to the new block
            block_items_[iter_last].block_idx = new_block_idx;
            
            // after the blockItems are swapped, we continue to the
            // next blockItems
            ++iter_first;
            --iter_last;
        }
        
        // creating new nodes
        nodes_.push_back({.first = nodes_[node_idx].first, .last = iter_last});
        nodes_.push_back({.first = iter_first, .last = nodes_[node_idx].last});
        
        // split blocks has to refer to the new nodes
        blocks_[i].node_idx = nodes_.size() - 2;
        blocks_.push_back({.node_idx = nodes_.size() - 1});
        
        // since a block has been split, we need to return information about
        // indices of components of split block and about the node which
        // correspond to the block which has been split        
        split.push_back({.former = i, .created = new_block_idx,
                         .old_node_idx = node_idx});
        
        // index of the following block which could be created
        ++new_block_idx;
    }
    return split;
}

/**
* Custom assignment operator which preserves reserved capacities for the
* partition vectors
* @brief assignment of the partition
* @param other partition which will be copied
* @return modified partition
*/
Partition& Partition::operator=(const Partition& other) {
    // since the default copying of the vectors do not preserve
    // reserved capacity, we need to reserve it manually and
    // then insert elements of the other partition to the reserved space
    // if we want to keep the former capacity
    states_.reserve(other.num_of_states());
    block_items_.reserve(other.num_of_states());
    blocks_.reserve(other.num_of_states());
    nodes_.reserve(2 * other.num_of_states() - 1);
    
    // copying vectors without losing information about reserved capacity
    size_t states_num = other.num_of_states();
    for(size_t i = 0; i < states_num; ++i) {
        states_.push_back(other.get_block_item_idx_from_state(i));
        block_items_.push_back(other.get_block_item(i));
    }
    size_t blocks_num = other.num_of_blocks();
    for(size_t i = 0; i < blocks_num; ++i) {
        blocks_.push_back(other.get_block(i));
    }
    size_t nodes_num = other.num_of_nodes();
    for(size_t i = 0; i < nodes_num; ++i) {
        nodes_.push_back(other.get_node(i));
    }
    return *this;
}

// debugging function which allows us to print text representation of
// the partition
std::ostream& operator<<(std::ostream& os, const Partition& p) {
    std::string result = std::string();
    result += "NUM OF STATES: " + std::to_string(p.num_of_states()) + "\n";
    result += "NUM OF BLOCKS: " + std::to_string(p.num_of_blocks()) + "\n";
    result += "NUM OF NODES: " + std::to_string(p.num_of_nodes()) + "\n";
    result += "\n";
    
    result += "BLOCKS:\n";
    size_t num_of_blocks = p.num_of_blocks();
    for(size_t block_idx = 0; block_idx < num_of_blocks; ++block_idx) {
        result += std::to_string(block_idx) + ": ";
        Node node = p.nodes_[p.get_node_idx_from_block_idx(block_idx)];
        for(size_t block_item_idx = node.first; 
            block_item_idx <= node.last; ++block_item_idx) {
            result += std::to_string(p.block_items_[block_item_idx].state) 
                   + " ";
        }
        result += "\n";
    }
    result += "\n";
    
    result += "NODES:\n";
    size_t num_of_nodes = p.num_of_nodes();
    for(size_t node_idx = 0; node_idx < num_of_nodes; ++node_idx) {
        result += std::to_string(node_idx) + ": ";
        Node node = p.nodes_[node_idx];
        for(size_t block_item_idx = node.first; 
            block_item_idx <= node.last; ++block_item_idx) {
            result += std::to_string(p.block_items_[block_item_idx].state) + 
                " ";
        }
        result += "\n";
    }
    result += "\n";
    
    return os << result;

}


/************************************************************************
*
*
*                        EXTENDABLE SQUARE MATRIX
*
*
*                        (RELATIONS AND COUNTERS)                   
*
*
*************************************************************************/

/**
 * ExtendableSquareMatrix
 * 
 * @brief interface for extendable square matrix implementations
 *
 * Square matrix "n x n" which can be extended to "(n+1) x (n+1)" matrix
 * if n is less than the maximal capacity. Such structure allows us to
 * represent binary relations over carrier set with n elements and adjust
 * it to n+1 elements whenever a new element of the carrier set is created
 * (for example when we represent relation over partition and a block of
 * partition is split in two) or matrices of counters etc.
 * 
 * This abstract structure declares methods for accessing elements of the 
 * matrix, assigning to the cells of matrix and extending the matrix by one row
 * and one column (changing the size). 
 * It defines attributes for maximal capacity (which cannot be changed) and
 * current size.
 * It does not define the data structure for storing data. Each substructure
 * which inherits from this abstract structure should:
 * - contain the storage for data of datatype T which represents n x n matrix 
 * - implement methods set, get and extend
 * - implement a method clone which creates a deep copy of a matrix
 * Then, the ExtendableSquareMatrix can be used independently of the inner
 * representation of the matrix. Therefore, one can dynamically choose from
 * various of implementations depending on the situation. If any new 
 * substructure is implemented, one should also modify the 'create' 
 * function and extend 'MatrixType' enumerator.
 *
 * Note that in context of an n x n matrix, this implementation uses the word
 * 'size' to refer to the number n (number of rows or columns). The word 
 * 'capacity' corresponds to the maximal allowed size (maximal number
 * of rows or columns). 
 *
**/

using MatrixType = enum MatrixType { None, Cascade, Dynamic, Hashed };

template <typename T>
struct ExtendableSquareMatrix {
    protected:
        
        // number of rows (or columns) of the current square matrix
        size_t size_{0};
        
        // maximal allowed number of rows (or columns) of the square matrix        
        size_t capacity_{0};
        
        // type of the matrix which will be chosen as soon as the
        // child structure will be created
        MatrixType m_type{MatrixType::None};

    public:

        // getters
        inline size_t size(void) const { return size_; }
        inline size_t capacity(void) const { return capacity_; }
        inline size_t type(void) const { return m_type; }
        
        // virtual functions which will be implemented in the substructures
        // according to the concrete representation of the matrix
        virtual inline void set(size_t i, size_t j, T value = T()) = 0;
        virtual inline T get(size_t i, size_t j) const = 0;
        virtual inline void extend(T placeholder = T()) = 0;

        // cloning
        virtual ExtendableSquareMatrix<T> *clone(void) const = 0;
        
        virtual ~ExtendableSquareMatrix() = default;
        
        // matrix properties
        bool is_reflexive(void);
        bool is_antisymetric(void);
        bool is_transitive(void);
                
};

/*************************************
*
*        CASCADE SQUARE MATRIX
*
**************************************/

/**
 * CascadeSquareMatrix
 * 
 * @brief Linearized square matrix implemented using single vector of
 * elements which stores data in some kind of "cascading" way
 *
 * This implementation tries to avoid
 * - moving the whole matrix when it is extended
 * - allocation of unneccessary data cells
 * - violation of data locality
 *
 * The matrix is represented as a single vector of a type T. Initially,
 * the maximal possible capacity is given to the constructor. It reserves
 * 'capacity * capacity' data cells for the vector (in the constant time O(1))
 * without need to allocate anything.
 * When the matrix is extended, additional (size * 2) + 1 elements of the
 * vector are allocated. The matrix is traversed in some kind of "cascading" way
 * as follows:
 *
 * Each number in the matrix corresponds to the order of accessing that element
 * using this "cascading traversal".
 *
 *   MATRIX:
 *   -----------------
 *   | 0 | 3 | 8 | 15|
 *   -----------------
 *   | 1 | 2 | 7 | 14|
 *   -----------------
 *   | 4 | 5 | 6 | 13|
 *   -----------------
 *   | 9 | 10| 11| 12|
 *   -----------------
 *
 *   VECTOR:
 *   -----------------------------------------------------------------------
 *   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
 *   -----------------------------------------------------------------------
 *
 * The data cell matrix[i][j] could be accessed using the formula
 * vector[i >= j ? i * i + j : j * j + 2 * j - i].
 *   
 * Using this approach, there is no need to allocate unnecessary data cells
 * when extending n x n matrix to the (n+1) x (n+1) matrix (in contrast with
 * using row or column traversal).
 * Since 'capacity * capacity' data cells were reserved, the vector won't ever
 * be moved in the memory due to the extending. However, it requieres to
 * reserve a lot of memory space when the capacity is a huge number.
 * The data locality won't be violated since all of the elements will be stored
 * as a contiguous vector.
 *
**/
template <typename T>
struct CascadeSquareMatrix : public ExtendableSquareMatrix<T> {
    private:
 
        // data are stored in a single vector
        std::vector<T> data_{};
    
    public:
    
        // constructors
        CascadeSquareMatrix(size_t max_rows, size_t init_rows = 0);
        CascadeSquareMatrix(const CascadeSquareMatrix& other);
        
        // implemented virtual functions
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        inline void extend(T placeholder = T()) override;
        
        // cloning
        CascadeSquareMatrix *clone(void) const { 
            return new CascadeSquareMatrix<T>(*this); }
            
        // operators
        CascadeSquareMatrix<T>& operator=(const CascadeSquareMatrix<T>& other);
        
};

/**
* @brief creates a Cascade square matrix
* @param max_rows capacity of the square matrix
* @param init_rows initial size of the square matrix
*/
template <typename T>
CascadeSquareMatrix<T>::CascadeSquareMatrix(
    size_t max_rows, size_t init_rows) {
    
    assert(init_rows <= max_rows && 
           "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Cascade;
    this->capacity_ = max_rows;
    data_.reserve(this->capacity_ * this->capacity_);
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < init_rows; ++i) {extend();}
}

/** This method provides a way to create a copy of a given CascadeSquareMatrix
* and preserves the reserved capacity of the vector 'data_'. This goal is
* achieved using the custom assignment operator.
* @brief copy constructor of a CascadeSquareMatrix
* @param other matrix which should be copied
*/
template <typename T>
CascadeSquareMatrix<T>::CascadeSquareMatrix(
    const CascadeSquareMatrix<T>& other) {

    *this = other;
}

/**
* @brief assings a value to the Cascade square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline void CascadeSquareMatrix<T>::set(size_t i, size_t j, T value) {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");
    
    // accessing the matrix in the cascading way
    data_[i >= j ? i * i + j : j * j + 2 * j - i] = value;
}

/**
* @brief returns a value of the Cascade square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline T CascadeSquareMatrix<T>::get(size_t i, size_t j) const {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");

    // accessing the matrix in the cascading way
    return data_[i >= j ? i * i + j : j * j + 2 * j - i];
}

/**
* @brief extends the Cascade square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
* (optional)
*/
template <typename T>
inline void CascadeSquareMatrix<T>::extend(T placeholder) {
    assert(this->size_ < this->capacity_ 
           && "The matrix cannot be extended anymore");

    // allocation of 2 * size + 1 new data cells
    data_.insert(data_.end(), 2 * this->size_ + 1, placeholder);
    
    // the size increases
    ++this->size_;
}

/** This method provides a way to assign a CascadeSquareMatrix to the variable.
* The method ensures us to keep the reserved capacity of the vector 'data_' 
* since the default vector assignment does not preserve it.
* @brief assignment operator for the CascadeSquareMatrix structure
* @param other matrix which should be copied assigned
*/
template <typename T>
CascadeSquareMatrix<T>& CascadeSquareMatrix<T>::operator=(
    const CascadeSquareMatrix<T>& other) {
                     
    // initialization of the matrix
    this->capacity_ = other.capacity();
    this->size_ = 0;
    this->data_ = std::vector<T>();
    this->data_.reserve(this->capacity_ * this->capacity_);
    size_t other_size = other.size();
    for(size_t i = 0; i < other_size; ++i) {this->extend();}
    
    // copying memory cells
    for(size_t i = 0; i < this->size_; ++i) {
        for(size_t j = 0; j < this->size_; ++j) {
            this->set(i, j, other.get(i, j));
        }
    }
    return *this;
}

/*************************************
*
*        DYNAMIC SQUARE MATRIX
*
**************************************/

/**
 * DynamicSquareMatrix
 * 
 * @brief Dynamic square matrix implemented using vector of vectors
 * of the type T
 *
 * This implementation tries to avoid
 * - allocation or reservation of data cells which won't ever be used
 *
 * The matrix is represented as a vector of vectors of the type T. It is
 * extended dynamically without any need to allocate or reserve any unnecessary
 * memory space before it is used.
 * However, the data locality is not ensured. Moreover, if the matrix is
 * extended, it could possibly be moved in the memory.
**/
template <typename T>
struct DynamicSquareMatrix : public ExtendableSquareMatrix<T> {
    private:
 
        // data are stored in a single vector
        std::vector<std::vector<T>> data_{};
    
    public:
    
        // constructors
        DynamicSquareMatrix(size_t max_rows, size_t init_rows = 0);
        
        // implemented virtual functions
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        void extend(T placeholder = T()) override;
        
        // cloning
        DynamicSquareMatrix *clone(void) const { 
            return new DynamicSquareMatrix(*this); }
};

/**
* @brief creates a Dynamic square matrix
* @param max_rows capacity of the square matrix
* @param init_rows initial size of the square matrix
*/
template <typename T>
DynamicSquareMatrix<T>::DynamicSquareMatrix(
    size_t max_rows, size_t init_rows) {
    
    assert(init_rows <= max_rows && 
           "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Dynamic;
    this->capacity_ = max_rows;
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < init_rows; ++i) {extend();}
}

/**
* @brief assings a value to the Dynamic square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline T DynamicSquareMatrix<T>::get(size_t i, size_t j) const {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");
    
    return data_[i][j];
}

/**
* @brief returns a value of the Dynamic square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline void DynamicSquareMatrix<T>::set(size_t i, size_t j, T value) {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");
    
    data_[i][j] = value;
}

/**
* @brief extends the Dynamic square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
*/
template <typename T>
void DynamicSquareMatrix<T>::extend(T placeholder) {
    assert(this->size_ < this->capacity_ 
           && "The matrix cannot be extened anymore");
    
    // creating a new column      
    for(size_t i = 0; i < this->size_; ++i) {
        data_[i].push_back(placeholder);
    }
    
    // creating a new row
    data_.push_back(std::vector<T>());
    ++this->size_;
    data_.back().insert(data_.back().end(), this->size_, placeholder);
}

/*************************************
*
*        HASHED SQUARE MATRIX
*
**************************************/

/**
 * HashedSquareMatrix
 * 
 * @brief Hashed square matrix implemented using unordered hash map
 *
 * The matrix is represented as a unordered hash map of the type T indexed
 * by the size_t type. It is referred as in context of row-traversal of the
 * matrix. To access matrix[i][j], we use map[i * capacity + j].
**/
template <typename T>
struct HashedSquareMatrix : public ExtendableSquareMatrix<T> {
    private:

        // data are stored in a hashmap
        mutable std::unordered_map<size_t, T> data_{};
    
    public:
    
        // constructors
        HashedSquareMatrix(size_t max_rows, size_t init_rows = 0);
        
        // implemented virtual functions        
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        inline void extend(T placeholder = T()) override;

        // cloning
        HashedSquareMatrix *clone(void) const { 
            return new HashedSquareMatrix(*this); }
        
};

/**
* @brief creates a Hashed square matrix
* @param max_rows capacity of the square matrix
* @param init_rows initial size of the square matrix
*/
template <typename T>
HashedSquareMatrix<T>::HashedSquareMatrix(
    size_t max_rows, size_t init_rows) {
    
    assert(init_rows <= max_rows && 
           "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Hashed;
    this->capacity_ = max_rows;
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < init_rows; ++i) {extend();}
}

/**
* @brief assings a value to the Hashed square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline void HashedSquareMatrix<T>::set(size_t i, size_t j, T value) {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");
    
    // accessing the hashmap using row matrix traversal
    data_[i * this->capacity_ + j] = value;
}

/**
* @brief returns a value of the Hashed square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline T HashedSquareMatrix<T>::get(size_t i, size_t j) const {
    assert(i < this->size_ && "Nonexisting row cannot be accessed");
    assert(j < this->size_ && "Nonexisting column cannot be accessed");
    
    // accessing the hashmap using row matrix traversal
    return data_[i * this->capacity_ + j];
}

/**
* @brief extends the Hashed square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
*/
template <typename T>
inline void HashedSquareMatrix<T>::extend(T placeholder) {
    assert(this->size_ < this->capacity_ 
           && "Matrix cannot be extened anymore");

    // creating a new row and column
    for(size_t i = 0; i < this->size_; ++i) {
        data_[this->size_ * this->capacity_ + i] = placeholder;
        data_[i * this->capacity_ + this->size_] = placeholder;
    }
    data_[this->size_ * this->capacity_ + this->size_] = placeholder;
    
    // increasing size
    ++this->size_;
}

/*************************************
*
*        ADDITIONAL FUNCTIONS
*
**************************************/

/**
* @brief factory function which creates an ExtendableSquareMatrix of given type
* @param type type of the new matrix
* @param capacity maximal matrix capacity
* @param size initial matrix size
* @return pointer to the newly created matrix
*/
template <typename T>
ExtendableSquareMatrix<T> *create(MatrixType type, 
    size_t capacity, size_t size = 0) {
    
    switch(type) {
        case MatrixType::Cascade:
            return new CascadeSquareMatrix<T>(capacity, size);
        case MatrixType::Dynamic:
            return new DynamicSquareMatrix<T>(capacity, size);    
        case MatrixType::Hashed:
            return new HashedSquareMatrix<T>(capacity, size);    
        default:
            return nullptr;
    }
}

// debugging function which allows us to print text representation of
// the Extendable square matrix
template <typename T>
std::ostream& operator<<(std::ostream& os, 
    const ExtendableSquareMatrix<T>& matrix) {
    
    size_t size = matrix.size();
    size_t capacity = matrix.capacity();    
    std::string result = "\nSIZE: " + std::to_string(size) + "\n";
    result += "CAPACITY: " + std::to_string(capacity) + "\n";
    result += "MATRIX:\n";
    for(size_t i = 0; i < size; ++i) {
        for(size_t j = 0; j < size; ++j) {
            result += std::to_string(matrix.get(i, j)) + " ";
        }
        result += "\n";
    }
    return os << result;
}

/** This function checks whether the matrix is reflexive. In this
* context, the matrix is reflexive iff none of the elements on the main
* diagonal is the zero element of the type T
* @brief checks whether the Extendable square matrix is reflexive
* @return true iff the matrix is reflexive
*/
template <typename T>
bool ExtendableSquareMatrix<T>::is_reflexive(void) {
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i) {
        if(!get(i, i)) { return false; }
    }
    return true;
}

/** This function checks whether the matrix is antisymetric. In this
* context, the matrix is antisymetric iff there are no indices i, j
* where i != j and both matrix[i][j], matrix[j][i] contain nonzero elementes
* of the type T
* @brief checks whether the Extendable square matrix is antisymetric
* @return true iff the matrix is antisymetric
*/
template <typename T>
bool ExtendableSquareMatrix<T>::is_antisymetric(void) {
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i) {
        for(size_t j = 0; j < size; ++j) {
            if(i == j) { continue; }
            if(get(i, j) && get(j, i)) { return false; }
        }
    }
    return true;
}

/** This function checks whether the matrix is transitive. In this
* context, the matrix is transitive iff it holds that the input matrix
* casted to the matrix of booleans (false for zero values of type T, otherwise 
* true) remains the same if it is multiplied by itself.
* @brief checks whether the Extendable square matrix is transitive
* @return true iff the matrix is transitive
*/
template <typename T>
bool ExtendableSquareMatrix<T>::is_transitive(void) {
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i) {
        for(size_t j = 0; j < size; ++j) {
            bool found = false;
            for(size_t k = 0; k < size; ++k) {
                if(get(i, k) && get(k, j)) { 
                    found = true;
                    break; 
                }
            }
            if(!found == static_cast<bool>(get(i, j))) { return false; }
        }
    }
    return true;
}

}

#endif
