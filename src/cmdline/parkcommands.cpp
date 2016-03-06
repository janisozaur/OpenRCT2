#include "../core/Memory.hpp"
#include "../core/String.hpp"
#include "CommandLine.hpp"

#define SZ_DEFAULT   "default"
#define SZ_CLOSEST   "closest"
#define SZ_DITHERING "dithering"

extern "C"
{
    int gGzipLevel = 0;

    int sv6_unrle(const char **argv, int argc);
}

static uint32 _gzip            = 0;

static const CommandLineOptionDefinition Sv6Options[]
{
    { CMDLINE_TYPE_INTEGER,  &_gzip,            NAC, "gzip",              "Use gzip compression, with specified level"                            },
    OptionTableEnd
};

static exitcode_t HandleSv6(CommandLineArgEnumerator *argEnumerator);

const CommandLineCommand CommandLine::Sv6Commands[]
{
    // Main commands
    DefineCommand("unrle",  "<input.sv6> <output.sv6>",        Sv6Options, HandleSv6),
    CommandTableEnd
};

static exitcode_t HandleSv6(CommandLineArgEnumerator *argEnumerator)
{
    gGzipLevel = _gzip;

    const char * * argv = (const char * *)argEnumerator->GetArguments() + argEnumerator->GetIndex() - 1;
    int argc = argEnumerator->GetCount() - argEnumerator->GetIndex() + 1;
    int result = sv6_unrle(argv, argc);
    if (result < 0) {
        return EXITCODE_FAIL;
    }
    return EXITCODE_OK;
}
