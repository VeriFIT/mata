/* standalone-headers/two-dimensional-map.cc -- checks that two-dimensional-map.hh compiles on its own.
 *
 * This translation unit deliberately includes a single Mata header and nothing else, so that a missing
 * include inside the header breaks the build here instead of in the code of a user who includes it first.
 */

#include "mata/utils/two-dimensional-map.hh"

static_assert(mata::utils::TwoDimensionalMap<unsigned>::no_value == std::numeric_limits<unsigned>::max());
