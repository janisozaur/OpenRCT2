#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#include <iostream>
#define ID Clang
#define VER 17.0.6 (tags/RELEASE_1706/final)
const char info[] = STR(ID) " " STR(VER);
int main() {
    std::cout << info << std::endl;
    return 0;
}
