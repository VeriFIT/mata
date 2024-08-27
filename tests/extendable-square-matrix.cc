#include <catch2/catch.hpp>

#include "mata/utils/extendable-square-matrix.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::ExtendableSquareMatrix") {

    SECTION("CascadeSquareMatrix") {

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e =
            create<unsigned long>(Cascade, 5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(!e->is_transitive());
        }
        
    SECTION("DynamicSquareMatrix") {

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e =
            create<unsigned long>(Dynamic, 5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(!e->is_transitive());       
        }
        
    SECTION("HashedSquareMatrix") {

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e =
            create<unsigned long>(Hashed, 5, 2);
        CHECK(e->size() == 2);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 3);
        CHECK(e->capacity() == 5);
        e->extend();
        CHECK(e->size() == 4);
        CHECK(e->capacity() == 5);
        CHECK(e->get(0, 0) == 0);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymmetric());
        CHECK(!e->is_transitive());       
        }

    SECTION("Matrix of the None type") {

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e =
            create<unsigned long>(None, 5, 2);
        CHECK(e == nullptr);
    
    }

    SECTION("Empty matrices") {
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e1 =
            create<unsigned long>(Cascade, 5, 0);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e2 =
            create<unsigned long>(Dynamic, 5, 0);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e3 =
            create<unsigned long>(Hashed, 5, 0);

        CHECK(!e1->size());
        CHECK(!e2->size());
        CHECK(!e3->size());

        CHECK(e1->capacity() == 5);
        CHECK(e2->capacity() == 5);
        CHECK(e3->capacity() == 5);

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c1 =
            e1->clone(); 
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c2 = 
            e2->clone();
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c3 = 
            e3->clone();

        CHECK(!c1->size());
        CHECK(!c2->size());
        CHECK(!c3->size());

        CHECK(c1->capacity() == 5);
        CHECK(c2->capacity() == 5);
        CHECK(c3->capacity() == 5);

    }
        
    SECTION("Matrices with only one element") {
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e1 =
            create<unsigned long>(Cascade, 5, 1);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e2 =
            create<unsigned long>(Dynamic, 5, 1);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> e3 =
            create<unsigned long>(Hashed, 5, 1);
                                                       

        CHECK(e1->size() == 1);
        CHECK(e2->size() == 1);
        CHECK(e3->size() == 1);

        CHECK(e1->capacity() == 5);
        CHECK(e2->capacity() == 5);
        CHECK(e3->capacity() == 5);

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c1 =
            e1->clone(); 
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c2 = 
            e2->clone();
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c3 = 
            e3->clone();

        CHECK(c1->size() == 1);
        CHECK(c2->size() == 1);
        CHECK(c3->size() == 1);

        CHECK(c1->capacity() == 5);
        CHECK(c2->capacity() == 5);
        CHECK(c3->capacity() == 5);           

    }

    SECTION("Copying matrices") {

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> m1 =
            create<unsigned long>(Cascade, 1000, 2);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> m2 =
            create<unsigned long>(Dynamic, 5, 2);
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> m3 =
            create<unsigned long>(Hashed, 5, 2);

        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c1 = m1->clone();
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c2 = m2->clone();
        std::unique_ptr<ExtendableSquareMatrix<unsigned long>> c3 = m3->clone();

        m1->set(0, 1, true);
        m2->set(0, 1, true);
        m3->set(0, 1, true);

        CHECK(m1->get(0, 1) != c1->get(0, 1));
        CHECK(m2->get(0, 1) != c2->get(0, 1));
        CHECK(m3->get(0, 1) != c3->get(0, 1));
        
        CHECK(!c1->get(0, 1));
        CHECK(!c2->get(0, 1));
        CHECK(!c3->get(0, 1));
        
        }
        
    SECTION("Extend and copy") {

        std::unique_ptr<ExtendableSquareMatrix<char>> m1 =
            create<char>(Cascade, 5, 3);
        std::unique_ptr<ExtendableSquareMatrix<char>> m2 =
            create<char>(Dynamic, 5, 3);
        std::unique_ptr<ExtendableSquareMatrix<char>> m3 =
            create<char>(Hashed, 5, 3);
            
        m1->set(0, 0, true);
        m1->set(1, 0, true);
        m1->set(1, 1, true);
        m1->set(1, 2, true);
        m2->set(0, 0, true);
        m2->set(1, 0, true);
        m2->set(1, 1, true);
        m2->set(1, 2, true);
        m3->set(0, 0, true);
        m3->set(1, 0, true);
        m3->set(1, 1, true);
        m3->set(1, 2, true);

        CHECK(m1->size() == 3);
        CHECK(m2->size() == 3);
        CHECK(m3->size() == 3);
        CHECK(m1->capacity() == 5);
        CHECK(m2->capacity() == 5);
        CHECK(m3->capacity() == 5);

        m1->extend_and_copy(3, 3);
        m2->extend_and_copy(3, 3);
        m3->extend_and_copy(3, 3);
        m1->extend_and_copy(1, 0);
        m2->extend_and_copy(1, 0);
        m3->extend_and_copy(1, 0);

        CHECK(m1->size() == 5);
        CHECK(m2->size() == 5);
        CHECK(m3->size() == 5);
        CHECK(m1->capacity() == 5);
        CHECK(m2->capacity() == 5);
        CHECK(m3->capacity() == 5);       

        CHECK(m1->get(0, 4));
        CHECK(m1->get(1, 4));
        CHECK(!m1->get(2, 4));
        CHECK(!m1->get(3, 4));
        CHECK(m1->get(4, 0));
        CHECK(m1->get(4, 1));
        CHECK(m1->get(4, 2));
        CHECK(!m1->get(4, 3));
        CHECK(!m1->get(4, 4));
        
        CHECK(m2->get(0, 4));
        CHECK(m2->get(1, 4));
        CHECK(!m2->get(2, 4));
        CHECK(!m2->get(3, 4));
        CHECK(m2->get(4, 0));
        CHECK(m2->get(4, 1));
        CHECK(m2->get(4, 2));
        CHECK(!m2->get(4, 3));
        CHECK(!m2->get(4, 4));
        
        CHECK(m3->get(0, 4));
        CHECK(m3->get(1, 4));
        CHECK(!m3->get(2, 4));
        CHECK(!m3->get(3, 4));
        CHECK(m3->get(4, 0));
        CHECK(m3->get(4, 1));
        CHECK(m3->get(4, 2));
        CHECK(!m3->get(4, 3));
        CHECK(!m3->get(4, 4));
        
    }
   
}
