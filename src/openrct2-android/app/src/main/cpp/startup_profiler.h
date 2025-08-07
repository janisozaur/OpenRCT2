#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#include <chrono>
#include <string>
#include <unordered_map>

#define STARTUP_LOG_TAG "StartupProfiler"
#define STARTUP_LOGPERF(...) __android_log_print(ANDROID_LOG_WARN, STARTUP_LOG_TAG, "[STARTUP] " __VA_ARGS__)

class StartupProfiler {
public:
    static StartupProfiler& getInstance();

    void startTimer(const std::string& name);
    void endTimer(const std::string& name);
    void logCheckpoint(const std::string& message);
    void dumpAllTimings();

private:
    StartupProfiler() = default;

    struct TimerData {
        std::chrono::high_resolution_clock::time_point start;
        bool active = false;
        long duration_ms = 0;
    };

    std::unordered_map<std::string, TimerData> m_timers;
    std::chrono::high_resolution_clock::time_point m_appStart;
    bool m_initialized = false;
};

// Helper macros for easy profiling
#define PROFILE_START(name) StartupProfiler::getInstance().startTimer(name)
#define PROFILE_END(name) StartupProfiler::getInstance().endTimer(name)
#define PROFILE_CHECKPOINT(msg) StartupProfiler::getInstance().logCheckpoint(msg)

#else
// No-op macros for non-Android builds
#define PROFILE_START(name)
#define PROFILE_END(name)
#define PROFILE_CHECKPOINT(msg)
#endif
