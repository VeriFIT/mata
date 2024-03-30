#include <catch2/catch.hpp>

#include "mata/utils/partition.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::Partition") {

    SECTION("Create a simple partition with 1 block") {
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
            CHECK(p.get_block_item_idx_from_state(i) == i);
            CHECK(p.get_block_idx_from_state(i) == 0);
            CHECK(p.get_node_idx_from_state(i) == 0);
            CHECK(p.get_block_item(i).state == i);
            CHECK(p.get_block_item(i).block_idx == 0);
            CHECK(p.get_node_idx_from_block_item_idx(i) == 0);
        }
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(0)).state == 0);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_block_idx(0)).block_idx == 0);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_node_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(0)).block_idx == 0);
        CHECK(p.get_node(0).first == 0);
        CHECK(p.get_node(0).last == 9);
        CHECK(p.get_block(0).node_idx == 0);
        
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
            CHECK(p.get_block_item_idx_from_state(i) == i);
            CHECK(p.get_block_idx_from_state(i) == 0);
            CHECK(p.get_node_idx_from_state(i) == 0);
            CHECK(p.get_block_item(i).state == i);
            CHECK(p.get_block_item(i).block_idx == 0);
            CHECK(p.get_node_idx_from_block_item_idx(i) == 0);
        }
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(0)).state == 0);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_block_idx(0)).block_idx == 0);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_node_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(0)).block_idx == 0);
        CHECK(p.get_node(0).first == 0);
        CHECK(p.get_node(0).last == 9);
        CHECK(p.get_block(0).node_idx == 0);
        
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
        
        CHECK(p.get_block_item_idx_from_state(0) == 0);
        CHECK(p.get_block_item(0).state == 0);
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_node_idx_from_state(0) == 0);            
        CHECK(p.get_block_item(0).block_idx == 0);
        CHECK(p.get_node_idx_from_block_item_idx(0) == 0);
                
        CHECK(p.get_block_item_idx_from_state(1) == 3);
        CHECK(p.get_block_item(3).state == 1);
        CHECK(p.get_block_idx_from_state(1) == 1);
        CHECK(p.get_node_idx_from_state(1) == 1);            
        CHECK(p.get_block_item(3).block_idx == 1);
        CHECK(p.get_node_idx_from_block_item_idx(3) == 1);
        
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(1)).state == 1);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_block_idx(0)).block_idx == 0);
        CHECK(p.get_block_item(
            p.get_repr_idx_from_block_idx(1)).block_idx == 1);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(0)).block_idx == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(1)).state == 1);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(1)).block_idx == 1);
        CHECK(p.get_node(0).first == 0);
        CHECK(p.get_node(0).last == 2);
        CHECK(p.get_node(1).first == 3);
        CHECK(p.get_node(1).last == 9);
        CHECK(p.get_block(0).node_idx == 0);
        CHECK(p.get_block(1).node_idx == 1);
        
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
            CHECK(p.get_block_item_idx_from_state(i) == i);
            CHECK(p.get_block_item(i).state == i);
        }
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_node_idx_from_state(0) == 0);            
        CHECK(p.get_block_item(0).block_idx == 0);
        CHECK(p.get_node_idx_from_block_item_idx(0) == 0);
        CHECK(p.get_block_idx_from_state(1) == 1);
        CHECK(p.get_node_idx_from_state(1) == 1);            
        CHECK(p.get_block_item(1).block_idx == 1);
        CHECK(p.get_node_idx_from_block_item_idx(1) == 1);
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(1)).state == 1);
        CHECK(p.get_block_item(p.get_repr_idx_from_block_idx(2)).state == 3);        
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(0)).state == 0);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(1)).state == 1);
        CHECK(p.get_block_item(p.get_repr_idx_from_node_idx(2)).state == 3);
        CHECK(p.get_node(0).first == 0);
        CHECK(p.get_node(0).last == 0);
        CHECK(p.get_node(1).first == 1);
        CHECK(p.get_node(1).last == 2);
        CHECK(p.get_node(2).first == 3);
        CHECK(p.get_node(2).last == 5);        
        CHECK(p.get_block(0).node_idx == 0);
        CHECK(p.get_block(1).node_idx == 1);
        CHECK(p.get_block(2).node_idx == 2);

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
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 0);
        CHECK(p.in_same_block({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        CHECK(p.states_in_same_block(0).size() == 10);
        CHECK(p.partition().size() == 1);
        p.split_blocks({0, 1, 2, 3, 4});
        CHECK(p.num_of_states() == 10);
        CHECK(p.num_of_block_items() == 10);
        CHECK(p.num_of_blocks() == 2);
        CHECK(p.num_of_nodes() == 3);
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 1);
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
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 3);
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
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 7);
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
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 7);
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
        CHECK(p.get_block_idx_from_state(0) == 0);
        CHECK(p.get_block_idx_from_state(9) == 7);
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
    
    SECTION("Custom copying and assigning")
    {
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
            CHECK(p.get_block_item_idx_from_state(i) == 
                  q.get_block_item_idx_from_state(i));
            CHECK(p.get_block_item_idx_from_state(i) ==
                  r.get_block_item_idx_from_state(i));
            CHECK(p.get_block_item(i).state == q.get_block_item(i).state);
            CHECK(p.get_block_item(i).state == r.get_block_item(i).state);           
            CHECK(p.get_block_item(i).block_idx == 
                q.get_block_item(i).block_idx);
            CHECK(p.get_block_item(i).block_idx == 
                r.get_block_item(i).block_idx);
        }
         
        for(size_t i = 0; i < blocksNum; ++i) {
            CHECK(p.get_block(i).node_idx == q.get_block(i).node_idx);
            CHECK(p.get_block(i).node_idx == r.get_block(i).node_idx);           
        } 
          
        for(size_t i = 0; i < nodesNum; ++i) {
            CHECK(p.get_node(i).first == q.get_node(i).first);
            CHECK(p.get_node(i).first == r.get_node(i).first);           
            CHECK(p.get_node(i).last == q.get_node(i).last);
            CHECK(p.get_node(i).last == r.get_node(i).last);
        }
                                     
        q.split_blocks({1, 2});
        r.split_blocks({1, 2});
        
        std::cout << q;
        std::cout << r;
    }  

}
