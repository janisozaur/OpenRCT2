/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ScenerySetFavouriteAction.h"

#include "../localisation/StringIds.h"
#include "../world/Scenery.h"

using namespace OpenRCT2;

ScenerySetFavouriteAction::ScenerySetFavouriteAction(const ScenerySelection& selection, bool isFavourite)
    : _selection(selection)
    , _isFavourite(isFavourite)
{
}

uint16_t ScenerySetFavouriteAction::GetActionFlags() const
{
    return GameAction::GetActionFlags() | GameActions::Flags::AllowWhilePaused;
}

void ScenerySetFavouriteAction::Serialise(DataSerialiser& stream)
{
    stream << DS_TAG(_selection.SceneryType) << DS_TAG(_selection.EntryIndex) << DS_TAG(_isFavourite);
}

GameActions::Result ScenerySetFavouriteAction::Query() const
{
    if (_selection.EntryIndex == kObjectEntryIndexNull)
    {
        return GameActions::Result(
            GameActions::Status::InvalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_VALUE_OUT_OF_RANGE);
    }
    return GameActions::Result();
}

GameActions::Result ScenerySetFavouriteAction::Execute() const
{
    SetSceneryItemFavourited(_selection, _isFavourite);
    return GameActions::Result();
}
