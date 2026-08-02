#pragma once

namespace byte_track {

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

        [[nodiscard]] float CenterX() const;
        [[nodiscard]] float CenterY() const;
        [[nodiscard]] float AspectRatio() const;

        [[nodiscard]] float CalcIoU(const Rect& other) const;

        static Rect FromXyah(float center_x, float center_y, float aspect_ratio, float height);

       private:
        float left_ = 0.0F;
        float top_ = 0.0F;
        float width_ = 0.0F;
        float height_ = 0.0F;
    };

}  // namespace byte_track
