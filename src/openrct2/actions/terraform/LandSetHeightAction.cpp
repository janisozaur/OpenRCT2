/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "LandSetHeightAction.h"

#include "../../GameState.h"
#include "../../OpenRCT2.h"
#include "../../localisation/StringIds.h"
#include "LandSetHeightBulkAction.h"

namespace OpenRCT2::GameActions
{
    LandSetHeightAction::LandSetHeightAction(const CoordsXY& coords, uint8_t height, uint8_t style)
        : _coords(coords)
        , _height(height)
        , _style(style)
    {
    }

    void LandSetHeightAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_coords);
        visitor.Visit("height", _height);
        visitor.Visit("style", _style);
    }

    uint16_t LandSetHeightAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void LandSetHeightAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);

        stream << DS_TAG(_coords) << DS_TAG(_height) << DS_TAG(_style);
    }

    Result LandSetHeightAction::Query(GameState_t& gameState) const
    {
        LandSetHeightBulkAction bulk({ { _coords, _height, _style } });
        return bulk.Query(gameState);
    }

    Result LandSetHeightAction::Execute(GameState_t& gameState) const
    {
        LandSetHeightBulkAction bulk({ { _coords, _height, _style } });
        return bulk.Execute(gameState);
    }
} // namespace OpenRCT2::GameActions
