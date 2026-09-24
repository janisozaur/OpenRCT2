#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#if defined(OPENRCT2_COMPILER_ID) && defined(OPENRCT2_COMPILER_VERSION)
const char gCompilerInfo[] = STR(OPENRCT2_COMPILER_ID) " " STR(OPENRCT2_COMPILER_VERSION);
#else
const char gCompilerInfo[] = "Unknown";
#endif

#include <iostream>

int main() {
    std::cout << "Info: " << gCompilerInfo << std::endl;
    return 0;
}
