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

#include "../core/Exception.hpp"
#include "../core/FileStream.hpp"
#include "../core/IStream.hpp"
#include "../core/String.hpp"
#include "../core/Util.hpp"
#include "../management/award.h"
#include "../object/Object.h"
#include "../object/ObjectManager.h"
#include "../object/ObjectRepository.h"
#include "../rct12/SawyerChunkWriter.h"
#include "S6Exporter.h"
#include <functional>

extern "C"
{
    #include "../config/Config.h"
    #include "../game.h"
    #include "../interface/viewport.h"
    #include "../interface/window.h"
    #include "../localisation/date.h"
    #include "../localisation/localisation.h"
    #include "../management/finance.h"
    #include "../management/marketing.h"
    #include "../management/news_item.h"
    #include "../management/research.h"
    #include "../object.h"
    #include "../OpenRCT2.h"
    #include "../peep/staff.h"
    #include "../rct2.h"
    #include "../ride/ride.h"
    #include "../ride/ride_ratings.h"
    #include "../ride/track_data.h"
    #include "../scenario/scenario.h"
    #include "../util/sawyercoding.h"
    #include "../util/util.h"
    #include "../world/Climate.h"
    #include "../world/map_animation.h"
    #include "../world/park.h"
    #include "../world/sprite.h"
}

S6Exporter::S6Exporter()
{
    RemoveTracklessRides = false;
    memset(&_s6, 0, sizeof(_s6));
}

void S6Exporter::SaveGame(const utf8 * path)
{
    auto fs = FileStream(path, FILE_MODE_WRITE);
    SaveGame(&fs);
}

void S6Exporter::SaveGame(IStream * stream)
{
    Save(stream, false);
}

void S6Exporter::SaveScenario(const utf8 * path)
{
    auto fs = FileStream(path, FILE_MODE_WRITE);
    SaveScenario(&fs);
}

void S6Exporter::SaveScenario(IStream * stream)
{
    Save(stream, true);
}

void S6Exporter::Save(IStream * stream, bool isScenario)
{
    _s6.header.type               = isScenario ? S6_TYPE_SCENARIO : S6_TYPE_SAVEDGAME;
    _s6.header.classic_flag       = 0;
    _s6.header.num_packed_objects = uint16(ExportObjectsList.size());
    _s6.header.version            = S6_RCT2_VERSION;
    _s6.header.magic_number       = S6_MAGIC_NUMBER;
    _s6.game_version_number       = 201028;

    auto chunkWriter = SawyerChunkWriter(stream);

    // 0: Write header chunk
    chunkWriter.WriteChunk(&_s6.header, SAWYER_ENCODING::ROTATE);

    // 1: Write scenario info chunk
    if (_s6.header.type == S6_TYPE_SCENARIO)
    {
        chunkWriter.WriteChunk(&_s6.info, SAWYER_ENCODING::ROTATE);
    }

    // 2: Write packed objects
    if (_s6.header.num_packed_objects > 0)
    {
        IObjectRepository * objRepo = GetObjectRepository();
        objRepo->WritePackedObjects(stream, ExportObjectsList);
    }

    // 3: Write available objects chunk
    chunkWriter.WriteChunk(_s6.objects, sizeof(_s6.objects), SAWYER_ENCODING::ROTATE);

    // 4: Misc fields (data, rand...) chunk
    chunkWriter.WriteChunk(&_s6.elapsed_months, 16, SAWYER_ENCODING::RLECOMPRESSED);

    // 5: Map elements + sprites and other fields chunk
    chunkWriter.WriteChunk(&_s6.map_elements, 0x180000, SAWYER_ENCODING::RLECOMPRESSED);

    if (_s6.header.type == S6_TYPE_SCENARIO)
    {
        // 6 to 13:
        chunkWriter.WriteChunk(&_s6.next_free_map_element_pointer_index, 0x27104C, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.guests_in_park, 4, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.last_guests_in_park, 8, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.park_rating, 2, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.active_research_types, 1082, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.current_expenditure, 16, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.park_value, 4, SAWYER_ENCODING::RLECOMPRESSED);
        chunkWriter.WriteChunk(&_s6.completed_company_value, 0x761E8, SAWYER_ENCODING::RLECOMPRESSED);
    }
    else
    {
        // 6: Everything else...
        chunkWriter.WriteChunk(&_s6.next_free_map_element_pointer_index, 0x2E8570, SAWYER_ENCODING::RLECOMPRESSED);
    }

    // Determine number of bytes written
    size_t fileSize = stream->GetLength();

    // Read all written bytes back into a single buffer
    stream->SetPosition(0);
    auto   data = std::unique_ptr<uint8, std::function<void(uint8 *)>>(stream->ReadArray<uint8>(fileSize), Memory::Free<uint8>);
    uint32 checksum = sawyercoding_calculate_checksum(data.get(), fileSize);

    // Write the checksum on the end
    stream->SetPosition(fileSize);
    stream->WriteValue(checksum);
}

void S6Exporter::Export()
{
    sint32 spatial_cycle = check_for_spatial_index_cycles(false);
    sint32 regular_cycle = check_for_sprite_list_cycles(false);
    sint32 disjoint_sprites_count = fix_disjoint_sprites();
    openrct2_assert(spatial_cycle == -1, "Sprite cycle exists in spatial list %d", spatial_cycle);
    openrct2_assert(regular_cycle == -1, "Sprite cycle exists in regular list %d", regular_cycle);
    // This one is less harmful, no need to assert for it ~janisozaur
    if (disjoint_sprites_count > 0)
    {
        log_error("Found %d disjoint null sprites", disjoint_sprites_count);
    }
    _s6.info = gS6Info;
    uint32 researchedTrackPiecesA[128];
    uint32 researchedTrackPiecesB[128];

    rct_object_entry entries[] = {
		    {207138944, { 'A', 'R', 'R', 'T', '1', ' ', ' ', ' '}, 108251991 },
		    {207138944, { 'A', 'M', 'L', '1', ' ', ' ', ' ', ' '}, 4032499410 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '0', '3'}, 292122438 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '2', '2'}, 4111112706 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '3', '7'}, 3538419901 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '3', '8'}, 1633522833 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '3', '9'}, 440312517 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '0'}, 1949610130 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '1'}, 1050921928 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '2'}, 556816059 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '3'}, 2145402207 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '4'}, 1074445810 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '5'}, 1786751961 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '6'}, 4256837498 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '7'}, 3515073965 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '8'}, 3969144398 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '2', '9'}, 1942582420 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '3', '0'}, 702850994 },
		    {237304098, { 'O', 'L', 'D', 'N', 'Y', 'K', '3', '2'}, 607619446 },
		    {2, { 'K', 'N', 'G', '-', 'D', 'C', 'F', 'R'}, 3947956635 },
		    {2, { 'K', 'N', 'G', '-', 'D', 'R', 'C', 'F'}, 824657639 },
		    {2, { 'K', 'N', 'G', '-', 'F', 'D', 'W', 'I'}, 2884743742 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '2', '3'}, 1212617361 },
		    {2, { 'X', 'X', 'R', 'F', 'M', 'T', '2', '4'}, 2315054550 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'B', '1', '6'}, 3096847219 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'B', '3', '2'}, 4287860467 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'B', '3', '3'}, 1231647257 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'B', '3', '4'}, 3650863395 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'B', '8', ' '}, 1716940599 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'R', '1', '6'}, 1820399556 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'R', '3', '2'}, 947306024 },
		    {3, { 'R', 'R', 'T', 'U', 'N', 'N', ' ', ' '}, 1087023038 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'R', 'D', 'R'}, 3081342510 },
		    {207140227, { 'W', 'A', 'L', 'L', 'B', 'R', 'W', 'N'}, 238151024 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'B', '1', '6'}, 2060229466 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'B', '3', '2'}, 2490091958 },
		    {207140227, { 'W', 'A', 'L', 'L', 'G', 'L', '1', '6'}, 488714764 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'B', 'D', 'R'}, 529711786 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'B', 'W', 'N'}, 3291722223 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'F', '1', '6'}, 3448607375 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'F', '3', '2'}, 1513321706 },
		    {207140227, { 'W', 'A', 'L', 'L', 'G', 'L', '3', '2'}, 672256381 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'F', 'A', 'R'}, 3861129305 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'F', 'D', 'R'}, 3693362218 },
		    {207140227, { 'W', 'A', 'L', 'L', 'C', 'F', 'W', 'N'}, 3384333744 },
		    {207140227, { 'W', 'A', 'L', 'L', 'G', 'L', '8', ' '}, 3222602807 },
		    {167074307, { '1', 'K', 'W', 'D', 'W', 'D', 'B', '4'}, 1208953145 },
		    {167074307, { 'A', 'R', 'G', 'O', '2', '0', ' ', ' '}, 3826387345 },
		    {207140483, { 'W', 'G', 'W', '2', ' ', ' ', ' ', ' '}, 2862249326 },
		    {207140227, { 'W', 'A', 'L', 'L', 'L', 'T', '3', '2'}, 2076956440 },
		    {207140227, { 'W', 'A', 'L', 'L', 'M', 'M', '1', '6'}, 396253040 },
		    {207140227, { 'W', 'A', 'L', 'L', 'M', 'M', '1', '7'}, 3467377441 },
		    {207140227, { 'W', 'A', 'L', 'L', 'P', 'O', 'S', 'T'}, 1150008683 },
		    {167074307, { 'A', 'R', 'G', 'O', '2', '1', ' ', ' '}, 3868139130 },
		    {207140227, { 'W', 'A', 'L', 'L', 'R', 'S', '1', '6'}, 3124577121 },
		    {207140227, { 'W', 'A', 'L', 'L', 'R', 'S', '3', '2'}, 779999931 },
		    {213714195, { 'W', 'L', 'O', 'G', '0', '1', ' ', ' '}, 4006568542 },
		    {207140227, { 'W', 'A', 'L', 'L', 'S', 'I', 'G', 'N'}, 3008435362 },
		    {207140227, { 'W', 'A', 'L', 'L', 'S', 'K', '1', '6'}, 2730794776 },
		    {207140227, { 'W', 'A', 'L', 'L', 'S', 'K', '3', '2'}, 2118368015 },
		    {207140227, { 'W', 'A', 'L', 'L', 'T', 'N', '3', '2'}, 998114095 },
		    {213714195, { 'W', 'L', 'O', 'G', '0', '2', ' ', ' '}, 2097808671 },
		    {167074307, { 'L', 'M', 'P', 'W', 'I', 'N', '0', '1'}, 3870486796 },
		    {213714195, { 'W', 'L', 'O', 'G', '0', '3', ' ', ' '}, 4215547280 },
		    {207140227, { 'W', 'B', 'R', '2', ' ', ' ', ' ', ' '}, 1817699740 },
		    {207140227, { 'W', 'B', 'R', '2', 'A', ' ', ' ', ' '}, 71596360 },
		    {207140227, { 'W', 'B', 'R', '3', ' ', ' ', ' ', ' '}, 845559201 },
		    {167074307, { 'M', 'G', '-', 'W', 'D', 'W', '0', '1'}, 2800207370 },
		    {3, { 'M', 'G', 'G', 'D', 'R', '1', ' ', ' '}, 2868091836 },
		    {169377795, { 'M', 'S', '-', 'W', 'I', 'N', '1', '8'}, 325280509 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '0', '3', ' '}, 449888106 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '0', '6', ' '}, 3589147511 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '1', '4', ' '}, 2294255533 },
		    {207140483, { 'W', 'F', 'W', '1', ' ', ' ', ' ', ' '}, 2501787952 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '1', '6', ' '}, 3589513616 },
		    {207140483, { 'W', 'H', 'G', ' ', ' ', ' ', ' ', ' '}, 3129655487 },
		    {207140483, { 'W', 'H', 'G', 'G', ' ', ' ', ' ', ' '}, 1834334778 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '1', '7', ' '}, 187010147 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '2', '1', ' '}, 1119324001 },
		    {207140483, { 'W', 'M', 'F', 'G', ' ', ' ', ' ', ' '}, 3814500730 },
		    {207140483, { 'W', 'P', 'F', ' ', ' ', ' ', ' ', ' '}, 1706407433 },
		    {207140483, { 'W', 'P', 'F', 'G', ' ', ' ', ' ', ' '}, 1222902233 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '2', '2', ' '}, 2564717564 },
		    {3, { '1', 'K', 'P', 'L', 'W', 'A', 'L', 'L'}, 2577547177 },
		    {207140483, { 'W', 'S', 'W', ' ', ' ', ' ', ' ', ' '}, 959084567 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '2', '4', ' '}, 4137542772 },
		    {167074307, { 'X', 'X', 'W', 'D', 'W', '2', '5', ' '}, 3620255233 },
		    {207140483, { 'W', 'S', 'W', 'G', ' ', ' ', ' ', ' '}, 3279450096 },
		    {213714195, { 'W', 'L', 'O', 'G', '0', '4', ' ', ' '}, 3177146731 },
		    {213714195, { 'W', 'L', 'O', 'G', '0', '5', ' ', ' '}, 3006556159 },
		    {213714451, { 'W', 'L', 'O', 'G', '0', '6', ' ', ' '}, 1437368121 },
		    {213714707, { 'W', 'P', 'A', 'L', 'M', '0', '1', ' '}, 3637918208 },
		    {213714707, { 'W', 'P', 'A', 'L', 'M', '0', '2', ' '}, 3363095945 },
		    {213714707, { 'W', 'P', 'A', 'L', 'M', '0', '3', ' '}, 85130227 },
		    {213714707, { 'W', 'P', 'A', 'L', 'M', '0', '4', ' '}, 3653812425 },
		    {213714707, { 'W', 'P', 'A', 'L', 'M', '0', '5', ' '}, 3688663891 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '1'}, 2474440161 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '2'}, 77043823 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '3'}, 2723201851 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '4'}, 798071626 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '5'}, 387969067 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '6'}, 3303328397 },
		    {3, { 'F', 'M', '1', '6', 'S', 'P', 'W', 'L'}, 2443551379 },
		    {167074307, { 'F', 'M', 'A', 'R', '1', '1', 'W', 'L'}, 3007697711 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '7'}, 3965486900 },
		    {167074307, { 'F', 'M', 'A', 'R', '1', '4', 'W', 'L'}, 1479721761 },
		    {167074307, { 'F', 'M', 'A', 'R', 'C', '1', 'W', 'L'}, 294908909 },
		    {167074307, { 'F', 'M', 'A', 'R', 'C', '3', 'W', 'L'}, 3633510571 },
		    {167074307, { 'F', 'M', 'A', 'R', 'C', '4', 'W', 'L'}, 2435176296 },
		    {167074307, { 'F', 'M', 'A', 'R', 'C', '5', 'W', 'L'}, 2262255087 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '8'}, 344887184 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '0', '9'}, 2591224689 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '1', '0'}, 3824734672 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '1', '1'}, 2560288174 },
		    {167074307, { 'F', 'M', 'P', 'O', 'S', 'T', 'S', ' '}, 3009571425 },
		    {167074307, { 'F', 'M', 'P', 'S', 'T', 'B', 'E', 'M'}, 2305469636 },
		    {167074307, { 'F', 'M', 'S', 'P', 'N', '1', 'W', 'L'}, 147334333 },
		    {167074307, { 'F', 'M', 'S', 'P', 'N', '2', 'W', 'L'}, 1704562820 },
		    {167074307, { 'F', 'M', 'S', 'P', 'N', '3', 'W', 'L'}, 1422340831 },
		    {3, { 'F', 'M', 'S', 'P', 'W', 'L', '3', '2'}, 3265115207 },
		    {213714451, { 'W', 'S', 'K', 'Y', 'S', 'C', '1', '2'}, 2115820948 },
		    {167074307, { 'J', 'A', 'C', 'S', 'W', '3', '2', ' '}, 3860185730 },
		    {167074307, { 'J', 'A', 'C', 'S', 'W', '3', '2', 'T'}, 243850743 },
		    {167074307, { 'L', 'K', 'A', 'R', 'B', '1', ' ', ' '}, 240242761 },
		    {167074307, { 'L', 'K', 'A', 'R', 'B', 'F', ' ', ' '}, 3717061360 },
		    {167074307, { 'L', 'K', 'A', 'R', 'L', 'E', 'G', '1'}, 3977570212 },
		    {3, { 'L', 'K', 'A', 'R', 'L', 'E', 'G', '3'}, 1256103988 },
		    {213714707, { 'W', 'W', 'I', 'N', 'D', '0', '3', ' '}, 4041039700 },
		    {3, { 'L', 'K', 'A', 'R', 'L', 'E', 'G', '5'}, 4202320765 },
		    {3, { 'L', 'K', 'A', 'R', 'R', 'H', ' ', ' '}, 893190179 },
		    {167074307, { 'L', 'K', 'A', 'R', 'V', '1', ' ', ' '}, 141682157 },
		    {213714707, { 'W', 'W', 'I', 'N', 'D', '0', '4', ' '}, 3159758451 },
		    {213714707, { 'W', 'W', 'I', 'N', 'D', '0', '5', ' '}, 401811739 },
		    {3, { 'M', 'G', '-', 'F', 'W', 'L', '1', '4'}, 2672375115 },
		    {213714707, { 'W', 'W', 'I', 'N', 'D', '0', '6', ' '}, 1338783325 },
		    {167074307, { 'M', 'G', '-', 'P', 'R', 'B', '3', '6'}, 1428444910 },
		    {167074307, { 'M', 'G', '-', 'P', 'R', 'B', '3', '7'}, 4023598147 },
		    {167074307, { 'M', 'G', '-', 'P', 'R', 'B', '8', ' '}, 1292468832 },
		    {167074307, { 'M', 'K', 'D', 'W', '2', '5', ' ', ' '}, 629775261 },
		    {167074307, { 'M', 'K', 'D', 'W', '2', '7', ' ', ' '}, 2198821961 },
		    {207140227, { 'W', 'A', 'L', 'L', 'M', 'N', '3', '2'}, 100085986 },
		    {167074307, { 'X', 'X', 'D', 'E', 'C', 'O', '0', '6'}, 432476700 },
		    {207140227, { 'W', 'A', 'L', 'L', 'W', 'D', '1', '6'}, 2730789722 },
		    {207140227, { 'W', 'A', 'L', 'L', 'W', 'D', '3', '2'}, 945721573 },
		    {207140227, { 'W', 'A', 'L', 'L', 'W', 'D', '3', '3'}, 3483753664 },
		    {207140227, { 'W', 'A', 'L', 'L', 'W', 'D', '8', ' '}, 2858906036 },
		    {3, { 'G', 'W', 'W', 'T', 'R', 'H', 'W', 'F'}, 2708089975 },
		    {3, { 'S', 'F', 'L', 'W', '2', ' ', ' ', ' '}, 410334196 },
		    {167074307, { 'F', 'M', 'S', 'T', 'R', 'B', 'W', 'L'}, 3206718453 },
		    {207140228, { 'B', 'N', '1', ' ', ' ', ' ', ' ', ' '}, 1537412557 },
		    {207140228, { 'B', 'N', '5', ' ', ' ', ' ', ' ', ' '}, 914962152 },
		    {207138949, { 'P', 'A', 'T', 'H', 'A', 'S', 'H', ' '}, 2029373228 },
		    {207138949, { 'P', 'A', 'T', 'H', 'C', 'R', 'Z', 'Y'}, 2934933314 },
		    {207138949, { 'P', 'A', 'T', 'H', 'D', 'I', 'R', 'T'}, 477452 },
		    {207138949, { 'P', 'A', 'T', 'H', 'S', 'P', 'C', 'E'}, 3262662714 },
		    {207138949, { 'T', 'A', 'R', 'M', 'A', 'C', ' ', ' '}, 2563205185 },
		    {207138949, { 'T', 'A', 'R', 'M', 'A', 'C', 'B', ' '}, 4294130972 },
		    {207138949, { 'R', 'O', 'A', 'D', ' ', ' ', ' ', ' '}, 418900582 },
		    {207138949, { 'T', 'A', 'R', 'M', 'A', 'C', 'G', ' '}, 3302286042 },
		    {167072773, { 'P', 'A', 'T', 'H', 'W', 'O', 'O', 'D'}, 93319345 },
		    {167072773, { 'E', 'C', 'C', '-', 'p', '2', ' ', ' '}, 4223776403 },
		    {167072773, { 'g', 'r', 'a', 'v', 'p', 'a', 't', 'h'}, 2918003560 },
		    {167072773, { 'J', 'B', 'p', 'c', 'n', ' ', ' ', ' '}, 3663714227 },
		    {169379845, { 'S', 'F', 'F', 'T', 'P', 'D', 'T', 'R'}, 3692513076 },
		    {167072773, { 'P', 'A', 'T', 'H', 'I', 'N', 'V', 'S'}, 3965820248 },
		    {167072773, { 'R', 'I', 'C', '-', 'J', 'B', 'D', '1'}, 1970402067 },
		    {169379845, { 'S', 'F', 'F', 'T', 'P', 'T', 'A', 'R'}, 883563815 },
		    {207138950, { 'B', 'E', 'N', 'C', 'H', '1', ' ', ' '}, 3822649119 },
		    {207138950, { 'L', 'I', 'T', 'T', 'E', 'R', 'M', 'N'}, 1451033982 },
		    {207138950, { 'L', 'A', 'M', 'P', '1', ' ', ' ', ' '}, 2052106443 },
		    {207138950, { 'L', 'A', 'M', 'P', '2', ' ', ' ', ' '}, 3490199017 },
		    {207138950, { 'L', 'I', 'T', 'T', 'E', 'R', '1', ' '}, 3174707644 },
		    {207138950, { 'Q', 'T', 'V', '1', ' ', ' ', ' ', ' '}, 708487234 },
		    {207140231, { 'S', 'C', 'G', 'F', 'E', 'N', 'C', 'E'}, 1834038820 },
		    {207140231, { 'S', 'C', 'G', 'G', 'A', 'R', 'D', 'N'}, 2746957708 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '1', ' '}, 3110622323 },
		    {207140231, { 'S', 'C', 'G', 'P', 'A', 'T', 'H', 'X'}, 890227440 },
		    {207140231, { 'S', 'C', 'G', 'S', 'H', 'R', 'U', 'B'}, 2664190634 },
		    {237303335, { 'S', 'C', 'G', '1', '9', '6', '0', 'S'}, 2742236182 },
		    {207140231, { 'S', 'C', 'G', 'T', 'R', 'E', 'E', 'S'}, 2999547766 },
		    {207140231, { 'S', 'C', 'G', 'W', 'A', 'L', 'L', 'S'}, 3518650219 },
		    {7, { 'R', 'R', 'P', 'A', 'C', 'K', ' ', ' '}, 542508538 },
		    {207140231, { 'S', 'C', 'G', 'A', 'B', 'S', 'T', 'R'}, 932253451 },
		    {207140231, { 'S', 'C', 'G', 'M', 'I', 'N', 'E', ' '}, 3638141733 },
		    {213713431, { 'S', 'C', 'G', 'N', 'A', 'M', 'R', 'C'}, 893883856 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '3', ' '}, 28737111 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '4', ' '}, 2227283565 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '5', ' '}, 3801110451 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '6', ' '}, 1723168995 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '7', ' '}, 1376856253 },
		    {7, { 'A', 'V', 'L', '-', 'W', 'F', 'L', '1'}, 1616626967 },
		    {7, { 'L', 'O', 'U', 'G', '1', '7', '9', ' '}, 1743292737 },
		    {213714968, { 'N', 'A', 'E', 'N', 'T', ' ', ' ', ' '}, 3542888893 },
		    {207140489, { 'W', 'T', 'R', 'C', 'Y', 'A', 'N', ' '}, 3147029885 },

    };

    for (sint32 i = 0; i < OBJECT_ENTRY_COUNT; i++)
    {
        const rct_object_entry * entry     = get_loaded_object_entry(i);
        void *                   entryData = get_loaded_object_chunk(i);
        if (entryData == (void *)-1)
        {
            Memory::Set(&_s6.objects[i], 0xFF, sizeof(rct_object_entry));
        }
        else
        {
            _s6.objects[i] = *((rct_object_entry *)entry);
        }
    }

    for (sint32 i = 0; i < Util::CountOf(entries); i++)
    {
	    _s6.objects[i] = entries[i];
    }

    _s6.elapsed_months   = gDateMonthsElapsed;
    _s6.current_day      = gDateMonthTicks;
    _s6.scenario_ticks   = gScenarioTicks;
    _s6.scenario_srand_0 = gScenarioSrand0;
    _s6.scenario_srand_1 = gScenarioSrand1;

    memcpy(_s6.map_elements, gMapElements, sizeof(_s6.map_elements));

    _s6.next_free_map_element_pointer_index = gNextFreeMapElementPointerIndex;
    // Sprites needs to be reset before they get used.
    // Might as well reset them in here to zero out the space and improve
    // compression ratios. Especially useful for multiplayer servers that
    // use zlib on the sent stream.
    sprite_clear_all_unused();
    for (sint32 i = 0; i < MAX_SPRITES; i++)
    {
        memcpy(&_s6.sprites[i], get_sprite(i), sizeof(rct_sprite));
    }

    for (sint32 i = 0; i < NUM_SPRITE_LISTS; i++)
    {
        _s6.sprite_lists_head[i]  = gSpriteListHead[i];
        _s6.sprite_lists_count[i] = gSpriteListCount[i];
    }
    _s6.park_name = gParkName;
    // pad_013573D6
    _s6.park_name_args    = gParkNameArgs;
    _s6.initial_cash      = gInitialCash;
    _s6.current_loan      = gBankLoan;
    _s6.park_flags        = gParkFlags;
    _s6.park_entrance_fee = gParkEntranceFee;
    // rct1_park_entrance_x
    // rct1_park_entrance_y
    // pad_013573EE
    // rct1_park_entrance_z
    memcpy(_s6.peep_spawns, gPeepSpawns, sizeof(_s6.peep_spawns));
    _s6.guest_count_change_modifier = gGuestChangeModifier;
    _s6.current_research_level      = gResearchFundingLevel;
    // pad_01357400
    memcpy(_s6.researched_ride_types, gResearchedRideTypes, sizeof(_s6.researched_ride_types));
    memcpy(_s6.researched_ride_entries, gResearchedRideEntries, sizeof(_s6.researched_ride_entries));
    // Not used by OpenRCT2 any more, but left in to keep RCT2 export working.
    for (uint8 i = 0; i < Util::CountOf(RideTypePossibleTrackConfigurations); i++) {
        researchedTrackPiecesA[i] = (RideTypePossibleTrackConfigurations[i]         ) & 0xFFFFFFFFULL;
        researchedTrackPiecesB[i] = (RideTypePossibleTrackConfigurations[i] >> 32ULL) & 0xFFFFFFFFULL;
    }
    memcpy(_s6.researched_track_types_a, researchedTrackPiecesA, sizeof(_s6.researched_track_types_a));
    memcpy(_s6.researched_track_types_b, researchedTrackPiecesB, sizeof(_s6.researched_track_types_b));

    _s6.guests_in_park          = gNumGuestsInPark;
    _s6.guests_heading_for_park = gNumGuestsHeadingForPark;

    memcpy(_s6.expenditure_table, gExpenditureTable, sizeof(_s6.expenditure_table));

    _s6.last_guests_in_park = gNumGuestsInParkLastWeek;
    // pad_01357BCA
    _s6.handyman_colour = gStaffHandymanColour;
    _s6.mechanic_colour = gStaffMechanicColour;
    _s6.security_colour = gStaffSecurityColour;

    memcpy(_s6.researched_scenery_items, gResearchedSceneryItems, sizeof(_s6.researched_scenery_items));

    _s6.park_rating = gParkRating;

    memcpy(_s6.park_rating_history, gParkRatingHistory, sizeof(_s6.park_rating_history));
    memcpy(_s6.guests_in_park_history, gGuestsInParkHistory, sizeof(_s6.guests_in_park_history));

    _s6.active_research_types        = gResearchPriorities;
    _s6.research_progress_stage      = gResearchProgressStage;
    _s6.last_researched_item_subject = gResearchLastItemSubject;
    // pad_01357CF8
    _s6.next_research_item           = gResearchNextItem;
    _s6.research_progress            = gResearchProgress;
    _s6.next_research_category       = gResearchNextCategory;
    _s6.next_research_expected_day   = gResearchExpectedDay;
    _s6.next_research_expected_month = gResearchExpectedMonth;
    _s6.guest_initial_happiness      = gGuestInitialHappiness;
    _s6.park_size                    = gParkSize;
    _s6.guest_generation_probability = _guestGenerationProbability;
    _s6.total_ride_value             = gTotalRideValue;
    _s6.maximum_loan                 = gMaxBankLoan;
    _s6.guest_initial_cash           = gGuestInitialCash;
    _s6.guest_initial_hunger         = gGuestInitialHunger;
    _s6.guest_initial_thirst         = gGuestInitialThirst;
    _s6.objective_type               = gScenarioObjectiveType;
    _s6.objective_year               = gScenarioObjectiveYear;
    // pad_013580FA
    _s6.objective_currency = gScenarioObjectiveCurrency;
    _s6.objective_guests   = gScenarioObjectiveNumGuests;
    memcpy(_s6.campaign_weeks_left, gMarketingCampaignDaysLeft, sizeof(_s6.campaign_weeks_left));
    memcpy(_s6.campaign_ride_index, gMarketingCampaignRideIndex, sizeof(_s6.campaign_ride_index));

    memcpy(_s6.balance_history, gCashHistory, sizeof(_s6.balance_history));

    _s6.current_expenditure            = gCurrentExpenditure;
    _s6.current_profit                 = gCurrentProfit;
    _s6.weekly_profit_average_dividend = gWeeklyProfitAverageDividend;
    _s6.weekly_profit_average_divisor  = gWeeklyProfitAverageDivisor;
    // pad_0135833A

    memcpy(_s6.weekly_profit_history, gWeeklyProfitHistory, sizeof(_s6.weekly_profit_history));

    _s6.park_value = gParkValue;

    memcpy(_s6.park_value_history, gParkValueHistory, sizeof(_s6.park_value_history));

    _s6.completed_company_value = gScenarioCompletedCompanyValue;
    _s6.total_admissions        = gTotalAdmissions;
    _s6.income_from_admissions  = gTotalIncomeFromAdmissions;
    _s6.company_value           = gCompanyValue;
    memcpy(_s6.peep_warning_throttle, gPeepWarningThrottle, sizeof(_s6.peep_warning_throttle));

    // Awards
    for (sint32 i = 0; i < RCT12_MAX_AWARDS; i++)
    {
        Award *       src = &gCurrentAwards[i];
        rct12_award * dst = &_s6.awards[i];
        dst->time         = src->Time;
        dst->type         = src->Type;
    }

    _s6.land_price                = gLandPrice;
    _s6.construction_rights_price = gConstructionRightsPrice;
    // unk_01358774
    // pad_01358776
    // _s6.cd_key
    // _s6.game_version_number
    _s6.completed_company_value_record = gScenarioCompanyValueRecord;
    _s6.loan_hash                      = GetLoanHash(gInitialCash, gBankLoan, gMaxBankLoan);
    _s6.ride_count                     = gRideCount;
    // pad_013587CA
    _s6.historical_profit = gHistoricalProfit;
    // pad_013587D4
    memcpy(_s6.scenario_completed_name, gScenarioCompletedBy, sizeof(_s6.scenario_completed_name));
    _s6.cash = gCashEncrypted;
    // pad_013587FC
    _s6.park_rating_casualty_penalty = gParkRatingCasualtyPenalty;
    _s6.map_size_units               = gMapSizeUnits;
    _s6.map_size_minus_2             = gMapSizeMinus2;
    _s6.map_size                     = gMapSize;
    _s6.map_max_xy                   = gMapSizeMaxXY;
    _s6.same_price_throughout        = gSamePriceThroughoutParkA;
    _s6.suggested_max_guests         = _suggestedGuestMaximum;
    _s6.park_rating_warning_days     = gScenarioParkRatingWarningDays;
    _s6.last_entrance_style          = gLastEntranceStyle;
    // rct1_water_colour
    // pad_01358842
    memcpy(_s6.research_items, gResearchItems, sizeof(_s6.research_items));
    _s6.map_base_z = gMapBaseZ;
    memcpy(_s6.scenario_name, gScenarioName, sizeof(_s6.scenario_name));
    memcpy(_s6.scenario_description, gScenarioDetails, sizeof(_s6.scenario_description));
    _s6.current_interest_rate = gBankLoanInterestRate;
    // pad_0135934B
    _s6.same_price_throughout_extended = gSamePriceThroughoutParkB;
    // Preserve compatibility with vanilla RCT2's save format.
    for (uint8 i = 0; i < RCT12_MAX_PARK_ENTRANCES; i++)
    {
        _s6.park_entrance_x[i]         = gParkEntrances[i].x;
        _s6.park_entrance_y[i]         = gParkEntrances[i].y;
        _s6.park_entrance_z[i]         = gParkEntrances[i].z;
        _s6.park_entrance_direction[i] = gParkEntrances[i].direction;
    }
    safe_strcpy(_s6.scenario_filename, _scenarioFileName, sizeof(_s6.scenario_filename));
    memcpy(_s6.saved_expansion_pack_names, gScenarioExpansionPacks, sizeof(_s6.saved_expansion_pack_names));
    memcpy(_s6.banners, gBanners, sizeof(_s6.banners));
    memcpy(_s6.custom_strings, gUserStrings, sizeof(_s6.custom_strings));
    _s6.game_ticks_1 = gCurrentTicks;
    memcpy(_s6.rides, gRideList, sizeof(_s6.rides));
    _s6.saved_age           = gSavedAge;
    _s6.saved_view_x        = gSavedViewX;
    _s6.saved_view_y        = gSavedViewY;
    _s6.saved_view_zoom     = gSavedViewZoom;
    _s6.saved_view_rotation = gSavedViewRotation;
    memcpy(_s6.map_animations, gAnimatedObjects, sizeof(_s6.map_animations));
    _s6.num_map_animations = gNumMapAnimations;
    // pad_0138B582

    _s6.ride_ratings_calc_data = gRideRatingsCalcData;
    memcpy(_s6.ride_measurements, gRideMeasurements, sizeof(_s6.ride_measurements));
    _s6.next_guest_index          = gNextGuestNumber;
    _s6.grass_and_scenery_tilepos = gGrassSceneryTileLoopPosition;
    memcpy(_s6.patrol_areas, gStaffPatrolAreas, sizeof(_s6.patrol_areas));
    memcpy(_s6.staff_modes, gStaffModes, sizeof(_s6.staff_modes));
    // unk_13CA73E
    // pad_13CA73F
    _s6.byte_13CA740 = gUnk13CA740;
    _s6.climate      = gClimate;
    // pad_13CA741;
    // byte_13CA742
    // pad_013CA747
    _s6.climate_update_timer   = gClimateUpdateTimer;
    _s6.current_weather        = gClimateCurrentWeather;
    _s6.next_weather           = gClimateNextWeather;
    _s6.temperature            = gClimateCurrentTemperature;
    _s6.next_temperature       = gClimateNextTemperature;
    _s6.current_weather_effect = gClimateCurrentWeatherEffect;
    _s6.next_weather_effect    = gClimateNextWeatherEffect;
    _s6.current_weather_gloom  = gClimateCurrentWeatherGloom;
    _s6.next_weather_gloom     = gClimateNextWeatherGloom;
    _s6.current_rain_level     = gClimateCurrentRainLevel;
    _s6.next_rain_level        = gClimateNextRainLevel;

    // News items
    for (size_t i = 0; i < RCT12_MAX_NEWS_ITEMS; i++)
    {
        const NewsItem *  src = &gNewsItems[i];
        rct12_news_item * dst = &_s6.news_items[i];

        dst->Type      = src->Type;
        dst->Flags     = src->Flags;
        dst->Assoc     = src->Assoc;
        dst->Ticks     = src->Ticks;
        dst->MonthYear = src->MonthYear;
        dst->Day       = src->Day;
        memcpy(dst->Text, src->Text, sizeof(dst->Text));
    }

    // pad_13CE730
    // rct1_scenario_flags
    _s6.wide_path_tile_loop_x = gWidePathTileLoopX;
    _s6.wide_path_tile_loop_y = gWidePathTileLoopY;
    // pad_13CE778

    String::Set(_s6.scenario_filename, sizeof(_s6.scenario_filename), _scenarioFileName);

    if (RemoveTracklessRides)
    {
        scenario_remove_trackless_rides(&_s6);
    }

    scenario_fix_ghosts(&_s6);
    game_convert_strings_to_rct2(&_s6);
}

uint32 S6Exporter::GetLoanHash(money32 initialCash, money32 bankLoan, uint32 maxBankLoan)
{
    sint32 value = 0x70093A;
    value -= initialCash;
    value = ror32(value, 5);
    value -= bankLoan;
    value = ror32(value, 7);
    value += maxBankLoan;
    value = ror32(value, 3);
    return value;
}

extern "C"
{
    enum {
        S6_SAVE_FLAG_EXPORT    = 1 << 0,
        S6_SAVE_FLAG_SCENARIO  = 1 << 1,
        S6_SAVE_FLAG_AUTOMATIC = 1u << 31,
    };

    /**
     *
     *  rct2: 0x006754F5
     * @param flags bit 0: pack objects, 1: save as scenario
     */
    sint32 scenario_save(const utf8 * path, sint32 flags)
    {
        if (flags & S6_SAVE_FLAG_SCENARIO)
        {
            log_verbose("saving scenario");
        }
        else
        {
            log_verbose("saving game");
        }

        if (!(flags & S6_SAVE_FLAG_AUTOMATIC))
        {
            window_close_construction_windows();
        }

        map_reorganise_elements();
        viewport_set_saved_view();

        bool result     = false;
        auto s6exporter = new S6Exporter();
        try
        {
            if (flags & S6_SAVE_FLAG_EXPORT)
            {
                IObjectManager * objManager   = GetObjectManager();
                s6exporter->ExportObjectsList = objManager->GetPackableObjects();
            }
            s6exporter->RemoveTracklessRides = true;
            s6exporter->Export();
            if (flags & S6_SAVE_FLAG_SCENARIO)
            {
                s6exporter->SaveScenario(path);
            }
            else
            {
                s6exporter->SaveGame(path);
            }
            result = true;
        }
        catch (const Exception &)
        {
        }
        delete s6exporter;

        gfx_invalidate_screen();

        if (result && !(flags & S6_SAVE_FLAG_AUTOMATIC))
        {
            gScreenAge = 0;
        }
        return result;
    }
}
