/** @file partition.cc
 *  @brief Definition of a partition
 *
 *  In this context, we consider a carrier set S which contains all
 *  natural numbers from 0 to |S|-1 and nothing else. These numbers are called
 *  states.
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
 *  - iterate through the node N in O(|N|)
 *  - split the whole partition such that each block
 *    is split in two pieces or remains unchanged in O(|S|)
 *  - remember all ancestors of current blocks and access them if necessary
 *    so we can manipulate multiple generations of a partition (before and
 *    after it has been split)
 *
 *  @author Tomáš Kocourek
 */

#include <iostream>
#include <vector>
#include "mata/utils/partition.hh"
#include "mata/utils/sparse-set.hh"

namespace mata::utils {

/** Constructor of the partition object. This method reserves memory space
* for the vectors used to represent partition to ensure us that they won't
* ever be moved in the memory when extended. The maximal sizes of these vectors
* can be computed using the num_of_states parameter.
* The partition can be initialized in linear time (in respect to the carrier
* set of the partition) using initial partition represented as a vector of 
* vectors of states.
* The constructor works as follows:
* - if there is any nonexistent state in the initial partition, the construction
*   fails (state >= num_of_states)
* - if there are duplicates in the initial partition, the construction fails
* - if there is an empty partition class, the construction fails
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
    if(num_of_states) [[likely]] {
        states_.reserve(num_of_states);
        block_items_.reserve(num_of_states);
        blocks_.reserve(num_of_states);
        nodes_.reserve(2 * num_of_states - 1);
    }
    
    // this vector says whether the given state has been already seen
    // in the given initial partition to detect duplicates
    // and to detect unused states
    std::vector<char> used;
    if(num_of_states) [[likely]] { used.reserve(num_of_states); }
    used.insert(used.end(), num_of_states, false);
    
    // initialization of the states_ vector
    states_.insert(states_.end(), num_of_states, 0);
    
    // creating the partition using given initial vector of vectors
    size_t num_of_blocks = partition.size();
    // iterating through initial partition blocks
    for(size_t block_idx = 0; block_idx < num_of_blocks; ++block_idx) {
        assert(!partition[block_idx].empty() &&
               "Partition class cannot be empty.");
        
        // iterating through one partition block
        for(auto state_idx : partition[block_idx]) {
            assert(state_idx < num_of_states &&
                   "Invalid state name detected while creating" 
                   "a partition relation pair.");
            assert(!used[state_idx] && 
                   "Partition could not be created."
                   "Duplicate occurrence of a state");
            
            used[state_idx] = true;
            
            // creating a corresponding BlockItem
            size_t block_item_idx = block_items_.size();
            states_[state_idx] = block_item_idx;
            block_items_.emplace_back(block_item_idx, state_idx,
                block_idx, *this);
            
        }
        
        // first and last states of the block will be used to create
        // a corresponding node
        size_t first = partition[block_idx].front();
        size_t last = partition[block_idx].back();

        // creating a corresponding block and node
        size_t node_idx = nodes_.size();
        nodes_.emplace_back(node_idx, states_[first], states_[last], *this);
        blocks_.emplace_back(block_idx, block_idx, *this);
    }
    
    // we need to detect whether there is a state which has not been used
    // to create an additional partition block
    bool all_states_used = true;
    
    // first and last unused states will surround a contiguous subvector
    // of BlockItems
    size_t first = 0;
    size_t last = 0;
    
    // Iterating through the vector of flags saying which states have been seen.
    // We need to create an additional block which will contain all states which
    // have not been represented in the input partition if such states exist
    for(size_t state_idx = 0; state_idx < num_of_states; ++state_idx) {
        // if a state has been already seen and processed, 
        // there is no need to add it to the additional block
        if(used[state_idx]) {
            continue;
        }
        
        // if there is at least one unused state, we need to 
        // create an additional block
        // this branch will be executed only once (first time it is reached)
        // and then it will never be executed again
        if(all_states_used) [[unlikely]] {
            all_states_used = false;
            first = state_idx;
            ++num_of_blocks;
        }
        
        // creating the new BlockItem
        size_t block_item_idx = block_items_.size();
        last = state_idx;
        states_[state_idx] = block_item_idx;
        block_items_.emplace_back(block_item_idx, state_idx, num_of_blocks-1,
            *this);
    }
    
    // creating a new block and node if there was an unused state
    if(!all_states_used) { 
        nodes_.emplace_back(nodes_.size(), states_[first], states_[last],
            *this);
        blocks_.emplace_back(num_of_blocks-1, num_of_blocks-1, *this);
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
const Partition::BlockItem& Partition::get_block_item(
    size_t block_item_idx) const {

    assert(block_item_idx < num_of_block_items() &&
           "Nonexistent block item index used.");
    return block_items_[block_item_idx];
}

/**
* @brief returns a block corresponding to the given index
* @param[in] block_idx index of the block
* @return corresponding block
*/
const Partition::Block& Partition::get_block(size_t block_idx) const {
    assert(block_idx < num_of_blocks() && "Nonexistent block index used.");
    return blocks_[block_idx];
}

/**
* @brief returns a node corresponding to the given index
* @param[in] node_idx index of the node
* @return corresponding node
*/
const Partition::Node& Partition::get_node(size_t node_idx) const {
    assert(node_idx < num_of_nodes() && "Nonexistent node index used.");
    return nodes_[node_idx];
}

/**
* @brief returns a block index corresponding to the given state
* @param[in] state given state
* @return corresponding block index
*/
size_t Partition::get_block_idx(State state) const {
    assert(state < num_of_states() && "Nonexistent state used");
    return block_items_[states_[state]].block_idx_;
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
    return get_block_idx(first) == get_block_idx(second);
}

/**
* @brief tests whether the given n states correspond to the same partition block
* @param[in] states vector of states to be checked
* @return true iff all of the given states belong to the same partition block
*/
bool Partition::in_same_block(const std::vector<size_t>& state_idxs) const {
    if(state_idxs.empty()) [[unlikely]] { return true; }
    size_t block_idx = get_block_idx(state_idxs.front());
    for(size_t state_idx : state_idxs) {
        assert(state_idx < states_.size() && "The given state does not exist.");
        if(get_block_idx(state_idx) != block_idx) { return false; }
    }
    return true;
}

/**
* @brief find all of the states which share the block with the input state
* @param[in] state input state
* @return vector of all the states in the corresponding block
*/
std::vector<size_t> Partition::states_in_same_block(size_t state_idx) const {
    assert(state_idx < num_of_states() && "The given state does not exist.");
    std::vector<size_t> result{};
    
    // iterating through the corresponding block
    for(const BlockItem& block_item : (*this)[state_idx].block()) {
        result.push_back(block_item.idx());
    }
    
    return result; 
}

/**
* @brief transforms inner representation of the partition to the vector of
* vectors of states
* @return vector of vectors of states
*/
StateBlocks Partition::partition() const {
    StateBlocks result{};
    for(const Block& block : blocks_) {
        result.emplace_back();
        for(const BlockItem& block_item : block) {
            result.back().push_back(block_item.state());
        }
    }
    return result;
}

/** Splitting the blocks of existing partition. According to the input
* sparse set of states 'marked', there will be two types of states - marked
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
* - if an nonexistent state is used, the function detects it and fails
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
* @return vector of SplitPairs which contain information about split blocks
*/
std::vector<SplitPair> Partition::split_blocks(const SparseSet<size_t>& marked) {
    
    // the vector which will be returned as the result
    std::vector<SplitPair> split{};
    
    // if there is no marked state, no block could be split
    if(marked.empty()) [[unlikely]] { return split; }

    // this vector contains information about blocks whose states have been
    // marked and keep number of states of each block which has been marked
    // to ease detecting whether the whole block has been marked
    std::vector<size_t> used_blocks{};
    if(!blocks_.empty()) [[likely]] { used_blocks.reserve(blocks_.size()); }
    used_blocks.insert(used_blocks.end(), blocks_.size(), 0);

    // iterating through the marked states to fill used_blocks vector
    for(size_t i : marked) {
        assert(i < states_.size() && "The given state does not exist.");
        ++used_blocks[get_block_idx(i)];
    }    
    
    size_t old_blocks_size, new_block_idx;
    old_blocks_size = new_block_idx = blocks_.size();
    
    // iterating through the existing blocks
    for(size_t i = 0; i < old_blocks_size; ++i) {
        // if no state of the given block has been marked, it won't be split
        if(!used_blocks[i]) { continue; }
        
        // looking for the subvector of BlockItems which forms processed
        // block and computing its size
        Node node = get_block(i).node();
        size_t iter_first = node.first().idx();
        size_t iter_last = node.last().idx();
        size_t block_size = node.size();
        
        // if all states of the processed block have been marked, the block
        // won't be split
        if(used_blocks[i] >= block_size) { continue; }
        
        // choosing the strategy of swapping BlocksItems such that
        // the representant of split block keeps its position        
        bool repr_marked = marked[node.repr().state()];

        // We access the first and last element of the subvector of BlockItems
        // which forms processed block. We look for the first unmarked element
        // from left and first marked element from right (or vice versa since
        // the exact strategy is chosen according to the fact whether the first
        // element is marked or not). As soon as such elements are found, they
        // are swapped. This procedure continues until these two indices used
        // to iterate through the BlockItems meet somewhere in the middle
        do {
        
            // we choose the swapping strategy using XOR operation
            while(repr_marked ^ (!marked[get_block_item(iter_first).state()])) {
                
                // this visited state will be part of the former block
                ++iter_first;
            }
            while(repr_marked ^ marked[get_block_item(iter_last).state()]) {
                
                // this visited state will be part of the new block
                block_items_[iter_last].block_idx_ = new_block_idx;
                --iter_last;
            }
            
            // if the used indices meet, we finish swapping         
            if(iter_first > iter_last) {
                break;
            }
            
            // swapping BlockItems
            State swapped_state = block_items_[iter_first].state();
            block_items_[iter_first].state_ = block_items_[iter_last].state_;
            block_items_[iter_last].state_ = swapped_state;
            
            // since states_ and block_items_ vectors should be bijectively
            // mapped, we need to update states_ after swapping two BlockItems           
            states_[block_items_[iter_first].state_] = iter_first;
            states_[block_items_[iter_last].state_] = iter_last;    
            
            // after the blockItems are swapped, one of them should
            // be assigned to the new block
            block_items_[iter_last].block_idx_ = new_block_idx;
            
            // after the blockItems are swapped, we continue to the
            // next blockItems
            ++iter_first;
            --iter_last;
        } while(iter_first <= iter_last);
        
        // creating new nodes
        size_t first_idx = node.first().idx();
        size_t last_idx = node.last().idx();
        nodes_.emplace_back(nodes_.size(), first_idx, iter_last, *this);
        nodes_.emplace_back(nodes_.size(), iter_first, last_idx, *this);
        
        // split blocks has to refer to the new nodes
        blocks_[i].node_idx_ = nodes_.size() - 2;
        blocks_.emplace_back(new_block_idx, nodes_.size() - 1, *this);
        blocks_.back().idx_ = new_block_idx;
        
        // since a block has been split, we need to return information about
        // indices of components of split block and about the node which
        // correspond to the block which has been split        
        split.emplace_back(i, new_block_idx, node.idx());
        
        // index of the following block which could be created
        ++new_block_idx;
    }
    return split;
}

/**
* Custom assignment operator which preserves reserved capacities for the
* partition vectors and assign a proper partition reference to the newly
* created block items, blocks and nodes
* @brief assignment of the partition
* @param[in] other partition which will be copied
* @return modified partition
*/
Partition& Partition::operator=(const Partition& other) {
    // since the default copying of the vectors do not preserve
    // reserved capacity, we need to reserve it manually and
    // then insert elements of the other partition to the reserved space
    // if we want to keep the former capacity
    states_.clear();
    block_items_.clear();
    blocks_.clear();
    nodes_.clear();
    
    size_t states_num = other.num_of_states();
    if(states_num) [[likely]] { 
        states_.reserve(states_num);
        block_items_.reserve(states_num);
        blocks_.reserve(states_num);
        nodes_.reserve(2 * states_num - 1);
    }
    
    // copying vectors without losing information about reserved capacity
    // and storing reference to the assigned partition when creating block
    // items, blocks and nodes
    for(size_t i = 0; i < states_num; ++i) {
        states_.push_back(other.states_[i]);
        const BlockItem& b = other.get_block_item(i);
        block_items_.emplace_back(b.idx_, b.state_, b.block_idx_, *this);
    }
    size_t blocks_num = other.num_of_blocks();
    for(size_t i = 0; i < blocks_num; ++i) {
        const Block& b = other.get_block(i);
        blocks_.emplace_back(b.idx_, b.node_idx_, *this);
    }
    size_t nodes_num = other.num_of_nodes();
    for(size_t i = 0; i < nodes_num; ++i) {
        const Node& n = other.get_node(i);
        nodes_.emplace_back(n.idx_, n.first_, n.last_, *this);
    }
    return *this;
}

/**
* @brief finding a block item corresponding to the given state
* @param[in] state state whose block item will be found
* @return corresponding block item
*/
const Partition::BlockItem& Partition::operator[](State state) const {
    assert(state < states_.size() && "The given state does not exist.");
    return block_items_[states_[state]];
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
    
    result += "BLOCK ITEMS:\n";
    result += "idx -> state block_idx\n";
    for(const Partition::BlockItem& block_item : p.block_items_) {
        result += std::to_string(block_item.idx()) + " -> " +
            std::to_string(block_item.state()) + " " +
            std::to_string(block_item.block().idx()) + "\n";
                    
    }
    result += "\n";
    result += "BLOCKS:\n";
    result += "idx: states -> node_idx\n";
    for(const Partition::Block& block : p.blocks_) {
        result += std::to_string(block.idx()) + ": ";
        for(const Partition::BlockItem& block_item : block) {
            result += std::to_string(block_item.state()) + " ";
        }
        result += "-> " + std::to_string(block.node().idx()) + "\n";
    }
    result += "\n";

    result += "NODES:\n";
    result += "idx: states -> first_idx last_idx\n";
    for(const Partition::Node& node : p.nodes_) {
        result += std::to_string(node.idx()) + ": ";
        for(const Partition::BlockItem& block_item : node) {
            result += std::to_string(block_item.state()) + " ";
        }
        result += "-> " + std::to_string(node.first().idx()) + " " +
            std::to_string(node.last().idx()) + "\n";
    }
    result += "\n";
    
    return os << result;
}

} // namespace mata::utils
