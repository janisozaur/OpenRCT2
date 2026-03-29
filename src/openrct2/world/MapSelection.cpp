/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MapSelection.h"

#include "../core/BitSet.hpp"
#include "../interface/Viewport.h"
#include "Map.h"

MapSelectFlags gMapSelectFlags;
MapSelectType gMapSelectType;
CoordsXY gMapSelectPositionA;
CoordsXY gMapSelectPositionB;
CoordsXYZ gMapSelectArrowPosition;
uint8_t gMapSelectArrowDirection;

static std::vector<CoordsXY> _mapSelectionTiles;
static bool _mapSelectionTilesInvalidate = false;
static OpenRCT2::BitSet<kMaximumMapSizeTechnical * kMaximumMapSizeTechnical> _mapSelectionBitSet;

static MapSelectFlags _previousMapSelectFlags;
static MapSelectType _previousMapSelectType;
static CoordsXY _previousMapSelectPositionA;
static CoordsXY _previousMapSelectPositionB;
static CoordsXYZ _previousMapSelectArrowPosition;
static uint8_t _previousMapSelectArrowDirection;

MapRange getMapSelectRange()
{
    return MapRange(gMapSelectPositionA, gMapSelectPositionB);
}

void setMapSelectRange(const MapRange& range)
{
    const auto normalised = range.Normalise();
    gMapSelectPositionA = normalised.Point1;
    gMapSelectPositionB = normalised.Point2;
}

void setMapSelectRange(const CoordsXY coords)
{
    setMapSelectRange({ coords, coords });
}

namespace OpenRCT2::MapSelection
{
    void clearSelectedTiles()
    {
        if (!_mapSelectionTiles.empty())
        {
            _mapSelectionTilesInvalidate = true;

            CoordsXY mins = { 32767, 32767 };
            CoordsXY maxs = { -32768, -32768 };
            for (const CoordsXY& coords : _mapSelectionTiles)
            {
                mins.x = std::min(mins.x, coords.x);
                mins.y = std::min(mins.y, coords.y);
                maxs.x = std::max(maxs.x, coords.x);
                maxs.y = std::max(maxs.y, coords.y);
            }
            MapInvalidateRegion(mins, maxs);
        }
        _mapSelectionBitSet.reset();
        _mapSelectionTiles.clear();
    }

    void addSelectedTile(const CoordsXY& coords)
    {
        int32_t x = coords.x / kCoordsXYStep;
        int32_t y = coords.y / kCoordsXYStep;
        if (x >= 0 && x < kMaximumMapSizeTechnical && y >= 0 && y < kMaximumMapSizeTechnical)
        {
            if (!_mapSelectionBitSet.get(y * kMaximumMapSizeTechnical + x))
            {
                _mapSelectionBitSet.set(y * kMaximumMapSizeTechnical + x, true);
                _mapSelectionTiles.push_back(coords);
                _mapSelectionTilesInvalidate = true;
            }
        }
    }

    const std::vector<CoordsXY>& getSelectedTiles()
    {
        return _mapSelectionTiles;
    }

    bool isTileSelected(const CoordsXY& coords)
    {
        if (_mapSelectionTiles.empty())
        {
            return false;
        }
        int32_t x = coords.x / kCoordsXYStep;
        int32_t y = coords.y / kCoordsXYStep;
        if (x < 0 || x >= kMaximumMapSizeTechnical || y < 0 || y >= kMaximumMapSizeTechnical)
        {
            return false;
        }
        return _mapSelectionBitSet.get(y * kMaximumMapSizeTechnical + x);
    }

    void invalidate()
    {
        if (!_previousMapSelectFlags.has(MapSelectFlag::enable) && gMapSelectFlags.has(MapSelectFlag::enable))
        {
            MapInvalidateRegion(gMapSelectPositionA, gMapSelectPositionB);
        }
        else if (_previousMapSelectFlags.has(MapSelectFlag::enable) && !gMapSelectFlags.has(MapSelectFlag::enable))
        {
            MapInvalidateRegion(_previousMapSelectPositionA, _previousMapSelectPositionB);
        }
        else if (
            gMapSelectFlags.has(MapSelectFlag::enable)
            && (_previousMapSelectPositionA != gMapSelectPositionA || _previousMapSelectPositionB != gMapSelectPositionB))
        {
            MapInvalidateRegion(_previousMapSelectPositionA, _previousMapSelectPositionB);
            MapInvalidateRegion(gMapSelectPositionA, gMapSelectPositionB);
        }
        else if (_previousMapSelectType != gMapSelectType)
        {
            MapInvalidateRegion(gMapSelectPositionA, gMapSelectPositionB);
        }

        if (!_previousMapSelectFlags.has(MapSelectFlag::enableArrow) && gMapSelectFlags.has(MapSelectFlag::enableArrow))
        {
            MapInvalidateTile({ gMapSelectArrowPosition, gMapSelectArrowPosition.z });
        }
        else if (_previousMapSelectFlags.has(MapSelectFlag::enableArrow) && !gMapSelectFlags.has(MapSelectFlag::enableArrow))
        {
            MapInvalidateTile({ _previousMapSelectArrowPosition, _previousMapSelectArrowPosition.z });
        }
        else if (gMapSelectFlags.has(MapSelectFlag::enableArrow) && _previousMapSelectArrowPosition != gMapSelectArrowPosition)
        {
            MapInvalidateTile({ _previousMapSelectArrowPosition, _previousMapSelectArrowPosition.z });
            MapInvalidateTile({ gMapSelectArrowPosition, gMapSelectArrowPosition.z });
        }
        else if (_previousMapSelectArrowDirection != gMapSelectArrowDirection)
        {
            MapInvalidateTile({ gMapSelectArrowPosition, gMapSelectArrowPosition.z });
        }

        if (_mapSelectionTilesInvalidate)
        {
            if (_mapSelectionTiles.size() > 10)
            {
                CoordsXY mins = { 32767, 32767 };
                CoordsXY maxs = { -32768, -32768 };
                for (const CoordsXY& coords : _mapSelectionTiles)
                {
                    mins.x = std::min(mins.x, coords.x);
                    mins.y = std::min(mins.y, coords.y);
                    maxs.x = std::max(maxs.x, coords.x);
                    maxs.y = std::max(maxs.y, coords.y);
                }
                MapInvalidateRegion(mins, maxs);
            }
            else
            {
                for (const CoordsXY& coords : _mapSelectionTiles)
                {
                    MapInvalidateTileFull(coords);
                }
            }
        }

        if (_previousMapSelectFlags.has(MapSelectFlag::enableConstruct) && !gMapSelectFlags.has(MapSelectFlag::enableConstruct))
        {
            // If we just disabled construct selection, we might have cleared the list already, but we still need to invalidate.
            // But clearSelectedTiles now sets _mapSelectionTilesInvalidate, so it should be handled above if it happened this
            // tick.
        }

        _mapSelectionTilesInvalidate = false;

        _previousMapSelectFlags = gMapSelectFlags;
        _previousMapSelectType = gMapSelectType;
        _previousMapSelectPositionA = gMapSelectPositionA;
        _previousMapSelectPositionB = gMapSelectPositionB;
        _previousMapSelectArrowPosition = gMapSelectArrowPosition;
        _previousMapSelectArrowDirection = gMapSelectArrowDirection;
    }
} // namespace OpenRCT2::MapSelection
