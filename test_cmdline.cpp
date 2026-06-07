#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#include <iostream>
int main() {
    std::cout << STR(VAL) << std::endl;
    return 0;
}
