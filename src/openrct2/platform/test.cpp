#ifdef _WIN32
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif // _WIN32

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
