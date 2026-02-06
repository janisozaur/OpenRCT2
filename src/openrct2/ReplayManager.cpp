/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ReplayManager.h"

#include "Context.h"
#include "Diagnostic.h"
#include "Game.h"
#include "GameState.h"
#include "GameStateSnapshots.h"
#include "OpenRCT2.h"
#include "ParkImporter.h"
#include "PlatformEnvironment.h"
#include "actions/CheatSetAction.h"
#include "actions/FootpathPlaceAction.h"
#include "actions/GameAction.h"
#include "actions/RideEntranceExitPlaceAction.h"
#include "actions/TileModifyAction.h"
#include "actions/TrackPlaceAction.h"
#include "config/Config.h"
#include "core/Compression.h"
#include "core/DataSerialiser.h"
#include "core/EnumUtils.hpp"
#include "core/FileStream.h"
#include "core/FileSystem.hpp"
#include "core/Guard.hpp"
#include "core/Path.hpp"
#include "core/String.hpp"
#include "entity/EntityRegistry.h"
#include "entity/EntityTweener.h"
#include "interface/Window.h"
#include "localisation/Formatting.h"
#include "localisation/StringIds.h"
#include "management/NewsItem.h"
#include "object/ObjectManager.h"
#include "object/ObjectRepository.h"
#include "park/ParkFile.h"
#include "scenario/Scenario.h"
#include "world/Park.h"

#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenRCT2
{
    using namespace OpenRCT2::GameActions;

    struct ReplayCommand
    {
        uint32_t tick = 0;
        GameAction::Ptr action;
        uint32_t commandIndex = 0;

        ReplayCommand() = default;

        ReplayCommand(uint32_t t, std::unique_ptr<GameAction>&& ga, uint32_t id)
            : tick(t)
            , action(std::move(ga))
            , commandIndex(id)

        {
        }

        bool operator<(const ReplayCommand& comp) const
        {
            // First sort by tick
            if (tick < comp.tick)
                return true;
            if (tick > comp.tick)
                return false;

            // If the ticks are equal sort by commandIndex
            return commandIndex < comp.commandIndex;
        }
    };

    struct ReplayRecordFile
    {
        uint32_t magic;
        uint16_t version;
        uint64_t uncompressedSize;
        MemoryStream data;
    };

    enum class ReplayEntryType : uint8_t
    {
        Action = 0,
        Checksum = 1,
        Snapshot = 2,
        End = 0xFF,
    };

    struct ReplayRecordData
    {
        uint32_t magic;
        uint16_t version;
        std::string networkId;
        MemoryStream parkData;
        MemoryStream parkParams;
        MemoryStream cheatData;
        std::string name;      // Name of play
        std::string filePath;  // File path of replay.
        uint64_t timeRecorded; // Posix Time.
        uint32_t tickStart;    // First tick of replay.
        uint32_t tickEnd;      // Last tick of replay.
        std::multiset<ReplayCommand> commands;
        std::vector<std::pair<uint32_t, EntitiesChecksum>> checksums;
        uint32_t checksumIndex;
        MemoryStream gameStateSnapshots;
    };

    class ReplayManager final : public IReplayManager
    {
        static constexpr uint16_t kReplayVersion = 12;
        static constexpr uint16_t kReplayMinCompatVersion = 10;
        static constexpr uint32_t kReplayMagic = 0x5243524F;    // ORCR.
        static constexpr uint32_t kReplayMagicTmp = 0x5452434F; // ORCT.
        static constexpr int kReplayCompressionLevel = 18;
        static constexpr int kNormalRecordingChecksumTicks = 1;
        static constexpr int kSilentRecordingChecksumTicks = 40; // Same as network server

        enum class ReplayMode
        {
            NONE = 0,
            RECORDING,
            PLAYING,
            NORMALISATION,
        };

        static constexpr std::array modeToName = {
            "NONE",
            "RECORDING",
            "PLAYING",
            "NORMALISATION",
        };

    public:
        virtual ~ReplayManager()
        {
        }

        virtual bool IsReplaying() const override
        {
            return _mode == ReplayMode::PLAYING;
        }

        virtual bool IsRecording() const override
        {
            return _mode == ReplayMode::RECORDING;
        }

        virtual bool IsNormalising() const override
        {
            return _mode == ReplayMode::NORMALISATION;
        }

        virtual bool ShouldDisplayNotice() const override
        {
            return IsRecording() && _recordType == RecordType::NORMAL;
        }

        virtual void AddGameAction(uint32_t tick, const GameAction* action) override
        {
            if (_currentRecording == nullptr)
                return;

            auto ga = Clone(action);

            ReplayCommand command(tick, std::move(ga), _commandId++);

            if (_incrementalSerialiser != nullptr)
            {
                try
                {
                    *_incrementalSerialiser << ReplayEntryType::Action;
                    SerialiseCommand(*_incrementalSerialiser, command);
                    _incrementalFileStream->Flush();
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("Failed to write incremental action: %s", ex.what());
                }
            }

            _currentRecording->commands.emplace(std::move(command));
        }

        void AddChecksum(uint32_t tick, EntitiesChecksum&& checksum)
        {
            if (_incrementalSerialiser != nullptr)
            {
                try
                {
                    *_incrementalSerialiser << ReplayEntryType::Checksum;
                    *_incrementalSerialiser << tick;
                    *_incrementalSerialiser << checksum.raw;
                    _incrementalFileStream->Flush();
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("Failed to write incremental checksum: %s", ex.what());
                }
            }

            _currentRecording->checksums.emplace_back(std::make_pair(tick, std::move(checksum)));
        }

        // Function runs each Tick.
        virtual void Update() override
        {
            if (_mode == ReplayMode::NONE)
                return;

            const auto currentTicks = getGameState().currentTicks;

            if ((_mode == ReplayMode::RECORDING || _mode == ReplayMode::NORMALISATION) && currentTicks == _nextChecksumTick)
            {
                EntitiesChecksum checksum = getGameState().entities.GetAllEntitiesChecksum();
                AddChecksum(currentTicks, std::move(checksum));

                _nextChecksumTick = currentTicks + ChecksumTicksDelta();
            }

            if (_mode == ReplayMode::RECORDING)
            {
                if (currentTicks >= _currentRecording->tickEnd)
                {
                    StopRecording();
                }
            }
            else if (_mode == ReplayMode::PLAYING)
            {
#ifndef DISABLE_NETWORK
                // If the network is disabled we will only get a dummy hash which will cause
                // false positives during replay.
                if (!gSilentReplays)
                {
                    CheckState();
                }
#endif
                ReplayCommands();

                // Normal playback will always end at the specific tick.
                if (currentTicks >= _currentReplay->tickEnd)
                {
                    StopPlayback();
                }
            }
            else if (_mode == ReplayMode::NORMALISATION)
            {
                ReplayCommands();

                // If we run out of commands we can just stop
                if (_currentReplay->commands.empty())
                {
                    StopPlayback();
                    StopRecording();

                    // Reset mode, in normalisation nothing will set it.
                    _mode = ReplayMode::NONE;
                }
            }
        }

        void TakeGameStateSnapshot(MemoryStream& snapshotStream)
        {
            IGameStateSnapshots* snapshots = GetContext()->GetGameStateSnapshots();

            auto& snapshot = snapshots->CreateSnapshot();
            snapshots->Capture(snapshot);
            snapshots->LinkSnapshot(snapshot, getGameState().currentTicks, ScenarioRandState().s0);
            DataSerialiser snapShotDs(true, snapshotStream);
            snapshots->SerialiseSnapshot(snapshot, snapShotDs);
        }

        virtual bool StartRecording(
            const std::string& name, uint32_t maxTicks /*= k_MaxReplayTicks*/, RecordType rt /*= RecordType::NORMAL*/) override
        {
            // If using silent recording, discard whatever recording there is going on, even if a new silent recording is to be
            // started.
            if (_mode == ReplayMode::RECORDING && _recordType == RecordType::SILENT)
                StopRecording(true);

            if (_mode != ReplayMode::NONE && _mode != ReplayMode::NORMALISATION)
                return false;

            auto& gameState = getGameState();
            const auto currentTicks = gameState.currentTicks;

            auto replayData = std::make_unique<ReplayRecordData>();
            replayData->magic = kReplayMagic;
            replayData->version = kReplayVersion;
            replayData->networkId = Network::GetVersion();
            replayData->name = name;
            replayData->tickStart = currentTicks;
            if (maxTicks != k_MaxReplayTicks)
                replayData->tickEnd = currentTicks + maxTicks;
            else
                replayData->tickEnd = k_MaxReplayTicks;

            replayData->filePath = name;

            auto context = GetContext();
            auto& objManager = context->GetObjectManager();
            auto objects = objManager.GetPackableObjects();

            auto exporter = std::make_unique<ParkFileExporter>();
            exporter->ExportObjectsList = objects;
            exporter->Export(gameState, replayData->parkData, Compression::kNoCompressionLevel);

            replayData->timeRecorded = std::chrono::seconds(std::time(nullptr)).count();

            DataSerialiser parkParamsDs(true, replayData->parkParams);
            SerialiseParkParameters(parkParamsDs);

            DataSerialiser cheatDataDs(true, replayData->cheatData);
            SerialiseCheats(cheatDataDs);

            TakeGameStateSnapshot(replayData->gameStateSnapshots);

            if (_mode != ReplayMode::NORMALISATION)
                _mode = ReplayMode::RECORDING;

            _currentRecording = std::move(replayData);
            _recordType = rt;
            _nextChecksumTick = currentTicks + 1;

            // Incremental saving start
            try
            {
                fs::path filePath = _currentRecording->filePath;
                if (filePath.is_relative())
                {
                    if (filePath.extension() != ".parkrep")
                        filePath += ".parkrep";
                    fs::path replayPath = GetContext()->GetPlatformEnvironment().GetDirectoryPath(
                                              DirBase::user, DirId::replayRecordings)
                        / filePath;
                    filePath = replayPath;
                    _currentRecording->filePath = filePath.u8string();
                }

                _incrementalFileStream = std::make_unique<FileStream>(_currentRecording->filePath, FileMode::write);
                _incrementalSerialiser = std::make_unique<DataSerialiser>(true, *_incrementalFileStream);

                // Initial preamble (Magic/Version will be Tmp/12)
                uint32_t oldMagic = _currentRecording->magic;
                uint32_t oldTickEnd = _currentRecording->tickEnd;
                _currentRecording->magic = kReplayMagicTmp;
                _currentRecording->tickEnd = 0xFFFFFFFF; // Mark as live/crashed
                SerialisePreamble(*_incrementalSerialiser, *_currentRecording);
                _currentRecording->magic = oldMagic;
                _currentRecording->tickEnd = oldTickEnd;

                // Write the initial snapshot incrementally as well
                *_incrementalSerialiser << ReplayEntryType::Snapshot;
                *_incrementalSerialiser << _currentRecording->gameStateSnapshots;

                _incrementalFileStream->Flush();
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("Failed to start incremental recording: %s", ex.what());
                _incrementalSerialiser.reset();
                _incrementalFileStream.reset();
            }

            return true;
        }

        virtual bool StopRecording(bool discard = false) override
        {
            if (_mode != ReplayMode::RECORDING && _mode != ReplayMode::NORMALISATION)
                return false;

            if (discard)
            {
                _currentRecording.reset();
                _mode = ReplayMode::NONE;
                return true;
            }

            const auto currentTicks = getGameState().currentTicks;

            _currentRecording->tickEnd = currentTicks;

            {
                EntitiesChecksum checksum = getGameState().entities.GetAllEntitiesChecksum();
                AddChecksum(currentTicks, std::move(checksum));
            }

            TakeGameStateSnapshot(_currentRecording->gameStateSnapshots);

            if (_incrementalSerialiser != nullptr)
            {
                try
                {
                    *_incrementalSerialiser << ReplayEntryType::End;
                    *_incrementalSerialiser << _currentRecording->gameStateSnapshots;
                    _incrementalFileStream->Flush();
                }
                catch (const std::exception& ex)
                {
                    LOG_ERROR("Failed to finalise incremental recording: %s", ex.what());
                }
                _incrementalSerialiser.reset();
                _incrementalFileStream.reset();
            }

            // Final compression to save space.
            // Serialise Body.
            DataSerialiser recSerialiser(true);
            Serialise(recSerialiser, *_currentRecording);
            auto& stream = recSerialiser.GetStream();

            MemoryStream compressed;
            stream.SetPosition(0);
            // header already has decompressed length, but no checksum, so use the ZStandard checksum
            bool compressStatus = Compression::zstdCompress(
                stream, stream.GetLength(), compressed, Compression::ZstdMetadata::checksum, kReplayCompressionLevel);
            if (!compressStatus)
                throw IOException("Compression Error");

            {
                ReplayRecordFile file{ _currentRecording->magic, _currentRecording->version, stream.GetLength(), compressed };

                FileStream filestream(_currentRecording->filePath, FileMode::write);
                DataSerialiser fileSerialiser(true, filestream);
                fileSerialiser << file.magic;
                fileSerialiser << file.version;
                fileSerialiser << file.uncompressedSize;
                fileSerialiser << file.data;
            }

            // When normalizing the output we don't touch the mode.
            if (_mode != ReplayMode::NORMALISATION)
                _mode = ReplayMode::NONE;

            _currentRecording.reset();

            News::Item* news = News::AddItemToQueue(News::ItemType::blank, "Replay recording stopped", 0);
            news->setFlags(News::ItemFlags::hasButton); // Has no subject.

            return true;
        }

        virtual bool GetCurrentReplayInfo(ReplayRecordInfo& info) const override
        {
            ReplayRecordData* data = nullptr;

            if (_mode == ReplayMode::PLAYING)
                data = _currentReplay.get();
            else if (_mode == ReplayMode::RECORDING)
                data = _currentRecording.get();
            else if (_mode == ReplayMode::NORMALISATION)
                data = _currentRecording.get();

            if (data == nullptr)
                return false;

            info.FilePath = data->filePath;
            info.Name = data->name;
            info.Version = data->version;
            info.TimeRecorded = data->timeRecorded;
            if (_mode == ReplayMode::RECORDING)
                info.Ticks = getGameState().currentTicks - data->tickStart;
            else if (_mode == ReplayMode::PLAYING)
                info.Ticks = data->tickEnd - data->tickStart;
            info.NumCommands = static_cast<uint32_t>(data->commands.size());
            info.NumChecksums = static_cast<uint32_t>(data->checksums.size());

            return true;
        }

        void LoadAndCompareSnapshot(MemoryStream& snapshotStream)
        {
            DataSerialiser ds(false, snapshotStream);

            IGameStateSnapshots* snapshots = GetContext()->GetGameStateSnapshots();

            GameStateSnapshot_t& replaySnapshot = snapshots->CreateSnapshot();
            snapshots->SerialiseSnapshot(replaySnapshot, ds);

            const auto currentTicks = getGameState().currentTicks;

            auto& localSnapshot = snapshots->CreateSnapshot();
            snapshots->Capture(localSnapshot);
            snapshots->LinkSnapshot(localSnapshot, currentTicks, ScenarioRandState().s0);
            try
            {
                GameStateCompareData cmpData = snapshots->Compare(replaySnapshot, localSnapshot);

                // Find out if there are any differences between the two states
                auto res = std::find_if(
                    cmpData.spriteChanges.begin(), cmpData.spriteChanges.end(),
                    [](const GameStateSpriteChange& diff) { return diff.changeType != GameStateSpriteChange::EQUAL; });

                // If there are difference write a log to the desyncs folder
                if (res != cmpData.spriteChanges.end())
                {
                    std::string outputPath = GetContext()->GetPlatformEnvironment().GetDirectoryPath(
                        DirBase::user, DirId::desyncLogs);
                    char uniqueFileName[128] = {};
                    snprintf(uniqueFileName, sizeof(uniqueFileName), "replay_desync_%u.txt", currentTicks);

                    std::string outputFile = Path::Combine(outputPath, uniqueFileName);
                    snapshots->LogCompareDataToFile(outputFile, cmpData);
                }
            }
            catch (const std::runtime_error& err)
            {
                LOG_WARNING("Snapshot data failed to be read. Snapshot not compared. %s", err.what());
            }
        }

        void StartPlayback(const std::string& file) override
        {
            if (_mode != ReplayMode::NONE && _mode != ReplayMode::NORMALISATION)
                throw std::invalid_argument(std::string("Unexpected mode ") + modeToName[EnumValue(_mode)]);

            auto replayData = std::make_unique<ReplayRecordData>();

            try
            {
                ReadReplayData(file, *replayData);
            }
            catch (const std::exception&)
            {
                throw;
            }

            if (!LoadReplayDataMap(*replayData))
            {
                throw std::runtime_error("Unable to load map.");
            }

            getGameState().currentTicks = replayData->tickStart;

            LoadAndCompareSnapshot(replayData->gameStateSnapshots);

            _currentReplay = std::move(replayData);
            _currentReplay->checksumIndex = 0;
            _faultyChecksumIndex = -1;

            // Make sure game is not paused.
            gGamePaused = 0;

            if (_mode != ReplayMode::NORMALISATION)
                _mode = ReplayMode::PLAYING;
        }

        virtual bool IsPlaybackStateMismatching() const override
        {
            return _faultyChecksumIndex != -1;
        }

        virtual bool StopPlayback() override
        {
            if (_mode != ReplayMode::PLAYING && _mode != ReplayMode::NORMALISATION)
                return false;

            LoadAndCompareSnapshot(_currentReplay->gameStateSnapshots);

            // During normal playback we pause the game if stopped.
            if (_mode == ReplayMode::PLAYING)
            {
                News::Item* news = News::AddItemToQueue(News::ItemType::blank, "Replay playback complete", 0);
                news->setFlags(News::ItemFlags::hasButton); // Has no subject.
            }

            // When normalizing the output we don't touch the mode.
            if (_mode != ReplayMode::NORMALISATION)
            {
                _mode = ReplayMode::NONE;
            }

            _currentReplay.reset();

            return true;
        }

        virtual bool NormaliseReplay(const std::string& file, const std::string& outFile) override
        {
            _mode = ReplayMode::NORMALISATION;

            try
            {
                StartPlayback(file);
            }
            catch (const std::invalid_argument&)
            {
                return false;
            }

            if (!StartRecording(outFile, k_MaxReplayTicks, RecordType::NORMAL))
            {
                StopPlayback();
                return false;
            }

            _nextReplayTick = getGameState().currentTicks + 1;

            return true;
        }

    private:
        int ChecksumTicksDelta() const
        {
            switch (_recordType)
            {
                default:
                case RecordType::NORMAL:
                    return kNormalRecordingChecksumTicks;
                case RecordType::SILENT:
                    return kSilentRecordingChecksumTicks;
            }
        }

        bool LoadReplayDataMap(ReplayRecordData& data)
        {
            try
            {
                data.parkData.SetPosition(0);

                auto context = GetContext();
                auto& objManager = context->GetObjectManager();
                auto importer = ParkImporter::CreateParkFile(context->GetObjectRepository());

                auto loadResult = importer->LoadFromStream(&data.parkData, false);
                objManager.LoadObjects(loadResult.RequiredObjects);

                // TODO: Have a separate GameState and exchange once loaded.
                auto& gameState = getGameState();
                importer->Import(gameState);

                EntityTweener::Get().Reset();

                // Load all map global variables.
                DataSerialiser parkParamsDs(false, data.parkParams);
                SerialiseParkParameters(parkParamsDs);

                GameLoadInit();
                FixInvalidVehicleSpriteSizes();
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR("Exception: %s", ex.what());
                return false;
            }
            return true;
        }

        /**
         * Returns true if decompression was not needed or succeeded
         * @param stream
         * @return
         */
        MemoryStream DecompressFile(FileStream& fileStream)
        {
            ReplayRecordFile recFile;
            fileStream.SetPosition(0);
            DataSerialiser fileSerializer(false, fileStream);
            fileSerializer << recFile.magic;
            if (recFile.magic != kReplayMagic && recFile.magic != kReplayMagicTmp)
            {
                throw std::runtime_error("Invalid replay magic");
            }
            fileSerializer << recFile.version;

            if (recFile.magic == kReplayMagicTmp)
            {
                // Temporary uncompressed format
                MemoryStream data;
                data.CopyFromStream(fileStream, fileStream.GetLength() - fileStream.GetPosition());
                return data;
            }

            if (recFile.version >= 2)
            {
                fileSerializer << recFile.uncompressedSize;
                fileSerializer << recFile.data;

                MemoryStream decompressed;
                bool decompressStatus = true;
                recFile.data.SetPosition(0);
                if (recFile.version <= 10)
                {
                    decompressStatus = Compression::zlibDecompress(
                        recFile.data, recFile.data.GetLength(), decompressed, recFile.uncompressedSize,
                        Compression::ZlibHeaderType::zlib);
                }
                else
                {
                    decompressStatus = Compression::zstdDecompress(
                        recFile.data, recFile.data.GetLength(), decompressed, recFile.uncompressedSize);
                }
                if (!decompressStatus)
                    throw IOException("Decompression Error");

                recFile.data = std::move(decompressed);
            }
            else
            {
                // Read whole file into memory
                fileStream.SetPosition(0);
                recFile.data.CopyFromStream(fileStream, fileStream.GetLength());
            }

            return recFile.data;
        }

        void ReadReplayData(const std::string& file, ReplayRecordData& data)
        {
            fs::path filePath = file;

            if (filePath.is_absolute())
            {
                if (!fs::exists(filePath))
                {
                    throw std::runtime_error(FormatStringID(STR_REPLAY_FILE_NOT_FOUND, filePath.u8string().c_str()));
                }
            }
            else if (filePath.is_relative())
            {
                if (filePath.extension() != ".parkrep")
                    filePath += ".parkrep";
                fs::path replayPath = GetContext()->GetPlatformEnvironment().GetDirectoryPath(
                                          DirBase::user, DirId::replayRecordings)
                    / filePath;
                filePath = replayPath;
            }

            if (!fs::is_regular_file(filePath))
            {
                throw std::runtime_error(FormatStringID(STR_REPLAY_FILE_NOT_FOUND, filePath.u8string().c_str()));
            }

            FileStream fileStream(filePath, FileMode::open);
            MemoryStream stream = DecompressFile(fileStream);

            stream.SetPosition(0);
            DataSerialiser serialiser(false, stream);
            if (!Serialise(serialiser, data))
            {
                throw std::runtime_error(LanguageGetString(STR_REPLAY_NOT_STARTED));
            }

            // Reset position of all streams.
            data.parkData.SetPosition(0);
            data.parkParams.SetPosition(0);
            data.cheatData.SetPosition(0);
            data.gameStateSnapshots.SetPosition(0);
        }

        bool SerialiseCheats(DataSerialiser& serialiser)
        {
            CheatsSerialise(serialiser);

            return true;
        }

        bool SerialiseParkParameters(DataSerialiser& serialiser)
        {
            auto& park = getGameState().park;

            serialiser << park.guestGenerationProbability;
            serialiser << park.suggestedGuestMaximum;
            serialiser << Config::Get().general.showRealNamesOfGuests;

            // To make this a little bit less volatile against updates
            // we reserve some space for future additions.
            uint64_t tempStorage = 0;

            // If another park parameter has to be added simply swap tempStorage.
            // and ensure the length read/write will stay uint64_t
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;
            serialiser << tempStorage;

            return true;
        }

        bool SerialiseCommand(DataSerialiser& serialiser, ReplayCommand& command)
        {
            serialiser << command.tick;
            serialiser << command.commandIndex;

            uint32_t actionType = 0;
            if (serialiser.IsSaving())
            {
                if (!command.action)
                {
                    return false;
                }
                actionType = EnumValue(command.action->GetType());
            }
            serialiser << actionType;

            if (serialiser.IsLoading())
            {
                command.action = Create(static_cast<GameCommand>(actionType));
            }

            Guard::Assert(command.action != nullptr);
            command.action->Serialise(serialiser);

            return true;
        }

        bool Compatible(ReplayRecordData& data)
        {
            return data.version >= kReplayMinCompatVersion;
        }

        bool SerialisePreamble(DataSerialiser& serialiser, ReplayRecordData& data)
        {
            serialiser << data.magic;
            if (data.magic != kReplayMagic && data.magic != kReplayMagicTmp)
            {
                LOG_ERROR("Magic does not match %08X", data.magic);
                return false;
            }
            serialiser << data.version;
            if (data.version != kReplayVersion && !Compatible(data))
            {
                LOG_ERROR("Invalid version detected %04X, expected: %04X", data.version, kReplayVersion);
                return false;
            }

            serialiser << data.networkId;
#ifndef DISABLE_NETWORK
            // NOTE: This does not mean the replay will not function, only a warning.
            if (data.networkId != Network::GetVersion())
            {
                LOG_WARNING(
                    "Replay network version mismatch: '%s', expected: '%s'", data.networkId.c_str(),
                    Network::GetVersion().c_str());
            }
#endif

            serialiser << data.name;
            serialiser << data.timeRecorded;
            serialiser << data.parkData;
            serialiser << data.parkParams;
            serialiser << data.cheatData;
            serialiser << data.tickStart;
            serialiser << data.tickEnd;

            return true;
        }

        bool Serialise(DataSerialiser& serialiser, ReplayRecordData& data)
        {
            if (!SerialisePreamble(serialiser, data))
                return false;

            if (data.version >= 12)
            {
                if (serialiser.IsSaving())
                {
                    // For version 12+, we save snapshots first (usually just the one at the start)
                    if (data.gameStateSnapshots.GetLength() > 0)
                    {
                        serialiser << ReplayEntryType::Snapshot;
                        serialiser << data.gameStateSnapshots;
                    }

                    for (auto& command : data.commands)
                    {
                        serialiser << ReplayEntryType::Action;
                        SerialiseCommand(serialiser, const_cast<ReplayCommand&>(command));
                    }
                    for (auto& checksum : data.checksums)
                    {
                        serialiser << ReplayEntryType::Checksum;
                        serialiser << checksum.first;
                        serialiser << checksum.second.raw;
                    }
                    serialiser << ReplayEntryType::End;
                }
                else
                {
                    ReplayEntryType type{};
                    try
                    {
                        while (true)
                        {
                            serialiser << type;
                            if (type == ReplayEntryType::End)
                            {
                                break;
                            }
                            if (type == ReplayEntryType::Action)
                            {
                                ReplayCommand command = {};
                                SerialiseCommand(serialiser, command);
                                data.commands.emplace(std::move(command));
                            }
                            else if (type == ReplayEntryType::Checksum)
                            {
                                uint32_t tick = 0;
                                EntitiesChecksum checksum;
                                serialiser << tick;
                                serialiser << checksum.raw;
                                data.checksums.emplace_back(tick, std::move(checksum));
                            }
                            else if (type == ReplayEntryType::Snapshot)
                            {
                                serialiser << data.gameStateSnapshots;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    catch (const std::exception&)
                    {
                        LOG_WARNING("Replay file is truncated or corrupted.");
                    }
                }
            }
            else
            {
                uint32_t countCommands = static_cast<uint32_t>(data.commands.size());
                serialiser << countCommands;

                if (serialiser.IsSaving())
                {
                    for (auto& command : data.commands)
                    {
                        SerialiseCommand(serialiser, const_cast<ReplayCommand&>(command));
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < countCommands; i++)
                    {
                        ReplayCommand command = {};
                        SerialiseCommand(serialiser, command);

                        data.commands.emplace(std::move(command));
                    }
                }

                uint32_t countChecksums = static_cast<uint32_t>(data.checksums.size());
                serialiser << countChecksums;

                if (serialiser.IsLoading())
                {
                    data.checksums.resize(countChecksums);
                }

                for (uint32_t i = 0; i < countChecksums; i++)
                {
                    serialiser << data.checksums[i].first;
                    serialiser << data.checksums[i].second.raw;
                }
            }

            if (data.version < 12)
            {
                try
                {
                    serialiser << data.gameStateSnapshots;
                }
                catch (const std::exception&)
                {
                    LOG_WARNING("Replay snapshots are missing (likely truncated).");
                }
            }
            return true;
        }

#ifndef DISABLE_NETWORK
        void CheckState()
        {
            uint32_t checksumIndex = _currentReplay->checksumIndex;

            if (checksumIndex >= _currentReplay->checksums.size())
                return;

            const auto currentTicks = getGameState().currentTicks;

            const auto& savedChecksum = _currentReplay->checksums[checksumIndex];
            if (_currentReplay->checksums[checksumIndex].first == currentTicks)
            {
                _currentReplay->checksumIndex++;

                EntitiesChecksum checksum = getGameState().entities.GetAllEntitiesChecksum();
                if (savedChecksum.second.raw != checksum.raw)
                {
                    uint32_t replayTick = currentTicks - _currentReplay->tickStart;

                    // Detected different game state.
                    LOG_WARNING(
                        "Different sprite checksum at tick %u (Replay Tick: %u) ; Saved: %s, Current: %s", currentTicks,
                        replayTick, savedChecksum.second.ToString().c_str(), checksum.ToString().c_str());

                    _faultyChecksumIndex = checksumIndex;
                }
                else
                {
                    // Good state.
                    LOG_VERBOSE(
                        "Good state at tick %u ; Saved: %s, Current: %s", currentTicks, savedChecksum.second.ToString().c_str(),
                        checksum.ToString().c_str());
                }
            }
        }
#endif // DISABLE_NETWORK

        void ReplayCommands()
        {
            auto& replayQueue = _currentReplay->commands;

            auto& gameState = getGameState();
            const auto currentTicks = gameState.currentTicks;

            while (replayQueue.begin() != replayQueue.end())
            {
                const ReplayCommand& command = (*replayQueue.begin());

                if (_mode == ReplayMode::PLAYING)
                {
                    // If this is a normal playback wait for the correct tick.
                    if (command.tick != currentTicks)
                        break;
                }
                else if (_mode == ReplayMode::NORMALISATION)
                {
                    // Allow one entry per tick.
                    if (currentTicks != _nextReplayTick)
                        break;

                    _nextReplayTick = currentTicks + 1;
                }

                bool isPositionValid = false;

                GameAction* action = command.action.get();
                action->SetFlags(action->GetFlags().with(CommandFlag::replay));

                Result result = Execute(action, gameState);
                if (result.error == Status::ok)
                {
                    isPositionValid = true;
                }

                // Focus camera on event.
                if (!gSilentReplays && isPositionValid && !result.position.IsNull())
                {
                    auto* mainWindow = WindowGetMain();
                    if (mainWindow != nullptr)
                        WindowScrollToLocation(*mainWindow, result.position);
                }

                replayQueue.erase(replayQueue.begin());
            }
        }

    private:
        ReplayMode _mode = ReplayMode::NONE;
        std::unique_ptr<ReplayRecordData> _currentRecording;
        std::unique_ptr<ReplayRecordData> _currentReplay;
        int32_t _faultyChecksumIndex = -1;
        uint32_t _commandId = 0;
        uint32_t _nextChecksumTick = 0;
        uint32_t _nextReplayTick = 0;
        RecordType _recordType = RecordType::NORMAL;

        std::unique_ptr<IStream> _incrementalFileStream;
        std::unique_ptr<DataSerialiser> _incrementalSerialiser;
    };

    std::unique_ptr<IReplayManager> CreateReplayManager()
    {
        return std::make_unique<ReplayManager>();
    }

} // namespace OpenRCT2
