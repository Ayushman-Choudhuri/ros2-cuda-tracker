#include "utils/logger.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

namespace {

    class CapturedStderr {
       public:
        CapturedStderr() : original_buffer_(std::cerr.rdbuf(captured_.rdbuf())) {}
        ~CapturedStderr() { std::cerr.rdbuf(original_buffer_); }

        CapturedStderr(const CapturedStderr&) = delete;
        CapturedStderr& operator=(const CapturedStderr&) = delete;

        std::string Text() const { return captured_.str(); }

       private:
        std::ostringstream captured_;
        std::streambuf* original_buffer_;
    };

    // Opaque to the optimizer's readability checks, so the branch below stays a
    // real if/else rather than a folded constant.
    bool AlwaysFalse() {
        return false;
    }

    struct CountingValue {
        int* format_count;
    };

    std::ostream& operator<<(std::ostream& out, const CountingValue& value) {
        ++*value.format_count;
        return out;
    }

}  // namespace

TEST(LoggerTest, EmitsLevelTagAndMessage) {
    vision::Logger::SetLevel(vision::LogLevel::kDebug);
    CapturedStderr capture;

    LOG_WARN("Camera") << "dropped " << 3 << " frames";

    const std::string output = capture.Text();
    EXPECT_NE(output.find("[WARN]"), std::string::npos) << output;
    EXPECT_NE(output.find("[Camera]"), std::string::npos) << output;
    EXPECT_NE(output.find("dropped 3 frames"), std::string::npos) << output;
}

TEST(LoggerTest, SuppressesRecordsBelowCurrentLevel) {
    vision::Logger::SetLevel(vision::LogLevel::kError);
    CapturedStderr capture;

    LOG_INFO("Engine") << "should not appear";
    LOG_ERROR("Engine") << "should appear";

    const std::string output = capture.Text();
    EXPECT_EQ(output.find("should not appear"), std::string::npos) << output;
    EXPECT_NE(output.find("should appear"), std::string::npos) << output;
}

TEST(LoggerTest, ReadsLevelFromEnvironment) {
    vision::Logger::SetLevel(vision::LogLevel::kInfo);

    setenv("LOG_LEVEL", "WARNING", 1);
    vision::Logger::SetLevelFromEnvironment();
    EXPECT_EQ(vision::Logger::GetLevel(), vision::LogLevel::kWarn);

    setenv("LOG_LEVEL", "not-a-level", 1);
    vision::Logger::SetLevelFromEnvironment();
    EXPECT_EQ(vision::Logger::GetLevel(), vision::LogLevel::kWarn);

    unsetenv("LOG_LEVEL");
}

// Guards against the macro ever growing an `if` of its own, which an unbraced
// call site's `else` would then bind to.
TEST(LoggerTest, ComposesWithUnbracedIfElse) {
    vision::Logger::SetLevel(vision::LogLevel::kInfo);
    CapturedStderr capture;

    bool took_else_branch = false;
    // NOLINTBEGIN(readability-braces-around-statements)
    if (AlwaysFalse())
        LOG_INFO("Test") << "then branch";
    else
        took_else_branch = true;
    // NOLINTEND(readability-braces-around-statements)

    EXPECT_TRUE(took_else_branch);
    EXPECT_TRUE(capture.Text().empty()) << capture.Text();
}

TEST(LoggerTest, DoesNotFormatMessageWhenSuppressed) {
    vision::Logger::SetLevel(vision::LogLevel::kError);
    CapturedStderr capture;

    int format_count = 0;
    LOG_DEBUG("Test") << CountingValue{&format_count};
    EXPECT_EQ(format_count, 0);

    LOG_ERROR("Test") << CountingValue{&format_count};
    EXPECT_EQ(format_count, 1);
}
