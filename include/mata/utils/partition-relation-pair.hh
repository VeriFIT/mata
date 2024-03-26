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

using BlockItem = struct BlockItem {State state; size_t blockIdx;};
using Block = struct Block {size_t nodeIdx;};
using Node = struct Node {size_t first; size_t last;};

using BlockItems = std::vector<BlockItem>;
using Blocks = std::vector<Block>;
using Nodes = std::vector<Node>;

using SplitPair = struct SplitPair 
                  {size_t former; size_t created; size_t oldNodeIdx;};

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
 * This representation works with the vector of indices 'm_states'
 * with the constant size |S|. Each state is represented by an index
 * to that vector so we can refer to a state in constant time using
 * m_states[state].
 *
 * BLOCK ITEMS:
 * The memory cell m_states[state] contains a corresponding index to the 
 * 'm_blockItems' vector. The vector 'm_blockItems' has the constant size |S|.
 * Each BlockItem contains an index of the corresponding state which means
 * that states and BlockItems are bijectively mapped. In addition, 
 * each BlockItem includes an index to the corresponding partition class 
 * (called block). The ordering of BlockItems satisfies the condition that 
 * the states of the same block should always form a contiguous subvector
 * so one could iterate through states in each block efficiently using 
 * 'm_blockItems' vector.
 *
 * BLOCKS:
 * The blocks themselves are represented by the vector 'm_blocks' with the
 * size |P|, where P is a partition of states. Each block can be accessed by
 * its index 0 <= i < |P|. The block can by accessed by its index using 
 * m_blocks[blockIdx]. The block contains only an index of its 
 * corresponding node. The total number of blocks can be changed as soon as
 * one block is split. However, the maximal number of blocks is equal to |S|
 * (the case when each block contains only one state). When a block 'B' is
 * split in two pieces 'B1' and 'B2', we create a brand new block 'B2' 
 * and modify the former block 'B' such that it will correspond to its
 * subblock 'B1'. The former block 'B' is thus not represented
 * in the 'm_blocks' vector anymore since 'B1' takes over its identity.
 *
 * NODES:
 * Each node represents a current block or a block which has been split before.
 * The node can by accessed by its index using m_nodes[nodeIdx]. If the given
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
 *            |   0   |   2   |   1   |   4   |   3   |    m_states
 *             ------- ------- ------- ------- -------
 *                ↑          ↑ ↑             ↑ ↑
 *                |          \ /             \ /
 *                |           X               X
 *                |          / \             / \
 *             0  ↓    1     ↓ ↓    2   3    ↓ ↓     4
 *             ------- ------- ------- ------- -------
 *            |   0   |   2   |   1   |   4   |   3   |    m_blockItems
 *       --→→→|-------|-------|-------|-------|--------←←←------------
 *       |    |   0   |   0   |   1   |   1   |   1   |               |
 *       |     ------- ------- ------- ------- -------                |
 *       |        |       |       |       |       |                   |
 *       |     0  ↓       ↓    1  ↓       ↓       ↓                   |
 *       |     ----------------------------------------               |
 *       |    |       1       |            2           |   m_blocks   |
 *       |     ----------------------------------------               |
 *       |                |       |                                   |
 *       |     0       1  ↓    2  ↓                                   |
 *       |     ------- ------- -------                                |
 *       -----|   0   |   0   |   2   |   m_nodes                     |
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
typedef struct Partition
{
    private:
        
        /* indices to the m_blockItems vector */
        std::vector<size_t> m_states{};
        
        /* indices to the m_states and m_blocks vectors */
        BlockItems m_blockItems{};
        
        /* indices to the m_nodes vector */
        Blocks m_blocks{};
        
        /* tuples of indices to the m_blockItems vectors */
        Nodes m_nodes{};
            
    public:
        
        // constructors
        Partition(size_t numOfStates, 
                  const StateBlocks& partition = StateBlocks());
        Partition(const Partition& other);

        // sizes of the used vectors
        inline size_t numOfStates(void) const { return m_states.size(); }
        inline size_t numOfBlockItems(void) const { return m_blockItems.size();}
        inline size_t numOfBlocks(void) const { return m_blocks.size(); }
        inline size_t numOfNodes(void) const { return m_nodes.size(); }
        
        // blocks splitting        
        std::vector<SplitPair> splitBlocks(const std::vector<State>& marked);
        
        // basic information about the partition
        inline bool inSameBlock(State first, State second) const;
        bool inSameBlock(const std::vector<State>& states) const;
        std::vector<State> statesInSameBlock(State state) const;
      
        // accessing blockItems, blocks, nodes through indices       
        inline BlockItem getBlockItem(size_t blockItemIdx) const;
        inline Block getBlock(size_t blockIdx) const;
        inline Node getNode(size_t nodeIdx) const;
        
        // refering between blockItems, blocks, nodes using indices        
        inline size_t getBlockIdxFromState(State state) const;
        inline size_t getNodeIdxFromState(State state) const;
        inline size_t getBlockItemIdxFromState(State state) const;
        inline size_t getNodeIdxFromBlockItemIdx(size_t blockItemIdx) const;
        inline size_t getNodeIdxFromBlockIdx(size_t blockIdx) const;
        inline size_t getReprIdxFromBlockIdx(size_t blockIdx) const;
        inline size_t getReprIdxFromNodeIdx(size_t nodeIdx) const;  
        
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
* @param numOfStates cardinality of the carrier set
* @param partition optional initial partition in the form of vectors of
*        vectors of states
*/
Partition::Partition(size_t numOfStates, const StateBlocks& partition)
{
    // reserving memory space to avoid moving extended vectors
    m_states.reserve(numOfStates);
    m_blockItems.reserve(numOfStates);
    m_blocks.reserve(numOfStates);
    m_nodes.reserve(2 * numOfStates - 1);

    // this vector says whether the given state has been already seen
    // in the given initial partition to detect duplicates
    // and to detect unused states
    std::vector<char> used;
    used.insert(used.end(), numOfStates, false);
    
    // initialization of the m_states vector
    m_states.insert(m_states.end(), numOfStates, 0);
    
    // creating partition using given initial vector of vectors
    size_t numOfBlocks = partition.size();
    // iterating through initial partition blocks
    for(size_t blockIdx = 0; blockIdx < numOfBlocks; ++blockIdx)
    {
        assert(!partition[blockIdx].empty() &&
        "Partition class cannot be empty.");
        
        // iterating through one partition block
        for(auto state : partition[blockIdx])
        {
            assert(state < numOfStates &&
            "Invalid state name detected while creating" 
            "a partition relation pair.");
            assert(!used[state] && 
            "Partition could not be created."
            "Duplicate occurence of a state");
            
            used[state] = true;
            
            // creating a corresponding BlockItem
            m_states[state] = m_blockItems.size();
            m_blockItems.push_back({.state = state, .blockIdx = blockIdx});
            
        }
        
        // first and last states of the block will be used to create
        // a corresponding node
        State first = partition[blockIdx].front();
        State last = partition[blockIdx].back();

        // creating a corresponding block and node
        m_nodes.push_back({.first = m_states[first], .last = m_states[last]});
        m_blocks.push_back({.nodeIdx = blockIdx});
    }
    
    // we need to detect whether there is a state which has not be used
    // to create an additional partition block
    bool allStatesUsed = true;
    
    // first and last unused states will surround a contiguous subvector
    // of BlockItems
    State first = 0;
    State last = 0;
    
    // iterating through the vector of flags saying which states has been seen
    for(State state = 0; state < numOfStates; ++state)
    {
        // if a state has been already seen and processed, 
        // there is no need to add it to the additional block
        if(used[state])
        {
            continue;
        }
        
        // if there is at least one unused state, we need to 
        // create an additional block
        if(allStatesUsed)
        {
            allStatesUsed = false;
            first = state;
            ++numOfBlocks;
        }
        
        // creating the new BlockItem
        last = state;
        m_states[state] = m_blockItems.size();
        m_blockItems.push_back({.state = state, .blockIdx = numOfBlocks-1});
    }
    
    // creating a new block and node if there was an unused state
    if(!allStatesUsed)
    { 
        m_nodes.push_back({.first = m_states[first], .last = m_states[last]});
        m_blocks.push_back({.nodeIdx = numOfBlocks-1});
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
Partition::Partition(const Partition& other)
{
    // using the custom assignment operator
    *this = other;
}


/**
* @brief returns a BlockItem corresponding to the given index
* @param blockItemIdx index of the BlockItem
* @return corresponding BlockItem
*/
inline BlockItem Partition::getBlockItem(size_t blockItemIdx) const
{
    assert(blockItemIdx < numOfBlockItems() &&
    "Nonexisting block item index used.");    
    return m_blockItems[blockItemIdx];
}

/**
* @brief returns a block corresponding to the given index
* @param blockIdx index of the block
* @return corresponding block
*/
inline Block Partition::getBlock(size_t blockIdx) const
{
    assert(blockIdx < numOfBlocks() &&
    "Nonexisting block index used.");    
    return m_blocks[blockIdx];
}

/**
* @brief returns a node corresponding to the given index
* @param nodeIdx index of the node
* @return corresponding node
*/
inline Node Partition::getNode(size_t nodeIdx) const
{
    assert(nodeIdx < numOfNodes() &&
    "Nonexisting node index used.");    
    return m_nodes[nodeIdx];
}

/**
* @brief returns a block index corresponding to the given state
* @param state given state
* @return corresponding block index
*/
inline size_t Partition::getBlockIdxFromState(State state) const
{
    assert(state < numOfStates() &&
    "Nonexisting state name used.");            
    return m_blockItems[m_states[state]].blockIdx;
}

/**
* @brief returns a node index corresponding to the given state
* @param state given state
* @return corresponding node index
*/
inline size_t Partition::getNodeIdxFromState(State state) const
{
    assert(state < numOfStates() &&
    "Nonexisting state name used.");           
    return m_blocks[m_blockItems[m_states[state]].blockIdx].nodeIdx;
}

/**
* @brief returns a BlockItem index corresponding to the given state
* @param state given state
* @return corresponding BlockItem index
*/
inline size_t Partition::getBlockItemIdxFromState(State state) const
{
    assert(state < numOfStates() &&
    "Nonexisting state name used.");           
    return m_states[state];
}

/**
* @brief returns a Node index corresponding to the given BlockItem index
* @param blockItemIdx BlockItem index
* @return corresponding node index
*/
inline size_t Partition::getNodeIdxFromBlockItemIdx(size_t blockItemIdx) const
{
    assert(blockItemIdx < numOfBlockItems() &&
    "Nonexisting BlockItem index used.");           
    return m_blocks[m_blockItems[blockItemIdx].blockIdx].nodeIdx;
}

/**
* @brief returns a node index corresponding to the given block index
* @param blockIdx given block index
* @return corresponding node index
*/
inline size_t Partition::getNodeIdxFromBlockIdx(size_t blockIdx) const
{
    assert(blockIdx < numOfBlocks() &&
    "Nonexisting block index used.");            
    return m_blocks[blockIdx].nodeIdx;
}

/** Get a representant from the block index
* @brief returns the first blockItem index corresponding to the given 
* block index
* @param blockIdx given block index
* @return first blockItem index corresponding to the given block index
*/
inline size_t Partition::getReprIdxFromBlockIdx(size_t blockIdx) const
{
    assert(blockIdx < numOfBlocks() &&
    "Nonexisting block index used.");
    return m_nodes[m_blocks[blockIdx].nodeIdx].first;
}

/** Get a representant from the node index
* @brief returns the first blockItem index corresponding to the given node index
* @param nodeIdx given node index
* @return first blockItem index corresponding to the given node index
*/
inline size_t Partition::getReprIdxFromNodeIdx(size_t nodeIdx) const
{
    assert(nodeIdx < numOfNodes() &&
    "Nonexisting node index used.");
    return m_nodes[nodeIdx].first;
}

/**
* @brief tests whether the two given states correspond 
* to the same partition block
* @param first first state to be checked
* @param second second state to be checked
* @return true iff both given states belong to the same partition block
*/
inline bool Partition::inSameBlock(State first, State second) const
{
    assert(first < m_states.size() && "The given state does not exist");
    assert(second < m_states.size() && "The given state does not exist");
    return getBlockIdxFromState(first) == getBlockIdxFromState(second);
}

/**
* @brief tests whether the given n states correspond to the same partition block
* @param states vector of states to be checked
* @return true iff all of the given states belong to the same partition block
*/
bool Partition::inSameBlock(const std::vector<State>& states) const
{
    if(states.empty()) { return true; }
    size_t blockIdx = getBlockIdxFromState(states.front());
    for(size_t state : states)
    {
        assert(state < m_states.size() && "The given state does not exist.");
        if(getBlockIdxFromState(state) != blockIdx) { return false; }
    }
    return true;
}

/**
* @brief find all of the states which share the block with the input state
* @param state input state
* @return vector of all the states in the corresponding block
*/
std::vector<State> Partition::statesInSameBlock(State state) const
{
    assert(state < numOfStates() && "The given state does not exist.");
    std::vector<State> result{};
    
    // first and last states in the block stored in the vector
    // of BlockItems
    size_t first = getNode(getNodeIdxFromState(state)).first;
    size_t last = getNode(getNodeIdxFromState(state)).last;
    
    // iterating through BlockItems
    for(size_t i = first; i <= last; ++i)
    {
        result.push_back(getBlockItem(i).state);
    }
    
    return result; 
}

/**
* @brief transforms inner representation of the partition to the vector of
* vectors of states
* @return vector of vectors of states
*/
StateBlocks Partition::partition(void)
{
    StateBlocks result{};
    result.insert(result.end(), m_blocks.size(), std::vector<State>());
    for(auto blockItem : m_blockItems)
    {
        result[blockItem.blockIdx].push_back(blockItem.state);
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
std::vector<SplitPair> Partition::splitBlocks(const std::vector<State>& marked)
{
    // the vector which will be returned as the result
    std::vector<SplitPair> split{};
    
    // if there is no marked state, no block could be split
    if(marked.empty()) { return split; }
    
    // this vector contains information about states which has been marked
    // and helps to detect states which has been marked multiple times
    std::vector<char> usedStates{};
    usedStates.insert(usedStates.end(), m_states.size(), false);

    // this vector contains information about blocks whose states has been
    // marked and keeps number of states of each block which has been marked
    // to ease detecting whether the whole block has been marked
    std::vector<size_t> usedBlocks{};
    usedBlocks.insert(usedBlocks.end(), m_blocks.size(), 0);

    // iterating through the marked states to fill usedStates and
    // usedBlocks vectors
    for(size_t i : marked)
    {
        assert(i < m_states.size() && "The given state does not exist.");
        assert(!usedStates[i] && "The given state was marked multiple times");
        usedStates[i] = true;
        ++usedBlocks[getBlockIdxFromState(i)];
    }    
    
    size_t oldBlocksSize, newBlockIdx;
    oldBlocksSize = newBlockIdx = m_blocks.size();
    
    // iterating through existing blocks
    for(size_t i = 0; i < oldBlocksSize; ++i)
    {
        // if no state of the given block has been marked, it
        // won't be split
        if(!usedBlocks[i]) { continue; }
        
        // looking for the subvector of BlockItems which forms processed
        // block and computing its size
        size_t nodeIdx = getNodeIdxFromBlockIdx(i);
        size_t iterFirst = getNode(nodeIdx).first;
        size_t iterLast = getNode(nodeIdx).last;
        size_t blockSize = iterLast - iterFirst + 1;
        
        // if all states of the processed block have been marked, the block
        // won't be split
        if(usedBlocks[i] >= blockSize) { continue; }
        
        // choosing the strategy of swapping BlocksItems such that
        // the representant of split block keeps its position        
        bool reprMarked = usedStates
                          [getBlockItem(getReprIdxFromNodeIdx(nodeIdx)).state];

        // We access the first and last element of the subvector of BlockItems
        // which forms processed block. We look for the first unmarked element
        // from left and first marked element from right (or vice versa since
        // the exact strategy is chosen according to the fact whether the first
        // element is marked or not). As soon as such elements are found, they
        // are swapped. This procedure continues until these two indices used
        // to iterate through the BlockItems meet somewhere in the middle
        while(iterFirst <= iterLast)
        {
            // we choose the swapping strategy using XOR operation
            while(reprMarked ^ !usedStates[getBlockItem(iterFirst).state])
            {
                // this visited state will be part of the former block
                ++iterFirst;
            }
            while(reprMarked ^ usedStates[getBlockItem(iterLast).state])
            {
                // this visited state will be part of the new block
                m_blockItems[iterLast].blockIdx = newBlockIdx;
                --iterLast;
            }
            
            // if the used indices meet, we finish swapping         
            if(iterFirst > iterLast)
            {
                break;
            }
            
            // swapping BlockItems
            BlockItem swappedBlockItem = getBlockItem(iterFirst);
            m_blockItems[iterFirst] = getBlockItem(iterLast);
            m_blockItems[iterLast] = swappedBlockItem;
            
            // since m_states and m_blockItems vectors should be bijectively
            // mapped, we need to update m_states after swapping two BlockItems           
            m_states[m_blockItems[iterFirst].state] = iterFirst;
            m_states[m_blockItems[iterLast].state] = iterLast;    
            
            // after the blockItems are swapped, one of them should
            // be assigned to the new block
            m_blockItems[iterLast].blockIdx = newBlockIdx;
            
            // after the blockItems are swapped, we continue to the
            // next blockItems
            ++iterFirst;
            --iterLast;
        }
        
        // creating new nodes
        m_nodes.push_back({.first = m_nodes[nodeIdx].first, .last = iterLast});
        m_nodes.push_back({.first = iterFirst, .last = m_nodes[nodeIdx].last});
        
        // split blocks has to refer to the new nodes
        m_blocks[i].nodeIdx = m_nodes.size() - 2;
        m_blocks.push_back({.nodeIdx = m_nodes.size() - 1});
        
        // since a block has been split, we need to return information about
        // indices of components of split block and about the node which
        // correspond to the block which has been split        
        split.push_back({.former = i, .created = newBlockIdx,
                         .oldNodeIdx = nodeIdx});
        
        // index of the following block which could be created
        ++newBlockIdx;
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
Partition& Partition::operator=(const Partition& other)
{
    // since the default copying of the vectors do not preserve
    // reserved capacity, we need to reserve it manually and
    // then insert elements of the other partition to the reserved space
    // if we want to keep the former capacity
    m_states.reserve(other.numOfStates());
    m_blockItems.reserve(other.numOfStates());
    m_blocks.reserve(other.numOfStates());
    m_nodes.reserve(2 * other.numOfStates() - 1);
    
    // copying vectors without losing information about reserved capacity
    size_t statesNum = other.numOfStates();
    for(size_t i = 0; i < statesNum; ++i)
    {
        m_states.push_back(other.getBlockItemIdxFromState(i));
        m_blockItems.push_back(other.getBlockItem(i));
    }
    size_t blocksNum = other.numOfBlocks();
    for(size_t i = 0; i < blocksNum; ++i)
    {
        m_blocks.push_back(other.getBlock(i));
    }
    size_t nodesNum = other.numOfNodes();
    for(size_t i = 0; i < nodesNum; ++i)
    {
        m_nodes.push_back(other.getNode(i));
    }
    return *this;
}

// debugging function which allows us to print text representation of
// the partition
std::ostream& operator<<(std::ostream& os, const Partition& p)
{
    std::string result = std::string();
    result += "NUM OF STATES: " + std::to_string(p.numOfStates()) + "\n";
    result += "NUM OF BLOCKS: " + std::to_string(p.numOfBlocks()) + "\n";
    result += "NUM OF NODES: " + std::to_string(p.numOfNodes()) + "\n";
    result += "\n";
    
    result += "BLOCKS:\n";
    size_t numOfBlocks = p.numOfBlocks();
    for(size_t blockIdx = 0; blockIdx < numOfBlocks; ++blockIdx)
    {
        result += std::to_string(blockIdx) + ": ";
        Node node = p.m_nodes[p.getNodeIdxFromBlockIdx(blockIdx)];
        for(size_t blockItemIdx = node.first; 
            blockItemIdx <= node.last; ++blockItemIdx)
        {
            result += std::to_string(p.m_blockItems[blockItemIdx].state) 
                   + " ";
        }
        result += "\n";
    }
    result += "\n";
    
    result += "NODES:\n";
    size_t numOfNodes = p.numOfNodes();
    for(size_t nodeIdx = 0; nodeIdx < numOfNodes; ++nodeIdx)
    {
        result += std::to_string(nodeIdx) + ": ";
        Node node = p.m_nodes[nodeIdx];
        for(size_t blockItemIdx = node.first; 
            blockItemIdx <= node.last; ++blockItemIdx)
        {
            result += std::to_string(p.m_blockItems[blockItemIdx].state) 
                   + " ";
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
struct ExtendableSquareMatrix
{
    protected:
        
        // number of rows (or columns) of the current square matrix
        size_t m_size{0};
        
        // maximal allowed number of rows (or columns) of the square matrix        
        size_t m_capacity{0};
        
        // type of the matrix which will be chosen as soon as the
        // child structure will be created
        MatrixType m_type{MatrixType::None};

    public:


        // getters
        inline size_t size(void) const { return m_size; }
        inline size_t capacity(void) const { return m_capacity; }
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
        bool isReflexive(void);
        bool isAntisymetric(void);
        bool isTransitive(void);
                
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
struct CascadeSquareMatrix : public ExtendableSquareMatrix<T>
{
    private:
 
        // data are stored in a single vector
        std::vector<T> data{};
    
    public:
    
        // constructors
        CascadeSquareMatrix(size_t maxRows, size_t initRows = 0);
        CascadeSquareMatrix(const CascadeSquareMatrix& other);
        
        // implemented virtual functions
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        inline void extend(T placeholder = T()) override;
        
        // cloning
        CascadeSquareMatrix *clone(void) const 
            { return new CascadeSquareMatrix<T>(*this); }
            
        // operators
        CascadeSquareMatrix<T>& operator=(const CascadeSquareMatrix<T>& other);
        
};

/**
* @brief creates a Cascade square matrix
* @param maxRows capacity of the square matrix
* @param initRows initial size of the square matrix
*/
template <typename T>
CascadeSquareMatrix<T>::CascadeSquareMatrix
    (size_t maxRows, size_t initRows)
{
    assert(initRows <= maxRows && 
    "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Cascade;
    this->m_capacity = maxRows;
    data.reserve(this->m_capacity * this->m_capacity);
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < initRows; ++i) {extend();}
}

/** This method provides a way to create a copy of a given CascadeSquareMatrix
* and preserves the reserved capacity of the vector 'data'. This goal is
* achieved using the custom assignment operator.
* @brief copy constructor of a CascadeSquareMatrix
* @param other matrix which should be copied
*/
template <typename T>
CascadeSquareMatrix<T>::CascadeSquareMatrix(const CascadeSquareMatrix<T>& other)
{
    *this = other;
}

/**
* @brief assings a value to the Cascade square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline void CascadeSquareMatrix<T>::set(size_t i, size_t j, T value)
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");
    
    // accessing the matrix in the cascading way
    data[i >= j ? i * i + j : j * j + 2 * j - i] = value;
}

/**
* @brief returns a value of the Cascade square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline T CascadeSquareMatrix<T>::get(size_t i, size_t j) const
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");

    // accessing the matrix in the cascading way
    return data[i >= j ? i * i + j : j * j + 2 * j - i];
}

/**
* @brief extends the Cascade square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
* (optional)
*/
template <typename T>
inline void CascadeSquareMatrix<T>::extend(T placeholder)
{
    assert(this->m_size < this->m_capacity 
          && "The matrix cannot be extended anymore");

    // allocation of 2 * size + 1 new data cells
    data.insert(data.end(), 2 * this->m_size + 1, placeholder);
    
    // the size increases
    ++this->m_size;
}

/** This method provides a way to assign a CascadeSquareMatrix to the variable.
* The method ensure us to keep the reserved capacity of the vector 'data' since * the default vector assignment do not preserve it.
* @brief assignment operator for the CascadeSquareMatrix structure
* @param other matrix which should be copied assigned
*/
template <typename T>
CascadeSquareMatrix<T>& CascadeSquareMatrix<T>::operator=
                     (const CascadeSquareMatrix<T>& other)
{
    // initialization of the matrix
    this->m_capacity = other.capacity();
    this->m_size = 0;
    this->data = std::vector<T>();
    this->data.reserve(this->m_capacity * this->m_capacity);
    size_t otherSize = other.size();
    for(size_t i = 0; i < otherSize; ++i) {this->extend();}
    
    // copying memory cells
    for(size_t i = 0; i < this->m_size; ++i)
    {
        for(size_t j = 0; j < this->m_size; ++j)
        {
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
struct DynamicSquareMatrix : public ExtendableSquareMatrix<T>
{
    private:
 
        // data are stored in a single vector
        std::vector<std::vector<T>> data{};
    
    public:
    
        // constructors
        DynamicSquareMatrix(size_t maxRows, size_t initRows = 0);
        
        // implemented virtual functions
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        void extend(T placeholder = T()) override;
        
        // cloning
        DynamicSquareMatrix *clone(void) const 
            { return new DynamicSquareMatrix(*this); }
};

/**
* @brief creates a Dynamic square matrix
* @param maxRows capacity of the square matrix
* @param initRows initial size of the square matrix
*/
template <typename T>
DynamicSquareMatrix<T>::DynamicSquareMatrix
    (size_t maxRows, size_t initRows)
{
    assert(initRows <= maxRows && 
    "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Dynamic;
    this->m_capacity = maxRows;
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < initRows; ++i) {extend();}
}

/**
* @brief assings a value to the Dynamic square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline T DynamicSquareMatrix<T>::get(size_t i, size_t j) const
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");
    
    return data[i][j];
}

/**
* @brief returns a value of the Dynamic square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline void DynamicSquareMatrix<T>::set(size_t i, size_t j, T value)
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");
    
    data[i][j] = value;
}

/**
* @brief extends the Dynamic square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
*/
template <typename T>
void DynamicSquareMatrix<T>::extend(T placeholder)
{
    assert(this->m_size < this->m_capacity 
          && "The matrix cannot be extened anymore");
    
    // creating a new column      
    for(size_t i = 0; i < this->m_size; ++i)
    {
        data[i].push_back(placeholder);
    }
    
    // creating a new row
    data.push_back(std::vector<T>());
    ++this->m_size;
    data.back().insert(data.back().end(), this->m_size, placeholder);
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
struct HashedSquareMatrix : public ExtendableSquareMatrix<T>
{
    private:

        // data are stored in a hashmap
        mutable std::unordered_map<size_t, T> data{};
    
    public:
    
        // constructors
        HashedSquareMatrix(size_t maxRows, size_t initRows = 0);
        
        // implemented virtual functions        
        inline void set(size_t i, size_t j, T value) override;
        inline T get(size_t i, size_t j) const override;
        inline void extend(T placeholder = T()) override;

        // cloning
        HashedSquareMatrix *clone(void) const 
            { return new HashedSquareMatrix(*this); }
        
};

/**
* @brief creates a Hashed square matrix
* @param maxRows capacity of the square matrix
* @param initRows initial size of the square matrix
*/
template <typename T>
HashedSquareMatrix<T>::HashedSquareMatrix
    (size_t maxRows, size_t initRows)
{
    assert(initRows <= maxRows && 
    "Initial size of the matrix cannot be bigger than the capacity");
    
    this->m_type = MatrixType::Hashed;
    this->m_capacity = maxRows;
    
    // creating the initial size and filling the data cells with
    // default values
    for(size_t i = 0; i < initRows; ++i) {extend();}
}

/**
* @brief assings a value to the Hashed square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @param value value to be assigned to the square matrix data cell
*/
template <typename T>
inline void HashedSquareMatrix<T>::set(size_t i, size_t j, T value)
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");
    
    // accessing the hashmap using row matrix traversal
    data[i * this->m_capacity + j] = value;
}

/**
* @brief returns a value of the Hashed square matrix
* @param i row of the square matrix
* @param j column of the square matrix
* @return value found in the square matrix data cell
*/
template <typename T>
inline T HashedSquareMatrix<T>::get(size_t i, size_t j) const
{
    assert(i < this->m_size && "Nonexisting row cannot be accessed");
    assert(j < this->m_size && "Nonexisting column cannot be accessed");
    
    // accessing the hashmap using row matrix traversal
    return data[i * this->m_capacity + j];
}

/**
* @brief extends the Hashed square matrix by a new row and column
* @param placeholder a value which will be assigned to all the new data cells
*/
template <typename T>
inline void HashedSquareMatrix<T>::extend(T placeholder)
{
    assert(this->m_size < this->m_capacity 
          && "Matrix cannot be extened anymore");

    // creating a new row and column
    for(size_t i = 0; i < this->m_size; ++i)
    {
        data[this->m_size * this->m_capacity + i] = placeholder;
        data[i * this->m_capacity + this->m_size] = placeholder;
    }
    data[this->m_size * this->m_capacity + this->m_size] = placeholder;
    
    // increasing size
    ++this->m_size;
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
                        size_t capacity, size_t size = 0)
{
    switch(type)
    {
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
    const ExtendableSquareMatrix<T>& matrix)
{
    size_t size = matrix.size();
    size_t capacity = matrix.capacity();    
    std::string result = "\nSIZE: " + std::to_string(size) + "\n";
    result += "CAPACITY: " + std::to_string(capacity) + "\n";
    result += "MATRIX:\n";
    for(size_t i = 0; i < size; ++i)
    {
        for(size_t j = 0; j < size; ++j)
        {
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
bool ExtendableSquareMatrix<T>::isReflexive(void)
{
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i)
    {
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
bool ExtendableSquareMatrix<T>::isAntisymetric(void)
{
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i)
    {
        for(size_t j = 0; j < size; ++j)
        {
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
bool ExtendableSquareMatrix<T>::isTransitive(void)
{
    size_t size = this->size();
    for(size_t i = 0; i < size; ++i)
    {
        for(size_t j = 0; j < size; ++j)
        {
            bool found = false;
            for(size_t k = 0; k < size; ++k)
            {
                if(get(i, k) && get(k, j)) 
                { 
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
