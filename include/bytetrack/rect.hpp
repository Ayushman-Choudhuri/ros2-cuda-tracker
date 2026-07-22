#pragma once

// Adapted from ByteTrack-cpp (https://github.com/Vertical-Beach/ByteTrack-cpp),
// MIT License. See third_party/ByteTrack-cpp.LICENSE. Reformatted to this
// repo's Google C++ style; tracking behaviour is unchanged.

namespace byte_track {

// Axis-aligned bounding box stored in top-left/width/height form. The Kalman
// filter operates in centre/aspect-ratio/height space, so conversions to and
// from that form are provided here.
class Rect {
   public:
    Rect() = default;
    Rect(float left, float top, float width, float height);

    [[nodiscard]] float Left() const;
    [[nodiscard]] float Top() const;
    [[nodiscard]] float Width() const;
    [[nodiscard]] float Height() const;
    [[nodiscard]] float Right() const;
    [[nodiscard]] float Bottom() const;

    [[nodiscard]] float CenterX() const;      // left + width / 2
    [[nodiscard]] float CenterY() const;      // top + height / 2
    [[nodiscard]] float AspectRatio() const;  // width / height

    [[nodiscard]] float CalcIoU(const Rect& other) const;

    static Rect FromXyah(float center_x, float center_y, float aspect_ratio, float height);

   private:
    float left_ = 0.0F;
    float top_ = 0.0F;
    float width_ = 0.0F;
    float height_ = 0.0F;
};

}  // namespace byte_track
