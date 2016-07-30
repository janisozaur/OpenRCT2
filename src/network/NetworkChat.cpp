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
#include "../core/Memory.hpp"
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../core/StringBuilder.hpp"
#include "NetworkChat.h"
#include "NetworkPlayer.h"

extern "C"
{
    #include "../config.h"
    #include "../interface/chat.h"
    #include "../interface/keyboard_shortcut.h"
    #include "../localisation/localisation.h"
    #include "../platform/platform.h"
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

    void ShowMessage(const utf8 * text) override
    {
        chat_history_add(text);
    }

    void ShowMessage(NetworkPlayer * sender, const utf8 * text) override
    {
        auto sb = StringBuilder();
    
        sb.Append(FORMAT_OUTLINE);
        if (sender != nullptr)
        {
            sb.Append(FORMAT_BABYBLUE);
            sb.Append(sender->name.c_str());
            sb.Append(": ");
        }
        sb.Append(FORMAT_WHITE);
        sb.Append(text);
    
        // Remove non-font format codes, like string arguments
        utf8 * formattedString = sb.StealString();
        utf8_remove_format_codes(formattedString, true);
        ShowMessage(formattedString);
        Memory::Free(formattedString);
    }

    void ShowChatHelp() override
    {
        // Get shortcut key for opening chat window
        char * templateString = (char *)language_get_string(STR_SHORTCUT_KEY_UNKNOWN);
        uint16 openChatShortcut = gShortcutKeys[SHORTCUT_OPEN_CHAT_WINDOW];
        keyboard_shortcut_format_string(templateString, openChatShortcut);

        utf8 buffer[256];
        NetworkPlayer server;
        server.name = "Server";
        format_string(buffer, STR_MULTIPLAYER_CONNECTED_CHAT_HINT, &templateString);
        ShowMessage(&server, buffer);
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

INetworkChat * CreateChat()
{
    return new NetworkChat();
}
