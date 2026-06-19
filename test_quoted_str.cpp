#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#include <iostream>
#define ID "GNU"
#define VER "13.2.0"
const char info[] = STR(ID) " " STR(VER);
int main() {
    std::cout << info << std::endl;
    return 0;
}
