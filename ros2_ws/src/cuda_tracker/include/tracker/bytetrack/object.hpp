#pragma once

#include "bytetrack/rect.hpp"

namespace byte_track {

    struct Object {
        Rect rect;
        int label = -1;
        float score = 0.0F;
    };

}  // namespace byte_track
