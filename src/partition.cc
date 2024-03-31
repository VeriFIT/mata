/** @file partition.cc
 *  @brief Definition of a partition
 *
 *  In this context, we consider a carrier set S which contains all
 *  natural numbers from 0 to |S|-1. These numbers are called states.
 *  Then, partition over S is a set of blocks such that:
 *  - each block contains only states
 *  - each state is represented in exactly one block
 *     - blocks are disjoint
 *     - there is no state which is not represented in any block
 *  - no block is empty
 *
 *  This file provides implementation of a partition P which allows us to:
 *  - find the block which contains a given state in O(1)
 *  - find a representative state of the given block in O(1)
 *  - test whether two states share the same block in O(1)
 *  - test whether all states in a vector A share the same block in O(|A|)
 *  - iterate through the block B in O(|B|)
 *  - split the whole partition such that each block
 *    is split in two pieces or remains unchanged in O(|S|)
 *  - remember all ancestors of current blocks and access them if necessary
 *
 *  @author Tomáš Kocourek
 */

#include <iostream>
#include <vector>
#include "mata/utils/partition.hh"

namespace mata::utils {

/** Constructor of the partition object. This method reserves memory space
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
* @param[in] num_of_states cardinality of the carrier set
* @param[in] partition optional initial partition in the form of vectors of
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
            block_items_.emplace_back(state, block_idx);
            
        }
        
        // first and last states of the block will be used to create
        // a corresponding node
        State first = partition[block_idx].front();
        State last = partition[block_idx].back();

        // creating a corresponding block and node
        nodes_.emplace_back(states_[first], states_[last]);
        blocks_.emplace_back(block_idx);
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
        // this branch will be executed only once (first time it is reached)
        // and then it will be never executed again
        if(all_states_used) [[unlikely]] {
            all_states_used = false;
            first = state;
            ++num_of_blocks;
        }
        
        // creating the new BlockItem
        last = state;
        states_[state] = block_items_.size();
        block_items_.emplace_back(state, num_of_blocks-1);
    }
    
    // creating a new block and node if there was an unused state
    if(!all_states_used) { 
        nodes_.emplace_back(states_[first], states_[last]);
        blocks_.emplace_back(num_of_blocks-1);
    }
}

/**
* Custom copy constructor which preserves reserved memory for the
* partition vectors. This method has to be implemented since the custom
* assignment operator is also implemented. The preservation of the reserved
* memory is provided by the custom assignment operator=.
* @brief copy constructor of the Partition
* @param[in] other partition which will be copied
*/
Partition::Partition(const Partition& other) {
    // using the custom assignment operator
    *this = other;
}


/**
* @brief returns a BlockItem corresponding to the given index
* @param[in] block_item_idx index of the BlockItem
* @return corresponding BlockItem
*/
const BlockItem& Partition::get_block_item(size_t block_item_idx) const {
    assert(block_item_idx < num_of_block_items() &&
           "Nonexisting block item index used.");    
    return block_items_[block_item_idx];
}

/**
* @brief returns a block corresponding to the given index
* @param[in] block_idx index of the block
* @return corresponding block
*/
const Block& Partition::get_block(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");    
    return blocks_[block_idx];
}

/**
* @brief returns a node corresponding to the given index
* @param[in] node_idx index of the node
* @return corresponding node
*/
const Node& Partition::get_node(size_t node_idx) const {
    assert(node_idx < num_of_nodes() && "Nonexisting node index used.");    
    return nodes_[node_idx];
}

/**
* @brief returns a block index corresponding to the given state
* @param[in] state given state
* @return corresponding block index
*/
size_t Partition::get_block_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");            
    return block_items_[states_[state]].block_idx;
}

/**
* @brief returns a node index corresponding to the given state
* @param[in] state given state
* @return corresponding node index
*/
size_t Partition::get_node_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");           
    return blocks_[block_items_[states_[state]].block_idx].node_idx;
}

/**
* @brief returns a BlockItem index corresponding to the given state
* @param[in] state given state
* @return corresponding BlockItem index
*/
size_t Partition::get_block_item_idx_from_state(State state) const {
    assert(state < num_of_states() && "Nonexisting state name used.");           
    return states_[state];
}

/**
* @brief returns a Node index corresponding to the given BlockItem index
* @param[in] block_item_idx BlockItem index
* @return corresponding node index
*/
size_t Partition::get_node_idx_from_block_item_idx(
    size_t block_item_idx) const {

    assert(block_item_idx < num_of_block_items() &&
           "Nonexisting BlockItem index used.");           
    return blocks_[block_items_[block_item_idx].block_idx].node_idx;
}

/**
* @brief returns a node index corresponding to the given block index
* @param[in] block_idx given block index
* @return corresponding node index
*/
size_t Partition::get_node_idx_from_block_idx(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");
    return blocks_[block_idx].node_idx;
}

/** Get a representant from the block index
* @brief returns the first blockItem index corresponding to the given 
* block index
* @param[in] block_idx given block index
* @return first blockItem index corresponding to the given block index
*/
size_t Partition::get_repr_idx_from_block_idx(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexisting block index used.");
    return nodes_[blocks_[block_idx].node_idx].first;
}

/** Get a representant from the node index
* @brief returns the first blockItem index corresponding to the given node index
* @param[in] node_idx given node index
* @return first blockItem index corresponding to the given node index
*/
size_t Partition::get_repr_idx_from_node_idx(size_t node_idx) const {
    assert(node_idx < num_of_nodes() && "Nonexisting node index used.");
    return nodes_[node_idx].first;
}

/**
* @brief tests whether the two given states correspond 
* to the same partition block
* @param[in] first first state to be checked
* @param[in] second second state to be checked
* @return true iff both given states belong to the same partition block
*/
bool Partition::in_same_block(State first, State second) const {
    assert(first < states_.size() && "The given state does not exist");
    assert(second < states_.size() && "The given state does not exist");
    return get_block_idx_from_state(first) == get_block_idx_from_state(second);
}

/**
* @brief tests whether the given n states correspond to the same partition block
* @param[in] states vector of states to be checked
* @return true iff all of the given states belong to the same partition block
*/
bool Partition::in_same_block(const std::vector<State>& states) const {
    if(states.empty()) [[unlikely]] { return true; }
    size_t block_idx = get_block_idx_from_state(states.front());
    for(size_t state : states) {
        assert(state < states_.size() && "The given state does not exist.");
        if(get_block_idx_from_state(state) != block_idx) { return false; }
    }
    return true;
}

/**
* @brief find all of the states which share the block with the input state
* @param[in] state input state
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
* @param[in] marked marked states which influence splitting
* @return vector of SplitPair which contains information about split blocks
*/
std::vector<SplitPair> Partition::split_blocks(
    const std::vector<State>& marked) {
    
    // the vector which will be returned as the result
    std::vector<SplitPair> split{};
    
    // if there is no marked state, no block could be split
    if(marked.empty()) [[unlikely]] { return split; }
    
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
        do {
            // we choose the swapping strategy using XOR operation
            while((repr_marked 
                  ^ (!used_states[get_block_item(iter_first).state]))) {
                // this visited state will be part of the former block
                ++iter_first;
            }
            while((repr_marked ^ used_states[get_block_item(iter_last).state])) {
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
        } while(iter_first <= iter_last);
        
        // creating new nodes
        nodes_.emplace_back(nodes_[node_idx].first, iter_last);
        nodes_.emplace_back(iter_first, nodes_[node_idx].last);
        
        // split blocks has to refer to the new nodes
        blocks_[i].node_idx = nodes_.size() - 2;
        blocks_.emplace_back(nodes_.size() - 1);
        
        // since a block has been split, we need to return information about
        // indices of components of split block and about the node which
        // correspond to the block which has been split        
        split.emplace_back(i, new_block_idx, node_idx);
        
        // index of the following block which could be created
        ++new_block_idx;
    }
    return split;
}

/**
* Custom assignment operator which preserves reserved capacities for the
* partition vectors
* @brief assignment of the partition
* @param[in] other partition which will be copied
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

/**
* @brief debugging function which allows us to print text representation of
* the partition
* @param[out] output stream
* @param[in] partition which will be printed
* @return output stream
*/
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

}
