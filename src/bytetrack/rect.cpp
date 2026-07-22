// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

#include "bytetrack/rect.hpp"

#include <algorithm>

namespace byte_track {

Rect::Rect(float left, float top, float width, float height)
    : left_(left), top_(top), width_(width), height_(height) {
}

float Rect::Left() const {
    return left_;
}
float Rect::Top() const {
    return top_;
}
float Rect::Width() const {
    return width_;
}
float Rect::Height() const {
    return height_;
}
float Rect::Right() const {
    return left_ + width_;
}
float Rect::Bottom() const {
    return top_ + height_;
}

float Rect::CenterX() const {
    return left_ + width_ / 2;
}
float Rect::CenterY() const {
    return top_ + height_ / 2;
}
float Rect::AspectRatio() const {
    return width_ / height_;
}

float Rect::CalcIoU(const Rect& other) const {
    float other_area = (other.width_ + 1) * (other.height_ + 1);
    float intersect_width = std::min(Right(), other.Right()) - std::max(left_, other.left_) + 1;
    if (intersect_width <= 0) {
        return 0.0F;
    }
    float intersect_height = std::min(Bottom(), other.Bottom()) - std::max(top_, other.top_) + 1;
    if (intersect_height <= 0) {
        return 0.0F;
    }
    float intersect_area = intersect_width * intersect_height;
    float union_area = (width_ + 1) * (height_ + 1) + other_area - intersect_area;
    return intersect_area / union_area;
}

Rect Rect::FromXyah(float center_x, float center_y, float aspect_ratio, float height) {
    float width = aspect_ratio * height;
    return {center_x - width / 2, center_y - height / 2, width, height};
}

}  // namespace byte_track
