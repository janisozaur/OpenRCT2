/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

struct ITitleSequencePlayer
{
    virtual ~ITitleSequencePlayer() = default;

    virtual int32_t GetCurrentPosition() const = 0;

    virtual bool Begin(size_t titleSequenceId) = 0;
    virtual void Reset() = 0;
    virtual bool Update() = 0;
    virtual void Seek(int32_t position) = 0;
    virtual void Eject() = 0;

    /**
     * Checks if a command has been executed since the last time this was called.
     * @return True if a command was executed, false otherwise.
     */
    virtual bool PopCommandExecutedSignal() = 0;
};
