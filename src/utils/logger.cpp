#include "utils/logger.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace vision {
    namespace {

        std::atomic<LogLevel> current_log_level{LogLevel::kInfo};

        // Function-local so the mutex is alive before any static constructor in
        // another translation unit can log.
        std::mutex& OutputMutex() {
            static std::mutex output_mutex;
            return output_mutex;
        }

        const char* LevelName(LogLevel level) {
            switch (level) {
                case LogLevel::kDebug:
                    return "DEBUG";
                case LogLevel::kInfo:
                    return "INFO";
                case LogLevel::kWarn:
                    return "WARN";
                case LogLevel::kError:
                    return "ERROR";
                case LogLevel::kFatal:
                    return "FATAL";
            }
            return "UNKNOWN";
        }

        std::string Timestamp() {
            using SystemClock = std::chrono::system_clock;

            const SystemClock::time_point now = SystemClock::now();
            const std::time_t seconds_since_epoch = SystemClock::to_time_t(now);
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
                1000;

            std::tm calendar_time{};
            localtime_r(&seconds_since_epoch, &calendar_time);

            std::ostringstream formatted;
            formatted << std::put_time(&calendar_time, "%Y-%m-%d %H:%M:%S") << '.'
                      << std::setfill('0') << std::setw(3) << milliseconds.count();
            return formatted.str();
        }

    }  // namespace

    void Logger::SetLevel(LogLevel level) {
        current_log_level.store(level, std::memory_order_relaxed);
    }

    LogLevel Logger::GetLevel() {
        return current_log_level.load(std::memory_order_relaxed);
    }

    void Logger::SetLevelFromEnvironment() {
        const char* requested = std::getenv("LOG_LEVEL");
        if (requested == nullptr) {
            return;
        }

        std::string name(requested);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        if (name == "debug") {
            SetLevel(LogLevel::kDebug);
        } else if (name == "info") {
            SetLevel(LogLevel::kInfo);
        } else if (name == "warn" || name == "warning") {
            SetLevel(LogLevel::kWarn);
        } else if (name == "error") {
            SetLevel(LogLevel::kError);
        } else if (name == "fatal") {
            SetLevel(LogLevel::kFatal);
        }
    }

    LogRecord::~LogRecord() {
        if (!enabled_) {
            return;
        }

        // A failing log must not propagate out of a destructor and abort the process.
        try {
            Logger::Log(level_, tag_, buffer_.str());
        } catch (...) {
        }
    }

    void Logger::Log(LogLevel level, const char* tag, const std::string& message) {
        if (!IsEnabled(level)) {
            return;
        }

        // std::cerr is unit-buffered, so each record reaches the container runtime
        // as it is written instead of sitting in a pipe buffer until exit.
        const std::lock_guard<std::mutex> lock(OutputMutex());
        std::cerr << '[' << Timestamp() << "] [" << LevelName(level) << "] [" << tag << "] "
                  << message << '\n';
    }

}  // namespace vision
