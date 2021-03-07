/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Console.hpp"

#include "../Context.h"
#include "../PlatformEnvironment.h"
#include "../Version.h"
#include "../core/Path.hpp"
#include "../platform/Platform2.h"
#include "../platform/platform.h"

#include <cstdio>
#include <fstream>
#include <string>

static std::ofstream _log_stream;

namespace
{
    void consoleinit()
    {
        if (!_log_stream)
        {
            auto env = OpenRCT2::GetContext()->GetPlatformEnvironment();
            auto str = env->GetDirectoryPath(OpenRCT2::DIRBASE::USER);
            str = Path::Combine(str, "OpenRCT2.log");
            _log_stream.open(str, std::ios::out);
            auto date = Platform::GetDateLocal();
            auto time = Platform::GetTimeLocal();
            char formatted[64];
            snprintf(
                formatted, sizeof(formatted), "%4d-%02d-%02d %02d:%02d:%02d", date.year, date.month, date.day, time.hour,
                time.minute, time.second);
            _log_stream << "OpenRCT2 " << gVersionInfoFull << " started on " << formatted << std::endl;
        }
    }
} // namespace

namespace Console
{
    void Write(char c)
    {
        fputc(c, stdout);
    }

    void Write(const utf8* str)
    {
        fputs(str, stdout);
    }

    void WriteSpace(size_t count)
    {
        std::string sz(count, ' ');
        Write(sz.c_str());
    }

    void WriteFormat(const utf8* format, ...)
    {
        va_list args;

        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }

    void WriteLine()
    {
        puts("");
    }

    void WriteLine(const utf8* format, ...)
    {
        consoleinit();
        va_list args;
        va_start(args, format);

        char buffer[4096];
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        auto ctx = OpenRCT2::GetContext();
        if (ctx != nullptr)
            ctx->WriteLine(buffer);
        else
            std::printf("%s\n", buffer);

        _log_stream << buffer << std::endl;

        va_end(args);
    }

    namespace Error
    {
        void Write(char c)
        {
            fputc(c, stderr);
        }

        void Write(const utf8* str)
        {
            fputs(str, stderr);
        }

        void WriteFormat(const utf8* format, ...)
        {
            va_list args;

            va_start(args, format);
            vfprintf(stderr, format, args);
            va_end(args);
        }

        void WriteLine()
        {
            fputs(PLATFORM_NEWLINE, stderr);
        }

        void WriteLine(const utf8* format, ...)
        {
            va_list args;
            va_start(args, format);
            WriteLine_VA(format, args);
            va_end(args);
        }

        void WriteLine_VA(const utf8* format, va_list args)
        {
            consoleinit();
            char buffer[4096];
            std::vsnprintf(buffer, sizeof(buffer), format, args);
            auto ctx = OpenRCT2::GetContext();
            if (ctx != nullptr)
                ctx->WriteErrorLine(buffer);
            else
                std::printf("%s\n", buffer);
            _log_stream << buffer << std::endl;
        }
    } // namespace Error
} // namespace Console
