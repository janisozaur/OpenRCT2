#include "startup_profiler.h"

#ifdef __ANDROID__

StartupProfiler& StartupProfiler::getInstance() {
    static StartupProfiler instance;
    if (!instance.m_initialized) {
        instance.m_appStart = std::chrono::high_resolution_clock::now();
        instance.m_initialized = true;
        STARTUP_LOGPERF("=== OpenRCT2 Android Startup Profiling Started ===");
    }
    return instance;
}

void StartupProfiler::startTimer(const std::string& name) {
    auto& timer = m_timers[name];
    timer.start = std::chrono::high_resolution_clock::now();
    timer.active = true;

    auto since_start = std::chrono::duration_cast<std::chrono::milliseconds>(
        timer.start - m_appStart);
    STARTUP_LOGPERF("TIMER START [%s] at +%lld ms", name.c_str(), (long long)since_start.count());
}

void StartupProfiler::endTimer(const std::string& name) {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto it = m_timers.find(name);
    if (it == m_timers.end() || !it->second.active) {
        STARTUP_LOGPERF("WARNING: Timer [%s] was not started or already ended", name.c_str());
        return;
    }

    auto& timer = it->second;
    timer.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - timer.start).count();
    timer.active = false;

    auto since_start = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - m_appStart);
    STARTUP_LOGPERF("TIMER END   [%s] took %lld ms (at +%lld ms)",
                   name.c_str(), (long long)timer.duration_ms, (long long)since_start.count());
}

void StartupProfiler::logCheckpoint(const std::string& message) {
    auto now = std::chrono::high_resolution_clock::now();
    auto since_start = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_appStart);
    STARTUP_LOGPERF("CHECKPOINT: %s (at +%lld ms)", message.c_str(), (long long)since_start.count());
}void StartupProfiler::dumpAllTimings() {
    STARTUP_LOGPERF("=== STARTUP TIMING SUMMARY ===");

    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - m_appStart);
    STARTUP_LOGPERF("Total startup time: %lld ms", (long long)total_time.count());

    STARTUP_LOGPERF("Individual timers:");
    for (const auto& pair : m_timers) {
        const auto& name = pair.first;
        const auto& timer = pair.second;
        if (timer.active) {
            STARTUP_LOGPERF("  [%s]: STILL ACTIVE", name.c_str());
        } else {
            STARTUP_LOGPERF("  [%s]: %lld ms", name.c_str(), (long long)timer.duration_ms);
        }
    }
    STARTUP_LOGPERF("=== END TIMING SUMMARY ===");
}

#endif
