#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
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

#pragma warning(disable : 4996) // GetVersionExA deprecated

#include <time.h>
#include "audio/audio.h"
#include "audio/AudioMixer.h"
#include "config/Config.h"
#include "Context.h"
#include "drawing/drawing.h"
#include "drawing/lightfx.h"
#include "editor.h"
#include "game.h"
#include "input.h"
#include "interface/chat.h"
#include "interface/console.h"
#include "interface/viewport.h"
#include "intro.h"
#include "localisation/date.h"
#include "localisation/localisation.h"
#include "management/news_item.h"
#include "network/network.h"
#include "network/twitch.h"
#include "object.h"
#include "object/ObjectManager.h"
#include "OpenRCT2.h"
#include "ParkImporter.h"
#include "peep/staff.h"
#include "platform/platform.h"
#include "rct1.h"
#include "ride/ride.h"
#include "ride/track.h"
#include "ride/track_design.h"
#include "ride/TrackDesignRepository.h"
#include "scenario/ScenarioRepository.h"
#include "title/TitleScreen.h"
#include "util/util.h"
#include "world/Climate.h"
#include "world/map.h"
#include "world/Park.h"
#include "world/scenery.h"
#include "world/sprite.h"

uint8 gScreenFlags;
uint32 gScreenAge;
uint8 gSavePromptMode;

char gRCT2AddressAppPath[MAX_PATH];
