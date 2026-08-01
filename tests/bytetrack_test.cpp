// Behavioural guard for the vendored tracker. Links only the tracker, so it
// runs on hosts without CUDA or TensorRT.

#include <gtest/gtest.h>

#include <cstddef>
#include <set>
#include <vector>

#include "tracker/bytetrack/byte_tracker.hpp"

namespace {

    constexpr float kTolerance = 1e-3F;
    constexpr float kHighScore = 0.9F;
    constexpr float kLowScore = 0.2F;

    byte_track::Object MakeObject(float left, float top, float width, float height, float score) {
        return {byte_track::Rect(left, top, width, height), /*label=*/0, score};
    }

}  // namespace

TEST(RectTest, IdenticalBoxesFullyOverlap) {
    const byte_track::Rect box(10, 10, 50, 100);
    EXPECT_NEAR(box.CalcIoU(box), 1.0F, kTolerance);
}

TEST(RectTest, DisjointBoxesDoNotOverlap) {
    const byte_track::Rect left_box(0, 0, 10, 10);
    const byte_track::Rect right_box(500, 500, 10, 10);
    EXPECT_NEAR(left_box.CalcIoU(right_box), 0.0F, kTolerance);
}

TEST(RectTest, XyahRoundTripsThroughRect) {
    const byte_track::Rect original(20, 40, 30, 60);
    const byte_track::Rect restored = byte_track::Rect::FromXyah(
        original.CenterX(), original.CenterY(), original.AspectRatio(), original.Height());

    EXPECT_NEAR(original.Left(), restored.Left(), kTolerance);
    EXPECT_NEAR(original.Top(), restored.Top(), kTolerance);
    EXPECT_NEAR(original.Width(), restored.Width(), kTolerance);
    EXPECT_NEAR(original.Height(), restored.Height(), kTolerance);
}

TEST(ByteTrackerTest, EmptyFrameProducesNoTracks) {
    byte_track::ByteTracker tracker;
    EXPECT_TRUE(tracker.Update({}).empty());
}

TEST(ByteTrackerTest, SteadyTargetKeepsOneIdentity) {
    byte_track::ByteTracker tracker;
    std::set<size_t> observed_track_ids;

    for (int frame = 0; frame < 20; ++frame) {
        const auto tracks =
            tracker.Update({MakeObject(100.0F + frame * 4, 100.0F, 40.0F, 80.0F, kHighScore)});
        ASSERT_EQ(tracks.size(), 1U) << "lost the target on frame " << frame;
        observed_track_ids.insert(tracks.front()->GetTrackId());
    }

    EXPECT_EQ(observed_track_ids.size(), 1U);
}

TEST(ByteTrackerTest, SeparateTargetsGetSeparateIdentities) {
    byte_track::ByteTracker tracker;
    std::set<size_t> observed_track_ids;

    for (int frame = 0; frame < 10; ++frame) {
        const auto tracks = tracker.Update({
            MakeObject(50.0F + frame, 50.0F, 40.0F, 80.0F, kHighScore),
            MakeObject(400.0F - frame, 300.0F, 40.0F, 80.0F, kHighScore),
        });
        ASSERT_EQ(tracks.size(), 2U) << "lost a target on frame " << frame;
        for (const auto& track : tracks) {
            observed_track_ids.insert(track->GetTrackId());
        }
    }

    EXPECT_EQ(observed_track_ids.size(), 2U);
}

// The second association pass is what distinguishes ByteTrack from SORT: a
// detection too weak to start a track must still keep an existing one alive.
TEST(ByteTrackerTest, LowScoreDetectionSustainsTrack) {
    byte_track::ByteTracker tracker;

    size_t established_id = 0;
    for (int frame = 0; frame < 5; ++frame) {
        const auto tracks =
            tracker.Update({MakeObject(100.0F + frame * 3, 100.0F, 40.0F, 80.0F, kHighScore)});
        ASSERT_EQ(tracks.size(), 1U);
        established_id = tracks.front()->GetTrackId();
    }

    const auto tracks = tracker.Update({MakeObject(115.0F, 100.0F, 40.0F, 80.0F, kLowScore)});
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks.front()->GetTrackId(), established_id);
}

TEST(ByteTrackerTest, LostTrackExpiresAfterTrackBuffer) {
    byte_track::ByteTracker tracker(/*frame_rate=*/30, /*track_buffer=*/5);

    size_t established_id = 0;
    for (int frame = 0; frame < 5; ++frame) {
        const auto tracks = tracker.Update({MakeObject(100.0F, 100.0F, 40.0F, 80.0F, kHighScore)});
        ASSERT_EQ(tracks.size(), 1U);
        established_id = tracks.front()->GetTrackId();
    }

    for (int frame = 0; frame < 20; ++frame) {
        ASSERT_TRUE(tracker.Update({}).empty()) << "track survived frame " << frame;
    }

    for (int frame = 0; frame < 3; ++frame) {
        for (const auto& track :
             tracker.Update({MakeObject(100.0F, 100.0F, 40.0F, 80.0F, kHighScore)})) {
            EXPECT_NE(track->GetTrackId(), established_id);
        }
    }
}
