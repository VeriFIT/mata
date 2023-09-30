/*
 * mintermization-domain.cc -- Mintermization domain for BDD.
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mata/parser/mintermization-domain.hh"

struct mata::MintermizationDomain mata::MintermizationDomain::getTrue() const {
    return MintermizationDomain(bdd_mng, bdd_mng.bddOne());
}

struct mata::MintermizationDomain mata::MintermizationDomain::getFalse() const {
    return MintermizationDomain(bdd_mng, bdd_mng.bddZero());
}

struct mata::MintermizationDomain mata::MintermizationDomain::getVar() const {
    return MintermizationDomain(bdd_mng, bdd_mng.bddVar());
}
