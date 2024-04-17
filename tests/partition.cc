#include <catch2/catch.hpp>

#include "mata/utils/partition.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::Partition") {
    SECTION("Create simple partition with 1 block") {
        Partition p{10};
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 1);
        CHECK(p.num_of_nodes() == 1);
        CHECK(p.in_same_block({}));
        CHECK(p.in_same_block({0})); 
        CHECK(p.in_same_block(0, 1));
        CHECK(p.in_same_block(1, 8));
        CHECK(p.in_same_block({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        for(size_t i = 0; i < 10; ++i) {
            CHECK(p.get_block_item(i).state() == i);
            CHECK(p.get_block_item(i).idx() == i);
            CHECK(p.get_block_item(i).block().idx() == 0);
            CHECK(p.get_block_idx(i) == 0);
            CHECK(p.get_block_item(i).node().idx() == 0);
            CHECK(p.get_block_item(i).repr().idx() == 0);
            CHECK(p.get_block_item(i).first().idx() == 0);
            CHECK(p.get_block_item(i).last().idx() == 9);
            CHECK(p.get_block_item(i).node().first().idx() == 0);
            CHECK(p.get_block_item(i).node().last().idx() == 9);
            CHECK(p[i].idx() == i);
        }
        CHECK(p.get_block(0).idx() == 0);
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p.get_block(0).repr().idx() == 0);
        CHECK(p.get_block(0).first().idx() == 0);
        CHECK(p.get_block(0).last().idx() == 9);
        CHECK(p.get_block(0).size() == 10);
        CHECK(p.get_node(0).idx() == 0);        
        CHECK(p.get_node(0).first().idx() == 0);
        CHECK(p.get_node(0).last().idx() == 9);
        CHECK(p.get_node(0).size() == 10);
        CHECK(p.get_node(0).contains_block(0));
        for(auto& block_item : p.get_block(0)) {
            CHECK(block_item.block().idx() == 0);
        }
        for(auto& block_item : p.get_node(0)) {
            CHECK(block_item.node().idx() == 0);
        }
        CHECK(p.states_in_same_block(0).size() == 10);
        CHECK(p.partition().size() == 1);
    }
    
    SECTION("Create another simple partition with 1 block") {
        Partition p = Partition(10, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 1);
        CHECK(p.num_of_nodes() == 1);
        CHECK(p.in_same_block({}));
        CHECK(p.in_same_block({0})); 
        CHECK(p.in_same_block(0, 1));
        CHECK(p.in_same_block(1, 8));
        CHECK(p.in_same_block({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        for(size_t i = 0; i < 10; ++i) {
            CHECK(p.get_block_item(i).state() == i);
            CHECK(p.get_block_item(i).idx() == i);
            CHECK(p.get_block_item(i).block().idx() == 0);
            CHECK(p.get_block_item(i).node().idx() == 0);
            CHECK(p.get_block_item(i).repr().idx() == 0);
            CHECK(p.get_block_item(i).first().idx() == 0);
            CHECK(p.get_block_item(i).last().idx() == 9);
            CHECK(p.get_block_item(i).node().first().idx() == 0);
            CHECK(p.get_block_item(i).node().last().idx() == 9);
            CHECK(p[i].idx() == i);
        }
        CHECK(p.get_block(0).idx() == 0);
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p.get_block(0).repr().idx() == 0);
        CHECK(p.get_block(0).first().idx() == 0);
        CHECK(p.get_block(0).last().idx() == 9);
        CHECK(p.get_block(0).size() == 10);
        CHECK(p.get_node(0).idx() == 0);        
        CHECK(p.get_node(0).first().idx() == 0);
        CHECK(p.get_node(0).last().idx() == 9);
        CHECK(p.get_node(0).size() == 10);
        CHECK(p.get_node(0).contains_block(0));
        for(auto& block_item : p.get_block(0)) {
            CHECK(block_item.block().idx() == 0);
        }
        for(auto& block_item : p.get_node(0)) {
            CHECK(block_item.node().idx() == 0);
        }
        CHECK(p.states_in_same_block(0).size() == 10);
        CHECK(p.partition().size() == 1);
    }
    
    SECTION("Create a simple partition with 2 blocks") {
        Partition p{10, {{0, 5, 8}}};
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 2);
        CHECK(p.num_of_nodes() == 2);
        CHECK(p.in_same_block({}));
        CHECK(p.in_same_block({0})); 
        CHECK(p.in_same_block(0, 5));
        CHECK(p.in_same_block(5, 8));
        CHECK(!p.in_same_block(6, 5));
        CHECK(p.in_same_block({0, 5, 8}));
        CHECK(p.in_same_block({1, 2, 3, 4, 6, 7, 9}));
        CHECK(!p.in_same_block({1, 2, 3, 4, 5, 7, 9}));
        CHECK(p[0].idx() == 0);
        CHECK(p[0].state() == 0);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[0].node().idx() == 0);
        CHECK(p.get_block_item(0).node().idx() == 0);
        CHECK(p.get_block_item(0).repr().idx() == 0);
        CHECK(p.get_block_item(0).first().idx() == 0);
        CHECK(p.get_block_item(0).last().idx() == 2);
        CHECK(p[1].idx() == 3);
        CHECK(p.get_block_item(3).state() == 1);
        CHECK(p.get_block_item(3).repr().state() == 1);
        CHECK(p.get_block_item(3).first().state() == 1);
        CHECK(p.get_block_item(3).last().state() == 9);
        CHECK(p[1].block().idx() == 1);
        CHECK(p[1].node().idx() == 1);
        CHECK(p.get_block_item(3).block().idx() == 1);
        CHECK(p.get_block_item(3).node().idx() == 1);
        CHECK(p.get_block(0).repr().state() == 0);
        CHECK(p.get_block(1).repr().state() == 1);
        CHECK(p.get_block(0).repr().block().idx() == 0);
        CHECK(p.get_block(1).repr().block().idx() == 1);
        CHECK(p.get_block(0).size() == 3);
        CHECK(p.get_block(1).size() == 7);
        CHECK(p.get_node(0).repr().state() == 0);
        CHECK(p.get_node(0).repr().block().idx() == 0);
        CHECK(p.get_node(0).contains_block(0));
        CHECK(!p.get_node(0).contains_block(1));
        CHECK(p.get_node(1).repr().state() == 1);
        CHECK(p.get_node(1).repr().block().idx() == 1);
        CHECK(p.get_node(0).first().idx() == 0);
        CHECK(p.get_node(0).last().idx() == 2);
        CHECK(p.get_node(1).first().idx() == 3);
        CHECK(p.get_node(1).last().idx() == 9);
        CHECK(!p.get_node(1).contains_block(0));
        CHECK(p.get_node(1).contains_block(1));
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p.get_block(1).node().idx() == 1);
        CHECK(p.states_in_same_block(0).size() == 3);
        CHECK(p.states_in_same_block(1).size() == 7);
        CHECK(p.partition().size() == 2);
    }
    
    SECTION("Create a simple partition with 3 blocks") {
        Partition p{6, {{0}, {1, 2}}};
        CHECK(p.num_of_states() == 6);
        CHECK(p.num_of_block_items() == 6);
        CHECK(p.num_of_blocks() == 3);
        CHECK(p.num_of_nodes() == 3);
        CHECK(p.in_same_block({}));
        CHECK(p.in_same_block({0})); 
        CHECK(p.in_same_block(3, 5));
        CHECK(p.in_same_block(1, 2));
        CHECK(!p.in_same_block(1, 4));
        CHECK(p.in_same_block({3, 4, 5}));
        CHECK(!p.in_same_block({2, 3, 4, 5}));
        for(size_t i = 0; i <= 5; ++i) {
            CHECK(p[i].idx() == i);
            CHECK(p[i].state() == i);
        }
        CHECK(p[0].block().idx() == 0);
        CHECK(p[0].node().idx() == 0);            
        CHECK(p.get_block_item(0).block().idx() == 0);
        CHECK(p.get_block_item(0).repr().idx() == 0);
        CHECK(p.get_block_item(0).first().idx() == 0);
        CHECK(p.get_block_item(0).last().idx() == 0);
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p[1].block().idx() == 1);
        CHECK(p[1].node().idx() == 1);            
        CHECK(p.get_block_item(1).block().idx() == 1);
        CHECK(p.get_block_item(1).node().idx() == 1);
        CHECK(p.get_block_item(1).repr().idx() == 1);
        CHECK(p.get_block_item(1).first().idx() == 1);
        CHECK(p.get_block_item(1).last().idx() == 2);
        CHECK(p.get_block(0).repr().state() == 0);
        CHECK(p.get_block(1).repr().state() == 1);
        CHECK(p.get_block(2).repr().state() == 3);
        CHECK(p.get_node(0).repr().state() == 0);
        CHECK(p.get_node(1).repr().state() == 1);
        CHECK(p.get_node(2).repr().state() == 3);
        CHECK(p.get_node(0).first().idx() == 0);
        CHECK(p.get_node(0).last().idx() == 0);
        CHECK(p.get_node(1).first().idx() == 1);
        CHECK(p.get_node(1).last().idx() == 2);
        CHECK(p.get_node(2).first().idx() == 3);
        CHECK(p.get_node(2).last().idx() == 5); 
        CHECK(p.get_node(0).contains_block(0));
        CHECK(!p.get_node(0).contains_block(1));
        CHECK(!p.get_node(0).contains_block(2));
        CHECK(!p.get_node(1).contains_block(0));
        CHECK(p.get_node(1).contains_block(1));
        CHECK(!p.get_node(1).contains_block(2));
        CHECK(!p.get_node(2).contains_block(0));
        CHECK(!p.get_node(2).contains_block(1));
        CHECK(p.get_node(2).contains_block(2));       
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p.get_block(1).node().idx() == 1);
        CHECK(p.get_block(2).node().idx() == 2);
        CHECK(p.get_block(0).size() == 1);
        CHECK(p.get_block(1).size() == 2);
        CHECK(p.get_block(2).size() == 3);
        CHECK(p.get_block_item(3).repr().idx() == 3);
        CHECK(p.get_block_item(3).first().idx() == 3);
        CHECK(p.get_block_item(3).last().idx() == 5);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 2);
        CHECK(p.states_in_same_block(3).size() == 3);
        CHECK(p.partition().size() == 3);
    }
    
    SECTION("Splitting blocks") {
        Partition p{10};
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 1);
        CHECK(p.num_of_nodes() == 1);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 0);
        CHECK(p.in_same_block({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        CHECK(p.states_in_same_block(0).size() == 10);
        CHECK(p.partition().size() == 1);
        CHECK(p.get_block(0).size() == 10);
        CHECK(p.get_node(0).contains_block(0));
        p.split_blocks({0, 1, 2, 3, 4});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 2);
        CHECK(p.num_of_nodes() == 3);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 1);
        CHECK(p.in_same_block({0, 1, 2, 3, 4}));
        CHECK(p.in_same_block({5, 6, 7, 8, 9}));
        CHECK(p.states_in_same_block(0).size() == 5);
        CHECK(p.states_in_same_block(5).size() == 5);
        CHECK(p.partition().size() == 2);
        CHECK(p.get_block(0).size() == 5);
        CHECK(p.get_block(1).size() == 5);
        CHECK(p.get_node(0).contains_block(0));
        CHECK(p.get_node(0).contains_block(1));
        CHECK(p.get_node(1).contains_block(0));
        CHECK(!p.get_node(1).contains_block(1));
        CHECK(!p.get_node(2).contains_block(0));
        CHECK(p.get_node(2).contains_block(1));
        p.split_blocks({0, 1, 2, 5, 6, 7});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 4);
        CHECK(p.num_of_nodes() == 7);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 3);
        CHECK(p.in_same_block({0, 1, 2}));
        CHECK(p.in_same_block({3, 4}));
        CHECK(p.in_same_block({5, 6, 7}));
        CHECK(p.in_same_block({8, 9}));
        CHECK(p.states_in_same_block(0).size() == 3);
        CHECK(p.states_in_same_block(3).size() == 2);
        CHECK(p.states_in_same_block(5).size() == 3);
        CHECK(p.states_in_same_block(8).size() == 2);
        CHECK(p.partition().size() == 4);
        CHECK(p.get_node(0).contains_block(0));
        CHECK(p.get_node(0).contains_block(1));
        CHECK(p.get_node(0).contains_block(2));
        CHECK(p.get_node(0).contains_block(3));
        CHECK(p.get_node(1).contains_block(0));
        CHECK(!p.get_node(1).contains_block(1));
        CHECK(p.get_node(1).contains_block(2));
        CHECK(!p.get_node(1).contains_block(3));
        CHECK(!p.get_node(2).contains_block(0));
        CHECK(p.get_node(2).contains_block(1));
        CHECK(!p.get_node(2).contains_block(2));
        CHECK(p.get_node(2).contains_block(3));
        CHECK(p.get_node(3).contains_block(0));
        CHECK(!p.get_node(3).contains_block(1));
        CHECK(!p.get_node(3).contains_block(2));
        CHECK(!p.get_node(3).contains_block(3));
        CHECK(!p.get_node(4).contains_block(0));
        CHECK(!p.get_node(4).contains_block(1));
        CHECK(p.get_node(4).contains_block(2));
        CHECK(!p.get_node(4).contains_block(3));
        CHECK(!p.get_node(5).contains_block(0));
        CHECK(p.get_node(5).contains_block(1));
        CHECK(!p.get_node(5).contains_block(2));
        CHECK(!p.get_node(5).contains_block(3));
        CHECK(!p.get_node(6).contains_block(0));
        CHECK(!p.get_node(6).contains_block(1));
        CHECK(!p.get_node(6).contains_block(2));
        CHECK(p.get_node(6).contains_block(3));
        p.split_blocks({0, 3, 5, 8});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 8);
        CHECK(p.num_of_nodes() == 15);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.in_same_block({0}));
        CHECK(p.in_same_block({1, 2}));
        CHECK(p.in_same_block({3}));
        CHECK(p.in_same_block({4}));
        CHECK(p.in_same_block({5}));
        CHECK(p.in_same_block({6, 7}));
        CHECK(p.in_same_block({8}));
        CHECK(p.in_same_block({9}));
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 2);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 2);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 8);
        p.split_blocks({1, 6});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 10);
        CHECK(p.num_of_nodes() == 19);          
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 1);
        CHECK(p.states_in_same_block(2).size() == 1);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 1);
        CHECK(p.states_in_same_block(7).size() == 1);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 10);
        p.split_blocks({0, 2, 4, 6, 8});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 10);
        CHECK(p.num_of_nodes() == 19);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 1);
        CHECK(p.states_in_same_block(2).size() == 1);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 1);
        CHECK(p.states_in_same_block(7).size() == 1);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 10);
    }
    
    SECTION("Complicated blocks splitting with swapping") {
        Partition p{10};
        p.split_blocks({0, 2, 4, 6, 8});
        CHECK(p.in_same_block(0, 2));
        CHECK(p.in_same_block(0, 4));
        CHECK(p.in_same_block(0, 6));
        CHECK(p.in_same_block(0, 8));
        CHECK(!p.in_same_block(0, 1));
        CHECK(!p.in_same_block(0, 3));
        CHECK(!p.in_same_block(0, 5));
        CHECK(!p.in_same_block(0, 7));
        CHECK(!p.in_same_block(0, 9));
        p.split_blocks({1, 9});
        CHECK(p.in_same_block(1, 9));
        CHECK(!p.in_same_block(1, 3));
        CHECK(!p.in_same_block(1, 5));
        CHECK(!p.in_same_block(1, 7));
    }

    SECTION("Custom copying and assigning with splitting") {
        Partition p = Partition(5, {{2, 3}});
        p.split_blocks({0});
        
        Partition q = p;
        Partition r = Partition(p);
        
        CHECK(p.num_of_states() == q.num_of_states());
        CHECK(p.num_of_states() == r.num_of_states());
        CHECK(p.num_of_block_items() == q.num_of_block_items());
        CHECK(p.num_of_block_items() == r.num_of_block_items());
        CHECK(p.num_of_blocks() == q.num_of_blocks());
        CHECK(p.num_of_blocks() == r.num_of_blocks());        
        CHECK(p.num_of_nodes() == q.num_of_nodes());
        CHECK(p.num_of_nodes() == r.num_of_nodes());          
         
        size_t statesNum = p.num_of_states();           
        size_t blocksNum = p.num_of_blocks();
        size_t nodesNum = p.num_of_nodes();
        
        for(size_t i = 0; i < statesNum; ++i) {
            CHECK(p[i].idx() == q[i].idx());
            CHECK(p[i].idx() == r[i].idx());
            CHECK(p[i].state() == q[i].state());
            CHECK(p[i].state() == r[i].state());           
            CHECK(p[i].block().idx() == q[i].block().idx());
            CHECK(p[i].block().idx() == r[i].block().idx());
        }
         
        for(size_t i = 0; i < blocksNum; ++i) {
            CHECK(p[i].node().idx() == q[i].node().idx());
            CHECK(p[i].node().idx() == r[i].node().idx());           
        } 
          
        for(size_t i = 0; i < nodesNum; ++i) {
            CHECK(p[i].node().first().idx() == q[i].node().first().idx());
            CHECK(p[i].node().first().idx() == r[i].node().first().idx());           
            CHECK(p[i].node().last().idx() == q[i].node().last().idx());
            CHECK(p[i].node().last().idx() == r[i].node().last().idx());
        }
        
        std::cout << q;                        
        r.split_blocks({1, 2});
        r.split_blocks({1, 2});
        std::cout << r;
    }

    SECTION("Custom copying and assigning without splitting") {
        Partition q{6, {{0}, {1, 2}}};
        Partition p = q;
        CHECK(p.num_of_states() == 6);
        CHECK(p.num_of_block_items() == 6);
        CHECK(p.num_of_blocks() == 3);
        CHECK(p.num_of_nodes() == 3);
        CHECK(p.in_same_block({}));
        CHECK(p.in_same_block({0})); 
        CHECK(p.in_same_block(3, 5));
        CHECK(p.in_same_block(1, 2));
        CHECK(!p.in_same_block(1, 4));
        CHECK(p.in_same_block({3, 4, 5}));
        CHECK(!p.in_same_block({2, 3, 4, 5}));
        for(size_t i = 0; i <= 5; ++i) {
            CHECK(p[i].idx() == i);
            CHECK(p[i].state() == i);
        }
        CHECK(p[0].block().idx() == 0);
        CHECK(p[0].node().idx() == 0);            
        CHECK(p.get_block_item(0).block().idx() == 0);
        CHECK(p.get_block_item(0).repr().idx() == 0);
        CHECK(p.get_block_item(0).first().idx() == 0);
        CHECK(p.get_block_item(0).last().idx() == 0);
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p[1].block().idx() == 1);
        CHECK(p[1].node().idx() == 1);            
        CHECK(p.get_block_item(1).block().idx() == 1);
        CHECK(p.get_block_item(1).node().idx() == 1);
        CHECK(p.get_block_item(1).repr().idx() == 1);
        CHECK(p.get_block_item(1).first().idx() == 1);
        CHECK(p.get_block_item(1).last().idx() == 2);
        CHECK(p.get_block(0).repr().state() == 0);
        CHECK(p.get_block(1).repr().state() == 1);
        CHECK(p.get_block(2).repr().state() == 3);
        CHECK(p.get_node(0).repr().state() == 0);
        CHECK(p.get_node(1).repr().state() == 1);
        CHECK(p.get_node(2).repr().state() == 3);
        CHECK(p.get_node(0).first().idx() == 0);
        CHECK(p.get_node(0).last().idx() == 0);
        CHECK(p.get_node(1).first().idx() == 1);
        CHECK(p.get_node(1).last().idx() == 2);
        CHECK(p.get_node(2).first().idx() == 3);
        CHECK(p.get_node(2).last().idx() == 5);        
        CHECK(p.get_block(0).node().idx() == 0);
        CHECK(p.get_block(1).node().idx() == 1);
        CHECK(p.get_block(2).node().idx() == 2);
        CHECK(p.get_block_item(3).repr().idx() == 3);
        CHECK(p.get_block_item(3).first().idx() == 3);
        CHECK(p.get_block_item(3).last().idx() == 5);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 2);
        CHECK(p.states_in_same_block(3).size() == 3);
        CHECK(p.partition().size() == 3);
    }

    SECTION("Another splitting blocks with partition copying") {
        Partition q{10};
        Partition p = q;
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 1);
        CHECK(p.num_of_nodes() == 1);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 0);
        CHECK(p.in_same_block({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        CHECK(p.states_in_same_block(0).size() == 10);
        CHECK(p.partition().size() == 1);
        p.split_blocks({0, 1, 2, 3, 4});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 2);
        CHECK(p.num_of_nodes() == 3);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 1);
        CHECK(p.in_same_block({0, 1, 2, 3, 4}));
        CHECK(p.in_same_block({5, 6, 7, 8, 9}));
        CHECK(p.states_in_same_block(0).size() == 5);
        CHECK(p.states_in_same_block(5).size() == 5);
        CHECK(p.partition().size() == 2);
        p.split_blocks({0, 1, 2, 5, 6, 7});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 4);
        CHECK(p.num_of_nodes() == 7);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 3);
        CHECK(p.in_same_block({0, 1, 2}));
        CHECK(p.in_same_block({3, 4}));
        CHECK(p.in_same_block({5, 6, 7}));
        CHECK(p.in_same_block({8, 9}));
        CHECK(p.states_in_same_block(0).size() == 3);
        CHECK(p.states_in_same_block(3).size() == 2);
        CHECK(p.states_in_same_block(5).size() == 3);
        CHECK(p.states_in_same_block(8).size() == 2);
        CHECK(p.partition().size() == 4);
        p.split_blocks({0, 3, 5, 8});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 8);
        CHECK(p.num_of_nodes() == 15);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.in_same_block({0}));
        CHECK(p.in_same_block({1, 2}));
        CHECK(p.in_same_block({3}));
        CHECK(p.in_same_block({4}));
        CHECK(p.in_same_block({5}));
        CHECK(p.in_same_block({6, 7}));
        CHECK(p.in_same_block({8}));
        CHECK(p.in_same_block({9}));
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 2);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 2);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 8);
        p.split_blocks({1, 6});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 10);
        CHECK(p.num_of_nodes() == 19);          
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 1);
        CHECK(p.states_in_same_block(2).size() == 1);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 1);
        CHECK(p.states_in_same_block(7).size() == 1);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 10);
        p.split_blocks({0, 2, 4, 6, 8});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 10);
        CHECK(p.num_of_nodes() == 19);
        CHECK(p[0].block().idx() == 0);
        CHECK(p[9].block().idx() == 7);
        CHECK(p.states_in_same_block(0).size() == 1);
        CHECK(p.states_in_same_block(1).size() == 1);
        CHECK(p.states_in_same_block(2).size() == 1);
        CHECK(p.states_in_same_block(3).size() == 1);
        CHECK(p.states_in_same_block(4).size() == 1);
        CHECK(p.states_in_same_block(5).size() == 1);
        CHECK(p.states_in_same_block(6).size() == 1);
        CHECK(p.states_in_same_block(7).size() == 1);
        CHECK(p.states_in_same_block(8).size() == 1);
        CHECK(p.states_in_same_block(9).size() == 1);
        CHECK(p.partition().size() == 10);
        std::cout << p;
    }
    
    SECTION("Another complicated blocks splitting with swapping and copying") {
        Partition q{10};
        Partition p = q;
        p.split_blocks({0, 2, 4, 6, 8});
        CHECK(p.in_same_block(0, 2));
        CHECK(p.in_same_block(0, 4));
        CHECK(p.in_same_block(0, 6));
        CHECK(p.in_same_block(0, 8));
        CHECK(!p.in_same_block(0, 1));
        CHECK(!p.in_same_block(0, 3));
        CHECK(!p.in_same_block(0, 5));
        CHECK(!p.in_same_block(0, 7));
        CHECK(!p.in_same_block(0, 9));
        p.split_blocks({1, 9});
        CHECK(p.in_same_block(1, 9));
        CHECK(!p.in_same_block(1, 3));
        CHECK(!p.in_same_block(1, 5));
        CHECK(!p.in_same_block(1, 7));
        std::cout << p;
    }
    
    SECTION("Partition over an empty set") {
        Partition q{0};
        Partition p = q;
        p.split_blocks({});
        CHECK(!p.num_of_states());
        CHECK(!p.num_of_block_items());
        CHECK(!p.num_of_blocks());
        CHECK(!p.num_of_nodes());
        CHECK(!p.partition().size());
        std::cout << p;
    }
    
    SECTION("Partition iterators") {
        Partition p = Partition(8, {{0, 1}, {2, 3, 4, 5}});
        size_t index = 0;
        for(auto& block_item : p.get_block(0)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 0);
            CHECK(block_item.node().idx() == 0);
            CHECK(block_item.state() == index);
            ++index;
        }
        for(auto& block_item : p.get_block(1)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 1);
            CHECK(block_item.node().idx() == 1);
            CHECK(block_item.state() == index);
            ++index;
        }
        for(auto& block_item : p.get_block(2)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 2);
            CHECK(block_item.node().idx() == 2);
            CHECK(block_item.state() == index);
            ++index;
        }
        index = 0;
        for(auto& block_item : p.get_node(0)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 0);
            CHECK(block_item.node().idx() == 0);
            CHECK(block_item.state() == index);
            ++index;
        }
        for(auto& block_item : p.get_node(1)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 1);
            CHECK(block_item.node().idx() == 1);
            CHECK(block_item.state() == index);
            ++index;
        }
        for(auto& block_item : p.get_node(2)) {
            CHECK(block_item.idx() == index);
            CHECK(block_item.block().idx() == 2);
            CHECK(block_item.node().idx() == 2);
            CHECK(block_item.state() == index);
            ++index;
        }
    }

}
