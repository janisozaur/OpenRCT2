#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <time.h>
#include <string>
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../platform/platform.h"
#include "NetworkChat.h"

extern "C"
{
    #include "../config.h"
    #include "../localisation/localisation.h"
}

class NetworkChat : public INetworkChat
{
private:
    bool        _loggingEnabled = false;
    std::string _logPath;

public:
    virtual ~NetworkChat() { }

    void StartLogging() override
    {
        _logPath = GetLogPathForCurrentTime();
        _loggingEnabled = true;
    }

    void StopLogging() override
    {
        _loggingEnabled = false;
    }

    void AppendToLog(const utf8 * text)
    {
        if (!_loggingEnabled) return;
        if (!gConfigNetwork.log_chat) return;

        const utf8 * chatLogPath = _logPath.c_str();

        utf8 directory[MAX_PATH];
        Path::GetDirectory(directory, sizeof(directory), chatLogPath);
        if (platform_ensure_directory_exists(directory))
        {
            SDL_RWops * chatLogStream = SDL_RWFromFile(chatLogPath, "a");
            if (chatLogStream != nullptr) {
                utf8 buffer[256];
                GetLogTimestamp(buffer, sizeof(buffer));

                String::Append(buffer, sizeof(buffer), text);
                utf8_remove_formatting(buffer, false);
                String::Append(buffer, sizeof(buffer), platform_get_new_line());

                SDL_RWwrite(chatLogStream, buffer, String::SizeOf(buffer), 1);
                SDL_RWclose(chatLogStream);
            }
        }
    }

private:
    void GetLogTimestamp(utf8 * buffer, size_t bufferSize)
    {
        tm * tmInfo = GetCurrentTimeInfo();
        strftime(buffer, bufferSize, "[%Y/%m/%d %H:%M:%S] ", tmInfo);
    }

    std::string GetLogPathForCurrentTime()
    {
        utf8 filename[32];
        tm * tmInfo = GetCurrentTimeInfo();
        strftime(filename, sizeof(filename), "%Y%m%d-%H%M%S.txt", tmInfo);

        utf8 path[MAX_PATH];
        platform_get_user_directory(path, "chatlogs");
        Path::Append(path, sizeof(path), filename);

        return std::string(path);
    }

    tm * GetCurrentTimeInfo()
    {
        time_t timer;
        time(&timer);
        tm * tmInfo = localtime(&timer);
        return tmInfo;
    }
};
