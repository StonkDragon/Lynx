#if defined(_WIN32)
#define OS_NAME "windows"
#elif defined(__APPLE__)
#define OS_NAME "macos"
#elif defined(__linux__)
#define OS_NAME "linux"
#else
#define OS_NAME "unknown"
#endif

#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <filesystem>
#include <vector>
#include <utility>
#include <functional>
#include <fstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define popen _popen
#define pclose _pclose
#endif

#include "LynxConf.hpp"

using namespace std;

std::unordered_map<std::string, BuiltinCommand> builtins {
    std::pair("func", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        DeclaredFunctionEntry* entry = new DeclaredFunctionEntry();
        i++;
        if (i >= tokens.size()) {
            LYNX_ERR << "Expected block start but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        i++;
        while (i < tokens.size() && tokens[i].type != Token::BlockEnd) {
            if (tokens[i].type != Token::Identifier) {
                LYNX_ERR << "Expected Identifier but got " << tokens[i].value << std::endl;
                return nullptr;
            }
            std::string name = tokens[i].value;
            i++;
            if (i >= tokens.size() || tokens[i].type != Token::Is) {
                LYNX_ERR << "Expected ':' but got " << tokens[i].value << std::endl;
                return nullptr;
            }
            i++;
            Type* type = parser->parseType(tokens, i, compoundStack);
            if (!type) {
                LYNX_ERR << "Failed to parse type for argument '" << name << "'" << std::endl;
                return nullptr;
            }
            entry->args.push_back({name, type});
            i++;
        }
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::BlockStart) {
            LYNX_ERR << "Expected block start but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        std::vector<Token> body;
        body.push_back(tokens[i]);
        i++;
        int blockDepth = 1;
        while (i < tokens.size() && blockDepth > 0) {
            if (tokens[i].type == Token::BlockStart) {
                blockDepth++;
            } else if (tokens[i].type == Token::BlockEnd) {
                blockDepth--;
            }
            body.push_back(tokens[i]);
            i++;
        }
        i--;
        entry->body = body;
        entry->compoundStack = compoundStack;
        return entry->clone();
    }),
    std::pair("true", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        NumberEntry* entry = new NumberEntry();
        entry->setValue(1);
        return ((ConfigEntry*) entry);
    }),
    std::pair("false", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        NumberEntry* entry = new NumberEntry();
        entry->setValue(0);
        return ((ConfigEntry*) entry);
    }),
    std::pair("for", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
            LYNX_ERR << "Invalid for loop: Expected identifier" << std::endl;
            return nullptr;
        }
        std::string iterVar = tokens[i].value;
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
            LYNX_ERR << "Invalid for loop: Expected 'in' but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        if (tokens[i].value != "in") {
            LYNX_ERR << "Invalid for loop: Expected 'in' but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        i++;
        if (!entry) {
            entry = new ListEntry();
        }
        if (entry->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type. Expected List but got " << entry->getType() << std::endl;
            return nullptr;
        }
        ListEntry* list = ((ListEntry*) entry);
        if (i >= tokens.size() || tokens[i].type != Token::BlockStart) {
            LYNX_ERR << "Invalid for loop: Expected block start but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        
        std::vector<Token> forBody;
        forBody.push_back(tokens[i]);
        if (i < tokens.size() && tokens[i].type == Token::BlockStart) {
            i++;
            int blockDepth = 1;
            while (i < tokens.size() && blockDepth > 0) {
                if (tokens[i].type == Token::BlockStart) {
                    blockDepth++;
                } else if (tokens[i].type == Token::BlockEnd) {
                    blockDepth--;
                }
                forBody.push_back(tokens[i]);
                i++;
            }
            i--;
        }
        
        if (i >= tokens.size()) {
            LYNX_ERR << "Unexpected end of file" << std::endl;
            return nullptr;
        }
        
        CompoundEntry* compound = nullptr;
        ConfigEntry* result = nullptr;
        for (size_t i = 0; i < list->size(); i++) {
            auto value = list->get(i);
            std::string oldKey = value->getKey();
            value->setKey(iterVar);
            compound = new CompoundEntry();
            compoundStack.push_back(compound);
            compound->add(value);
            int newI = 0;
            ConfigEntry* next = parser->parseValue(forBody, newI, compoundStack);
            if (!next) {
                LYNX_ERR << "Failed to parse for loop block" << std::endl;
                value->setKey(oldKey);
                return nullptr;
            }
            compoundStack.pop_back();
            if (!result) {
                result = next;
            } else if (next->getType() != result->getType()) {
                LYNX_ERR << "Invalid entry type in for loop block. Expected " << result->getType() << " but got " << next->getType() << std::endl;
                value->setKey(oldKey);
                return nullptr;
            } else {
                bool sumEntries(ConfigEntry* a, ConfigEntry* b);
                if (!sumEntries(result, next)) {
                    value->setKey(oldKey);
                    return nullptr;
                }
            }
            value->setKey(oldKey);
        }
        if (!result) {
            return new StringEntry();
        }
        return result;
    }),
    std::pair("if", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        i++;
        if (!entry) {
            LYNX_ERR << "Failed to parse value" << std::endl;
            return nullptr;
        }
        if (entry->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type. Expected Number but got " << entry->getType() << std::endl;
            entry->print(std::cerr);
            return nullptr;
        }

        std::vector<Token> ifBlock;
        ifBlock.push_back(tokens[i]);
        if (i < tokens.size() && tokens[i].type == Token::BlockStart) {
            i++;
            int blockDepth = 1;
            while (i < tokens.size() && blockDepth > 0) {
                if (tokens[i].type == Token::BlockStart) {
                    blockDepth++;
                } else if (tokens[i].type == Token::BlockEnd) {
                    blockDepth--;
                }
                ifBlock.push_back(tokens[i]);
                i++;
            }
            i--;
        }

        std::vector<Token> elseBlock;
        if (i + 1 < tokens.size() && tokens[i + 1].type == Token::Identifier && tokens[i + 1].value == "else") {
            i++;
            i++;
            elseBlock.push_back(tokens[i]);
            if (i < tokens.size() && tokens[i].type == Token::BlockStart) {
                i++;
                int blockDepth = 1;
                while (i < tokens.size() && blockDepth > 0) {
                    if (tokens[i].type == Token::BlockStart) {
                        blockDepth++;
                    } else if (tokens[i].type == Token::BlockEnd) {
                        blockDepth--;
                    }
                    elseBlock.push_back(tokens[i]);
                    i++;
                }
                i--;
            }
        } else {
            Token t;
            t.type = Token::String;
            t.value = "";
            elseBlock.push_back(t);
        }

        bool condition = ((NumberEntry*) entry)->getValue() != 0;

        std::vector<Token>& ifBlockToUse = condition ? ifBlock : elseBlock;

        int newI = 0;
        ConfigEntry* result = parser->parseValue(ifBlockToUse, newI, compoundStack);
        if (!result) {
            LYNX_ERR << "Failed to parse if block" << std::endl;
            return nullptr;
        }

        return result;
    }),
    std::pair("exists", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        auto byPath = [](std::string value, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
            ConfigEntry* entry = nullptr;
            for (size_t i = compoundStack.size(); i > 0 && !entry; i--) {
                entry = compoundStack[i - 1]->getByPath(value);
            }
            if (!entry) {
                return nullptr;
            }
            return entry->clone();
        };

        auto makePath = [](std::vector<Token>& tokens, int& i) -> std::string {
            std::string path = tokens[i].value;
            i++;
            while (i < tokens.size() && tokens[i].type == Token::Dot) {
                path += ".";
                i++;
                if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
                    LYNX_ERR << "Invalid path" << std::endl;
                    return nullptr;
                }
                path += tokens[i].value;
                i++;
            }
            i--;
            return path;
        };
        
        i++;
        std::string path = makePath(tokens, i);
        ConfigEntry* entry = byPath(path, compoundStack);
        
        bool condition = entry != nullptr;
        NumberEntry* result = new NumberEntry();
        result->setValue(condition ? 1 : 0);
        return ((ConfigEntry*) result);
    }),
    std::pair("set", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
            LYNX_ERR << "Invalid set block: Expected identifier" << std::endl;
            return nullptr;
        }
        std::string key = tokens[i].value;
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse value" << std::endl;
            return nullptr;
        }
        
        entry = entry->clone();
        entry->setKey(key);
        compoundStack.back()->add(entry);
        return ((ConfigEntry*) entry);
    }),
};

void recursiveDelete(std::string path) {
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
};

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

ConfigEntry* ConfigEntry::Null = nullptr;

FunctionEntry::FunctionEntry() {
    this->setType(EntryType::Function);
    this->isDotCallable = false;
}

bool FunctionEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != this->getType()) return false;
    const FunctionEntry* otherFunc = (const FunctionEntry*) &other;
    if (this->args != otherFunc->args) return false;
    if (this->body != otherFunc->body) return false;
    return true;
}
bool FunctionEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}
void FunctionEntry::print(std::ostream& stream, int indent) {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "func(";
    std::ostringstream argsStream;
    for (size_t i = 0; i < this->args.size(); i++) {
        if (i > 0) {
            argsStream << " ";
        }
        argsStream << this->args[i].key << ": ";
        if (this->args[i].type) {
            argsStream << this->args[i].type->toString();
        } else {
            argsStream << "any";
        }
    }
    std::string args = argsStream.str();
    if (args.length() > 16) {
        args = "...";
    }
    stream << args;
    stream << ")" << std::endl;
}
ConfigEntry* FunctionEntry::clone() {
    FunctionEntry* newFunc = new FunctionEntry();
    newFunc->body = this->body;
    newFunc->args = this->args;
    newFunc->isDotCallable = this->isDotCallable;
    return newFunc;
}

ConfigEntry* DeclaredFunctionEntry::clone() {
    DeclaredFunctionEntry* newFunc = new DeclaredFunctionEntry();
    newFunc->body = this->body;
    newFunc->args = this->args;
    newFunc->isDotCallable = this->isDotCallable;
    newFunc->compoundStack = this->compoundStack;
    return newFunc;
}

DeclaredFunctionEntry::DeclaredFunctionEntry() {
    this->setType(EntryType::Function);
    this->isDotCallable = false;
}

ConfigEntry* FunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = new CompoundEntry();
    for (size_t n = 0; n < this->args.size(); n++) {
        i++;
        ConfigEntry* arg = parser->parseValue(tokens, i, compoundStack);
        if (!arg) {
            LYNX_ERR << "Failed to parse argument '" << this->args[n].key << "'" << std::endl;
            return nullptr;
        }
        arg = arg->clone();
        if (!this->args[n].type->validate(arg, {})) {
            LYNX_ERR << "Invalid argument type" << std::endl;
            return nullptr;
        }
        arg->setKey(this->args[n].key);
        args->add(arg);
    }
    compoundStack.push_back(args);
    int x = 0;
    ConfigEntry* result = parser->parseValue(this->body, x, compoundStack);
    compoundStack.pop_back();
    if (!result) {
        LYNX_ERR << "Failed to run function" << std::endl;
        return nullptr;
    }
    return result;
}

ConfigEntry* DeclaredFunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = new CompoundEntry();
    for (size_t n = 0; n < this->args.size(); n++) {
        i++;
        ConfigEntry* arg = parser->parseValue(tokens, i, compoundStack);
        if (!arg) {
            LYNX_ERR << "Failed to parse argument '" << this->args[n].key << "'" << std::endl;
            return nullptr;
        }
        arg = arg->clone();
        if (!this->args[n].type->validate(arg, {})) {
            LYNX_ERR << "Invalid argument type" << std::endl;
            return nullptr;
        }
        arg->setKey(this->args[n].key);
        args->add(arg);
    }
    this->compoundStack.push_back(args);
    int x = 0;
    ConfigEntry* result = parser->parseValue(this->body, x, this->compoundStack);
    this->compoundStack.pop_back();
    if (!result) {
        LYNX_ERR << "Failed to run function" << std::endl;
        return nullptr;
    }
    return result;
}

ConfigEntry* NativeFunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = new CompoundEntry();
    for (size_t n = 0; n < this->args.size(); n++) {
        i++;
        ConfigEntry* arg = parser->parseValue(tokens, i, compoundStack);
        if (!arg) {
            LYNX_ERR << "Failed to parse argument '" << this->args[n].key << "'" << std::endl;
            return nullptr;
        }
        arg = arg->clone();
        if (!this->args[n].type->validate(arg, {})) {
            LYNX_ERR << "Invalid argument type" << std::endl;
            return nullptr;
        }
        arg->setKey(this->args[n].key);
        args->add(arg);
    }
    compoundStack.push_back(args);
    ConfigEntry* result = this->func(parser, compoundStack, args);
    compoundStack.pop_back();
    if (!result) {
        LYNX_ERR << "Failed to run function" << std::endl;
        return nullptr;
    }
    return result;
}

NativeFunctionEntry::NativeFunctionEntry(std::vector<Type::CompoundType> args, typeof(NativeFunctionEntry::func) func) {
    this->setType(EntryType::Function);
    this->args = args;
    this->func = func;
}

std::string ConfigEntry::getKey() const {
    return key;
}
EntryType ConfigEntry::getType() const {
    return type;
}
void ConfigEntry::setKey(const std::string& key) {
    this->key = key;
}
void ConfigEntry::setType(EntryType type) {
    this->type = type;
}
void ConfigEntry::print(std::ostream& out, int indent) {
    (void) out; (void) indent;
}
std::ostream& operator<<(std::ostream& out, EntryType type) {
    switch (type) {
        case EntryType::String:
            out << "String";
            break;
        case EntryType::Number:
            out << "Number";
            break;
        case EntryType::List:
            out << "List";
            break;
        case EntryType::Compound:
            out << "Compound";
            break;
        case EntryType::Type:
            out << "Type";
            break;
        case EntryType::Function:
            out << "Function";
            break;
        case EntryType::Any:
            out << "Any";
            break;
        case EntryType::Invalid:
            out << "Invalid";
            break;
    }
    return out;
}

StringEntry::StringEntry() {
    this->setType(EntryType::String);
}
std::string StringEntry::getValue() {
    return this->value;
}
void StringEntry::setValue(std::string value) {
    this->value = value;
}
bool StringEntry::isEmpty() {
    return this->value.empty();
}
bool StringEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::String) {
        return false;
    }
    const StringEntry& otherString = (const StringEntry&) other;
    return this->value == otherString.value;
}
bool StringEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}
void StringEntry::print(std::ostream& stream, int indent) {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "\"" << this->value << "\"" << std::endl;
}

NumberEntry::NumberEntry() {
    this->setType(EntryType::Number);
}
double NumberEntry::getValue() {
    return this->value;
}
void NumberEntry::setValue(double value) {
    this->value = value;
}
bool NumberEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::Number) {
        return false;
    }
    const NumberEntry& otherNumber = (const NumberEntry&) other;
    return this->value == otherNumber.value;
}
bool NumberEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}
void NumberEntry::print(std::ostream& stream, int indent) {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << this->value << std::endl;
}

ListEntry::ListEntry() {
    this->setType(EntryType::List);
    this->values = nullptr;
    this->length = 0;
    this->capacity = 0;
}
ConfigEntry*& ListEntry::get(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return ConfigEntry::Null;
    }
    return this->values[index];
}
ConfigEntry*& ListEntry::operator[](unsigned long index) {
    return this->get(index);
}
StringEntry* ListEntry::getString(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::String ? ((StringEntry*) this->values[index]) : nullptr);
}
CompoundEntry* ListEntry::getCompound(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::Compound ? ((CompoundEntry*) this->values[index]) : nullptr);
}
ListEntry* ListEntry::getList(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::List ? ((ListEntry*) this->values[index]) : nullptr);
}
unsigned long ListEntry::size() {
    return this->length;
}
void ListEntry::add(ConfigEntry* value) {
    if (this->length >= this->capacity) {
        this->capacity = this->capacity ? this->capacity * 2 : 16;
        try { 
            ConfigEntry** newValues = new ConfigEntry*[this->capacity];
            for (unsigned long i = 0; i < this->length; i++) {
                newValues[i] = this->values[i];
            }
            delete[] this->values;
            this->values = newValues;
        } catch (std::bad_array_new_length& e) {
            LYNX_RT_ERR << "Failed to reserve " << this->capacity << " elements" << std::endl;
            LYNX_RT_ERR << e.what() << std::endl;
            std::exit(1);
        }
    }
    this->values[this->length++] = value;
}
void ListEntry::remove(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return;
    }
    for (unsigned long i = index; i < this->length - 1; i++) {
        this->values[i] = this->values[i + 1];
    }
    this->length--;
}
void ListEntry::setListType(EntryType type) {
    this->listType = type;
}
EntryType ListEntry::getListType() {
    return this->listType;
}
void ListEntry::clear() {
    this->length = 0;
}
bool ListEntry::isEmpty() {
    return this->length == 0;
}
void ListEntry::merge(ListEntry* other) {
    for (unsigned long i = 0; i < other->length; i++) {
        this->add(other->values[i]);
    }
}
bool ListEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::List) {
        return false;
    }
    const ListEntry& list = (const ListEntry&) other;
    if (this->length != list.length) {
        return false;
    }

    for (unsigned long i = 0; i < this->length; i++) {
        if (this->values[i]->operator!=(*list.values[i])) {
            return false;
        }
    }
    return true;
}
bool ListEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}
void ListEntry::print(std::ostream& stream, int indent) {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "[" << std::endl;
    indent += 2;
    for (unsigned long i = 0; i < this->length; i++) {
        this->values[i]->print(stream, indent);
    }
    indent -= 2;
    stream << std::string(indent, ' ') << "]" << std::endl;
}

CompoundEntry::CompoundEntry() {
    this->setType(EntryType::Compound);
    this->entries = nullptr;
    this->length = 0;
    this->capacity = 0;
}
bool CompoundEntry::hasMember(const std::string& key) {
    for (unsigned long i = 0; i < this->length; i++) {
        if (this->entries[i]->getKey() == key) {
            return true;
        }
    }
    return false;
}

StringEntry* CompoundEntry::getString(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::String) {
        return ((StringEntry*) x);
    }
    return nullptr;
}
StringEntry* CompoundEntry::getStringOrDefault(const std::string& key, const std::string& defaultValue) {
    StringEntry* entry = getString(key);
    if (entry) {
        return ((StringEntry*) entry);
    }
    entry = new StringEntry();
    entry->setValue(defaultValue);
    return entry;
}
NumberEntry* CompoundEntry::getNumber(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::Number) {
        return ((NumberEntry*) x);
    }
    return nullptr;
}
ListEntry* CompoundEntry::getList(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::List) {
        return ((ListEntry*) x);
    }
    return nullptr;
}
CompoundEntry* CompoundEntry::getCompound(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::Compound) {
        return ((CompoundEntry*) x);
    }
    return nullptr;
}
ConfigEntry*& CompoundEntry::get(const std::string& key) {
    for (unsigned long i = 0; i < this->length; i++) {
        if (this->entries[i]->getKey() == key) {
            return this->entries[i];
        }
    }
    return ConfigEntry::Null;
}
ConfigEntry* CompoundEntry::getByPath(const std::string& path) {
    std::string key = "";
    CompoundEntry* current = this;
    for (char c : path) {
        if (c == '.') {
            if (key.size()) {
                current = current->getCompound(key);
                if (!current) {
                    return nullptr;
                }
                key = "";
            }
        } else {
            key += c;
        }
    }
    return current->get(key);
}
ConfigEntry*& CompoundEntry::operator[](const std::string& key) {
    return this->get(key);
}
void CompoundEntry::setString(const std::string& key, const std::string& value) {
    StringEntry* entry = getString(key);
    if (entry) {
        entry->setValue(value);
        return;
    }
    entry = new StringEntry();
    entry->setKey(key);
    entry->setValue(value);
    add(((ConfigEntry*) entry));
}
void CompoundEntry::add(ConfigEntry* entry) {
    for (unsigned long i = 0; i < this->length; i++) {
        if (this->entries[i]->getKey() == entry->getKey()) {
            this->entries[i] = entry;
            return;
        }
    }
    if (this->length >= this->capacity) {
        this->capacity = this->capacity ? this->capacity * 2 : 16;
        try { 
            ConfigEntry** newEntries = new ConfigEntry*[this->capacity];
            for (unsigned long i = 0; i < this->length; i++) {
                newEntries[i] = this->entries[i];
            }
            delete[] this->entries;
            this->entries = newEntries;
        } catch (std::bad_array_new_length& e) {
            LYNX_RT_ERR << "Failed to reserve " << this->capacity << " elements" << std::endl;
            LYNX_RT_ERR << e.what() << std::endl;
            std::exit(1);
        }
    }
    this->entries[this->length++] = entry;
}
void CompoundEntry::addString(const std::string& key, const std::string& value) {
    if (this->hasMember(key)) {
        std::cerr << "String with key '" << key << "' already exists" << std::endl;
        return;
    }
    StringEntry* newEntry = new StringEntry();
    newEntry->setKey(key);
    newEntry->setValue(value);
    add(((ConfigEntry*) newEntry));
}
void CompoundEntry::addList(const std::string& key, const std::vector<ConfigEntry*>& value) {
    if (this->hasMember(key)) {
        std::cerr << "List with key '" << key << "' already exists!" << std::endl;
        return;
    }
    ListEntry* newEntry = new ListEntry();
    newEntry->setKey(key);
    for (auto entry : value) {
        newEntry->add(entry);
    }
    add(((ConfigEntry*) newEntry));
}
void CompoundEntry::addList(const std::string& key, ConfigEntry* value) {
    if (this->hasMember(key)) {
        std::cerr << "List with key '" << key << "' already exists!" << std::endl;
        return;
    }
    ListEntry* newEntry = new ListEntry();
    newEntry->setKey(key);
    newEntry->add(value);
    add(((ConfigEntry*) newEntry));
}
void CompoundEntry::addList(ListEntry* value) {
    if (this->hasMember(value->getKey())) {
        std::cerr << "List with key '" << value->getKey() << "' already exists!" << std::endl;
        return;
    }
    add(((ConfigEntry*) value));
}
void CompoundEntry::addCompound(CompoundEntry* value) {
    if (this->hasMember(value->getKey())) {
        std::cerr << "Compound with key '" << value->getKey() << "' already exists!" << std::endl;
        return;
    }
    add(((ConfigEntry*) value));
}
size_t CompoundEntry::merge(CompoundEntry* other) {
    size_t count = 0;
    for (unsigned long i = 0; i < other->length; i++) {
        if (!this->hasMember(other->entries[i]->getKey())) {
            this->add(other->entries[i]);
            count++;
        }
    }
    return count;
}
void CompoundEntry::remove(const std::string& key) {
    for (u_long i = 0; i < this->length; i++) {
        if (this->entries[i]->getKey() == key) {
            this->entries[i] = nullptr;
            return;
        }
    }
}
void CompoundEntry::removeAll() {
    this->length = 0;
}
bool CompoundEntry::isEmpty() {
    return this->length == 0;
}
bool CompoundEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::Compound) {
        return false;
    }
    const CompoundEntry& otherCompound = (const CompoundEntry&) other;
    if (this->length != otherCompound.length) {
        return false;
    }
    for (u_long i = 0; i < this->length; i++) {
        if (this->entries[i]->operator!=(*otherCompound.entries[i])) {
            return false;
        }
    }
    return true;
}
bool CompoundEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}
void CompoundEntry::print(std::ostream& stream, int indent) {
    if (this->getKey() == ".root") {
        for (u_long i = 0; i < this->length; i++) {
            this->entries[i]->print(stream, indent);
        }
    } else {
        stream << std::string(indent, ' ');
        if (this->getKey().size()) {
            stream << this->getKey() << ": ";
        } 
        stream << "{" << std::endl;
        indent += 2;
        for (u_long i = 0; i < this->length; i++) {
            this->entries[i]->print(stream, indent);
        }
        indent -= 2;
        stream << std::string(indent, ' ') << "}" << std::endl;
    }
}

bool isValidIdentifier(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static bool isnumber(char c) {
    return (c >= '0' && c <= '9');
}

#define skipWhitespace() while (i < data.size() && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) { if (data[i] == '\n') {line++; column = 0;} else column++; i++;  }
#define next(_r) ({ i++; column++; if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return _r; } data[i]; })
#define maybeNext(_r) ({ i++; column++; i < data.size() ? data[i] : '\0'; })
#define prev(_r) ({ i--; if (i < 0) { LYNX_ERR << "Unexpected start of file" << std::endl; return _r; } data[i]; })
#define nextChar(_r) ({ i++; column++; skipWhitespace(); if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return _r; } data[i]; })
#define peek() ({ skipWhitespace(); data[i]; })

std::vector<Token> tokenize(std::string file, std::string& data, int& i) {
    std::vector<Token> tokens;
    int line = 1;
    int column = 0;
    char c = peek();
    while (c != '\0') {
        Token token;
        token.file = file;
        token.line = line;
        token.column = column;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            skipWhitespace();
            c = peek();
            continue;
        } else if (c == '"') {
            token.type = Token::String;
            token.value = "";
            c = next({});
            while (c != '"' && c != '\0') {
                if (c == '\\') {
                    c = next({});
                    switch (c) {
                        case 'n': token.value += '\n'; break;
                        case 'r': token.value += '\r'; break;
                        case 't': token.value += '\t'; break;
                        case '0': token.value += '\0'; break;
                        case '\\': token.value += '\\'; break;
                        case '"': token.value += '"'; break;
                        default: LYNX_ERR << "Invalid escape sequence: \\" << c << std::endl; return tokens;
                    }
                } else {
                    token.value += c;
                }
                c = next({});
            }
            if (c == '\0') {
                LYNX_ERR << "Unexpected end of file" << std::endl;
                return tokens;
            }
        } else if (isnumber(c) || c == '-' || c == '+') {
            token.type = Token::Number;
            token.value = "";
            while (isnumber(c) || c == '.' || c == '-' || c == '+') {
                token.value += c;
                c = next({});
            }
            prev({});
        } else if (c == '[') {
            token.type = Token::ListStart;
            token.value = "[";
        } else if (c == ']') {
            token.type = Token::ListEnd;
            token.value = "]";
        } else if (c == '{') {
            token.type = Token::CompoundStart;
            token.value = "{";
        } else if (c == '}') {
            token.type = Token::CompoundEnd;
            token.value = "}";
        } else if (c == '(') {
            token.type = Token::BlockStart;
            token.value = "(";
        } else if (c == ')') {
            token.type = Token::BlockEnd;
            token.value = ")";
        } else if (c == '.') {
            token.type = Token::Dot;
            token.value = ".";
        } else if (c == '=') {
            token.type = Token::Assign;
            token.value = "=";
        } else if (c == ':') {
            token.type = Token::Is;
            token.value = ":";
        } else if (isValidIdentifier(c)) {
            token.type = Token::Identifier;
            token.value = "";
            while (isValidIdentifier(c)) {
                token.value += c;
                c = next({});
            }
            prev({});
        } else {
            LYNX_ERR << "Invalid character: " << c << std::endl;
            return tokens;
        }
        tokens.push_back(token);
        c = maybeNext({});
    }
    return tokens;
}

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

ConfigEntry* StringEntry::clone() {
    StringEntry* entry = new StringEntry();
    entry->setKey(this->getKey());
    entry->setValue(this->value);
    return ((ConfigEntry*) entry);
}

ConfigEntry* NumberEntry::clone() {
    NumberEntry* entry = new NumberEntry();
    entry->setKey(this->getKey());
    entry->setValue(this->value);
    return ((ConfigEntry*) entry);
}

ConfigEntry* ListEntry::clone() {
    ListEntry* entry = new ListEntry();
    entry->setKey(this->getKey());
    entry->setListType(this->listType);
    entry->merge(this);
    return ((ConfigEntry*) entry);
}

ConfigEntry* CompoundEntry::clone() {
    CompoundEntry* entry = new CompoundEntry();
    entry->setKey(this->getKey());
    entry->merge(this);
    return ((ConfigEntry*) entry);
}

bool Type::validate(ConfigEntry* what, const std::vector<std::string>& flags, std::ostream& out) {
    bool hasError = false;

    bool optional = this->isOptional || std::find(flags.begin(), flags.end(), "optional") != flags.end();
    if (what->getType() != this->type) {
        LYNX_RT_ERR_TO(out) << "Invalid entry type. Expected " << this->type << " but got " << what->getType() << std::endl;
        return false;
    }
    
    if (what->getType() == EntryType::List) {
        ListEntry* list = ((ListEntry*) what);
        if (list->isEmpty()) {
            return true;
        }
        if (list->getListType() == EntryType::Invalid) {
            LYNX_RT_ERR_TO(out) << "Invalid entry type in list '" << what->getKey() << "'. Expected " << this->listType->type << " but got Invalid" << std::endl;
            return false;
        }
        if (!this->listType->validate(list->get(0), flags, out)) {
            LYNX_RT_ERR_TO(out) << "Invalid entry type in list '" << what->getKey() << "'. Expected " << this->listType->type << " but got " << list->getListType() << std::endl;
            hasError = true;
        }
    } else if (what->getType() == EntryType::Compound) {
        CompoundEntry* compound = ((CompoundEntry*) what);
        for (size_t i = 0; i < this->compoundTypes->size(); i++) {
            Type::CompoundType compoundType = this->compoundTypes->at(i);
            if (!compound->hasMember(compoundType.key)) {
                if (optional || compoundType.type->isOptional) {
                    continue;
                }
                LYNX_RT_ERR_TO(out) << "Missing property '" << compoundType.key << "' in compound '" << what->getKey() << "'" << std::endl;
                hasError = true;
                continue;
            }
            if (!compoundType.type->validate(compound->get(compoundType.key), flags, out)) {
                LYNX_RT_ERR_TO(out) << "Member '" << compoundType.key << "' of compound '" << what->getKey() << "' is type " << compound->get(compoundType.key)->getType() << " but expected " << compoundType.type->type << std::endl;
                hasError = true;
            }
        }
    } else if (what->getType() == EntryType::String) {
        return true;
    } else if (what->getType() == EntryType::Number) {
        return true;
    }
    return !hasError;
}

std::string Type::toString() const {
    std::ostringstream oss;
    if (this->isOptional) {
        oss << "optional ";
    }
    switch (this->type) {
        case EntryType::String: oss << "string"; break;
        case EntryType::Number: oss << "number"; break;
        case EntryType::List: {
            oss << "list[";
            if (this->listType) {
                oss << this->listType->toString();
            } else {
                oss << "any";
            }
            oss << "]";
            break;
        }
        case EntryType::Function: {
            oss << "func";
            if (this->compoundTypes) {
                oss << "(";
                for (size_t i = 0; i < this->compoundTypes->size(); i++) {
                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << this->compoundTypes->at(i).key << ": " << this->compoundTypes->at(i).type->toString();
                }
                oss << ")";
            }
            break;
        }
        case EntryType::Compound: {
            oss << "compound {";
            if (this->compoundTypes) {
                for (size_t i = 0; i < this->compoundTypes->size(); i++) {
                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << this->compoundTypes->at(i).key << ": " << this->compoundTypes->at(i).type->toString();
                }
            }
            oss << "}";
            break;
        } 
        case EntryType::Invalid: return "<invalid>";
        default: return "<unknown>";
    }
    return oss.str();
}

bool Type::operator==(const Type& other) const {
    if (this->type != other.type) return false;
    switch (this->type) {
        case EntryType::List: return (*(this->listType) == (*other.listType));
        case EntryType::Function: [[fallthrough]];
        case EntryType::Compound: {
            if (!this->compoundTypes || !other.compoundTypes) {
                return false;
            }
            if (this->compoundTypes->size() != other.compoundTypes->size()) {
                return false;
            }
            for (size_t i = 0; i < this->compoundTypes->size(); i++) {
                if (this->compoundTypes->at(i).key != other.compoundTypes->at(i).key) {
                    return false;
                }
                if (!this->compoundTypes->at(i).type->operator==(*other.compoundTypes->at(i).type)) {
                    return false;
                }
            }
            return true;
        }
        default: return true;
    }
}

bool Type::operator!=(const Type& other) const {
    return !operator==(other);
}

Type* Type::clone() {
    Type* t = new Type();
    t->type = this->type;
    if (this->listType) t->listType = this->listType->clone();
    if (this->compoundTypes) {
        t->compoundTypes = new std::vector<CompoundType>();
        *t->compoundTypes = *this->compoundTypes;
    }
    return t;
}

bool Type::CompoundType::operator==(const Type::CompoundType& other) const {
    return this->key == other.key && this->type->operator==(*other.type);
}

bool Type::CompoundType::operator!=(const Type::CompoundType& other) const {
    return !operator==(other);
}

Type* Type::String() {
    Type* type = new Type();
    type->type = EntryType::String;
    return type;
}

Type* Type::Number() {
    Type* type = new Type();
    type->type = EntryType::Number;
    return type;
}

Type* Type::List(Type* type) {
    Type* t = new Type();
    t->type = EntryType::List;
    t->listType = type;
    return t;
}

Type* Type::Compound(std::vector<CompoundType> types) {
    Type* type = new Type();
    type->type = EntryType::Compound;
    type->compoundTypes = new std::vector<CompoundType>();
    for (const auto& compoundType : types) {
        type->compoundTypes->push_back(compoundType);
    }
    return type;
}

Type* Type::Function(std::vector<CompoundType> types) {
    Type* type = Type::Compound(types);
    type->type = EntryType::Function;
    return type;
}

Type* Type::Optional(Type* type) {
    Type* t = type->clone();
    t->isOptional = true;
    return t;
}

Type* Type::Any() {
    return new AnyType();
}

bool AnyType::validate(ConfigEntry* what, const std::vector<std::string>& flags, std::ostream& out) {
    return true;
}

AnyType::AnyType() {
    this->type = EntryType::Any;
}

bool Token::operator==(const Token& other) const {
    return this->type == other.type && this->value == other.value;
}

bool Token::operator!=(const Token& other) const {
    return !operator==(other);
}

TypeEntry::TypeEntry() {
    this->setType(EntryType::Type);
}

bool TypeEntry::validate(ConfigEntry* what, const std::vector<std::string>& flags, std::ostream& out) {
    return type->validate(what, flags, out);
}

bool TypeEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::Type) {
        return false;
    }
    const TypeEntry& otherType = (const TypeEntry&) other;
    return this->type->operator==(*otherType.type);
}

bool TypeEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}

void TypeEntry::print(std::ostream& stream, int indent) {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "type " << this->type->type << std::endl;
}

ConfigEntry* TypeEntry::clone() {
    TypeEntry* entry = new TypeEntry();
    entry->setKey(this->getKey());
    entry->type = this->type->clone();
    return ((ConfigEntry*) entry);
}

ListEntry* ConfigParser::parseList(std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    ListEntry* list = new ListEntry();
    if (i >= tokens.size() || tokens[i].type != Token::ListStart) {
        LYNX_ERR << "Invalid list" << std::endl;
        return nullptr;
    }
    i++;
    while (i < tokens.size() && tokens[i].type != Token::ListEnd) {
        ConfigEntry* entry = this->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse value" << std::endl;
            return nullptr;
        }
        if (list->getListType() == EntryType::Invalid) {
            list->setListType(entry->getType());
        } else if (list->getListType() != entry->getType()) {
            LYNX_ERR << "Invalid entry type in list. Expected " << list->getListType() << " but got " << entry->getType() << std::endl;
            return nullptr;
        }
        entry->setKey("");
        list->add(entry);
        i++;
    }
    return list;
}

bool sumEntries(ConfigEntry* finalEntry, ConfigEntry* entry) {
    switch (entry->getType()) {
        case EntryType::String:
            ((StringEntry*) finalEntry)->setValue(((StringEntry*) finalEntry)->getValue() + ((StringEntry*) entry)->getValue());
            break;
        case EntryType::Number:
            ((NumberEntry*) finalEntry)->setValue(((NumberEntry*) finalEntry)->getValue() + ((NumberEntry*) entry)->getValue());
            break;
        case EntryType::List:
            ((ListEntry*) finalEntry)->merge(((ListEntry*) entry));
            break;
        case EntryType::Compound:
            ((CompoundEntry*) finalEntry)->merge(((CompoundEntry*) entry));
            break;
        default:
            LYNX_RT_ERR << "Invalid entry type" << std::endl;
            return false;
    }
    return true;
}

ConfigEntry* ConfigParser::parseValue(std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    auto byPath = [&i, &compoundStack](std::string value, ConfigEntry** pParent) -> ConfigEntry* {
        ConfigEntry* entry = nullptr;
        ConfigEntry* parent = nullptr;
        size_t x = value.find_last_of('.');
        std::string parentPath = value.substr(0, x);
        for (size_t i = compoundStack.size(); i > 0 && !entry; i--) {
            if (x != std::string::npos) parent = compoundStack[i - 1]->getByPath(parentPath);
            else parent = compoundStack[i - 1];
            entry = compoundStack[i - 1]->getByPath(value);
        }
        if (pParent && parent) {
            *pParent = parent->clone();
        }
        if (!entry) {
            return nullptr;
        }
        return entry->clone();
    };
    auto makePath = [](std::vector<Token>& tokens, int& i) -> std::string {
        std::string path = tokens[i].value;
        i++;
        while (i < tokens.size() && tokens[i].type == Token::Dot) {
            path += ".";
            i++;
            if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
                LYNX_ERR << "Invalid path" << std::endl;
                return nullptr;
            }
            path += tokens[i].value;
            i++;
        }
        i--;
        return path;
    };
    if (i >= tokens.size()) {
        return nullptr;
    }
    switch (tokens[i].type) {
        case Token::ListStart: return parseList(tokens, i, compoundStack);
        case Token::CompoundStart: return parseCompound(tokens, i, compoundStack);
        case Token::String: {
            StringEntry* entry = new StringEntry();
            entry->setValue(tokens[i].value);
            return ((ConfigEntry*) entry);
        }
        case Token::Number: {
            NumberEntry* entry = new NumberEntry();
            try {
                entry->setValue(std::stod(tokens[i].value));
            } catch (const std::out_of_range& e) {
                LYNX_ERR << "Failed to parse number: " << e.what() << std::endl;
                return nullptr;
            } catch (const std::invalid_argument& e) {
                LYNX_ERR << "Invalid argument to stod(): " << tokens[i].value << std::endl;
                return nullptr;
            }
            return ((ConfigEntry*) entry);
        }

        case Token::Identifier: {
            auto x = builtins.find(tokens[i].value);
            if (x == builtins.end()) {
                std::string path = makePath(tokens, i);
                if (path.empty()) {
                    return nullptr;
                }
                ConfigEntry* parent = nullptr;
                ConfigEntry* entry;
                auto nativeFunc = nativeFunctions.find(tokens[i].value);
                if (nativeFunc != nativeFunctions.end()) {
                    entry = nativeFunc->second;
                } else {
                    entry = byPath(path, &parent);
                }
                if (!entry) {
                    LYNX_ERR << "Failed to find entry by path '" << path << "'" << std::endl;
                    return nullptr;
                }
                if (entry->getType() == EntryType::Function) {
                    return ((FunctionEntry*) entry)->call(this, compoundStack, tokens, i);
                } else {
                    return ((ConfigEntry*) entry);
                }
            }
            ConfigEntry* entry = x->second(tokens, i, this, compoundStack);
            return ((ConfigEntry*) entry);
        }
        case Token::BlockStart: {
            i++;
            ConfigEntry* finalEntry = parseValue(tokens, i, compoundStack);
            if (!finalEntry) {
                LYNX_ERR << "Failed to parse value" << std::endl;
                return nullptr;
            }
            i++;

            while (i < tokens.size() && tokens[i].type != Token::BlockEnd) {
                ConfigEntry* entry = parseValue(tokens, i, compoundStack);
                i++;
                if (!entry) {
                    LYNX_ERR << "Failed to parse value" << std::endl;
                    return nullptr;
                }
                if (entry->getType() != finalEntry->getType()) {
                    if (entry->getType() == EntryType::List) {
                        if (((ListEntry*) entry)->getListType() != finalEntry->getType()) {
                            LYNX_ERR << "Invalid entry type in list. Expected " << finalEntry->getType() << " but got " << entry->getType() << std::endl;
                            return nullptr;
                        }
                        ListEntry* list = (ListEntry*) entry;
                        for (size_t i = 0; i < list->size(); i++) {
                            if (!sumEntries(finalEntry, list->get(i))) {
                                return nullptr;
                            }
                        }
                    } else if (entry->getType() == EntryType::Number && finalEntry->getType() == EntryType::String) {
                        double value = (((NumberEntry*) entry))->getValue();
                        ((StringEntry*) finalEntry)->setValue(((StringEntry*) finalEntry)->getValue() + std::to_string(value));
                    } else if (finalEntry->getType() == EntryType::Number && entry->getType() == EntryType::String) {
                        std::string value = std::to_string(((NumberEntry*) finalEntry)->getValue());
                        finalEntry = new StringEntry();
                        ((StringEntry*) finalEntry)->setValue(value + ((StringEntry*) entry)->getValue());
                    } else {
                        LYNX_ERR << "Invalid entry type. Expected " << finalEntry->getType() << " but got " << entry->getType() << std::endl;
                        return nullptr;
                    }
                } else if (!sumEntries(finalEntry, entry)) {
                    return nullptr;
                }
            }
            return finalEntry;
        }
        case Token::Dot: {
            return compoundStack.back()->clone();
        }
        default:
            LYNX_ERR << "Invalid token: " << tokens[i].value << std::endl;
            return nullptr;
    }
}

Type* ConfigParser::parseType(std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    auto byPath = [&i, &compoundStack](std::string value) -> ConfigEntry* {
        ConfigEntry* entry = nullptr;
        for (size_t i = compoundStack.size(); i > 0 && !entry; i--) {
            entry = compoundStack[i - 1]->getByPath(value);
        }
        if (!entry) {
            return nullptr;
        }
        return entry->clone();
    };
    auto makePath = [](std::vector<Token>& tokens, int& i) -> std::string {
        std::string path = tokens[i].value;
        i++;
        while (i < tokens.size() && tokens[i].type == Token::Dot) {
            path += ".";
            i++;
            if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
                LYNX_ERR << "Invalid path" << std::endl;
                return nullptr;
            }
            path += tokens[i].value;
            i++;
        }
        i--;
        return path;
    };
    Type* t = new Type();
    if (i >= tokens.size() || tokens[i].type != Token::Identifier) {
        LYNX_ERR << "Invalid type: " << tokens[i].value << std::endl;
        return nullptr;
    }

    if (tokens[i].value == "optional") {
        t->isOptional = true;
        i++;
    }

    if (tokens[i].value == "string") {
        t->type = EntryType::String;
    } else if (tokens[i].value == "number") {
        t->type = EntryType::Number;
    } else if (tokens[i].value == "list") {
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::ListStart) {
            LYNX_ERR << "Expected list start but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        i++;
        t->type = EntryType::List;
        t->listType = parseType(tokens, i, compoundStack);
        if (!t->listType) {
            LYNX_ERR << "Failed to parse list type" << std::endl;
            return nullptr;
        }
        i++;
    } else if (tokens[i].value == "compound") {
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::CompoundStart) {
            LYNX_ERR << "Expected compound start but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        t->type = EntryType::Compound;
        t->compoundTypes = parseCompoundTypes(tokens, i, compoundStack);
    } else {
        std::string path = makePath(tokens, i);
        ConfigEntry* typeEntry = byPath(path);
        if (!typeEntry) {
            LYNX_ERR << "Failed to find entry by path '" << path << "'" << std::endl;
            return nullptr;
        }
        if (typeEntry->getType() != EntryType::Type) {
            LYNX_ERR << "Invalid entry type. Expected Type but got " << typeEntry->getType() << std::endl;
            return nullptr;
        }
        TypeEntry* entry = ((TypeEntry*) typeEntry);
        t = entry->type;
    }
    return t;
}

ConfigEntry* typeToEntry(Type* type) {
    TypeEntry* typeEntry = new TypeEntry();
    typeEntry->type = type;
    return typeEntry;
}

std::vector<Type::CompoundType>* ConfigParser::parseCompoundTypes(std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    std::vector<Type::CompoundType>* compound = new std::vector<Type::CompoundType>();
    if (i >= tokens.size() || tokens[i].type != Token::CompoundStart) {
        LYNX_ERR << "Invalid compound" << std::endl;
        compoundStack.pop_back();
        return nullptr;
    }
    i++;
    while (tokens.size() > 0 && tokens[i].type != Token::CompoundEnd) {
        if (tokens[i].type != Token::Identifier) {
            LYNX_ERR << "Invalid type specification: " << tokens[i].value << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }
        std::string key = tokens[i].value;
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::Is) {
            LYNX_ERR << "Invalid type assignment: " << tokens[i].value << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }
        i++;
        
        Type* type = parseType(tokens, i, compoundStack);
        if (!type) {
            LYNX_ERR << "Failed to parse type for key '" << key << "'" << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }

        Type::CompoundType t;
        t.key = key;
        t.type = type;
        compound->push_back(t);
        i++;
    }
    // i--;
    return compound;
}

CompoundEntry* ConfigParser::parseCompound(std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    CompoundEntry* compound = new CompoundEntry();
    compoundStack.push_back(compound);
    if (i >= tokens.size() || tokens[i].type != Token::CompoundStart) {
        LYNX_ERR << "Invalid compound" << std::endl;
        compoundStack.pop_back();
        return nullptr;
    }
    i++;
    while (i < tokens.size() && tokens[i].type != Token::CompoundEnd) {
        if (tokens[i].type == Token::BlockStart) {
            i++;
            ConfigEntry* entry = parseValue(tokens, i, compoundStack);
            if (!entry) {
                LYNX_ERR << "Failed to parse value for merge" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }
            i++;
            if (i >= tokens.size() || tokens[i].type != Token::BlockEnd) {
                LYNX_ERR << "Invalid block end" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }
            i++;
            continue;
        }
        if (tokens[i].type != Token::Identifier) {
            LYNX_ERR << "Invalid key: " << tokens[i].value << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }
        std::string key = tokens[i].value;
        i++;
        if (i < tokens.size() && tokens[i].type == Token::Is) {
            i++;
            Type* type = parseType(tokens, i, compoundStack);
            if (!type) {
                LYNX_ERR << "Failed to parse type for key '" << key << "'" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }

            ConfigEntry* entry = typeToEntry(type);
            entry->setKey(key);
            ConfigEntry* current = compound->get(key);
            if (current && current->getType() == EntryType::Type) {
                LYNX_ERR << "Type entry already exists for key '" << key << "'" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            } else if (current) {
                LYNX_ERR << "Assigning type to existing entry for key '" << key << "' has no effect" << std::endl;
            }
            compound->add(entry);
            i++;
            if (i >= tokens.size() || tokens[i].type != Token::Assign) {
                continue;
            }
        } else if (i >= tokens.size() || tokens[i].type != Token::Assign) {
            LYNX_ERR << "Invalid assignment: " << tokens[i].value << " in key '" << key << "'" << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }
        i++;
        
        ConfigEntry* entry = parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse value for key '" << key << "'" << std::endl;
            compoundStack.pop_back();
            return nullptr;
        }
        
        entry->setKey(key);
        ConfigEntry* current = compound->get(key);
        if (current && current->getType() == EntryType::Type) {
            TypeEntry* typeEntry = ((TypeEntry*) current);
            if (!typeEntry->validate(entry, {}, std::cerr)) {
                LYNX_ERR << "Invalid entry type for key '" << key << "'" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }
        }
        compound->add(entry);
        i++;
    }
    compoundStack.pop_back();
    return compound;
}
