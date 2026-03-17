/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// Windows.h needs to be included first
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Enable visual styles
#pragma comment(                                                                                                               \
    linker,                                                                                                                    \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Then the rest
#include <algorithm>
#include <iterator>
#include <openrct2-ui/Ui.h>
#include <openrct2/core/String.hpp>
#include <shellapi.h>
#include <string>
#include <vector>

static std::vector<std::string> GetCommandLineArgs(int argc, wchar_t** argvW);

/**
 * Windows entry point to OpenRCT2 with a console window using a traditional C main function.
 */
int WINAPI wWinMain(
    [[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] LPWSTR lpCmdLine,
    [[maybe_unused]] int nShowCmd)
{
    int argc;
    wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW == nullptr)
    {
        return 1;
    }

    auto argvStrings = GetCommandLineArgs(argc, argvW);

    LocalFree(argvW);

    std::vector<const char*> argv;
    std::transform(
        argvStrings.begin(), argvStrings.end(), std::back_inserter(argv), [](const auto& string) { return string.c_str(); });

    // Ensure that argv[argc] == nullptr, as mandated by the standard
    argv.push_back(nullptr);
    return NormalisedMain(static_cast<int>(argv.size()) - 1, argv.data());
}

static std::vector<std::string> GetCommandLineArgs(int argc, wchar_t** argvW)
{
    // Allocate UTF-8 strings
    std::vector<std::string> argv;
    for (int i = 0; i < argc; i++)
    {
        argv.push_back(OpenRCT2::String::toUtf8(argvW[i]));
    }
    return argv;
}
