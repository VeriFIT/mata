#include <catch2/catch.hpp>

#include "mata/utils/partition-relation-pair.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::Partition") {

    SECTION("Create a simple partition with 1 block") {
        Partition p{10};
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 1);
        CHECK(p.numOfNodes() == 1);
        CHECK(p.inSameBlock({}));
        CHECK(p.inSameBlock({0})); 
        CHECK(p.inSameBlock(0, 1));
        CHECK(p.inSameBlock(1, 8));
        CHECK(p.inSameBlock({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        for(size_t i = 0; i < 10; ++i)
        {
            CHECK(p.getBlockItemIdxFromState(i) == i);
            CHECK(p.getBlockIdxFromState(i) == 0);
            CHECK(p.getNodeIdxFromState(i) == 0);
            CHECK(p.getBlockItem(i).state == i);
            CHECK(p.getBlockItem(i).blockIdx == 0);
            CHECK(p.getNodeIdxFromBlockItemIdx(i) == 0);
        }
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(0)).blockIdx == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(0)).blockIdx == 0);
        CHECK(p.getNode(0).first == 0);
        CHECK(p.getNode(0).last == 9);
        CHECK(p.getBlock(0).nodeIdx == 0);
    }
    
    SECTION("Create a simple partition with 2 blocks") {
        Partition p{10, {{0, 5, 8}}};
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 2);
        CHECK(p.numOfNodes() == 2);
        CHECK(p.inSameBlock({}));
        CHECK(p.inSameBlock({0})); 
        CHECK(p.inSameBlock(0, 5));
        CHECK(p.inSameBlock(5, 8));
        CHECK(!p.inSameBlock(6, 5));
        CHECK(p.inSameBlock({0, 5, 8}));
        CHECK(p.inSameBlock({1, 2, 3, 4, 6, 7, 9}));
        CHECK(!p.inSameBlock({1, 2, 3, 4, 5, 7, 9}));
        
        CHECK(p.getBlockItemIdxFromState(0) == 0);
        CHECK(p.getBlockItem(0).state == 0);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getNodeIdxFromState(0) == 0);            
        CHECK(p.getBlockItem(0).blockIdx == 0);
        CHECK(p.getNodeIdxFromBlockItemIdx(0) == 0);
                
        CHECK(p.getBlockItemIdxFromState(1) == 3);
        CHECK(p.getBlockItem(3).state == 1);
        CHECK(p.getBlockIdxFromState(1) == 1);
        CHECK(p.getNodeIdxFromState(1) == 1);            
        CHECK(p.getBlockItem(3).blockIdx == 1);
        CHECK(p.getNodeIdxFromBlockItemIdx(3) == 1);
        
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(1)).state == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(0)).blockIdx == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(1)).blockIdx == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(0)).blockIdx == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(1)).state == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(1)).blockIdx == 1);
        CHECK(p.getNode(0).first == 0);
        CHECK(p.getNode(0).last == 2);
        CHECK(p.getNode(1).first == 3);
        CHECK(p.getNode(1).last == 9);
        CHECK(p.getBlock(0).nodeIdx == 0);
        CHECK(p.getBlock(1).nodeIdx == 1);
    }


    SECTION("Create a simple partition with 3 blocks") {
        Partition p{6, {{0}, {1, 2}}};
        CHECK(p.numOfStates() == 6);
        CHECK(p.numOfBlockItems() == 6);
        CHECK(p.numOfBlocks() == 3);
        CHECK(p.numOfNodes() == 3);
        CHECK(p.inSameBlock({}));
        CHECK(p.inSameBlock({0})); 
        CHECK(p.inSameBlock(3, 5));
        CHECK(p.inSameBlock(1, 2));
        CHECK(!p.inSameBlock(1, 4));
        CHECK(p.inSameBlock({3, 4, 5}));
        CHECK(!p.inSameBlock({2, 3, 4, 5}));
        for(size_t i = 0; i <= 5; ++i)
        {
            CHECK(p.getBlockItemIdxFromState(i) == i);
            CHECK(p.getBlockItem(i).state == i);
        }
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getNodeIdxFromState(0) == 0);            
        CHECK(p.getBlockItem(0).blockIdx == 0);
        CHECK(p.getNodeIdxFromBlockItemIdx(0) == 0);
        CHECK(p.getBlockIdxFromState(1) == 1);
        CHECK(p.getNodeIdxFromState(1) == 1);            
        CHECK(p.getBlockItem(1).blockIdx == 1);
        CHECK(p.getNodeIdxFromBlockItemIdx(1) == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(1)).state == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromBlockIdx(2)).state == 3);        
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(0)).state == 0);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(1)).state == 1);
        CHECK(p.getBlockItem(p.getReprIdxFromNodeIdx(2)).state == 3);
        CHECK(p.getNode(0).first == 0);
        CHECK(p.getNode(0).last == 0);
        CHECK(p.getNode(1).first == 1);
        CHECK(p.getNode(1).last == 2);
        CHECK(p.getNode(2).first == 3);
        CHECK(p.getNode(2).last == 5);        
        CHECK(p.getBlock(0).nodeIdx == 0);
        CHECK(p.getBlock(1).nodeIdx == 1);
        CHECK(p.getBlock(2).nodeIdx == 2);
    }
    
    SECTION("Splitting blocks") {
        Partition p{10};
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 1);
        CHECK(p.numOfNodes() == 1);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 0);
        CHECK(p.inSameBlock({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        p.splitBlocks({0, 1, 2, 3, 4});
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 2);
        CHECK(p.numOfNodes() == 3);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 1);
        CHECK(p.inSameBlock({0, 1, 2, 3, 4}));
        CHECK(p.inSameBlock({5, 6, 7, 8, 9}));
        p.splitBlocks({0, 1, 2, 5, 6, 7});
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 4);
        CHECK(p.numOfNodes() == 7);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 3);
        CHECK(p.inSameBlock({0, 1, 2}));
        CHECK(p.inSameBlock({3, 4}));
        CHECK(p.inSameBlock({5, 6, 7}));
        CHECK(p.inSameBlock({8, 9}));
        p.splitBlocks({0, 3, 5, 8});
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 8);
        CHECK(p.numOfNodes() == 15);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 7);
        CHECK(p.inSameBlock({0}));
        CHECK(p.inSameBlock({1, 2}));
        CHECK(p.inSameBlock({3}));
        CHECK(p.inSameBlock({4}));
        CHECK(p.inSameBlock({5}));
        CHECK(p.inSameBlock({6, 7}));
        CHECK(p.inSameBlock({8}));
        CHECK(p.inSameBlock({9}));
        p.splitBlocks({1, 6});
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 10);
        CHECK(p.numOfNodes() == 19);          
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 7);
        p.splitBlocks({0, 2, 4, 6, 8});
        CHECK(p.numOfStates() == 10);
        CHECK(p.numOfBlockItems() == 10);
        CHECK(p.numOfBlocks() == 10);
        CHECK(p.numOfNodes() == 19);
        CHECK(p.getBlockIdxFromState(0) == 0);
        CHECK(p.getBlockIdxFromState(9) == 7);
    }    

}


TEST_CASE("mata::utils::ExtendableSquareMatrix") {

    SECTION("CascadeSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = 
            new CascadeSquareMatrix<unsigned long>(5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));        
        e->set(0, 0, 1);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        std::cout << (*e);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(!isTransitive(e));
        delete e;              
        }
        
    SECTION("DynamicSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = 
            new DynamicSquareMatrix<unsigned long>(5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));        
        e->set(0, 0, 1);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        std::cout << (*e);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(!isTransitive(e));
        delete e;              
        }
        
    SECTION("HashedSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = 
            new HashedSquareMatrix<unsigned long>(5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));        
        e->set(0, 0, 1);
        CHECK(!isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(isTransitive(e));                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        std::cout << (*e);
        CHECK(isReflexive(e));
        CHECK(isAntisymetric(e));
        CHECK(!isTransitive(e));
        delete e;              
        }
}
