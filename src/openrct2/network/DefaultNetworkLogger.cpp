/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_NETWORK

    #include "DefaultNetworkLogger.h"

    #include "../Context.h"
    #include "../PlatformEnvironment.h"
    #include "../config/Config.h"
    #include "../core/File.h"
    #include "../core/Path.hpp"
    #include "../core/String.hpp"
    #include "../localisation/StringIds.h"
    #include "../localisation/Formatting.h"
    #include "../platform/Platform.h"

    #include <ctime>

namespace OpenRCT2::Network
{
    DefaultNetworkLogger::DefaultNetworkLogger(IContext& context)
        : _context(context)
    {
        _chat_log_fs << std::unitbuf;
        _server_log_fs << std::unitbuf;
    }

    DefaultNetworkLogger::~DefaultNetworkLogger()
    {
        CloseChatLog();
        if (_server_log_fs.is_open())
        {
            CloseServerLog(false); // Mode unknown at this point, but this is a fallback
        }
    }

    void DefaultNetworkLogger::BeginChatLog()
    {
        auto& env = _context.GetPlatformEnvironment();
        auto directory = env.GetDirectoryPath(DirBase::user, DirId::chatLogs);
        _chatLogPath = BeginLog(directory, "", _chatLogFilenameFormat);
        _chat_log_fs.open(fs::u8path(_chatLogPath), std::ios::out | std::ios::app);
    }

    void DefaultNetworkLogger::AppendChatLog(std::string_view s)
    {
        if (Config::Get().network.logChat && _chat_log_fs.is_open())
        {
            AppendLog(_chat_log_fs, s);
        }
    }

    void DefaultNetworkLogger::CloseChatLog()
    {
        if (_chat_log_fs.is_open())
        {
            _chat_log_fs.close();
        }
    }

    void DefaultNetworkLogger::BeginServerLog(const std::string& serverName, bool isClient)
    {
        auto& env = _context.GetPlatformEnvironment();
        auto directory = env.GetDirectoryPath(DirBase::user, DirId::serverLogs);
        _serverLogPath = BeginLog(directory, serverName, _serverLogFilenameFormat);
        _server_log_fs.open(fs::u8path(_serverLogPath), std::ios::out | std::ios::app | std::ios::binary);

        // Log server start event
        utf8 logMessage[256];
        if (isClient)
        {
            FormatStringLegacy(logMessage, sizeof(logMessage), STR_LOG_CLIENT_STARTED, nullptr);
        }
        else
        {
            FormatStringLegacy(logMessage, sizeof(logMessage), STR_LOG_SERVER_STARTED, nullptr);
        }
        AppendServerLog(logMessage);
    }

    void DefaultNetworkLogger::AppendServerLog(std::string_view s)
    {
        if (Config::Get().network.logServerActions && _server_log_fs.is_open())
        {
            AppendLog(_server_log_fs, s);
        }
    }

    void DefaultNetworkLogger::CloseServerLog(bool isClient)
    {
        if (_server_log_fs.is_open())
        {
            // Log server stopped event
            char logMessage[256];
            if (isClient)
            {
                FormatStringLegacy(logMessage, sizeof(logMessage), STR_LOG_CLIENT_STOPPED, nullptr);
            }
            else
            {
                FormatStringLegacy(logMessage, sizeof(logMessage), STR_LOG_SERVER_STOPPED, nullptr);
            }
            AppendServerLog(logMessage);
            _server_log_fs.close();
        }
    }

    std::string DefaultNetworkLogger::BeginLog(
        const std::string& directory, const std::string& midName, const std::string& filenameFormat)
    {
        utf8 filename[256];
        time_t timer;
        time(&timer);
        auto tmInfo = localtime(&timer);
        if (strftime(filename, sizeof(filename), filenameFormat.c_str(), tmInfo) == 0)
        {
            return "";
        }

        auto directoryMidName = Path::Combine(directory, midName);
        Path::CreateDirectory(directoryMidName);
        return Path::Combine(directoryMidName, filename);
    }

    void DefaultNetworkLogger::AppendLog(std::ostream& fs, std::string_view s)
    {
        if (fs.fail())
        {
            return;
        }
        try
        {
            utf8 buffer[1024];
            time_t timer;
            time(&timer);
            auto tmInfo = localtime(&timer);
            if (strftime(buffer, sizeof(buffer), "[%Y/%m/%d %H:%M:%S] ", tmInfo) != 0)
            {
                String::append(buffer, sizeof(buffer), std::string(s).c_str());
                String::append(buffer, sizeof(buffer), PLATFORM_NEWLINE);

                fs.write(buffer, strlen(buffer));
            }
        }
        catch (const std::exception&)
        {
        }
    }
} // namespace OpenRCT2::Network

#endif
