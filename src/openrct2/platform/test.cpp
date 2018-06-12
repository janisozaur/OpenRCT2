#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <time.h>

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#undef M_PI

#ifdef _MSC_VER
#include <ctime>
#endif

#include <cassert>
#include <cstdint>

using sint8 = int8_t;
using sint16 = int16_t;
using sint32 = int32_t;
using sint64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

#ifndef _DIAGNOSTIC_H_
#define _DIAGNOSTIC_H_

enum DIAGNOSTIC_LEVEL {
    DIAGNOSTIC_LEVEL_FATAL,
    DIAGNOSTIC_LEVEL_ERROR,
    DIAGNOSTIC_LEVEL_WARNING,
    DIAGNOSTIC_LEVEL_VERBOSE,
    DIAGNOSTIC_LEVEL_INFORMATION,
    DIAGNOSTIC_LEVEL_COUNT
};

/**
 * Compile-time debug levels.
 *
 * When compiling, just add -DDEBUG={0,1,2,3} (where 0 means disabled)
 * Regardless of DEBUG value, a set of defines will be created:
 * - DEBUG_LEVEL_1
 * - DEBUG_LEVEL_2
 * - DEBUG_LEVEL_3
 * which you would use like so:
 *
 * #if DEBUG_LEVEL_1
 *     (... some debug code ...)
 *     #if DEBUG_LEVEL_2
 *         (... more debug code ...)
 *    #endif // DEBUG_LEVEL_2
 * #endif // DEBUG_LEVEL_1
 *
 * The defines will be either 0 or 1 so compiler will complain about undefined
 * macro if you forget to include the file, which would not happen if we were
 * only checking whether the define is present or not.
 */

#if defined(DEBUG)
    #if DEBUG > 0
        #define DEBUG_LEVEL_1 1
        #if DEBUG > 1
            #define DEBUG_LEVEL_2 1
            #if DEBUG > 2
                #define DEBUG_LEVEL_3 1
            #else
                #define DEBUG_LEVEL_3 0
            #endif // DEBUG > 2
        #else
            #define DEBUG_LEVEL_3 0
            #define DEBUG_LEVEL_2 0
        #endif // DEBUG > 1
    #else
        #define DEBUG_LEVEL_1 0
        #define DEBUG_LEVEL_2 0
        #define DEBUG_LEVEL_3 0
    #endif // DEBUG > 0
#else
    #define DEBUG_LEVEL_3 0
    #define DEBUG_LEVEL_2 0
    #define DEBUG_LEVEL_1 0
#endif // defined(DEBUG)

extern bool _log_levels[DIAGNOSTIC_LEVEL_COUNT];

void diagnostic_log(DIAGNOSTIC_LEVEL diagnosticLevel, const char *format, ...);
void diagnostic_log_with_location(DIAGNOSTIC_LEVEL diagnosticLevel, const char *file, const char *function, sint32 line, const char *format, ...);

#ifdef _MSC_VER
#define diagnostic_log_macro(level, format, ...)    diagnostic_log_with_location(level, __FILE__, __FUNCTION__, __LINE__, format, ## __VA_ARGS__)
#else
    #define diagnostic_log_macro(level, format, ...)    diagnostic_log_with_location(level, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)
#endif // _MSC_VER

#define log_fatal(format, ...)      diagnostic_log_macro(DIAGNOSTIC_LEVEL_FATAL, format, ## __VA_ARGS__)
#define log_error(format, ...)      diagnostic_log_macro(DIAGNOSTIC_LEVEL_ERROR, format, ## __VA_ARGS__)
#define log_warning(format, ...)    diagnostic_log_macro(DIAGNOSTIC_LEVEL_WARNING, format, ## __VA_ARGS__)
#define log_verbose(format, ...)    diagnostic_log(DIAGNOSTIC_LEVEL_VERBOSE, format, ## __VA_ARGS__)
#define log_info(format, ...)       diagnostic_log_macro(DIAGNOSTIC_LEVEL_INFORMATION, format, ## __VA_ARGS__)

#endif

using utf8             = char;
using utf8string       = utf8 *;
using const_utf8string = const utf8 *;
#ifdef _WIN32
using utf16 = wchar_t;
using utf16string = utf16*;
#endif

// Define MAX_PATH for various headers that don't want to include system headers
// just for MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

using codepoint_t = uint32;
using colour_t = uint8;

#define rol8(x, shift)      (((uint8)(x) << (shift)) | ((uint8)(x) >> (8 - (shift))))
#define ror8(x, shift)      (((uint8)(x) >> (shift)) | ((uint8)(x) << (8 - (shift))))
#define rol16(x, shift)     (((uint16)(x) << (shift)) | ((uint16)(x) >> (16 - (shift))))
#define ror16(x, shift)     (((uint16)(x) >> (shift)) | ((uint16)(x) << (16 - (shift))))
#define rol32(x, shift)     (((uint32)(x) << (shift)) | ((uint32)(x) >> (32 - (shift))))
#define ror32(x, shift)     (((uint32)(x) >> (shift)) | ((uint32)(x) << (32 - (shift))))
#define rol64(x, shift)     (((uint64)(x) << (shift)) | ((uint32)(x) >> (64 - (shift))))
#define ror64(x, shift)     (((uint64)(x) >> (shift)) | ((uint32)(x) << (64 - (shift))))

// Rounds an integer down to the given power of 2. y must be a power of 2.
#define floor2(x, y)        ((x) & (~((y) - 1)))

// Rounds an integer up to the given power of 2. y must be a power of 2.
#define ceil2(x, y)         (((x) + (y) - 1) & (~((y) - 1)))

// Gets the name of a symbol as a C string
#define nameof(symbol) #symbol

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#define STUB() log_warning("Function %s at %s:%d is a stub.\n", __PRETTY_FUNCTION__, __FILE__, __LINE__)
#define _strcmpi _stricmp
#define _stricmp(x, y) strcasecmp((x), (y))
#define _strnicmp(x, y, n) strncasecmp((x), (y), (n))
#define _strdup(x) strdup((x))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RCT2_ENDIANESS __ORDER_LITTLE_ENDIAN__
#define LOBYTE(w) ((uint8)(w))
#define HIBYTE(w) ((uint8)(((uint16)(w)>>8)&0xFF))
#endif // __BYTE_ORDER__

#ifndef RCT2_ENDIANESS
#error Unknown endianess!
#endif // RCT2_ENDIANESS

#endif // defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#if !((defined (_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700) || (defined(__APPLE__) && defined(__MACH__)) || defined(__ANDROID_API__) || defined(__FreeBSD__))
char *strndup(const char *src, size_t size);
#endif // !(POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

// BSD and macOS have MAP_ANON instead of MAP_ANONYMOUS
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define OPENRCT2_MASTER_SERVER_URL  "https://servers.openrct2.io"

// Time (represented as number of 100-nanosecond intervals since 0001-01-01T00:00:00Z)
using datetime64 = uint64;

#define DATETIME64_MIN ((datetime64)0)

// Represent fixed point numbers. dp = decimal point
using fixed8_1dp = uint8;
using fixed8_2dp = uint8;
using fixed16_1dp = sint16;
using fixed16_2dp = sint16;
using fixed32_1dp = sint32;
using fixed32_2dp = sint32;
using fixed64_1dp = sint64;

// Money is stored as a multiple of 0.10.
using money8 = fixed8_1dp;
using money16 = fixed16_1dp;
using money32 = fixed32_1dp;
using money64 = fixed64_1dp;

// Construct a fixed point number. For example, to create the value 3.65 you
// would write FIXED_2DP(3,65)
#define FIXED_XDP(x, whole, fraction)   ((whole) * (10 * x) + (fraction))
#define FIXED_1DP(whole, fraction)      FIXED_XDP(1, whole, fraction)
#define FIXED_2DP(whole, fraction)      FIXED_XDP(10, whole, fraction)

// Construct a money value in the format MONEY(10,70) to represent 10.70. Fractional part must be two digits.
#define MONEY(whole, fraction)          ((whole) * 10 + ((fraction) / 10))

#define MONEY_FREE                      MONEY(0,00)
#define MONEY16_UNDEFINED               (money16)(uint16)0xFFFF
#define MONEY32_UNDEFINED               ((money32)0x80000000)

using EMPTY_ARGS_VOID_POINTER = void();
using rct_string_id           = uint16;

#define SafeFree(x) do { free(x); (x) = nullptr; } while (false)

#define SafeDelete(x) do { delete (x); (x) = nullptr; } while (false)
#define SafeDeleteArray(x) do { delete[] (x); (x) = nullptr; } while (false)

#ifndef interface
    #define interface struct
#endif
#define abstract = 0

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #define OPENRCT2_X86
#elif defined(_MSC_VER) && (_MSC_VER >= 1500) && (defined(_M_X64) || defined(_M_IX86)) // VS2008
    #define OPENRCT2_X86
#endif

#if defined(__i386__) || defined(_M_IX86)
#define PLATFORM_X86
#endif

#if defined(__LP64__) || defined(_WIN64)
    #define PLATFORM_64BIT
#else
    #define PLATFORM_32BIT
#endif

// C99's restrict keywords guarantees the pointer in question, for the whole of its lifetime,
// will be the only way to access a given memory region. In other words: there is no other pointer
// aliasing the same memory area. Using it lets compiler generate better code. If your compiler
// does not support it, feel free to drop it, at some performance hit.
#ifdef _MSC_VER
    #define RESTRICT __restrict
#else
    #define RESTRICT __restrict__
#endif

#define assert_struct_size(x, y) static_assert(sizeof(x) == (y), "Improper struct size")

#ifdef PLATFORM_X86
    #ifndef FASTCALL
        #ifdef __GNUC__
            #define FASTCALL __attribute__((fastcall))
        #elif defined(_MSC_VER)
            #define FASTCALL __fastcall
        #else
            #pragma message "Not using fastcall calling convention, please check your compiler support"
            #define FASTCALL
        #endif
    #endif // FASTCALL
#else // PLATFORM_X86
    #define FASTCALL
#endif // PLATFORM_X86

/**
 * x86 register structure, only used for easy interop to RCT2 code.
 */
#pragma pack(push, 1)
struct registers {
    union {
        sint32 eax;
        sint16 ax;
        struct {
            char al;
            char ah;
        };
    };
    union {
        sint32 ebx;
        sint16 bx;
        struct {
            char bl;
            char bh;
        };
    };
    union {
        sint32 ecx;
        sint16 cx;
        struct {
            char cl;
            char ch;
        };
    };
    union {
        sint32 edx;
        sint16 dx;
        struct {
            char dl;
            char dh;
        };
    };
    union {
        sint32 esi;
        sint16 si;
    };
    union {
        sint32 edi;
        sint16 di;
    };
    union {
        sint32 ebp;
        sint16 bp;
    };
};
assert_struct_size(registers, 7 * 4);
#pragma pack(pop)

#endif

struct TTFFontDescriptor;
struct rct2_install_info;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define INVALID_HANDLE -1

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#define PLATFORM_NEWLINE "\r\n"
#else
#define PATH_SEPARATOR "/"
#define PLATFORM_NEWLINE "\n"
#endif

struct resolution_t {
    sint32 width, height;
};

struct file_info {
    const char *path;
    uint64 size;
    uint64 last_modified;
};

struct rct2_date {
    uint8 day;
    uint8 month;
    sint16 year;
    uint8 day_of_week;
};

struct rct2_time {
    uint8 hour;
    uint8 minute;
    uint8 second;
};

enum FILEDIALOG_TYPE
{
    FD_OPEN,
    FD_SAVE
};

struct file_dialog_desc {
    uint8 type;
    const utf8 *title;
    const utf8 *initial_directory;
    const utf8 *default_filename;
    struct {
        const utf8 *name;           // E.g. "Image Files"
        const utf8 *pattern;        // E.g. "*.png;*.jpg;*.gif"
    } filters[8];
};

// Platform shared definitions
void platform_update_palette(const uint8 *colours, sint32 start_index, sint32 num_colours);
void platform_toggle_windowed_mode();
void platform_refresh_video(bool recreate_window);
void platform_get_date_utc(rct2_date *out_date);
void platform_get_time_utc(rct2_time *out_time);
void platform_get_date_local(rct2_date *out_date);
void platform_get_time_local(rct2_time *out_time);

// Platform specific definitions
bool platform_file_exists(const utf8 *path);
bool platform_directory_exists(const utf8 *path);
bool platform_original_game_data_exists(const utf8 *path);
time_t platform_file_get_modified_time(const utf8* path);
bool platform_ensure_directory_exists(const utf8 *path);
bool platform_directory_delete(const utf8 *path);
utf8 * platform_get_absolute_path(const utf8 * relative_path, const utf8 * base_path);
bool platform_lock_single_instance();
bool platform_place_string_on_clipboard(utf8* target);

// Returns the bitmask of the GetLogicalDrives function for windows, 0 for other systems
sint32 platform_get_drives();

bool platform_file_copy(const utf8 *srcPath, const utf8 *dstPath, bool overwrite);
bool platform_file_move(const utf8 *srcPath, const utf8 *dstPath);
bool platform_file_delete(const utf8 *path);
uint32 platform_get_ticks();
void platform_sleep(uint32 ms);
void platform_get_openrct_data_path(utf8 *outPath, size_t outSize);
void platform_get_user_directory(utf8 *outPath, const utf8 *subDirectory, size_t outSize);
utf8* platform_get_username();
bool platform_open_common_file_dialog(utf8 *outFilename, file_dialog_desc *desc, size_t outSize);
utf8 *platform_open_directory_browser(const utf8 *title);
uint8 platform_get_locale_currency();
uint8 platform_get_currency_value(const char *currencyCode);
uint16 platform_get_locale_language();
uint8 platform_get_locale_measurement_format();
uint8 platform_get_locale_temperature_format();
uint8 platform_get_locale_date_format();
bool platform_process_is_elevated();
bool platform_get_steam_path(utf8 * outPath, size_t outSize);

#ifndef NO_TTF
bool platform_get_font_path(TTFFontDescriptor *font, utf8 *buffer, size_t size);
#endif // NO_TTF

datetime64 platform_get_datetime_now_utc();

float platform_get_default_scale();

// Called very early in the program before parsing commandline arguments.
void core_init();

// Windows specific definitions
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #undef CreateDirectory
    #undef CreateWindow
    #undef GetMessage

    void platform_setup_file_associations();
    void platform_remove_file_associations();
    bool platform_setup_uri_protocol();
    // This function cannot be marked as 'static', even though it may seem to be,
    // as it requires external linkage, which 'static' prevents
    __declspec(dllexport) sint32 StartOpenRCT(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, sint32 nCmdShow);
#endif // _WIN32

#if defined(__APPLE__) && defined(__MACH__)
    void macos_disallow_automatic_window_tabbing();
    utf8* macos_str_decomp_to_precomp(utf8 *input);
#endif

#endif

// The name of the mutex used to prevent multiple instances of the game from running
#define SINGLE_INSTANCE_MUTEX_NAME "RollerCoaster Tycoon 2_GSKMUTEX"

#define OPENRCT2_DLL_MODULE_NAME "openrct2.dll"


#include <wchar.h>
#include <cstdio>
int main() {
    wchar_t dateFormat[20] = L"yyyy-MM-dd";
    wchar_t first[sizeof(dateFormat)];
    wchar_t second[sizeof(dateFormat)];
    printf("test 1\n");
    if (swscanf(dateFormat, L"%l[dyM]%*l[^dyM]%l[dyM]%*l[^dyM]%*l[dyM]", first, second) != 2)
    {
        printf("test 2\n");
    }
    printf("test 3\n");
    return 0;
}
