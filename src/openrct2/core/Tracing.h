/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "perfetto/perfetto.h"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("game").SetDescription("General game logic"),
    perfetto::Category("rendering").SetDescription("Rendering and drawing events"),
    perfetto::Category("world").SetDescription("World and map processing"),
    perfetto::Category("peep").SetDescription("Peep and guest logic"),
    perfetto::Category("scripting").SetDescription("Scripting engine events"),
    perfetto::Category("management").SetDescription("Park management, finance, research")
);
