// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include "bytetrack/object.hpp"

namespace byte_track {

Object::Object(const Rect& rect, int label, float prob) : rect(rect), label(label), prob(prob) {
}

}  // namespace byte_track
