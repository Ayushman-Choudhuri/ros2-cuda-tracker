#pragma once

#include <cstddef>

namespace byte_track {

    // Jonker-Volgenant solver for the square linear assignment problem. `cost` is a
    // row-pointer view over an n x n matrix; `x` receives the column assigned to
    // each row and `y` the row assigned to each column. Returns 0 on success.
    //
    // Signature of the upstream numerical kernel, kept verbatim down to the
    // parameter names; see src/lapjv.cpp.
    // NOLINTNEXTLINE(readability-identifier-naming,readability-identifier-length,modernize-avoid-c-arrays)
    int lapjv_internal(size_t n, double* cost[], int* x, int* y);

}  // namespace byte_track
