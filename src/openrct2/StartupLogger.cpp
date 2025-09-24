/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "StartupLogger.h"

#include "Context.h"
#include "PlatformEnvironment.h"
#include "Version.h"
#include "core/Path.hpp"
#include "core/String.hpp"
#include "platform/Platform.h"

#include <cstdarg>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace OpenRCT2
{
    StartupLogger* StartupLogger::_instance = nullptr;

    StartupLogger::StartupLogger()
    {
        _instance = this;
    }

    StartupLogger::~StartupLogger()
    {
        Close();
        _instance = nullptr;
    }

    StartupLogger& StartupLogger::GetInstance()
    {
        static StartupLogger instance;
        return instance;
    }

    bool StartupLogger::Initialize()
    {
        if (_initialized)
            return true;

        try
        {
            auto logPath = GetLogFilePath();

            // Create directory if it doesn't exist
            auto directory = Path::GetDirectory(logPath);
            if (!Path::DirectoryExists(directory))
            {
                Path::CreateDirectory(directory);
            }

            _logFile.open(fs::u8path(logPath), std::ios::out | std::ios::app | std::ios::binary);
            if (!_logFile.is_open())
            {
                return false;
            }

            _initialized = true;

            // Write session header
            WriteTimestamp();
            _logFile << "========== OpenRCT2 Startup Session Begin ==========\n";
            WriteTimestamp();
            _logFile << "Version: " << gVersionInfoFull << "\n";
            _logFile.flush();

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void StartupLogger::Log(std::string_view message)
    {
        if (!_initialized && !Initialize())
            return;

        try
        {
            WriteTimestamp();
            _logFile << message << "\n";
            _logFile.flush();
        }
        catch (...)
        {
            // Ignore errors to prevent startup failures
        }
    }

    void StartupLogger::LogFormat(const char* format, ...)
    {
        if (!_initialized && !Initialize())
            return;

        try
        {
            va_list args;
            va_start(args, format);
            auto message = String::formatVA(format, args);
            va_end(args);

            Log(message);
        }
        catch (...)
        {
            // Ignore errors to prevent startup failures
        }
    }

    void StartupLogger::Close()
    {
        if (_initialized && _logFile.is_open())
        {
            try
            {
                WriteTimestamp();
                _logFile << "========== OpenRCT2 Startup Session End ==========\n\n";
                _logFile.flush();
                _logFile.close();
                _initialized = false;
            }
            catch (...)
            {
                // Ignore errors
            }
        }
    }

    void StartupLogger::WriteTimestamp()
    {
        try
        {
            char buffer[32];
            time_t timer;
            time(&timer);
            auto tmInfo = localtime(&timer);
            if (strftime(buffer, sizeof(buffer), "[%Y/%m/%d %H:%M:%S] ", tmInfo) != 0)
            {
                _logFile << buffer;
            }
        }
        catch (...)
        {
            // Ignore timestamp errors
        }
    }

    std::string StartupLogger::GetLogFilePath()
    {
        // Try to use the platform environment if available
        try
        {
            auto ctx = GetContext();
            if (ctx != nullptr)
            {
                auto& env = ctx->GetPlatformEnvironment();
                auto logsDir = env.GetDirectoryPath(DirBase::user, DirId::serverLogs);
                return Path::Combine(logsDir, "startup.log");
            }
        }
        catch (...)
        {
            // Fall back to simple path
        }

        // Fallback: use user data directory with basic path construction
        try
        {
            auto userDataPath = Platform::GetFolderPath(SpecialFolder::userData);
            auto openrct2Dir = Path::Combine(userDataPath, "OpenRCT2");
            auto logsDir = Path::Combine(openrct2Dir, "Logs");
            return Path::Combine(logsDir, "startup.log");
        }
        catch (...)
        {
            // Final fallback: use current directory
            return "startup.log";
        }
    }
} // namespace OpenRCT2