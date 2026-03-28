/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "CommandLine.hpp"

#include "../core/Console.hpp"
#include "../core/File.h"
#include "../core/FileScanner.h"
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../core/FileSystem.hpp"
#include "../core/Zip.h"

#ifndef __ANDROID__
    #include <zip.h>
#endif
#include <string>
#include <vector>

namespace OpenRCT2
{
    exitcode_t HandleAssetBundle(CommandLineArgEnumerator* enumerator)
    {
        const char* zipPath;
        if (!enumerator->TryPopString(&zipPath))
        {
            Console::Error::WriteLine("Expected zip path.");
            return EXITCODE_FAIL;
        }

        const char* dataDir;
        if (!enumerator->TryPopString(&dataDir))
        {
            Console::Error::WriteLine("Expected data directory.");
            return EXITCODE_FAIL;
        }

        std::vector<std::string> additionalFiles;
        const char* extraFile;
        while (enumerator->TryPopString(&extraFile))
        {
            additionalFiles.push_back(extraFile);
        }

        try
        {
            auto archive = Zip::Open(zipPath, ZipAccess::write);

            // Add files from data directory
            auto pattern = Path::Combine(dataDir, u8"*");
            auto dataFiles = Path::ScanDirectory(pattern, true);
            while (dataFiles->Next())
            {
                auto fullPath = dataFiles->GetPath();
                auto relativePath = dataFiles->GetPathRelative();
                auto data = File::ReadAllBytes(fullPath);
                archive->SetFileData(relativePath, std::move(data));
                archive->SetFileCompression(relativePath, ZIP_CM_STORE);
            }

            // Add additional files (usually .dat files)
            for (const auto& file : additionalFiles)
            {
                auto data = File::ReadAllBytes(file);
                auto filename = Path::GetFileName(file);
                archive->SetFileData(filename, std::move(data));
                archive->SetFileCompression(filename, ZIP_CM_STORE);
            }

            // Important: ZipArchive destructor calls zip_close which actually writes the file.
            archive.reset();

            Console::WriteLine("Successfully bundled assets into %s", zipPath);
            return EXITCODE_OK;
        }
        catch (const std::exception& e)
        {
            Console::Error::WriteLine("Failed to bundle assets: %s", e.what());
            return EXITCODE_FAIL;
        }
    }

} // namespace OpenRCT2
