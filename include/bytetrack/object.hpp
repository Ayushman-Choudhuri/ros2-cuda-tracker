#pragma once

// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include "bytetrack/rect.hpp"

namespace byte_track {

// A single detection fed into the tracker: box, class label and confidence.
struct Object {
    Rect rect;
    int label;
    float prob;

    Object(const Rect& rect, int label, float prob);
};

}  // namespace byte_track
