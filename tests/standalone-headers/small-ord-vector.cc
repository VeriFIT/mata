/* standalone-headers/small-ord-vector.cc -- checks that small-ord-vector.hh compiles on its own.
 *
 * This translation unit deliberately includes a single Mata header and nothing else, so that a missing
 * include inside the header breaks the build here instead of in the code of a user who includes it first.
 */

#include "mata/utils/small-ord-vector.hh"

static_assert(mata::utils::SmallOrdVector<unsigned long>::inline_capacity >= 1);
