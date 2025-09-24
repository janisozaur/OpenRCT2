/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <fstream>
#include <string_view>

namespace OpenRCT2
{
    class StartupLogger
    {
    private:
        std::ofstream _logFile;
        bool _initialized = false;
        static StartupLogger* _instance;

    public:
        StartupLogger();
        ~StartupLogger();

        static StartupLogger& GetInstance();
        bool Initialize();
        void Log(std::string_view message);
        void LogFormat(const char* format, ...);
        void Close();

    private:
        void WriteTimestamp();
        std::string GetLogFilePath();
    };

// Convenience macros for startup logging
#define LOG_STARTUP(format, ...) StartupLogger::GetInstance().LogFormat("[STARTUP] " format, ##__VA_ARGS__)
#define LOG_STARTUP_INFO(format, ...) StartupLogger::GetInstance().LogFormat("[STARTUP-INFO] " format, ##__VA_ARGS__)
#define LOG_STARTUP_ERROR(format, ...) StartupLogger::GetInstance().LogFormat("[STARTUP-ERROR] " format, ##__VA_ARGS__)
#define LOG_STARTUP_WARNING(format, ...) StartupLogger::GetInstance().LogFormat("[STARTUP-WARNING] " format, ##__VA_ARGS__)
} // namespace OpenRCT2