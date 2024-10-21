#include <LynxConf.hpp>
#include <Lynx.hpp>

#if !defined(LYNX_VERSION)
#define LYNX_VERSION "0.0.1"
#endif

CompoundEntry* config = nullptr;

int main(int argc, char const *argv[]) {
    config = ConfigParser().parse("main.lynx");
    config->print(std::cout);
    return 0;
}
