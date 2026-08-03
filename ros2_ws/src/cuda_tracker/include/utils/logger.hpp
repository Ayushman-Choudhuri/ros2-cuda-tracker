#pragma once

#include <ostream>
#include <sstream>
#include <string>

namespace vision {

    enum class LogLevel { kDebug = 0, kInfo, kWarn, kError, kFatal };

    class Logger {
       public:
        static void SetLevel(LogLevel level);

        static LogLevel GetLevel();

        static void SetLevelFromEnvironment();

        static bool IsEnabled(LogLevel level) { return level >= GetLevel(); }

        static void Log(LogLevel level, const char* tag, const std::string& message);
    };

    class LogRecord {
       public:
        LogRecord(LogLevel level, const char* tag)
            : level_(level), tag_(tag), enabled_(Logger::IsEnabled(level)) {}
        ~LogRecord();

        LogRecord(const LogRecord&) = delete;
        LogRecord& operator=(const LogRecord&) = delete;

        template <typename Value>
        LogRecord& operator<<(const Value& value) {
            if (enabled_) {
                buffer_ << value;
            }
            return *this;
        }

       private:
        LogLevel level_;
        const char* tag_;
        bool enabled_;
        std::ostringstream buffer_;
    };

}  // namespace vision

#define LOG_AT(level, tag) ::vision::LogRecord((level), (tag))

#define LOG_DEBUG(tag) LOG_AT(::vision::LogLevel::kDebug, tag)
#define LOG_INFO(tag) LOG_AT(::vision::LogLevel::kInfo, tag)
#define LOG_WARN(tag) LOG_AT(::vision::LogLevel::kWarn, tag)
#define LOG_ERROR(tag) LOG_AT(::vision::LogLevel::kError, tag)
#define LOG_FATAL(tag) LOG_AT(::vision::LogLevel::kFatal, tag)
