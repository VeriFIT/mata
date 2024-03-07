/* composition.cc -- Composition of two NFTs
 */

// MATA headers
#include "mata/nft/nft.hh"
#include <cassert>


using namespace mata::utils;


namespace mata::nft
{

Nft compose(const Nft& lhs, const Nft& rhs, const OrdVector<Level>& lhs_sync_levels, const OrdVector<Level>& rhs_sync_levels, const JumpMode jump_mode) {
    assert(!lhs_sync_levels.empty());
    assert(lhs_sync_levels.size() == rhs_sync_levels.size());

    // Inserts loop into the given Nft for each state with level 0.
    // The loop word is constructed using the EPSILON symbol for all levels, except for the levels
    // where is_dcare_on_transition is true, in which case the DONT_CARE symbol is used.
    auto insert_self_loops = [&](Nft &nft, const BoolVector &is_dcare_on_transition) {
        Word loop_word(nft.num_of_levels, EPSILON);
        for (size_t i{ 0 }; i < nft.num_of_levels; i++) {
            if (is_dcare_on_transition[i]) {
                loop_word[i] = DONT_CARE;
            }
        }

        for (State s{ 0 }; s < nft.num_of_states(); s++) {
            if (nft.levels[s]== 0) {
                nft.insert_word(s, loop_word, s);
            }
        }
    };

    // Calculate new_levels_mask, which is used for inserting levels to match the synchronization
    // transitions in lhs and rhs. It also calculates the vector levels_to_project_out, which contains
    // levels of synchronized transitions. These levels will be projected out from the Nft after the composition.
    // Example:
    // lhs_sync_levels: 1 4
    // rhs_sync_levels: 2 3
    // lhs_new_levels_mask: 0 1 0 0 0
    // rhs_new_levels_mask: 0 0 1 1 0
    // levels_to_project_out: 2 5
    Level min_level = std::min(*lhs_sync_levels.begin(), *rhs_sync_levels.begin());
    size_t lhs_suffix_len = lhs.num_of_levels - 1 - *--lhs_sync_levels.end();
    size_t rhs_suffix_len = rhs.num_of_levels - 1 - *--rhs_sync_levels.end();
    size_t biggest_suffix_len = std::max(lhs_suffix_len, rhs_suffix_len);
    BoolVector lhs_new_levels_mask(min_level, false);
    BoolVector rhs_new_levels_mask(min_level, false);
    OrdVector<Level> levels_to_project_out;
    Level lhs_offset{ 0 };
    Level rhs_offset{ 0 };
    Level lhs_lvl{ 0 };
    Level rhs_lvl{ 0 };
    for (auto lhs_sync_levels_it{ lhs_sync_levels.begin() }, rhssync_levels_it{ rhs_sync_levels.begin ()};
         lhs_sync_levels_it != lhs_sync_levels.end();
         ++lhs_sync_levels_it, ++rhssync_levels_it)
    {
        lhs_lvl = *lhs_sync_levels_it + lhs_offset;
        rhs_lvl = *rhssync_levels_it + rhs_offset;
        if (lhs_lvl < rhs_lvl) {
            lhs_new_levels_mask.insert(lhs_new_levels_mask.end(), rhs_lvl - lhs_lvl, true);
            rhs_new_levels_mask.insert(rhs_new_levels_mask.end(), rhs_lvl - lhs_lvl, false);
            lhs_offset += rhs_lvl - lhs_lvl ;
        } else if (lhs_lvl > rhs_lvl) {
            lhs_new_levels_mask.insert(lhs_new_levels_mask.end(), lhs_lvl - rhs_lvl, false);
            rhs_new_levels_mask.insert(rhs_new_levels_mask.end(), lhs_lvl - rhs_lvl, true);
            rhs_offset = lhs_lvl - rhs_lvl;
        } else {
            lhs_new_levels_mask.resize(lhs_lvl, false);
            rhs_new_levels_mask.resize(rhs_lvl, false);
        }
        lhs_new_levels_mask.push_back(false);
        rhs_new_levels_mask.push_back(false);
        levels_to_project_out.push_back(static_cast<Level>(lhs_new_levels_mask.size() - 1));
    }
    // Match the size of vectors and num_of_levels in lhs and rhs after the insertion of new levels.
    lhs_new_levels_mask.insert(lhs_new_levels_mask.end(), lhs_suffix_len, false);
    rhs_new_levels_mask.insert(rhs_new_levels_mask.end(), rhs_suffix_len, false);
    lhs_new_levels_mask.insert(lhs_new_levels_mask.end(), biggest_suffix_len - lhs_suffix_len, true);
    rhs_new_levels_mask.insert(rhs_new_levels_mask.end(), biggest_suffix_len - rhs_suffix_len, true);

    Nft lhs_synced = insert_levels(lhs, lhs_new_levels_mask, jump_mode);
    Nft rhs_synced = insert_levels(rhs, rhs_new_levels_mask, jump_mode);

    // Two auxiliary states (states from inserted loops) can not create a product state.
    const State lhs_first_aux_state = lhs_synced.num_of_states();
    const State rhs_first_aux_state = rhs_synced.num_of_states();

    insert_self_loops(lhs_synced, lhs_new_levels_mask);
    insert_self_loops(rhs_synced, rhs_new_levels_mask);

    Nft result{ intersection(lhs_synced, rhs_synced, nullptr, jump_mode, lhs_first_aux_state, rhs_first_aux_state) };
    result = project_out(result, levels_to_project_out, jump_mode);
    return result;
}

Nft compose(const Nft& lhs, const Nft& rhs, const Level lhs_sync_level, const Level rhs_sync_level, const JumpMode jump_mode) {
    return compose(lhs, rhs, OrdVector{ lhs_sync_level }, OrdVector{ rhs_sync_level }, jump_mode);
}

} // mata::nft
