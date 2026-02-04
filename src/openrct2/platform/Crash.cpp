/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Crash.h"

#ifdef USE_CRASHPAD
    #include <atomic>
    #include <base/files/file_path.h>
    #include <client/crash_report_database.h>
    #include <client/crashpad_client.h>
    #include <client/settings.h>
    #include <map>
    #include <memory>
    #include <mutex>
    #include <string>
    #include <vector>

    #ifdef _WIN32
        #include <shlobj.h>
        #include <windows.h>
    #endif

    #include "../Context.h"
    #include "../Game.h"
    #include "../GameState.h"
    #include "../OpenRCT2.h"
    #include "../PlatformEnvironment.h"
    #include "../Version.h"
    #include "../config/Config.h"
    #include "../core/Console.hpp"
    #include "../core/File.h"
    #include "../core/Guard.hpp"
    #include "../core/Path.hpp"
    #include "../core/String.hpp"
    #include "../drawing/IDrawingEngine.h"
    #include "../interface/Screenshot.h"
    #include "../object/ObjectManager.h"
    #include "../park/ParkFile.h"
    #include "Platform.h"

    #define BACKTRACE_TOKEN "0ff94b99e7911919eaadfa9b44ab3d1cefe4862df849aa873c5185446525c2cc"

using namespace OpenRCT2;

static std::map<std::string, std::string> _additionalFiles;
static std::mutex _additionalFilesMutex;
static crashpad::CrashpadClient* _crashpadClient;

    #ifdef _WIN32
static LPTOP_LEVEL_EXCEPTION_FILTER _previousExceptionFilter = nullptr;

static void EnableUploads(bool enable)
{
    auto& env = GetContext()->GetPlatformEnvironment();
    auto crashDumpsPath = env.GetDirectoryPath(DirBase::user, DirId::crashDumps);
    auto database = crashpad::CrashReportDatabase::Initialize(base::FilePath(String::toWideChar(crashDumpsPath.c_str())));
    if (database)
    {
        database->GetSettings()->SetUploadsEnabled(enable);
    }
}

static void PerformPreCrashTasks(const std::string& dumpPath)
{
    auto savePath = Path::Combine(dumpPath, "crash.park");
    auto configPath = Path::Combine(dumpPath, "crash.ini");
    auto screenshotPath = Path::Combine(dumpPath, "crash.png");
    auto replayPath = Path::Combine(dumpPath, "crash.parkrep");
    auto extraPath = Path::Combine(dumpPath, "crash_extra.dat");

    // Save game
    try
    {
        PrepareMapForSave();
        auto exporter = std::make_unique<ParkFileExporter>();
        auto ctx = OpenRCT2::GetContext();
        auto& objManager = ctx->GetObjectManager();
        exporter->ExportObjectsList = objManager.GetPackableObjects();
        auto& gameState = getGameState();
        exporter->Export(gameState, savePath.c_str(), kParkFileSaveCompressionLevel);
    }
    catch (const std::exception& e)
    {
        printf("Failed to export save. Error: %s\n", e.what());
    }

    // Save config
    Config::SaveToPath(configPath);

    // Screenshot
    if (OpenRCT2::GetContext()->GetDrawingEngineType() != DrawingEngine::OpenGL)
    {
        std::string tempScreenshot = ScreenshotDump();
        if (!tempScreenshot.empty())
        {
            File::Copy(tempScreenshot, screenshotPath, true);
        }
    }

    // Replay
    bool with_record = StopSilentRecord();
    if (with_record)
    {
        File::Copy(gSilentRecordingName, replayPath, true);
    }

    // Extra file
    {
        std::lock_guard<std::mutex> lock(_additionalFilesMutex);
        if (!_additionalFiles.empty())
        {
            // Just take the first one for now as a slot
            File::Copy(_additionalFiles.begin()->second, extraPath, true);
        }
    }
}

static LONG WINAPI OpenRCT2CrashFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    static std::atomic<bool> handlingCrash(false);
    if (handlingCrash.exchange(true))
        return EXCEPTION_CONTINUE_SEARCH;

    auto& env = GetContext()->GetPlatformEnvironment();
    auto dumpPath = env.GetDirectoryPath(DirBase::user, DirId::crashDumps);

    PerformPreCrashTasks(dumpPath);

    if (gOpenRCT2SilentBreakpad)
    {
        EnableUploads(true);
        return _previousExceptionFilter ? _previousExceptionFilter(exceptionInfo) : EXCEPTION_CONTINUE_SEARCH;
    }

    constexpr const wchar_t* MessageFormat = L"A crash has occurred and a dump was created in\n%s.\n\nPlease file an issue "
                                             L"with OpenRCT2 on GitHub, and provide "
                                             L"the dump and saved game there.\n\nVersion: %S\n\n"
                                             L"We would like to upload the crash dump for automated analysis, do you agree?\n"
                                             L"The automated analysis is done by courtesy of https://backtrace.io/";
    wchar_t message[MAX_PATH * 3];
    swprintf_s(message, MessageFormat, String::toWideChar(dumpPath).c_str(), kOpenRCT2Version);

    int answer = MessageBoxW(nullptr, message, L"" OPENRCT2_NAME, MB_YESNO | MB_ICONERROR);
    if (answer == IDYES)
    {
        EnableUploads(true);
    }

    // Highlighting files
    HRESULT coInitializeResult = CoInitialize(nullptr);
    if (SUCCEEDED(coInitializeResult))
    {
        auto dumpPathW = String::toWideChar(dumpPath);
        LPITEMIDLIST pidl = ILCreateFromPathW(dumpPathW.c_str());
        if (pidl != nullptr)
        {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
        }
        CoUninitialize();
    }

    return _previousExceptionFilter ? _previousExceptionFilter(exceptionInfo) : EXCEPTION_CONTINUE_SEARCH;
}
    #endif

#endif // USE_CRASHPAD

CExceptionHandler CrashInit()
{
#ifdef USE_CRASHPAD
    auto& env = GetContext()->GetPlatformEnvironment();
    auto crashDumpsPath = env.GetDirectoryPath(DirBase::user, DirId::crashDumps);
    auto handlerPath = Path::Combine(Platform::GetCurrentExecutableDirectory(), "crashpad_handler.exe");

    std::string url = "https://openrct2.sp.backtrace.io:6098/post?format=minidump&token=" BACKTRACE_TOKEN;

    std::map<std::string, std::string> annotations;
    annotations["product_name"] = "openrct2";
    annotations["version"] = gVersionInfoFull;
    #ifdef OPENRCT2_COMMIT_SHA1_SHORT
    annotations["commit"] = OPENRCT2_COMMIT_SHA1_SHORT;
    #else
    annotations["commit"] = gVersionInfoFull;
    #endif

    std::vector<std::string> arguments;
    arguments.push_back("--no-rate-limit");

    // Pre-define attachment paths
    auto savePath = Path::Combine(crashDumpsPath, "crash.park");
    auto configPath = Path::Combine(crashDumpsPath, "crash.ini");
    auto screenshotPath = Path::Combine(crashDumpsPath, "crash.png");
    auto replayPath = Path::Combine(crashDumpsPath, "crash.parkrep");
    auto extraPath = Path::Combine(crashDumpsPath, "crash_extra.dat");

    std::vector<base::FilePath> attachments;
    attachments.push_back(base::FilePath(String::toWideChar(savePath)));
    attachments.push_back(base::FilePath(String::toWideChar(configPath)));
    attachments.push_back(base::FilePath(String::toWideChar(screenshotPath)));
    attachments.push_back(base::FilePath(String::toWideChar(replayPath)));
    attachments.push_back(base::FilePath(String::toWideChar(extraPath)));

    _crashpadClient = new crashpad::CrashpadClient();
    bool success = _crashpadClient->StartHandler(
        base::FilePath(String::toWideChar(handlerPath)), base::FilePath(String::toWideChar(crashDumpsPath)),
        base::FilePath(String::toWideChar(crashDumpsPath)), url, annotations, arguments, true, false, attachments);

    if (success)
    {
        EnableUploads(false);
    #ifdef _WIN32
        _previousExceptionFilter = SetUnhandledExceptionFilter(OpenRCT2CrashFilter);
    #endif
        return reinterpret_cast<CExceptionHandler>(_crashpadClient);
    }
#endif
    return nullptr;
}

void CrashRegisterAdditionalFile(const std::string& key, const std::string& path)
{
#ifdef USE_CRASHPAD
    std::lock_guard<std::mutex> lock(_additionalFilesMutex);
    _additionalFiles[key] = path;
#endif
}

void CrashUnregisterAdditionalFile(const std::string& key)
{
#ifdef USE_CRASHPAD
    std::lock_guard<std::mutex> lock(_additionalFilesMutex);
    _additionalFiles.erase(key);
#endif
}
