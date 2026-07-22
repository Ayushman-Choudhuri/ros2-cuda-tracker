#pragma once

// Vendored from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE.
//
// Jonker-Volgenant dense linear-assignment solver. This is a numerical kernel
// kept close to upstream on purpose: its short, algorithm-local names mirror
// the reference implementation. Do not hand-edit — replace wholesale if the
// upstream solver changes.

#include <cstddef>

namespace byte_track {

int lapjv_internal(size_t n, double* cost[], int* x, int* y);

}  // namespace byte_track
