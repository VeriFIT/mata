//
// Created by Lukáš Holík on 06.12.2022.
//

#pragma once

#ifndef LIBMATA_NUMBER_PREDICATE_HH
#define LIBMATA_NUMBER_PREDICATE_HH

#include <vector>
#include <mata/ord-vector.hh>

namespace Mata {
    namespace Util {

        template <class Number> class OrdVector;

        template<typename Number>
        class NumPredicate {
        private:
            mutable std::vector<bool> predicate = {};
            mutable std::vector <Number> elements = {};
            mutable bool elements_exact = true;
            mutable bool elements_watched = true; //invariant: implies that elements overapproximate the true values
            //TODO: if it is never used with elements_watched = false, then we can simplify.

            using const_iterator = typename std::vector<Number>::const_iterator;

            void prune_elements() const {
                Number new_pos = 0;
                for (Number orig_pos = 0; orig_pos < elements.size(); ++orig_pos) {
                    if (predicate[elements[orig_pos]]) {
                        elements[new_pos] = elements[orig_pos];
                        ++new_pos;
                    }
                }
                elements.resize(new_pos);
                elements_exact = true;
            }

            void compute_elements() const {
                elements.clear();
                for (Number q = 0; q < predicate.size(); ++q) {
                    if (predicate[q])
                        elements.push_back(q);
                }
                elements_exact = true;
            }

            void update_elements() const {
                if (!elements_exact) {
                    if (!elements_watched) {
                        compute_elements();
                    } else {
                        prune_elements();
                    }
                }
            }

        public:
            NumPredicate(bool watch_elements = true) : elements_watched(watch_elements), elements_exact(true) {};

            NumPredicate(std::initializer_list <Number> list, bool watch_elements = true) : elements_watched(
                    watch_elements), elements_exact(true) {
                for (auto q: list)
                    add(q);
            }

            NumPredicate(std::vector <Number> list, bool watch_elements = true) : elements_watched(watch_elements),
                                                                                  elements_exact(true) {
                for (auto q: list)
                    add(q);
            }

            NumPredicate(Mata::Util::OrdVector<Number> vec, bool watch_elements = true) : elements_watched(watch_elements),
                                                                                          elements_exact(true) {
                for (auto q: vec)
                    add(q);
            }

            template <class InputIterator>
            NumPredicate(InputIterator first, InputIterator last)
            {
                while (first < last) {
                    add(*first);
                    ++first;
                }
            }

            void add(Number q) {
                while (predicate.size() <= q)
                    predicate.push_back(false);
                if (elements_watched) {
                    Number q_was_there = predicate[q];
                    predicate[q] = true;
                    if (!q_was_there) {
                        elements.push_back(q);
                    }
                } else {
                    predicate[q] = true;
                    elements_exact = false;
                }
            }

            void remove(Number q) {
                elements_exact = false;
                if (q < predicate.size() && predicate[q]) {
                    predicate[q] = false;
                }
            }

            void add(const std::vector <Number> &elems) {
                for (Number q: elems)
                    add(q);
            }

            void remove(const std::vector <Number> &elems) {
                for (Number q: elems)
                    remove(q);
            }

            void watch_elements() {
                if (!elements_watched) {
                    update_elements();
                    elements_watched = true;
                }
            }

            void dont_watch_elements() {
                elements_watched = false;
            }

            bool operator[](Number q) const {
                if (q < predicate.size())
                    return predicate[q];
                else
                    return false;
            }

            Number size() const {
                if (elements_exact)
                    return elements.size();
                if (elements_watched) {
                    prune_elements();
                    return elements.size();
                } else {
                    Number cnt = 0;
                    for (Number q = 0; q < predicate.size(); ++q) {
                        if (predicate[q])
                            cnt++;
                    }
                    return cnt;
                }
            }

            void clear() {
                if (elements_watched)
                    for (size_t i = 0; i < elements.size(); i++)
                        predicate[elements[i]] = false;
                else
                    for (size_t i = 0; i < predicate.size(); i++)
                        predicate[i] = false;
                elements.clear();
                elements_exact = true;
            }

            void reserve(Number n) {
                predicate.reserve(n);
                if (elements_watched) {
                    elements.reserve(n);
                }
            }

            void flip(Number q) {
                if (this[q])
                    add(q);
                else
                    remove(q);
            }

            //TODO: add tests when starting to use this
            void complement(Number domain_size) {
                Number old_domain_size = predicate.size();
                predicate.reserve(domain_size);
                Number to_flip = min(domain_size,old_domain_size);
                for (Number q = 0; q < to_flip; ++q) {
                     predicate[q] = !predicate[q];
                }
                if (domain_size > old_domain_size)
                    for (Number q = old_domain_size; q < domain_size; ++q) {
                        predicate.push_back(true);
                    }
                if (elements_watched) {
                    compute_elements();
                }
            }

            void complement() {
                complement(predicate.size());
            }

            inline const_iterator begin() const {
                update_elements();
                return elements.begin();
            }

            inline const_iterator end() const {
                update_elements();
                return elements.end();
            }

            inline const_iterator cbegin() const {
                update_elements();
                return elements.cbegin();
            }

            inline const_iterator cend() const {
                update_elements();
                return elements.cend();
            }

            inline const_iterator begin() {
                update_elements();
                return elements.begin();
            }

            inline const_iterator end() {
                update_elements();
                return elements.end();
            }

            const std::vector <Number> &get_elements() const {
                update_elements();
                return elements;
            }

            bool are_disjoint(const NumPredicate<Number> &other) const {
                for (auto q: *this)
                    if (other[q])
                        return false;
                return true;
            }

            bool empty() const {
                return (size() == 0);
            }
        };
    }

}
#endif //LIBMATA_NUMBER_PREDICATE_HH
