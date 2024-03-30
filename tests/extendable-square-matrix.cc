#include <catch2/catch.hpp>

#include "mata/utils/extendable-square-matrix.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::ExtendableSquareMatrix") {

    SECTION("CascadeSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = create<unsigned long>(
                                                       Cascade, 5, 2);
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
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(!e->is_transitive());
        delete e;              
        }
        
    SECTION("DynamicSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = create<unsigned long>(
                                                       Dynamic, 5, 2);
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
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(!e->is_transitive());
        delete e;              
        }
        
    SECTION("HashedSquareMatrix") {

        ExtendableSquareMatrix<unsigned long> *e = create<unsigned long>(
                                                       Hashed, 5, 2);
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
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());        
        e->set(0, 0, 1);
        CHECK(!e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());
        e->set(1, 1, 1);
        e->set(2, 2, 1);
        e->set(3, 3, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(e->is_transitive());                  
        e->set(3, 1, 1);
        e->set(1, 2, 1);
        CHECK(e->is_reflexive());
        CHECK(e->is_antisymetric());
        CHECK(!e->is_transitive());
        delete e;              
        }

    SECTION("Matrix of the None type") {

        ExtendableSquareMatrix<unsigned long> *e = create<unsigned long>(
                                                       None, 5, 2);        
        CHECK(e == nullptr);
        
        delete e;
    
    }

    SECTION("Empty matrices") {
        ExtendableSquareMatrix<unsigned long> *e1 = create<unsigned long>(
                                                       Cascade, 5);        
        ExtendableSquareMatrix<unsigned long> *e2 = create<unsigned long>(
                                                       Dynamic, 5); 
        ExtendableSquareMatrix<unsigned long> *e3 = create<unsigned long>(
                                                       Hashed, 5);                                                        

        CHECK(!e1->size());
        CHECK(!e2->size());
        CHECK(!e3->size());

        CHECK(e1->capacity() == 5);
        CHECK(e2->capacity() == 5);
        CHECK(e3->capacity() == 5);

        ExtendableSquareMatrix<unsigned long> *c1 = e1->clone(); 
        ExtendableSquareMatrix<unsigned long> *c2 = e2->clone();
        ExtendableSquareMatrix<unsigned long> *c3 = e3->clone();

        CHECK(!c1->size());
        CHECK(!c2->size());
        CHECK(!c3->size());

        CHECK(c1->capacity() == 5);
        CHECK(c2->capacity() == 5);
        CHECK(c3->capacity() == 5);

        delete e1;
        delete e2;
        delete e3;
        delete c1;
        delete c2;
        delete c3;

    }
        
    SECTION("Matrices with only one element") {
        ExtendableSquareMatrix<unsigned long> *e1 = create<unsigned long>(
                                                       Cascade, 5, 1);        
        ExtendableSquareMatrix<unsigned long> *e2 = create<unsigned long>(
                                                       Dynamic, 5, 1); 
        ExtendableSquareMatrix<unsigned long> *e3 = create<unsigned long>(
                                                       Hashed, 5, 1);                                                        

        CHECK(e1->size() == 1);
        CHECK(e2->size() == 1);
        CHECK(e3->size() == 1);

        CHECK(e1->capacity() == 5);
        CHECK(e2->capacity() == 5);
        CHECK(e3->capacity() == 5);

        ExtendableSquareMatrix<unsigned long> *c1 = e1->clone(); 
        ExtendableSquareMatrix<unsigned long> *c2 = e2->clone();
        ExtendableSquareMatrix<unsigned long> *c3 = e3->clone();

        CHECK(c1->size() == 1);
        CHECK(c2->size() == 1);
        CHECK(c3->size() == 1);

        CHECK(c1->capacity() == 5);
        CHECK(c2->capacity() == 5);
        CHECK(c3->capacity() == 5);

        delete e1;
        delete e2;
        delete e3;                    
        delete c1;
        delete c2;
        delete c3;                    

    }

    SECTION("Copying matrices") {

        ExtendableSquareMatrix<char> *m1 = create<char>(Cascade, 1000, 2);
        ExtendableSquareMatrix<char> *m2 = create<char>(Dynamic, 5, 2);
        ExtendableSquareMatrix<char> *m3 = create<char>(Hashed, 5, 2);

        ExtendableSquareMatrix<char> *c1 = m1;
        ExtendableSquareMatrix<char> *c2 = m2;
        ExtendableSquareMatrix<char> *c3 = m3;
        
        m1->set(1, 1, true);
        m2->set(1, 1, true);
        m3->set(1, 1, true);
        
        CHECK(m1->get(1, 1) == c1->get(1, 1));
        CHECK(m2->get(1, 1) == c2->get(1, 1));
        CHECK(m3->get(1, 1) == c3->get(1, 1));

        c1 = m1->clone();
        c2 = m2->clone();
        c3 = m3->clone();

        m1->set(0, 1, true);
        m2->set(0, 1, true);
        m3->set(0, 1, true);

        CHECK(m1->get(0, 1) != c1->get(0, 1));
        CHECK(m2->get(0, 1) != c2->get(0, 1));
        CHECK(m3->get(0, 1) != c3->get(0, 1));
        
        CHECK(!c1->get(0, 1));
        CHECK(!c2->get(0, 1));
        CHECK(!c3->get(0, 1));
        
        delete m1;
        delete m2;
        delete m3;
        delete c1;
        delete c2;
        delete c3;
        
        }
        
}
