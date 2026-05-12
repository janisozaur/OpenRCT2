/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <string>
#include <string_view>

namespace OpenRCT2::Network
{
    struct INetworkLogger
    {
        virtual ~INetworkLogger() = default;

        virtual void BeginChatLog() = 0;
        virtual void AppendChatLog(std::string_view s) = 0;
        virtual void CloseChatLog() = 0;

        virtual void BeginServerLog(const std::string& serverName, bool isClient) = 0;
        virtual void AppendServerLog(std::string_view s) = 0;
        virtual void CloseServerLog(bool isClient) = 0;
    };
} // namespace OpenRCT2::Network
