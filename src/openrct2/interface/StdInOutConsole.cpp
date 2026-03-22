/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "StdInOutConsole.h"

#include "../Context.h"
#include "../OpenRCT2.h"
#include "../config/ConfigTypes.h"
#include "../localisation/FormatCodes.h"
#include "../platform/Platform.h"
#include "../scripting/ScriptEngine.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <linenoise.hpp>

using namespace OpenRCT2;

// Ignore isatty warning on WIN32
#ifdef _MSC_VER
    #pragma warning(disable : 4996)
#endif

#ifndef _WIN32
static volatile sig_atomic_t _terminalNeedsRestoration = 0;
static void HandleSignal(int signal)
{
    if (_terminalNeedsRestoration)
    {
        _terminalNeedsRestoration = 0;
        // Note: linenoiseAtExit is not strictly async-signal-safe, but common for terminal apps
        linenoise::linenoiseAtExit();
    }
    std::raise(signal);
}
#endif

void StdInOutConsole::Start()
{
    // Only start if stdin/stdout is a TTY
    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        return;
    }

    // Allow user to disable the console REPL. Setting this environment variable to any value will prevent REPL from starting.
    if (getenv("OPENRCT2_NO_REPL"))
    {
        return;
    }

#ifndef _WIN32
    struct sigaction sa{};
    sa.sa_handler = HandleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
#endif

    std::thread replThread([this]() -> void {
        linenoise::SetMultiLine(true);
        linenoise::SetHistoryMaxLen(32);

        _commands = GetCommandNames();
        _variables = GetVariableNames();

        linenoise::SetCompletionCallback([this](const char* buf, std::vector<std::string>& completions) {
            std::string input(buf);
            for (const auto& cmd : _commands)
            {
                if (cmd.find(input) == 0)
                {
                    completions.push_back(cmd);
                }
            }

            if (input.find("get ") == 0 || input.find("set ") == 0)
            {
                std::string prefix = input.substr(0, 4);
                std::string varPart = input.substr(4);
                for (const auto& var : _variables)
                {
                    if (var.find(varPart) == 0)
                    {
                        completions.push_back(prefix + var);
                    }
                }
            }
        });

        std::string prompt = "\033[32mopenrct2 $\x1b[0m ";
        bool lastPromptQuit = false;
        while (true)
        {
            std::string line;
            std::string left = prompt;
            _isPromptShowing = true;
#ifndef _WIN32
            _terminalNeedsRestoration = 1;
#endif
            auto quit = linenoise::Readline(left.c_str(), line);
#ifndef _WIN32
            _terminalNeedsRestoration = 0;
#endif
            _isPromptShowing = false;
            if (quit)
            {
                if (lastPromptQuit)
                {
                    GetContext()->Finish();
                    break;
                }

                lastPromptQuit = true;
                std::puts("(To exit, press ^C again)");
            }
            else
            {
                lastPromptQuit = false;
                linenoise::AddHistory(line.c_str());
                Eval(line).wait();
            }
        }
    });
    replThread.detach();
}

std::future<void> StdInOutConsole::Eval(const std::string& s)
{
#ifdef ENABLE_SCRIPTING
    auto& scriptEngine = GetContext()->GetScriptEngine();
    return scriptEngine.Eval(s);
#else
    // Push on-demand evaluations onto a queue so that it can be processed deterministically
    // on the main thead at the right time.
    std::promise<void> barrier;
    auto future = barrier.get_future();
    _evalQueue.emplace(std::move(barrier), s);
    return future;
#endif
}

void StdInOutConsole::ProcessEvalQueue()
{
#ifndef ENABLE_SCRIPTING
    while (_evalQueue.size() > 0)
    {
        auto item = std::move(_evalQueue.front());
        _evalQueue.pop();
        auto promise = std::move(std::get<0>(item));
        auto command = std::move(std::get<1>(item));

        Execute(command);

        // Signal the promise so caller can continue
        promise.set_value();
    }
#endif
}

void StdInOutConsole::Clear()
{
    linenoise::linenoiseClearScreen();
}

void StdInOutConsole::Close()
{
    GetContext()->Finish();
}

void StdInOutConsole::WriteLine(const std::string& s, FormatToken colourFormat)
{
    std::string formatBegin;
    switch (colourFormat)
    {
        case FormatToken::colourRed:
            formatBegin = "\033[31m";
            break;
        case FormatToken::colourYellow:
            formatBegin = "\033[33m";
            break;
        default:
            break;
    }

    if (!Platform::IsColourTerminalSupported())
    {
        std::printf("%s\n", s.c_str());
        std::fflush(stdout);
    }
    else
    {
        if (_isPromptShowing)
        {
            auto* mainString = s.c_str();

            // If string contains \n, we need to replace with \r\n
            std::string newString;
            if (s.find('\n') != std::string::npos)
            {
                for (auto ch : s)
                {
                    if (ch == '\n')
                        newString += "\r\n";
                    else
                        newString += ch;
                }
                mainString = newString.c_str();
            }

            std::printf("\r%s%s\x1b[0m\x1b[0K\r\n", formatBegin.c_str(), mainString);
            std::fflush(stdout);
            linenoise::linenoiseEditRefreshLine();
        }
        else
        {
            std::printf("%s%s\x1b[0m\n", formatBegin.c_str(), s.c_str());
            std::fflush(stdout);
        }
    }
}
