#include <LynxConf.hpp>

#include <memory>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>

static void recursiveDelete(std::string path) {
    if (std::filesystem::is_directory(path)) {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                recursiveDelete(entry.path().string());
            } else {
                std::filesystem::remove(entry.path());
            }
        }
        std::filesystem::remove_all(path);
    } else {
        std::filesystem::remove(path);
    }
}

std::unordered_map<std::string, NativeFunctionEntry*> nativeFunctions {
    std::pair("runshell", new NativeFunctionEntry({{"command", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* command = args->getString("command");
        if (!command) {
            std::cerr << "Failed to parse runshell block" << std::endl;
            return nullptr;
        }
        StringEntry* entry = new StringEntry();

        std::array<char, 128> buffer;
        std::string resultStr;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command->getValue().c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            resultStr += buffer.data();
        }
        entry->setValue(resultStr);
        return ((ConfigEntry*) entry);
    })),
    std::pair("print", new NativeFunctionEntry({{"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ConfigEntry* result = args->get("value");
        if (!result) {
            std::cerr << "Failed to parse print block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cout << ((StringEntry*) result)->getValue(); break;
            case EntryType::Number: std::cout << ((NumberEntry*) result)->getValue(); break;
            case EntryType::List: result->print(std::cout); break;
            case EntryType::Compound: result->print(std::cout); break;
            default:
                std::cerr << "Invalid entry type in print block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        return result;
    })),
    std::pair("use", new NativeFunctionEntry({{"file", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* result = args->getString("file");
        if (!result) {
            std::cerr << "Failed to parse use block" << std::endl;
            return nullptr;
        }
        std::string file = result->getValue();
        if (!std::filesystem::exists(file)) {
            std::filesystem::path p(file);
            if (p.is_relative()) {
                file = std::filesystem::path("lynx-libs") / p;
            }
        }

        if (!std::filesystem::exists(file)) {
            std::cerr << "File '" << file << "' does not exist" << std::endl;
            return nullptr;
        }

        CompoundEntry* entry = parser->parse(file, compoundStack);
        if (!entry) {
            std::cerr << "Failed to parse file '" << file << "'" << std::endl;
            return nullptr;
        }
        if (compoundStack.size() < 2) {
            std::cerr << "Invalid compound stack size: " << compoundStack.size() << std::endl;
            return nullptr;
        }
        compoundStack.at(compoundStack.size() - 2)->merge(entry);
        return ((ConfigEntry*) entry);
    })),
    std::pair("printLn", new NativeFunctionEntry({{"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ConfigEntry* result = args->get("value");
        if (!result) {
            std::cerr << "Failed to parse printLn block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cout << ((StringEntry*) result)->getValue(); break;
            case EntryType::Number: std::cout << ((NumberEntry*) result)->getValue(); break;
            case EntryType::List: result->print(std::cout); break;
            case EntryType::Compound: result->print(std::cout); break;
            default:
                std::cerr << "Invalid entry type in printLn block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        std::cout << std::endl;
        return result;
    })),
    std::pair("readLn", new NativeFunctionEntry({}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        std::string line;
        std::getline(std::cin, line);
        StringEntry* entry = new StringEntry();
        entry->setValue(line);
        return ((ConfigEntry*) entry);
    })),
    std::pair("eq", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse eq block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(entryA->getType() == entryB->getType() && entryA->operator==(*entryB) ? 1 : 0);
        return ((ConfigEntry*) result);
    })),
    std::pair("ne", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse ne block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(entryA->getType() != entryB->getType() || entryA->operator!=(*entryB) ? 1 : 0);
        return ((ConfigEntry*) result);
    })),
    std::pair("string-length", new NativeFunctionEntry({{"value", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* entry = args->getString("value");
        if (!entry) {
            std::cerr << "Failed to parse string-length block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(entry->getValue().length());
        return ((ConfigEntry*) result);
    })),
    std::pair("string-substring", new NativeFunctionEntry({{"string", Type::String()}, {"start", Type::Number()}, {"end", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* str = args->getString("string");
        NumberEntry* start = args->getNumber("start");
        NumberEntry* end = args->getNumber("end");
        if (!str || !start || !end) {
            std::cerr << "Failed to parse string-substring block" << std::endl;
            return nullptr;
        }
        std::string value = str->getValue();
        long long startValue = start->getValue();
        long long endValue = end->getValue();
        if (startValue < 0 || startValue >= value.size() || endValue < 0 || endValue >= value.size()) {
            std::cerr << "Invalid start or end value in string-substring block" << std::endl;
            return nullptr;
        }
        StringEntry* result = new StringEntry();
        result->setValue(value.substr(startValue, endValue - startValue));
        return ((ConfigEntry*) result);
    })),

#define BINARY_OP(_name, _op) \
    std::pair(_name, new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* { \
        NumberEntry* entryA = args->getNumber("a"); \
        NumberEntry* entryB = args->getNumber("b"); \
        if (!entryA || !entryB) { \
            std::cerr << "Failed to parse " _name " block" << std::endl; \
            return nullptr; \
        } \
        NumberEntry* result = new NumberEntry(); \
        result->setValue(entryA->getValue() _op entryB->getValue()); \
        return ((ConfigEntry*) result); \
    })),

    BINARY_OP("add", +)
    BINARY_OP("sub", -)
    BINARY_OP("mul", *)
    BINARY_OP("div", /)
    BINARY_OP("gt", >)
    BINARY_OP("lt", <)
    BINARY_OP("ge", >=)
    BINARY_OP("le", <=)
    BINARY_OP("or", ||)
    BINARY_OP("and", &&)

#undef BINARY_OP
#define UNARY_OP(_name, _op) \
    std::pair(_name, new NativeFunctionEntry({{"value", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* { \
        NumberEntry* entry = args->getNumber("value"); \
        if (!entry) { \
            std::cerr << "Failed to parse " _name " block" << std::endl; \
            return nullptr; \
        } \
        if (entry->getType() != EntryType::Number) { \
            std::cerr << "Invalid entry type in " _name " block. Expected Number but got " << entry->getType() << std::endl; \
            return nullptr; \
        } \
        NumberEntry* result = new NumberEntry(); \
        result->setValue(_op entry->getValue()); \
        return ((ConfigEntry*) result); \
    })),

    UNARY_OP("not", !)
    
#undef UNARY_OP
    std::pair("mod", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse modulo block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(std::fmod(entryA->getValue(), entryB->getValue()));
        return ((ConfigEntry*) result);
    })),
    std::pair("shl", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse left shift block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue((long long) entryA->getValue() << (long long) entryB->getValue());
        return ((ConfigEntry*) result);
    })),
    std::pair("shr", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse right shift block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue((long long) entryA->getValue() >> (long long) entryB->getValue());
        return ((ConfigEntry*) result);
    })),
    std::pair("range", new NativeFunctionEntry({{"a", Type::Number()}, {"b", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* entryA = args->getNumber("a");
        NumberEntry* entryB = args->getNumber("b");
        if (!entryA || !entryB) {
            std::cerr << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        ListEntry* result = new ListEntry();
        long long start = entryA->getValue();
        long long end = entryB->getValue();
        for (long long i = start; i < end; i++) {
            NumberEntry* entry = new NumberEntry();
            entry->setValue(i);
            result->add(((ConfigEntry*) entry));
        }
        return ((ConfigEntry*) result);
    })),
    std::pair("list-length", new NativeFunctionEntry({{"list", Type::List(Type::Any())}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ListEntry* list = args->getList("list");
        if (!list) {
            std::cerr << "Failed to parse list" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(list->size());
        return ((ConfigEntry*) result);
    })),
    std::pair("list-get", new NativeFunctionEntry({{"list", Type::List(Type::Any())}, {"index", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ListEntry* list = args->getList("list");
        if (!list) {
            std::cerr << "Failed to parse list" << std::endl;
            return nullptr;
        }
        NumberEntry* index = args->getNumber("index");
        if (!index) {
            std::cerr << "Failed to parse index" << std::endl;
            return nullptr;
        }
        long long idx = index->getValue();
        if (idx < 0 || idx >= list->size()) {
            std::cerr << "Index out of bounds" << std::endl;
            return nullptr;
        }
        return list->get(idx)->clone();
    })),
    std::pair("list-set", new NativeFunctionEntry({{"list", Type::List(Type::Any())}, {"index", Type::Number()}, {"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ListEntry* list = args->getList("list");
        if (!list) {
            std::cerr << "Failed to parse list" << std::endl;
            return nullptr;
        }
        NumberEntry* index = args->getNumber("index");
        if (!index) {
            std::cerr << "Failed to parse index" << std::endl;
            return nullptr;
        }
        ConfigEntry* value = args->get("value");
        if (!value) {
            std::cerr << "Failed to parse value" << std::endl;
            return nullptr;
        }
        if (value->getType() != list->getListType()) {
            std::cerr << "Invalid entry type in set. Expected " << list->getListType() << " but got " << value->getType() << std::endl;
            return nullptr;
        }
        long long idx = index->getValue();
        if (idx < 0 || idx >= list->size()) {
            std::cerr << "Index out of bounds" << std::endl;
            return nullptr;
        }
        return list->get(idx) = value->clone();
    })),
    std::pair("list-append", new NativeFunctionEntry({{"list", Type::List(Type::Any())}, {"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ListEntry* list = args->getList("list");
        if (!list) {
            std::cerr << "Failed to parse list" << std::endl;
            return nullptr;
        }
        ConfigEntry* value = args->get("value");
        if (!value) {
            std::cerr << "Failed to parse value" << std::endl;
            return nullptr;
        }
        if (value->getType() != list->getListType()) {
            std::cerr << "Invalid entry type in append. Expected " << list->getListType() << " but got " << value->getType() << std::endl;
            return nullptr;
        }
        list->add(value->clone());
        return new StringEntry();
    })),
    std::pair("list-remove", new NativeFunctionEntry({{"list", Type::List(Type::Any())}, {"index", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ListEntry* list = args->getList("list");
        if (!list) {
            std::cerr << "Failed to parse list" << std::endl;
            return nullptr;
        }
        NumberEntry* index = args->getNumber("index");
        if (!index) {
            std::cerr << "Failed to parse index" << std::endl;
            return nullptr;
        }
        long long idx = index->getValue();
        if (idx < 0 || idx >= list->size()) {
            std::cerr << "Index out of bounds" << std::endl;
            return nullptr;
        }
        list->remove(idx);
        return new StringEntry();
    })),
    std::pair("inc", new NativeFunctionEntry({{"value", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* value = args->getNumber("value");
        if (!value) {
            std::cerr << "Failed to parse inc block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(value->getValue() + 1);
        return ((ConfigEntry*) result);
    })),
    std::pair("dec", new NativeFunctionEntry({{"value", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* value = args->getNumber("value");
        if (!value) {
            std::cerr << "Failed to parse dec block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(value->getValue() - 1);
        return ((ConfigEntry*) result);
    })),
    std::pair("exit", new NativeFunctionEntry({{"value", Type::Number()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        NumberEntry* value = args->getNumber("value");
        if (!value) {
            std::cerr << "Failed to parse exit block" << std::endl;
            return nullptr;
        }
        int exitCode = value->getValue();
        std::exit(exitCode);
        return new StringEntry();
    })),
    std::pair("file-mkdir", new NativeFunctionEntry({{"path", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* str = args->getString("path");
        if (!str) {
            std::cerr << "Failed to parse file-mkdir block" << std::endl;
            return nullptr;
        }
        if (str->getValue().empty()) {
            std::cerr << "Invalid path in file-mkdir block" << std::endl;
            return nullptr;
        }
        if (std::filesystem::exists(str->getValue())) {
            std::cerr << "Path already exists: " << str->getValue() << std::endl;
            return nullptr;
        }
        if (std::filesystem::is_directory(str->getValue())) {
            std::cerr << "Path is already a directory: " << str->getValue() << std::endl;
            return nullptr;
        }
        if (!std::filesystem::create_directories(str->getValue())) {
            std::cerr << "Failed to create directory: " << str->getValue() << std::endl;
            return nullptr;
        }
        return new StringEntry();
    })),
    std::pair("file-rmdir", new NativeFunctionEntry({{"path", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* str = args->getString("path");
        if (!str) {
            std::cerr << "Failed to parse file-rmdir block" << std::endl;
            return nullptr;
        }
        if (str->getValue().empty()) {
            std::cerr << "Invalid path in file-rmdir block" << std::endl;
            return nullptr;
        }
        if (!std::filesystem::exists(str->getValue())) {
            std::cerr << "Path does not exist: " << str->getValue() << std::endl;
            return nullptr;
        }
        recursiveDelete(str->getValue());
        return new StringEntry();
    })),
    std::pair("file-remove", new NativeFunctionEntry({{"path", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* str = args->getString("path");
        if (!str) {
            std::cerr << "Failed to parse file-remove block" << std::endl;
            return nullptr;
        }
        if (str->getValue().empty()) {
            std::cerr << "Invalid path in file-remove block" << std::endl;
            return nullptr;
        }
        if (!std::filesystem::exists(str->getValue())) {
            std::cerr << "Path does not exist: " << str->getValue() << std::endl;
            return nullptr;
        }
        std::filesystem::remove(str->getValue());
        return new StringEntry();
    })),
    std::pair("file-write", new NativeFunctionEntry({{"path", Type::String()}, {"content", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* path = args->getString("path");
        if (!path) {
            std::cerr << "Failed to parse file-write block" << std::endl;
            return nullptr;
        }
        if (path->getValue().empty()) {
            std::cerr << "Invalid path in file-write block" << std::endl;
            return nullptr;
        }
        StringEntry* content = args->getString("content");
        if (!content) {
            std::cerr << "Failed to parse content in file-write block" << std::endl;
            return nullptr;
        }
        if (std::filesystem::exists(path->getValue())) {
            if (std::filesystem::is_directory(path->getValue())) {
                recursiveDelete(path->getValue());
            } else {
                if (!std::filesystem::remove(path->getValue())) {
                    std::cerr << "Failed to remove existing file: " << path->getValue() << std::endl;
                    return nullptr;
                }
            }
        }
        std::ofstream file(path->getValue());
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << path->getValue() << std::endl;
            return nullptr;
        }
        file << content->getValue();
        if (file.fail()) {
            std::cerr << "Failed to write to file: " << path->getValue() << std::endl;
            return nullptr;
        }
        file.close();
        StringEntry* result = new StringEntry();
        result->setValue(path->getValue());
        return ((ConfigEntry*) result);
    })),
    std::pair("file-read", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-read block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-read block" << std::endl;
            return nullptr;
        }
        std::ifstream file(filename->getValue());
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename->getValue() << std::endl;
            return nullptr;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (file.fail()) {
            std::cerr << "Failed to read from file: " << filename->getValue() << std::endl;
            return nullptr;
        }
        file.close();
        StringEntry* result = new StringEntry();
        result->setValue(content);
        return ((ConfigEntry*) result);
    })),
    std::pair("file-exists", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-exists block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-exists block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(std::filesystem::exists(filename->getValue()) ? 1 : 0);
        return ((ConfigEntry*) result);
    })),
    std::pair("file-isdir", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-isdir block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-isdir block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(std::filesystem::is_directory(filename->getValue()) ? 1 : 0);
        return ((ConfigEntry*) result);
    })),
    std::pair("file-isfile", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-isfile block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-isfile block" << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(std::filesystem::is_regular_file(filename->getValue()) ? 1 : 0);
        return ((ConfigEntry*) result);
    })),
    std::pair("file-dirname", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-dirname block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-dirname block" << std::endl;
            return nullptr;
        }
        StringEntry* result = new StringEntry();
        result->setValue(std::filesystem::path(filename->getValue()).parent_path().string());
        return ((ConfigEntry*) result);
    })),
    std::pair("file-basename", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-basename block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-basename block" << std::endl;
            return nullptr;
        }
        StringEntry* result = new StringEntry();
        result->setValue(std::filesystem::path(filename->getValue()).filename().string());
        return ((ConfigEntry*) result);
    })),
    std::pair("file-extname", new NativeFunctionEntry({{"filename", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* filename = args->getString("filename");
        if (!filename) {
            std::cerr << "Failed to parse file-extname block" << std::endl;
            return nullptr;
        }
        if (filename->getValue().empty()) {
            std::cerr << "Invalid filename in file-extname block" << std::endl;
            return nullptr;
        }
        StringEntry* result = new StringEntry();
        result->setValue(std::filesystem::path(filename->getValue()).extension().string());
        return ((ConfigEntry*) result);
    })),
    std::pair("file-copy", new NativeFunctionEntry({{"from", Type::String()}, {"to", Type::String()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        StringEntry* from = args->getString("from");
        if (!from) {
            std::cerr << "Failed to parse file-copy block" << std::endl;
            return nullptr;
        }
        if (from->getValue().empty()) {
            std::cerr << "Invalid from path in file-copy block" << std::endl;
            return nullptr;
        }
        StringEntry* to = args->getString("to");
        if (!to) {
            std::cerr << "Failed to parse file-copy block" << std::endl;
            return nullptr;
        }
        if (to->getValue().empty()) {
            std::cerr << "Invalid to path in file-copy block" << std::endl;
            return nullptr;
        }
        if (!std::filesystem::exists(from->getValue())) {
            std::cerr << "Source file does not exist: " << from->getValue() << std::endl;
            return nullptr;
        }
        if (std::filesystem::exists(to->getValue())) {
            if (std::filesystem::is_directory(to->getValue())) {
                std::cerr << "Destination path is a directory: " << to->getValue() << std::endl;
                return nullptr;
            } else {
                if (!std::filesystem::remove(to->getValue())) {
                    std::cerr << "Failed to remove existing file: " << to->getValue() << std::endl;
                    return nullptr;
                }
            }
        }
        if (std::filesystem::is_directory(from->getValue())) {
            std::cerr << "Source path is a directory: " << from->getValue() << std::endl;
            return nullptr;
        }
        if (std::filesystem::exists(to->getValue())) {
            std::cerr << "Destination path already exists: " << to->getValue() << std::endl;
            return nullptr;
        }
        if (!std::filesystem::exists(std::filesystem::path(to->getValue()).parent_path())) {
            std::filesystem::create_directories(std::filesystem::path(to->getValue()).parent_path());
        }
        if (!std::filesystem::exists(std::filesystem::path(to->getValue()).parent_path())) {
            std::cerr << "Failed to create directory: " << std::filesystem::path(to->getValue()).parent_path() << std::endl;
            return nullptr;
        }
        std::filesystem::copy(from->getValue(), to->getValue());
        if (std::filesystem::exists(to->getValue())) {
            StringEntry* result = new StringEntry();
            result->setValue(to->getValue());
            return ((ConfigEntry*) result);
        } else {
            std::cerr << "Failed to copy file from " << from->getValue() << " to " << to->getValue() << std::endl;
            return nullptr;
        }
    })),
    std::pair("printErr", new NativeFunctionEntry({{"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ConfigEntry* result = args->get("value");
        if (!result) {
            std::cerr << "Failed to parse printErr block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cerr << ((StringEntry*) result)->getValue(); break;
            case EntryType::Number: std::cerr << ((NumberEntry*) result)->getValue(); break;
            case EntryType::List: result->print(std::cerr); break;
            case EntryType::Compound: result->print(std::cerr); break;
            default:
                std::cerr << "Invalid entry type in printErr block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        return result;
    })),
    std::pair("printErrLn", new NativeFunctionEntry({{"value", Type::Any()}}, [](ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, CompoundEntry* args) -> ConfigEntry* {
        ConfigEntry* result = args->get("value");
        if (!result) {
            std::cerr << "Failed to parse printErrLn block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cerr << ((StringEntry*) result)->getValue(); break;
            case EntryType::Number: std::cerr << ((NumberEntry*) result)->getValue(); break;
            case EntryType::List: result->print(std::cerr); break;
            case EntryType::Compound: result->print(std::cerr); break;
            default:
                std::cerr << "Invalid entry type in printErrLn block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        std::cerr << std::endl;
        return result;
    })),
};
