/** @file partition.hh
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

#ifndef PARTITION_HH_
#define PARTITION_HH_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "mata/utils/sparse-set.hh"

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

/*
*   @struct SplitPair
*   @brief contains information about block which has been split
*
*   The structure SplitPair is created as soon as a block of the partition
*   is split. It contains:
*   - index of the new block which keeps the identity of the split block
*     (first_block_idx)
*   - index of the new block which has been created (second_block_idx)
*   - index of the node which had represented the former block before
*     (node_idx)
*   Using first_block_idx and second_block_idx, we can manipulate with the
*   current generation of the partition. Using node_idx, we are able to work
*   with the older generation of the partition.
*/
using SplitPair = struct SplitPair {

    size_t first_block_idx;
    size_t second_block_idx;
    size_t node_idx;

    /**
    *   Initialization of the SplitPair   
    *
    *   @brief default constructor
    *   @param[in] first first part of the split block
    *   @param[in] second second part of the split block
    *   @param[in] node node corresponding to the split block
    */
    SplitPair(size_t first, size_t second, size_t node) : 
        first_block_idx(first), second_block_idx(second), node_idx(node) {}
};

/**
 * @class Partition
 * @brief Partition of a set of states
 *
 * This data structure provides a partition of a set of states S. In this
 * context, the term 'state' refers to any natural number from the
 * interval <0, |S|-1>.
 *
 * This representation defines:
 * - states - element from a consecutive interval of natural numbers
 * - blocks - objects which represent the current generation of the partition.
 *            Each block refers to several states which belong to the block.
 *            The block could be possibly split.
 * - nodes  - objects which represent blocks either from the current generation
 *            of the partition or the block from the previous generations of
 *            the partition (block which had been split). Once a node is
 *            created, it is never changed. When a block is split, two new nodes
 *            are created.
 * - block items
 *          - objects which serve as intermediate data structure between states
 *            and blocks. Each block item contains indices of both corresponding
 *            state and block. Block items are sorted in such way that one could
 *            iterate through each block B or each node N in the O(B) or O(N)
 *            time, respectively.  
 * 
 * Detailed explanation:
 *
 * STATES:
 * States are represented by indices of the vector 'states_' with the
 * unchangeable size |S|. Each element of the vector represents the state
 * by its order in the 'states_' vector. The vector itself contains indices
 * of corresponding block items. Index of the block item corresponding
 * to the state 's' could be get in constant time using 'states_[s]'. 
 *
 * BLOCK ITEMS:
 * Block items are stored in the 'block_items_' vector of the unchangeable size
 * |S|. The block item can by accessed by its index using 
 * block_items[block_item_idx]. Each block item contains its index (which is
 * always the same as its order in the 'block_items_' vector but
 * it is stored directly in the object to simplify manipulations with
 * the partition). It also contains the index of its corresponding state
 * which means that states and block items are bijectively mapped. 
 * In addition, each BlockItem includes an index of the corresponding partition
 * block.
 * The ordering of 'block_items_' vector satisfies the condition that the states
 * of the same block (or node) should always form a contiguous subvector so one
 * could iterate through states in each block (or node) efficiently using
 * 'block_items' vector.
 *
 * BLOCKS:
 * The blocks themselves are represented by the vector 'blocks_' with the
 * size |P|, where P is a partition of states. Each block can be accessed by
 * its index 0 <= i < |P|. The block can by accessed by its index using 
 * blocks_[block_idx]. The block contains an index of itself and an index of its 
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
 * maximal number of nodes is equal to 2 * |S| - 1 since once a node is created,
 * it is never changed. Each node contains its own index and two indices of
 * block items (namely 'first' and 'last') which could be used to access
 * block items corresponding to the first and last block items which form a   
 * contiguous subvector of 'block_items_' vector included in such node.
 * When a block is split, the corresponding block items are swapped 
 * in situ such that the indices 'first' and 'last' still surround the
 * corresponding node and both new nodes also point to the contiguous subvector
 * of its block items.
 * 
 * EXAMPLE:
 * In the example below, we represent a partition {{0, 1, 2, 3, 4}}
 * which had been split to the partition {{0, 2}, {1, 3, 4}}.
 * Thus, we have two blocks: 0 ({0, 2}) and 1 ({1, 3, 4}) which form the current
 * generation of the partition.
 * We also have three nodes: 0 ({0, 1, 2, 3, 4}), 1 ({0, 2}) and 2 ({1, 3, 4})
 * which represent both current generation of the partition and also block which
 * does not exist anymore since it had been split.
 * The block 0 corresponds to the node 1 and the block 1 corresponds to the 
 * node 2.
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
 * In the picture below, indices of vectors are depicted outside of the vectors.
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
 */
class Partition {
     
    private:
    
        class BlockItem;
        class Block;
        class Node;

        using States = std::vector<size_t>;
        using BlockItems = std::vector<BlockItem>;
        using Blocks = std::vector<Block>;
        using Nodes = std::vector<Node>;   
                
        /**< vector of states referring to the block items */
        States states_{};
        
        /**< vector of block items referring to the states and blocks */
        BlockItems block_items_{};
        
        /**< vector of blocks referring to the nodes  */
        Blocks blocks_{};
        
        /**< vector of nodes referring to the first and last block item
             of the node */
        Nodes nodes_{};

        /**
        * @class BlockItem
        * @brief Intermediate between states and blocks
        **/
        class BlockItem {
            private:
            
                /**< index of itself */
                size_t idx_;
                
                /**< corresponding state */
                State state_;
                
                /**< index of the corresponding block */
                size_t block_idx_;
                
                /**< reference to the partition which works 
                     with this block item */
                const Partition& partition_;
                
                // BlockItem class need to access private members
                // of Partition class 
                friend class Partition;

            public:

                // constructors
                BlockItem(size_t idx, size_t state, size_t block_idx,
                    const Partition& partition) :
                    
                    idx_(idx), state_(state), block_idx_(block_idx),
                    partition_(partition) {}
                
                // getters
                size_t idx() const { return idx_; }
                size_t state() const { return state_; }
                
                // methods which refer to the partition vectors
                const Block& block() const {
                    return partition_.blocks_[block_idx_];
                }
                const Node& node() const { return block().node(); }
                const BlockItem& repr() const { return node().repr(); }
                const BlockItem& first() const { return node().first(); }
                const BlockItem& last() const { return node().last(); }
        };

        /**
        * @class Block
        * @brief Contains information about block from the current generation
        * of the partition.
        **/        
        class Block {
            private:
            
                /**< index of itself */
                size_t idx_;
                
                /**< index of the corresponding node */
                size_t node_idx_;
                
                /**< reference to the partition which works 
                     with this block */
                const Partition& partition_;

                // Blocks need to access private members of Partition class                 
                friend class Partition;
                
            public:
            
                // constructors
                Block(size_t idx, size_t node_idx, const Partition& partition) :
                    idx_(idx), node_idx_(node_idx), partition_(partition) {}
                
                // getters
                size_t idx() const { return idx_; }
                
                // methods which refer to the partition vectors 
                const Node& node() const {
                    return partition_.nodes_[node_idx_];
                }
                const BlockItem& repr() const { return node().repr(); }
                const BlockItem& first() const { return node().first(); }
                const BlockItem& last() const { return node().last(); }
                
                // iterators
                using const_iterator = typename BlockItems::const_iterator;
                    
                const_iterator begin() const {
                    const_iterator it = partition_.block_items_.begin();
                    std::advance(it, static_cast<long>(node().first().idx()));
                    return it;
                }
                
                const_iterator end() const { 
                    const_iterator it = partition_.block_items_.begin();
                    std::advance(it, static_cast<long>(node().last().idx()) +1);
                    return it;
                }
                
                // information about the block
                size_t size() const {
                    return last().idx() - first().idx() + 1;
                }
                
        };

        /**
        * @class Node
        * @brief Contains information about block from the current generation
        * of the partition or from the previous generation of the partition.
        **/         
        class Node {
            private:
            
                /**< index of itself */
                size_t idx_;
                
                /**< index of the first block item in the node */
                size_t first_;
                
                /**< index of the last block item in the node */
                size_t last_;
                
                /**< reference to the partition which works 
                     with this block */                
                const Partition& partition_;
                
                // Blocks need to access private members of Partition class
                friend class Partition;
                
            public:
            
                // constructors
                Node(size_t idx, size_t first, size_t last, 
                    const Partition& partition) :
                    
                    idx_(idx), first_(first), last_(last), partition_(partition)
                    {}
            
                // getters
                size_t idx() const { return idx_; }
                
                // methods which refer to the partition vectors
                const BlockItem& first() const {
                    return partition_.block_items_[first_];
                }
                const BlockItem& last() const {
                    return partition_.block_items_[last_];
                }
                const BlockItem& repr() const {
                    return partition_.block_items_[first_];
                }
                
                // iterators
                using const_iterator = 
                    typename BlockItems::const_iterator;
                    
                const_iterator begin() const {  
                    const_iterator it = partition_.block_items_.begin();
                    std::advance(it, static_cast<long>(first().idx()));
                    return it;
                }
                
                const_iterator end() const { 
                    const_iterator it = partition_.block_items_.begin();
                    std::advance(it, static_cast<long>(last().idx()) + 1);
                    return it;
                }
                
                // information about the node
                size_t size() const {
                    return last().idx() - first().idx() + 1;
                }
                
                bool contains_block(size_t block_idx) const {
                    const Block& block = partition_.get_block(block_idx);
                    return first_ <= block.first().idx() &&
                        last_ >= block.last().idx(); 
                }
        };
            
    public:
        
        // constructors
        Partition() = default;
        explicit Partition(size_t num_of_states,
                  const StateBlocks& partition = StateBlocks());
        Partition(const Partition& other);

        // sizes of the used vectors
        size_t num_of_states() const { return states_.size(); }
        size_t num_of_block_items() const { return block_items_.size(); }
        size_t num_of_blocks() const { return blocks_.size(); }
        size_t num_of_nodes() const { return nodes_.size(); }
        
        // blocks splitting        
        std::vector<SplitPair> split_blocks(const SparseSet<State>& marked);
        
        // basic information about the partition
        size_t get_block_idx(State state) const;
        bool in_same_block(State first, State second) const;
        bool in_same_block(const std::vector<State>& states) const;
        std::vector<State> states_in_same_block(State state) const;
      
        // accessing block items, blocks, nodes through indices     
        const BlockItem& get_block_item(size_t block_item_idx) const;
        const Block& get_block(size_t block_idx) const;
        const Node& get_node(size_t node_idx) const;  
        
        // converts the partition to the vector of vectors of states
        StateBlocks partition() const;
        
        // operators
        Partition& operator=(const Partition& other);        
        friend std::ostream& operator<<(std::ostream& os, 
                                        const Partition& p);
        
        const BlockItem& operator[](State state) const;            
            
}; // Partition

}

#endif // PARTITION_HH_
