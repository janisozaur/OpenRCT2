#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#include <iostream>
int main() {
    std::cout << "Define: " << STR(OPENRCT2_COMPILER_VERSION) << std::endl;
    return 0;
}
