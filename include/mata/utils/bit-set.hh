/* ord-vector.hh -- Implementation of a set (ordered vector) using std::vector.
 */

#ifndef MATA_BIT_SET_HH_
#define MATA_BIT_SET_HH_

#include <vector>
#include <algorithm>
#include <cassert>

#include "utils.hh"

namespace mata::utils {
    struct BitSet {
        using SlotType = uint64_t;

        std::vector<SlotType> data;
        size_t bit_count;

        BitSet(size_t bit_count) : bit_count(bit_count),
                                   data((bit_count / sizeof(SlotType)) + (bit_count % sizeof(SlotType) > 0)) {}  // Zero initialize
        BitSet(size_t bit_count, std::vector<SlotType>&& bit_data) : data(bit_data), bit_count(bit_count) {}

        bool operator[](size_t pos) const {
            size_t slot = pos / sizeof(SlotType);
            size_t slot_offset = slot % sizeof(SlotType);

            SlotType slot_data = this->data[slot];
            return static_cast<bool>((slot_data & (1u << slot_offset)) > 0);
        }

        void set(size_t pos, bool value = true) {
            size_t slot = pos / sizeof(SlotType);
            size_t slot_offset = slot % sizeof(SlotType);

            SlotType zero_affected_bit = ~(1u << slot_offset);
            SlotType set_affected_bit  = static_cast<SlotType>(value) << slot_offset;
            this->data[slot] = (this->data[slot] & zero_affected_bit) | set_affected_bit;
        }

        BitSet make_union(const BitSet& other) const {
            size_t new_slot_count = std::max(this->data.size(), other.data.size());

            std::vector<SlotType> new_data(new_slot_count);

            for (size_t slot_idx = 0; slot_idx < this->data.size(); slot_idx++) {
                new_data[slot_idx] = this->data[slot_idx];
            }

            for (size_t slot_idx = 0; slot_idx < other.data.size(); slot_idx++) {
                new_data[slot_idx] |= other.data[slot_idx];
            }

            size_t new_bit_count = std::max(this->bit_count, other.bit_count);
            return BitSet(new_bit_count, std::move(new_data));
        }

        BitSet make_intersection(const BitSet& other) const {
            size_t new_slot_count = std::max(this->data.size(), other.data.size());

            std::vector<SlotType> new_data(new_slot_count);

            for (size_t slot_idx = 0; slot_idx < this->data.size(); slot_idx++) {
                new_data[slot_idx] = this->data[slot_idx];
            }

            for (size_t slot_idx = 0; slot_idx < other.data.size(); slot_idx++) {
                new_data[slot_idx] &= other.data[slot_idx];
            }

            size_t new_bit_count = std::max(this->bit_count, other.bit_count);
            return BitSet(new_bit_count, std::move(new_data));
        }

        bool is_empty() const {
            for (size_t slot_idx = 0; slot_idx < this->data.size(); slot_idx++) {
                if (this->data[slot_idx] != 0) return false;
            }
            return true;
        }
    }
} // Namespace mata::utils.

#endif
