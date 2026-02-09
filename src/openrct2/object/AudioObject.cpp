/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioObject.h"

#include "../AssetPackManager.h"
#include "../Context.h"
#include "../Diagnostic.h"
#include "../PlatformEnvironment.h"
#include "../audio/AudioContext.h"
#include "../core/Guard.hpp"
#include "../core/Json.hpp"
#include "../core/Path.hpp"

namespace OpenRCT2
{
    void AudioObject::Load()
    {
        auto* context = GetContext();
        if (context == nullptr)
        {
            LOG_WARNING("AudioObject::Load skipped: context is null for '%s'", std::string(GetIdentifier()).c_str());
            return;
        }

        // Start with base samples
        _loadedSampleTable.LoadFrom(_sampleTable, 0, _sampleTable.GetCount());

        // Override samples from asset packs
        auto assetManager = context->GetAssetPackManager();
        if (assetManager != nullptr)
        {
            assetManager->LoadSamplesForObject(GetIdentifier(), _loadedSampleTable);
        }

        _loadedSampleTable.Load();
    }

    void AudioObject::Unload()
    {
        _loadedSampleTable.Unload();
    }

    void AudioObject::ReadJson(IReadObjectContext* context, json_t& root)
    {
        Guard::Assert(root.is_object(), "AudioObject::ReadJson expects parameter root to be object");
        _sampleTable.ReadFromJson(context, root);
        PopulateTablesFromJson(context, root);
    }

    Audio::IAudioSource* AudioObject::GetSample(uint32_t index) const
    {
        return _loadedSampleTable.GetSample(index);
    }

    int32_t AudioObject::GetSampleModifier(uint32_t index) const
    {
        return _loadedSampleTable.GetSampleModifier(index);
    }
} // namespace OpenRCT2
