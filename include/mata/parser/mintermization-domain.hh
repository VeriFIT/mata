/*
 * mintermization-domain.hh -- Mintermization domain for BDD.
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


#ifndef MATA_BDD_DOMAIN_HH
#define MATA_BDD_DOMAIN_HH

#include "mata/cudd/cuddObj.hh"

namespace mata {
    struct MintermizationDomain {
        Cudd bdd_mng; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
        BDD val;

        MintermizationDomain() : bdd_mng(0), val(BDD()) {}

        MintermizationDomain(Cudd mng) : bdd_mng(mng), val(BDD()) {}

        MintermizationDomain(Cudd mng, BDD val) : bdd_mng(mng), val(val) {};

        friend MintermizationDomain operator&&(const MintermizationDomain& lhs, const MintermizationDomain &rhs) {
            return {lhs.bdd_mng, lhs.val * rhs.val};
        }

        friend MintermizationDomain operator||(const MintermizationDomain& lhs, const MintermizationDomain &rhs) {
            return {lhs.bdd_mng, lhs.val + rhs.val};
        }

        friend MintermizationDomain operator!(const MintermizationDomain &lhs) {
            return {lhs.bdd_mng, !lhs.val};
        }

        bool operator==(const MintermizationDomain &rhs) const {
            return this->val == rhs.val;
        }

        bool isFalse() const {
            return val.IsZero();
        }

        MintermizationDomain getTrue() const;
        MintermizationDomain getFalse() const;
        MintermizationDomain getVar() const;
    };
}

#endif //LIBMATA_MINTERMIZATION_DOMAIN_HH
