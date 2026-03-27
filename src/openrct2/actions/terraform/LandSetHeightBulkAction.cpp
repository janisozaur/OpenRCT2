/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "LandSetHeightBulkAction.h"

#include "../../Context.h"
#include "../../GameState.h"
#include "../../OpenRCT2.h"
#include "../../localisation/StringIds.h"
#include "../../management/Finance.h"
#include "../../object/SmallSceneryEntry.h"
#include "../../ride/RideData.h"
#include "../../windows/Intent.h"
#include "../../world/ConstructionClearance.h"
#include "../../world/Footpath.h"
#include "../../world/Map.h"
#include "../../world/Park.h"
#include "../../world/QuarterTile.h"
#include "../../world/Scenery.h"
#include "../../world/TileElementsView.h"
#include "../../world/Wall.h"
#include "../../world/tile_element/PathElement.h"
#include "../../world/tile_element/Slope.h"
#include "../../world/tile_element/SmallSceneryElement.h"
#include "../../world/tile_element/SurfaceElement.h"
#include "../../world/tile_element/TrackElement.h"

namespace OpenRCT2::GameActions
{
    LandSetHeightBulkAction::LandSetHeightBulkAction(std::vector<LandSetHeightUpdate> updates)
        : _updates(std::move(updates))
    {
    }

    void LandSetHeightBulkAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("updates", _updates);
    }

    uint16_t LandSetHeightBulkAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void LandSetHeightBulkAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);

        uint32_t count = static_cast<uint32_t>(_updates.size());
        stream << count;

        if (stream.IsLoading())
        {
            _updates.resize(count);
        }

        for (auto& update : _updates)
        {
            stream << DS_TAG(update.Coords) << DS_TAG(update.Height) << DS_TAG(update.Style);
        }
    }

    Result LandSetHeightBulkAction::Query(GameState_t& gameState) const
    {
        if (gameState.park.flags & PARK_FLAGS_FORBID_LANDSCAPE_CHANGES)
        {
            return Result(Status::disallowed, STR_FORBIDDEN_BY_THE_LOCAL_AUTHORITY, kStringIdNone);
        }

        money64 totalCost = 0;

        for (const auto& update : _updates)
        {
            StringId errorMessage = CheckParameters(gameState, update);
            if (errorMessage != kStringIdNone)
            {
                return Result(Status::disallowed, kStringIdNone, errorMessage);
            }

            if (gLegacyScene != LegacyScene::scenarioEditor && !gameState.cheats.sandboxMode)
            {
                if (!MapIsLocationInPark(update.Coords))
                {
                    return Result(Status::disallowed, STR_LAND_NOT_OWNED_BY_PARK, kStringIdNone);
                }
            }

            money64 sceneryRemovalCost = 0;
            if (!gameState.cheats.disableClearanceChecks)
            {
                if (gameState.park.flags & PARK_FLAGS_FORBID_TREE_REMOVAL)
                {
                    // Check for obstructing large trees
                    TileElement* tileElement = CheckTreeObstructions(update);
                    if (tileElement != nullptr)
                    {
                        auto res = Result(Status::disallowed, kStringIdNone, kStringIdNone);
                        MapGetObstructionErrorText(tileElement, res);
                        return res;
                    }
                }
                sceneryRemovalCost = GetSmallSceneryRemovalCost(update);
            }

            // Check for ride support limits
            if (!gameState.cheats.disableSupportLimits)
            {
                errorMessage = CheckRideSupports(update);
                if (errorMessage != kStringIdNone)
                {
                    return Result(Status::disallowed, kStringIdNone, errorMessage);
                }
            }

            auto* surfaceElement = MapGetSurfaceElementAt(update.Coords);
            if (surfaceElement == nullptr)
                return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_SURFACE_ELEMENT_NOT_FOUND);

            // We need to check if there is _currently_ a level crossing on the tile.
            // For that, we need the old height, so we can't use the _height variable.
            auto oldCoords = CoordsXYZ{ update.Coords, surfaceElement->GetBaseZ() };
            auto* pathElement = MapGetFootpathElement(oldCoords);
            if (pathElement != nullptr && pathElement->AsPath()->IsLevelCrossing(oldCoords))
            {
                return Result(Status::disallowed, STR_REMOVE_LEVEL_CROSSING_FIRST, kStringIdNone);
            }

            TileElement* tileElement = CheckFloatingStructures(
                reinterpret_cast<TileElement*>(surfaceElement), update.Height, update.Style);
            if (tileElement != nullptr)
            {
                auto res = Result(Status::disallowed, kStringIdNone, kStringIdNone);
                MapGetObstructionErrorText(tileElement, res);
                return res;
            }

            if (!gameState.cheats.disableClearanceChecks)
            {
                uint8_t zCorner = update.Height;
                if (update.Style & kTileSlopeRaisedCornersMask)
                {
                    zCorner += 2;
                    if (update.Style & kTileSlopeDiagonalFlag)
                    {
                        zCorner += 2;
                    }
                }

                auto clearResult = MapCanConstructWithClearAt(
                    { update.Coords, update.Height * kCoordsZStep, zCorner * kCoordsZStep }, MapSetLandHeightClearFunc,
                    { 0b1111, 0 }, {}, update.Style, CreateCrossingMode::none);
                if (clearResult.error != Status::ok)
                {
                    clearResult.error = Status::disallowed;
                    return clearResult;
                }
            }

            totalCost += sceneryRemovalCost + GetSurfaceHeightChangeCost(surfaceElement, update.Height, update.Style);
        }

        auto res = Result();
        res.cost = totalCost;
        res.expenditure = ExpenditureType::landscaping;
        return res;
    }

    Result LandSetHeightBulkAction::Execute(GameState_t& gameState) const
    {
        money64 totalCost = 0.00_GBP;

        CoordsXY mins = { 32767, 32767 };
        CoordsXY maxs = { -32768, -32768 };

        for (const auto& update : _updates)
        {
            auto surfaceHeight = TileElementHeight(update.Coords);
            FootpathRemoveLitter({ update.Coords, surfaceHeight });

            if (!gameState.cheats.disableClearanceChecks)
            {
                WallRemoveAt({ update.Coords, update.Height * 8 - 16, update.Height * 8 + 32 });
                totalCost += GetSmallSceneryRemovalCost(update);
                SmallSceneryRemoval(update);
            }

            auto* surfaceElement = MapGetSurfaceElementAt(update.Coords);
            if (surfaceElement == nullptr)
                return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_SURFACE_ELEMENT_NOT_FOUND);

            totalCost += GetSurfaceHeightChangeCost(surfaceElement, update.Height, update.Style);
            SetSurfaceHeight(update, reinterpret_cast<TileElement*>(surfaceElement));

            if (_updates.size() > 10)
            {
                mins.x = std::min(mins.x, update.Coords.x);
                mins.y = std::min(mins.y, update.Coords.y);
                maxs.x = std::max(maxs.x, update.Coords.x);
                maxs.y = std::max(maxs.y, update.Coords.y);
            }
            else
            {
                MapInvalidateTileFull(update.Coords);
            }
        }

        if (_updates.size() > 10)
        {
            MapInvalidateRegion(mins, maxs);
        }

        auto res = Result();
        if (!_updates.empty())
        {
            res.position = { _updates[0].Coords.x + 16, _updates[0].Coords.y + 16, TileElementHeight(_updates[0].Coords) };
        }
        res.cost = totalCost;
        res.expenditure = ExpenditureType::landscaping;
        return res;
    }

    StringId LandSetHeightBulkAction::CheckParameters(const GameState_t& gameState, const LandSetHeightUpdate& update) const
    {
        if (!LocationValid(update.Coords) || MapIsEdge(update.Coords))
        {
            return STR_OFF_EDGE_OF_MAP;
        }

        if (update.Height < kMinimumLandHeight)
        {
            return STR_TOO_LOW;
        }

        // Divide by 2 and subtract 7 to get the in-game units.
        if (update.Height > kMaximumLandHeight)
        {
            return STR_TOO_HIGH;
        }

        if (update.Height > kMaximumLandHeight - 2 && (update.Style & kTileSlopeMask) != 0)
        {
            return STR_TOO_HIGH;
        }

        if (update.Height == kMaximumLandHeight - 2 && (update.Style & kTileSlopeDiagonalFlag))
        {
            return STR_TOO_HIGH;
        }

        return kStringIdNone;
    }

    TileElement* LandSetHeightBulkAction::CheckTreeObstructions(const LandSetHeightUpdate& update)
    {
        for (auto* sceneryElement : TileElementsView<SmallSceneryElement>(update.Coords))
        {
            if (update.Height > sceneryElement->ClearanceHeight)
                continue;
            if (update.Height + 4 < sceneryElement->BaseHeight)
                continue;

            auto* sceneryEntry = sceneryElement->GetEntry();
            if (!sceneryEntry->flags.has(SmallSceneryFlag::isTree))
                continue;

            return sceneryElement->as<TileElement>();
        }
        return nullptr;
    }

    money64 LandSetHeightBulkAction::GetSmallSceneryRemovalCost(const LandSetHeightUpdate& update)
    {
        money64 cost{ 0 };

        for (auto* sceneryElement : TileElementsView<SmallSceneryElement>(update.Coords))
        {
            if (update.Height > sceneryElement->ClearanceHeight)
                continue;
            if (update.Height + 4 < sceneryElement->BaseHeight)
                continue;

            auto* sceneryEntry = sceneryElement->GetEntry();
            if (sceneryEntry == nullptr)
                continue;

            cost += sceneryEntry->removal_price;
        }

        return cost;
    }

    void LandSetHeightBulkAction::SmallSceneryRemoval(const LandSetHeightUpdate& update)
    {
        TileElement* tileElement = MapGetFirstElementAt(update.Coords);
        do
        {
            if (tileElement == nullptr)
                break;
            if (tileElement->GetType() != TileElementType::SmallScenery)
                continue;
            if (update.Height > tileElement->ClearanceHeight)
                continue;
            if (update.Height + 4 < tileElement->BaseHeight)
                continue;
            TileElementRemove(tileElement--);
        } while (!(tileElement++)->IsLastForTile());
    }

    StringId LandSetHeightBulkAction::CheckRideSupports(const LandSetHeightUpdate& update)
    {
        for (auto* trackElement : TileElementsView<TrackElement>(update.Coords))
        {
            RideId rideIndex = trackElement->GetRideIndex();

            auto ride = GetRide(rideIndex);
            if (ride == nullptr)
                continue;

            const auto* rideEntry = ride->getRideEntry();
            if (rideEntry == nullptr)
                continue;

            int32_t maxHeight = rideEntry->maxHeight;
            if (maxHeight == 0)
            {
                maxHeight = ride->getRideTypeDescriptor().Heights.MaxHeight;
            }

            int32_t zDelta = trackElement->ClearanceHeight - update.Height;
            if (zDelta >= 0 && zDelta / 2 > maxHeight)
            {
                return STR_SUPPORTS_CANT_BE_EXTENDED;
            }
        }
        return kStringIdNone;
    }

    TileElement* LandSetHeightBulkAction::CheckFloatingStructures(TileElement* surfaceElement, uint8_t height, uint8_t style)
    {
        if (surfaceElement->AsSurface()->HasTrackThatNeedsWater())
        {
            uint32_t waterHeight = surfaceElement->AsSurface()->GetWaterHeight();
            if (waterHeight != 0)
            {
                uint8_t zCorner = height;
                if (style & kTileSlopeMask)
                {
                    zCorner += 2;
                    if (style & kTileSlopeDiagonalFlag)
                    {
                        zCorner += 2;
                    }
                }
                if (zCorner > (waterHeight / kCoordsZStep) - 2)
                {
                    return ++surfaceElement;
                }
            }
        }
        return nullptr;
    }

    money64 LandSetHeightBulkAction::GetSurfaceHeightChangeCost(SurfaceElement* surfaceElement, uint8_t height, uint8_t style)
    {
        money64 cost{ 0 };
        for (Direction i : kAllDirections)
        {
            int32_t cornerHeight = TileElementGetCornerHeight(surfaceElement, i);
            cornerHeight -= MapGetCornerHeight(height, style & kTileSlopeMask, i);
            cost += 2.50_GBP * abs(cornerHeight);
        }
        return cost;
    }

    void LandSetHeightBulkAction::SetSurfaceHeight(const LandSetHeightUpdate& update, TileElement* surfaceElement)
    {
        surfaceElement->BaseHeight = update.Height;
        surfaceElement->ClearanceHeight = update.Height;
        surfaceElement->AsSurface()->SetSlope(update.Style);
        int32_t waterHeight = surfaceElement->AsSurface()->GetWaterHeight() / kCoordsZStep;
        if (waterHeight != 0 && waterHeight <= update.Height)
        {
            surfaceElement->AsSurface()->SetWaterHeight(0);
        }
    }

    bool LandSetHeightBulkAction::MapSetLandHeightClearFunc(
        TileElement** tile_element, [[maybe_unused]] const CoordsXY& coords, [[maybe_unused]] CommandFlags flags,
        [[maybe_unused]] money64* price)
    {
        if ((*tile_element)->GetType() == TileElementType::Surface)
            return true;

        if ((*tile_element)->GetType() == TileElementType::SmallScenery)
            return true;

        return false;
    }
} // namespace OpenRCT2::GameActions
