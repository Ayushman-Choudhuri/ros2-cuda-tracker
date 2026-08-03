#include "bytetrack/rect.hpp"

#include <algorithm>

namespace byte_track {
    namespace {

        // Boxes are inclusive of their end pixel, so extents are measured as width + 1.
        constexpr float kInclusivePixel = 1.0F;

        float InclusiveArea(float width, float height) {
            return (width + kInclusivePixel) * (height + kInclusivePixel);
        }

    }  // namespace

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
        const float overlap_width =
            std::min(Right(), other.Right()) - std::max(left_, other.left_) + kInclusivePixel;
        const float overlap_height =
            std::min(Bottom(), other.Bottom()) - std::max(top_, other.top_) + kInclusivePixel;
        if (overlap_width <= 0 || overlap_height <= 0) {
            return 0.0F;
        }

        const float intersection = overlap_width * overlap_height;
        const float union_area = InclusiveArea(width_, height_) +
                                 InclusiveArea(other.width_, other.height_) - intersection;
        return intersection / union_area;
    }

    Rect Rect::FromXyah(float center_x, float center_y, float aspect_ratio, float height) {
        const float width = aspect_ratio * height;
        return {center_x - width / 2, center_y - height / 2, width, height};
    }

}  // namespace byte_track
