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

static ConfigEntry* typeToEntry(Type* type) {
    TypeEntry* typeEntry = new TypeEntry();
    typeEntry->type = type;
    return typeEntry;
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

extern std::unordered_map<std::string, BuiltinCommand> builtins;
extern std::unordered_map<std::string, NativeFunctionEntry*> nativeFunctions;

ConfigEntry* ConfigEntry::Null = nullptr;

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
    } else if (tokens[i].value == "any") {
        t->type = EntryType::Any;
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
