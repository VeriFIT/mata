/*
 * bdd-domain.hh -- Mintermization domain for BDD.
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
    struct BDDDomain {
        Cudd bdd_mng; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
        BDD val;

        BDDDomain() : bdd_mng(0), val(BDD()) {}

        BDDDomain(Cudd mng) : bdd_mng(mng), val(BDD()) {}

        BDDDomain(Cudd mng, BDD val) : bdd_mng(mng), val(val) {};

        friend BDDDomain operator&&(const BDDDomain& lhs, const BDDDomain &rhs) {
            return {lhs.bdd_mng, lhs.val * rhs.val};
        }

        friend BDDDomain operator||(const BDDDomain& lhs, const BDDDomain &rhs) {
            return {lhs.bdd_mng, lhs.val + rhs.val};
        }

        friend BDDDomain operator!(const BDDDomain &lhs) {
            return {lhs.bdd_mng, !lhs.val};
        }

        bool operator==(const BDDDomain &rhs) const {
            return this->val == rhs.val;
        }

        bool isFalse() const {
            return val.IsZero();
        }

        BDDDomain get_true() const;
        BDDDomain get_false() const;
        BDDDomain get_var() const;
    };
}

namespace std {
    template<>
    struct hash<struct mata::BDDDomain> {
        size_t operator()(const struct mata::BDDDomain &algebra) const noexcept {
            return hash<BDD>{}(algebra.val);
        }
    };
}

#endif //LIBMATA_BDD_DOMAIN_HH
