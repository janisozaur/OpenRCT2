/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "INetworkLogger.h"

#include <fstream>
#include <string>

namespace OpenRCT2
{
    struct IContext;
}

namespace OpenRCT2::Network
{
    class DefaultNetworkLogger final : public INetworkLogger
    {
    private:
        IContext& _context;
        std::ofstream _chat_log_fs;
        std::ofstream _server_log_fs;
        std::string _chatLogPath;
        std::string _serverLogPath;

        static constexpr const char* _chatLogFilenameFormat = "%Y%m%d-%H%M%S.txt";
        static constexpr const char* _serverLogFilenameFormat = "%Y%m%d-%H%M%S.txt";

    public:
        DefaultNetworkLogger(IContext& context);
        ~DefaultNetworkLogger();

        void BeginChatLog() override;
        void AppendChatLog(std::string_view s) override;
        void CloseChatLog() override;

        void BeginServerLog(const std::string& serverName, bool isClient) override;
        void AppendServerLog(std::string_view s) override;
        void CloseServerLog(bool isClient) override;

    private:
        std::string BeginLog(const std::string& directory, const std::string& midName, const std::string& filenameFormat);
        void AppendLog(std::ostream& fs, std::string_view s);
    };
} // namespace OpenRCT2::Network
