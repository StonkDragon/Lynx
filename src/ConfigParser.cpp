#include <LynxConf.hpp>

std::vector<Token> tokenize(std::string file, std::string& data, int& i);

CompoundEntry* ConfigParser::parse(const std::string& configFile) {
    std::vector<CompoundEntry*> compoundStack;
    return this->parse(configFile, compoundStack);
}

CompoundEntry* ConfigParser::parse(const std::string& configFile, std::vector<CompoundEntry*>& compoundStack) {
    FILE* fp = fopen(configFile.c_str(), "r");
    if (!fp) {
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buf = new char[size + 1];
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    std::string config = "{";
    config.reserve(size + 3);
    for (size_t i = 0; buf[i]; i++) {
        if (buf[i] == '-' && buf[i + 1] == '-') {
            while (buf[i] != '\n' && buf[i] != '\0') {
                i++;
            }
        }
        config += buf[i];
    }
    delete[] buf;
    config += "}";

    int configSize = config.size();
    bool inStr = false;

    std::string key = ".root";
    int i = 0;
    auto tokens = tokenize(configFile, config, i);
    if (tokens.empty() && configSize > 0) {
        LYNX_ERR << "Failed to tokenize" << std::endl;
        return nullptr;
    }
    i = 0;
    CompoundEntry* rootEntry = parseCompound(tokens, i, compoundStack);
    if (!rootEntry) {
        LYNX_ERR << "Failed to parse compound" << std::endl;
        return nullptr;
    }
    rootEntry->setKey(".root");
    return rootEntry;
}
