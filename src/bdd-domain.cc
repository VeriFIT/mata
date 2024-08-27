/*
 * bdd-domain.cc -- Mintermization domain for BDD.
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

#include "mata/parser/bdd-domain.hh"

struct mata::BDDDomain mata::BDDDomain::get_true() const {
    return BDDDomain(bdd_mng, bdd_mng.bddOne());
}

struct mata::BDDDomain mata::BDDDomain::get_false() const {
    return BDDDomain(bdd_mng, bdd_mng.bddZero());
}

struct mata::BDDDomain mata::BDDDomain::get_var() const {
    return BDDDomain(bdd_mng, bdd_mng.bddVar());
}
