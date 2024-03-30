/** @file partition.hh
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

#ifndef _PARTITION_HH_
#define _PARTITION_HH_

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
    
    BlockItem(State s, size_t idx) : state(s), block_idx(idx) {}
};

using Block = struct Block {
    size_t node_idx;

    Block(size_t idx) : node_idx(idx) {}
};

using Node = struct Node {
    size_t first;
    size_t last;

    Node(size_t fst, size_t lst) : first(fst), last(lst) {}
};

using BlockItems = std::vector<BlockItem>;
using Blocks = std::vector<Block>;
using Nodes = std::vector<Node>;

using SplitPair = struct SplitPair {
    size_t former;
    size_t created;
    size_t old_node_idx;

    SplitPair(size_t former, size_t created, size_t idx) : 
        former(former), created(created), old_node_idx(idx) {}
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
 *  Using this class, we can:
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
class Partition {
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
        size_t num_of_states(void) const { return states_.size(); }
        size_t num_of_block_items(void) const { return block_items_.size(); }
        size_t num_of_blocks(void) const { return blocks_.size(); }
        size_t num_of_nodes(void) const { return nodes_.size(); }
        
        // blocks splitting        
        std::vector<SplitPair> split_blocks(const std::vector<State>& marked);
        
        // basic information about the partition
        bool in_same_block(State first, State second) const;
        bool in_same_block(const std::vector<State>& states) const;
        std::vector<State> states_in_same_block(State state) const;
      
        // accessing blockItems, blocks, nodes through indices       
        const BlockItem& get_block_item(size_t block_item_idx) const;
        const Block& get_block(size_t block_idx) const;
        const Node& get_node(size_t node_idx) const;
        
        // refering between blockItems, blocks, nodes using indices        
        size_t get_block_idx_from_state(State state) const;
        size_t get_node_idx_from_state(State state) const;
        size_t get_block_item_idx_from_state(State state) const;
        size_t get_node_idx_from_block_item_idx(size_t block_item_idx) const;
        size_t get_node_idx_from_block_idx(size_t block_idx) const;
        size_t get_repr_idx_from_block_idx(size_t block_idx) const;
        size_t get_repr_idx_from_node_idx(size_t node_idx) const;  
        
        // converts the partition to the vector of vectors of states
        StateBlocks partition(void);
        
        // operators
        Partition& operator=(const Partition& other);        
        friend std::ostream& operator<<(std::ostream& os, 
                                        const Partition& p);
        
        
            
            
}; // Partition

}

#endif
