#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#include <iostream>
int main() {
    std::cout << STR(GNU) << " " << STR(13.2.0) << std::endl;
    return 0;
}
