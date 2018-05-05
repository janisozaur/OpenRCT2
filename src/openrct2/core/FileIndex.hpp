#pragma region Copyright (c) 2017 OpenRCT2 Developers
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

#pragma once

#include <chrono>
#include <string>
#include <tuple>
#include <vector>
#include <list>
#include "../common.h"
#include "Console.hpp"
#include "File.h"
#include "FileScanner.h"
#include "FileStream.hpp"
#include "JobPool.hpp"
#include "Path.hpp"

template<typename TItem>
class FileIndex
{
private:
    struct DirectoryStats
    {
        uint32 TotalFiles = 0;
        uint64 TotalFileSize = 0;
        uint32 FileDateModifiedChecksum = 0;
        uint32 PathChecksum = 0;
    };

    struct ScanResult
    {
        DirectoryStats const Stats;
        std::vector<std::string> const Files;

        ScanResult(DirectoryStats stats, std::vector<std::string> files)
            : Stats(stats),
              Files(files)
        {
        }
    };

    struct FileIndexHeader
    {
        uint32          HeaderSize = sizeof(FileIndexHeader);
        uint32          MagicNumber = 0;
        uint8           VersionA = 0;
        uint8           VersionB = 0;
        uint16          LanguageId = 0;
        DirectoryStats  Stats;
        uint32          NumItems = 0;
    };

    // Index file format version which when incremented forces a rebuild
    static constexpr uint8 FILE_INDEX_VERSION = 4;

    std::string const _name;
    uint32 const _magicNumber;
    uint8 const _version;
    std::string const _indexPath;
    std::string const _pattern;

public:
    std::vector<std::string> const SearchPaths;

public:
    /**
     * Creates a new FileIndex.
     * @param name Name of the index (used for logging).
     * @param magicNumber Magic number for the index (to distinguish between different index files).
     * @param version Version of the specialised index, increment this to force a rebuild.
     * @param indexPath Full path to read and write the index file to.
     * @param pattern The search pattern for indexing files.
     * @param paths A list of search directories.
     */
    FileIndex(std::string name,
              uint32 magicNumber,
              uint8 version,
              std::string indexPath,
              std::string pattern,
              std::vector<std::string> paths) :
        _name(name),
        _magicNumber(magicNumber),
        _version(version),
        _indexPath(indexPath),
        _pattern(pattern),
        SearchPaths(paths)
    {
    }

    virtual ~FileIndex() = default;

    /**
     * Queries and directories and loads the index header. If the index is up to date,
     * the items are loaded from the index and returned, otherwise the index is rebuilt.
     */
    std::vector<TItem> LoadOrBuild(sint32 language) const
    {
        std::vector<TItem> items;
        auto scanResult = Scan();
        auto readIndexResult = ReadIndexFile(language, scanResult.Stats);
        if (std::get<0>(readIndexResult))
        {
            // Index was loaded
            items = std::get<1>(readIndexResult);
        }
        else
        {
            // Index was not loaded
            items = Build(language, scanResult);
        }
        return items;
    }

    std::vector<TItem> Rebuild(sint32 language) const
    {
        auto scanResult = Scan();
        auto items = Build(language, scanResult);
        return items;
    }

protected:
    /**
     * Loads the given file and creates the item representing the data to store in the index.
     * TODO Use std::optional when C++17 is available.
     */
    virtual std::tuple<bool, TItem> Create(const std::string &path) const abstract;

    /**
     * Serialises an index item to the given stream.
     */
    virtual void Serialise(IStream * stream, const TItem &item) const abstract;

    /**
     * Deserialises an index item from the given stream.
     */
    virtual TItem Deserialise(IStream * stream) const abstract;

private:
    ScanResult Scan() const
    {
        DirectoryStats stats {};
        std::vector<std::string> files;
        for (const auto directory : SearchPaths)
        {
            log_verbose("FileIndex:Scanning for %s in '%s'", _pattern.c_str(), directory.c_str());

            auto pattern = Path::Combine(directory, _pattern);
            auto scanner = Path::ScanDirectory(pattern, true);
            while (scanner->Next())
            {
                auto fileInfo = scanner->GetFileInfo();
                auto path = std::string(scanner->GetPath());

                files.push_back(path);

                stats.TotalFiles++;
                stats.TotalFileSize += fileInfo->Size;
                stats.FileDateModifiedChecksum ^=
                    (uint32)(fileInfo->LastModified >> 32) ^
                    (uint32)(fileInfo->LastModified & 0xFFFFFFFF);
                stats.FileDateModifiedChecksum = ror32(stats.FileDateModifiedChecksum, 5);
                stats.PathChecksum += GetPathChecksum(path);
            }
            delete scanner;
        }
        return ScanResult(stats, files);
    }

    void BuildRange(const ScanResult &scanResult,
                    size_t rangeStart,
                    size_t rangeEnd,
                    std::vector<TItem>& items,
                    std::atomic<size_t>& processed,
                    std::mutex& printLock) const
    {
        items.reserve(rangeEnd - rangeStart);
        for (size_t i = rangeStart; i < rangeEnd; i++)
        {
            const auto& filePath = scanResult.Files.at(i);

            if (_log_levels[DIAGNOSTIC_LEVEL_VERBOSE])
            {
                std::lock_guard<std::mutex> lock(printLock);
                log_verbose("FileIndex:Indexing '%s'", filePath.c_str());
            }

            auto item = Create(filePath);
            if (std::get<0>(item))
            {
                items.push_back(std::get<1>(item));
            }

            processed++;
        }
    }

    std::vector<TItem> Build(sint32 language, const ScanResult &scanResult) const
    {
        std::vector<TItem> allItems;
        Console::WriteLine("Building %s (%zu items)", _name.c_str(), scanResult.Files.size());

        auto startTime = std::chrono::high_resolution_clock::now();

        const size_t totalCount = scanResult.Files.size();
        if (totalCount > 0)
        {
            JobPool jobPool;
            std::mutex printLock; // For verbose prints.

            std::list<std::vector<TItem>> containers;

            size_t stepSize = 100; // Handpicked, seems to work well with 4/8 cores.

            std::atomic<size_t> processed = ATOMIC_VAR_INIT(0);

            auto reportProgress =
                [&]()
                {
                    const size_t completed = processed;
                    Console::WriteFormat("File %5d of %d, done %3d%%\r", completed, totalCount, completed * 100 / totalCount);
                };

            for (size_t rangeStart = 0; rangeStart < totalCount; rangeStart += stepSize)
            {
                if (rangeStart + stepSize > totalCount)
                {
                    stepSize = totalCount - rangeStart;
                }

                auto& items = containers.emplace_back();

                jobPool.AddTask(std::bind(&FileIndex<TItem>::BuildRange,
                    this,
                    std::cref(scanResult),
                    rangeStart,
                    rangeStart + stepSize,
                    std::ref(items),
                    std::ref(processed),
                    std::ref(printLock)));

                reportProgress();
            }

            jobPool.Join(reportProgress);

            for (auto&& itr : containers)
            {
                allItems.insert(allItems.end(), itr.begin(), itr.end());
            }

            WriteIndexFile(language, scanResult.Stats, allItems);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = (std::chrono::duration<float>)(endTime - startTime);
        Console::WriteLine("Finished building %s in %.2f seconds.", _name.c_str(), duration.count());

        return allItems;
    }

    std::tuple<bool, std::vector<TItem>> ReadIndexFile(sint32 language, const DirectoryStats &stats) const
    {
        bool loadedItems = false;
        std::vector<TItem> items;
        if (File::Exists(_indexPath))
        {
            try
            {
                log_verbose("FileIndex:Loading index: '%s'", _indexPath.c_str());
                auto fs = FileStream(_indexPath, FILE_MODE_OPEN);

                // Read header, check if we need to re-scan
                auto header = fs.ReadValue<FileIndexHeader>();
                if (header.HeaderSize == sizeof(FileIndexHeader) &&
                    header.MagicNumber == _magicNumber &&
                    header.VersionA == FILE_INDEX_VERSION &&
                    header.VersionB == _version &&
                    header.LanguageId == language &&
                    header.Stats.TotalFiles == stats.TotalFiles &&
                    header.Stats.TotalFileSize == stats.TotalFileSize &&
                    header.Stats.FileDateModifiedChecksum == stats.FileDateModifiedChecksum &&
                    header.Stats.PathChecksum == stats.PathChecksum)
                {
                    items.reserve(header.NumItems);
                    // Directory is the same, just read the saved items
                    for (uint32 i = 0; i < header.NumItems; i++)
                    {
                        auto item = Deserialise(&fs);
                        items.push_back(item);
                    }
                    loadedItems = true;
                }
                else
                {
                    Console::WriteLine("%s out of date", _name.c_str());
                }
            }
            catch (const std::exception &e)
            {
                Console::Error::WriteLine("Unable to load index: '%s'.", _indexPath.c_str());
                Console::Error::WriteLine("%s", e.what());
            }
        }
        return std::make_tuple(loadedItems, items);
    }

    void WriteIndexFile(sint32 language, const DirectoryStats &stats, const std::vector<TItem> &items) const
    {
        try
        {
            log_verbose("FileIndex:Writing index: '%s'", _indexPath.c_str());
            Path::CreateDirectory(Path::GetDirectory(_indexPath));
            auto fs = FileStream(_indexPath, FILE_MODE_WRITE);

            // Write header
            FileIndexHeader header;
            header.MagicNumber = _magicNumber;
            header.VersionA = FILE_INDEX_VERSION;
            header.VersionB = _version;
            header.LanguageId = language;
            header.Stats = stats;
            header.NumItems = (uint32)items.size();
            fs.WriteValue(header);

            // Write items
            for (const auto item : items)
            {
                Serialise(&fs, item);
            }
        }
        catch (const std::exception &e)
        {
            Console::Error::WriteLine("Unable to save index: '%s'.", _indexPath.c_str());
            Console::Error::WriteLine("%s", e.what());
        }
    }

    static uint32 GetPathChecksum(const std::string &path)
    {
        uint32 hash = 0xD8430DED;
        for (const utf8 * ch = path.c_str(); *ch != '\0'; ch++)
        {
            hash += (*ch);
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }
        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);
        return hash;
    }
};
