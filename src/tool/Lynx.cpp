#include <iostream>

#include <LynxConf.hpp>

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file> [path]" << std::endl;
    }

    std::string file = argv[1];
    auto parsed = ConfigParser().parse(file);
    if (!parsed) {
        std::cerr << "Failed to parse file: " << file << std::endl;
        return 1;
    }
    if (argc > 2) {
        std::string path = argv[2];
        if (parsed->getType() != EntryType::Compound) {
            std::cerr << "Invalid entry type. Expected Compound but got " << parsed->getType() << std::endl;
            return 1;
        }
        auto entry = static_cast<CompoundEntry*>(parsed)->getByPath(path);
        if (!entry) {
            std::cerr << "Failed to find entry: " << path << std::endl;
            return 1;
        }
        entry->print(std::cout);
    }
    return 0;
}
