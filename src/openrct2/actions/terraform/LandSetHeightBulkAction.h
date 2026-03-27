/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../GameAction.hpp"
#include "LandSetHeightUpdate.h"

#include <vector>

namespace OpenRCT2::GameActions
{
    class LandSetHeightBulkAction final : public GameActionBase<GameCommand::SetLandHeightBulk>
    {
    private:
        std::vector<LandSetHeightUpdate> _updates;

    public:
        LandSetHeightBulkAction() = default;
        LandSetHeightBulkAction(std::vector<LandSetHeightUpdate> updates);

        void AcceptParameters(GameActionParameterVisitor&) final;

        uint16_t GetActionFlags() const override;

        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState) const override;
        Result Execute(GameState_t& gameState) const override;

    public:
        StringId CheckParameters(const GameState_t& gameState, const LandSetHeightUpdate& update) const;
        static TileElement* CheckTreeObstructions(const LandSetHeightUpdate& update);
        static money64 GetSmallSceneryRemovalCost(const LandSetHeightUpdate& update);
        static void SmallSceneryRemoval(const LandSetHeightUpdate& update);
        static StringId CheckRideSupports(const LandSetHeightUpdate& update);
        static TileElement* CheckFloatingStructures(
            TileElement* surfaceElement, uint8_t height, uint8_t style);
        static money64 GetSurfaceHeightChangeCost(SurfaceElement* surfaceElement, uint8_t height, uint8_t style);
        static void SetSurfaceHeight(const LandSetHeightUpdate& update, TileElement* surfaceElement);

        /**
         *
         *  rct2: 0x00663CB9
         */
        static bool MapSetLandHeightClearFunc(
            TileElement** tile_element, [[maybe_unused]] const CoordsXY& coords, [[maybe_unused]] CommandFlags flags,
            [[maybe_unused]] money64* price);
    };
} // namespace OpenRCT2::GameActions
