#include <iostream>
#include <unordered_map>
#include <cmath>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define popen _popen
#define pclose _pclose
#endif

#include <LynxConf.hpp>
#include <Lynx.hpp>

#if !defined(LYNX_VERSION)
#define LYNX_VERSION "0.0.1"
#endif

ConfigEntry* byPath(std::string value, std::vector<CompoundEntry*>& compoundStack) {
    ConfigEntry* entry = nullptr;
    for (size_t i = compoundStack.size(); i > 0 && !entry; i--) {
        entry = compoundStack[i - 1]->getByPath(value);
    }
    if (!entry) {
        return nullptr;
    }
    return entry->clone();
}

std::string makePath(std::vector<Token>& tokens, int& i) {
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
}

bool add(ConfigEntry* finalEntry, ConfigEntry* entry) {
    switch (entry->getType()) {
        case EntryType::String:
            ((StringEntry*) finalEntry)->setValue(((StringEntry*) finalEntry)->getValue() + ((StringEntry*) entry)->getValue());
            break;
        case EntryType::Number:
            ((NumberEntry*) finalEntry)->setValue(((NumberEntry*) finalEntry)->getValue() + ((NumberEntry*) entry)->getValue());
            break;
        case EntryType::List:
            ((ListEntry*) finalEntry)->merge((ListEntry*) entry);
            break;
        case EntryType::Compound:
            ((CompoundEntry*) finalEntry)->merge((CompoundEntry*) entry);
            break;
        default:
            LYNX_RT_ERR << "Invalid entry type" << std::endl;
            return false;
    }
    return true;
}

std::unordered_map<std::string, Command> commands {
    std::pair("func", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        FunctionEntry* entry = new FunctionEntry();
        i++;
        if (i >= tokens.size()) {
            LYNX_ERR << "Expected block start or type but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        if (tokens[i].type != Token::BlockStart) {
            Type* t = parser->parseType(tokens, i, compoundStack);
            if (!t) {
                LYNX_ERR << "Failed to parse type" << std::endl;
                return nullptr;
            }
            entry->args.push_back({"self", t});
            entry->isDotCallable = true;
            i++;
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
                LYNX_ERR << "Expected '=' but got " << tokens[i].value << std::endl;
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
        return entry;
    }),
    std::pair("true", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        NumberEntry* entry = new NumberEntry();
        entry->setValue(1);
        return entry;
    }),
    std::pair("false", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        NumberEntry* entry = new NumberEntry();
        entry->setValue(0);
        return entry;
    }),
    std::pair("runshell", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* result = parser->parseValue(tokens, i, compoundStack);
        if (!result) {
            LYNX_ERR << "Failed to parse runshell block" << std::endl;
            return nullptr;
        }
        if (result->getType() != EntryType::String) {
            LYNX_ERR << "Invalid entry type in runshell block. Expected String but got " << result->getType() << std::endl;
            return nullptr;
        }
        std::string command = ((StringEntry*) result)->getValue();
        std::string output = "";
        FILE* fp = popen(command.c_str(), "r");
        if (!fp) {
            LYNX_ERR << "Failed to run shell command" << std::endl;
            return nullptr;
        }
        char buf[1024];
        while (fgets(buf, 1024, fp)) {
            output += buf;
        }
        int res = pclose(fp);
        if (res != 0) {
            LYNX_ERR << "Failed to run shell command" << std::endl;
            return nullptr;
        }
        StringEntry* entry = new StringEntry();
        entry->setValue(output);
        return entry;
    }),
    std::pair("print", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* result = parser->parseValue(tokens, i, compoundStack);
        if (!result) {
            LYNX_ERR << "Failed to parse print block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cout << ((StringEntry*) result)->getValue(); break;
            case EntryType::Number:
                try {
                    std::cout << std::to_string(((NumberEntry*) result)->getValue());
                } catch(const std::out_of_range& e) {
                    std::cerr << "Number is too large to print" << std::endl;
                }
                break;
            case EntryType::List: result->print(std::cout); break;
            case EntryType::Compound: result->print(std::cout); break;
            default:
                LYNX_ERR << "Invalid entry type in print block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        return new StringEntry();
    }),
    std::pair("use", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* result = parser->parseValue(tokens, i, compoundStack);
        if (!result) {
            LYNX_ERR << "Failed to parse use block" << std::endl;
            return nullptr;
        }
        if (result->getType() != EntryType::String) {
            LYNX_ERR << "Invalid entry type in use block. Expected String but got " << result->getType() << std::endl;
            return nullptr;
        }
        std::string file = ((StringEntry*) result)->getValue();
        CompoundEntry* entry = parser->parse(file);
        if (!entry) {
            LYNX_ERR << "Failed to parse file '" << file << "'" << std::endl;
            return nullptr;
        }
        return entry;
    }),
    std::pair("printLn", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* result = parser->parseValue(tokens, i, compoundStack);
        if (!result) {
            LYNX_ERR << "Failed to parse printLn block" << std::endl;
            return nullptr;
        }
        switch (result->getType()) {
            case EntryType::String: std::cout << ((StringEntry*) result)->getValue() << std::endl; break;
            case EntryType::Number:
                try {
                    std::cout << std::to_string(((NumberEntry*) result)->getValue()) << std::endl;
                } catch(const std::out_of_range& e) {
                    std::cerr << "Number is too large to print" << std::endl;
                }
                break;
            case EntryType::List: result->print(std::cout); break;
            case EntryType::Compound: result->print(std::cout); break;
            default:
                LYNX_ERR << "Invalid entry type in printLn block. Expected String or Number but got " << result->getType() << std::endl;
                return nullptr;
        }
        return new StringEntry();
    }),
    std::pair("readLn", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        std::string line;
        std::getline(std::cin, line);
        StringEntry* entry = new StringEntry();
        entry->setValue(line);
        return entry;
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
        ListEntry* list = (ListEntry*) entry;
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
            value->setKey(iterVar);
            compound = new CompoundEntry();
            compoundStack.push_back(compound);
            compound->add(value);
            int newI = 0;
            ConfigEntry* next = parser->parseValue(forBody, newI, compoundStack);
            if (!next) {
                LYNX_ERR << "Failed to parse for loop block" << std::endl;
                return nullptr;
            }
            compoundStack.pop_back();
            if (!result) {
                result = next;
            } else if (next->getType() != result->getType()) {
                LYNX_ERR << "Invalid entry type in for loop block. Expected " << result->getType() << " but got " << next->getType() << std::endl;
                return nullptr;
            } else {
                if (!add(result, next)) {
                    return nullptr;
                }
            }
        }
        if (!result) {
            return new StringEntry();
        }
        return result;
    }),
    std::pair("eq", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack);
        if (!entryA || !entryB) {
            LYNX_ERR << "Failed to parse eq block" << std::endl;
            return nullptr;
        }
        if (entryA->getType() != entryB->getType()) {
            LYNX_ERR << "Invalid entry type in eq block. Expected " << entryA->getType() << " but got " << entryB->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(entryA->operator==(*entryB) ? 1 : 0);
        return result;
    }),
    std::pair("string-length", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse string-length block" << std::endl;
            return nullptr;
        }
        if (entry->getType() != EntryType::String) {
            LYNX_ERR << "Invalid entry type in string-length block. Expected String but got " << entry->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(((StringEntry*) entry)->getValue().length());
        return result;
    }),
    std::pair("string-substring", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* str = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* start = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* end = parser->parseValue(tokens, i, compoundStack);
        if (!str || !start || !end) {
            LYNX_ERR << "Failed to parse string-substring block" << std::endl;
            return nullptr;
        }
        if (str->getType() != EntryType::String) {
            LYNX_ERR << "Invalid entry type in string-substring block. Expected String but got " << str->getType() << std::endl;
            return nullptr;
        }
        if (start->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in string-substring block. Expected Number but got " << start->getType() << std::endl;
            return nullptr;
        }
        if (end->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in string-substring block. Expected Number but got " << end->getType() << std::endl;
            return nullptr;
        }
        std::string value = ((StringEntry*) str)->getValue();
        long long startValue = ((NumberEntry*) start)->getValue();
        long long endValue = ((NumberEntry*) end)->getValue();
        if (startValue < 0 || startValue >= value.size() || endValue < 0 || endValue >= value.size()) {
            LYNX_ERR << "Invalid start or end value in string-substring block" << std::endl;
            return nullptr;
        }
        StringEntry* result = new StringEntry();
        result->setValue(value.substr(startValue, endValue));
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
        i++;
        std::string path = makePath(tokens, i);
        ConfigEntry* entry = byPath(path, compoundStack);
        
        bool condition = entry != nullptr;
        NumberEntry* result = new NumberEntry();
        result->setValue(condition ? 1 : 0);
        return result;
    }),
    std::pair("validate", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        std::string path = makePath(tokens, i);
        ConfigEntry* entry = byPath(path, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to find entry '" << path << "'" << std::endl;
            return nullptr;
        }
        i++;
        std::vector<std::string> flags;
        if (i < tokens.size() && tokens[i].type == Token::BlockStart) {
            i++;
            while (i < tokens.size() && tokens[i].type != Token::BlockEnd) {
                if (tokens[i].type != Token::Identifier) {
                    LYNX_ERR << "Invalid flag: " << tokens[i].value << std::endl;
                    return nullptr;
                }
                flags.push_back(tokens[i].value);
                i++;
            }
            i++;
        }
        ConfigEntry* typeEntry = parser->parseValue(tokens, i, compoundStack);
        if (!typeEntry) {
            LYNX_ERR << "Failed to parse type" << std::endl;
            return nullptr;
        }
        if (typeEntry->getType() != EntryType::Type) {
            LYNX_ERR << "Invalid entry type in validate block. Expected Type but got " << typeEntry->getType() << std::endl;
            return nullptr;
        }
        Type* type = ((TypeEntry*) typeEntry)->type;
        NumberEntry* result = new NumberEntry();
        result->setValue(type->validate(entry, flags) == true);
        return result;
    }),

#define BINARY_OP(_name, _op) \
    std::pair(_name, [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* { \
        i++; \
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack); \
        i++; \
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack); \
        if (!entryA || !entryB) { \
            LYNX_ERR << "Failed to parse " _name " block" << std::endl; \
            return nullptr; \
        } \
        if (entryA->getType() != EntryType::Number) { \
            LYNX_ERR << "Invalid entry type in " _name " block. Expected Number but got " << entryA->getType() << std::endl; \
            return nullptr; \
        } \
        if (entryB->getType() != EntryType::Number) { \
            LYNX_ERR << "Invalid entry type in " _name " block. Expected Number but got " << entryB->getType() << std::endl; \
            return nullptr; \
        } \
        NumberEntry* result = new NumberEntry(); \
        result->setValue(((NumberEntry*) entryA)->getValue() _op ((NumberEntry*) entryB)->getValue()); \
        return result; \
    }),

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
    std::pair(_name, [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* { \
        i++; \
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack); \
        if (!entry) { \
            LYNX_ERR << "Failed to parse " _name " block" << std::endl; \
            return nullptr; \
        } \
        if (entry->getType() != EntryType::Number) { \
            LYNX_ERR << "Invalid entry type in " _name " block. Expected Number but got " << entry->getType() << std::endl; \
            return nullptr; \
        } \
        NumberEntry* result = new NumberEntry(); \
        result->setValue(_op ((NumberEntry*) entry)->getValue()); \
        return result; \
    }),

    UNARY_OP("not", !)
    
#undef UNARY_OP
    std::pair("mod", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack);
        if (!entryA || !entryB) {
            LYNX_ERR << "Failed to parse modulo block" << std::endl;
            return nullptr;
        }
        if (entryA->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in modulo block. Expected Number but got " << entryA->getType() << std::endl;
            return nullptr;
        }
        if (entryB->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in modulo block. Expected Number but got " << entryB->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(std::fmod(((NumberEntry*) entryA)->getValue(), ((NumberEntry*) entryB)->getValue()));
        return result;
    }),
    std::pair("shl", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack);
        if (!entryA || !entryB) {
            LYNX_ERR << "Failed to parse left shift block" << std::endl;
            return nullptr;
        }
        if (entryA->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in left shift block. Expected Number but got " << entryA->getType() << std::endl;
            return nullptr;
        }
        if (entryB->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in left shift block. Expected Number but got " << entryB->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue((long long) ((NumberEntry*) entryA)->getValue() << (long long) ((NumberEntry*) entryB)->getValue());
        return result;
    }),
    std::pair("shr", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack);
        if (!entryA || !entryB) {
            LYNX_ERR << "Failed to parse right shift block" << std::endl;
            return nullptr;
        }
        if (entryA->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in right shift block. Expected Number but got " << entryA->getType() << std::endl;
            return nullptr;
        }
        if (entryB->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in right shift block. Expected Number but got " << entryB->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue((long long) ((NumberEntry*) entryA)->getValue() >> (long long) ((NumberEntry*) entryB)->getValue());
        return result;
    }),
    std::pair("range", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entryA = parser->parseValue(tokens, i, compoundStack);
        i++;
        ConfigEntry* entryB = parser->parseValue(tokens, i, compoundStack);
        if (!entryA || !entryB) {
            LYNX_ERR << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        if (entryA->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in range block. Expected Number but got " << entryA->getType() << std::endl;
            return nullptr;
        }
        if (entryB->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in range block. Expected Number but got " << entryB->getType() << std::endl;
            return nullptr;
        }
        ListEntry* result = new ListEntry();
        long long start = ((NumberEntry*) entryA)->getValue();
        long long end = ((NumberEntry*) entryB)->getValue();
        for (long long i = start; i < end; i++) {
            NumberEntry* entry = new NumberEntry();
            entry->setValue(i);
            result->add(entry);
        }
        return result;
    }),
    std::pair("len", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* list = parser->parseValue(tokens, i, compoundStack);
        if (!list) {
            LYNX_ERR << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        if (list->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type in len block. Expected List but got " << list->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(((ListEntry*) list)->size());
        return result;
    }),
    std::pair("get", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* list = parser->parseValue(tokens, i, compoundStack);
        if (!list) {
            LYNX_ERR << "Failed to parse list" << std::endl;
            return nullptr;
        }
        if (list->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type in get. Expected List but got " << list->getType() << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* index = parser->parseValue(tokens, i, compoundStack);
        if (!index) {
            LYNX_ERR << "Failed to parse index" << std::endl;
            return nullptr;
        }
        if (index->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in get. Expected Number but got " << index->getType() << std::endl;
            return nullptr;
        }
        ListEntry* listEntry = (ListEntry*) list;
        long long idx = ((NumberEntry*) index)->getValue();
        if (idx < 0 || idx >= listEntry->size()) {
            LYNX_ERR << "Index out of bounds" << std::endl;
            return nullptr;
        }
        return listEntry->get(idx)->clone();
    }),
    std::pair("set", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* list = parser->parseValue(tokens, i, compoundStack);
        if (!list) {
            LYNX_ERR << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        if (list->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type in len block. Expected List but got " << list->getType() << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* index = parser->parseValue(tokens, i, compoundStack);
        if (!index) {
            LYNX_ERR << "Failed to parse index" << std::endl;
            return nullptr;
        }
        if (index->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in get. Expected Number but got " << index->getType() << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* value = parser->parseValue(tokens, i, compoundStack);
        if (!value) {
            LYNX_ERR << "Failed to parse value" << std::endl;
            return nullptr;
        }
        ListEntry* listEntry = (ListEntry*) list;
        if (value->getType() != listEntry->getListType()) {
            LYNX_ERR << "Invalid entry type in set. Expected " << listEntry->getListType() << " but got " << value->getType() << std::endl;
            return nullptr;
        }
        long long idx = ((NumberEntry*) index)->getValue();
        if (idx < 0 || idx >= listEntry->size()) {
            LYNX_ERR << "Index out of bounds" << std::endl;
            return nullptr;
        }
        listEntry->get(idx) = value->clone();
        return new StringEntry();
    }),
    std::pair("append", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* list = parser->parseValue(tokens, i, compoundStack);
        if (!list) {
            LYNX_ERR << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        if (list->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type in len block. Expected List but got " << list->getType() << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* value = parser->parseValue(tokens, i, compoundStack);
        if (!value) {
            LYNX_ERR << "Failed to parse value" << std::endl;
            return nullptr;
        }
        ListEntry* listEntry = (ListEntry*) list;
        if (value->getType() != listEntry->getListType()) {
            LYNX_ERR << "Invalid entry type in append. Expected " << listEntry->getListType() << " but got " << value->getType() << std::endl;
            return nullptr;
        }
        listEntry->add(value->clone());
        return new StringEntry();
    }),
    std::pair("remove", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* list = parser->parseValue(tokens, i, compoundStack);
        if (!list) {
            LYNX_ERR << "Failed to parse range block" << std::endl;
            return nullptr;
        }
        if (list->getType() != EntryType::List) {
            LYNX_ERR << "Invalid entry type in len block. Expected List but got " << list->getType() << std::endl;
            return nullptr;
        }
        i++;
        ConfigEntry* index = parser->parseValue(tokens, i, compoundStack);
        if (!index) {
            LYNX_ERR << "Failed to parse index" << std::endl;
            return nullptr;
        }
        if (index->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in get. Expected Number but got " << index->getType() << std::endl;
            return nullptr;
        }
        ListEntry* listEntry = (ListEntry*) list;
        long long idx = ((NumberEntry*) index)->getValue();
        if (idx < 0 || idx >= listEntry->size()) {
            LYNX_ERR << "Index out of bounds" << std::endl;
            return nullptr;
        }
        listEntry->remove(idx);
        return new StringEntry();
    }),
    std::pair("inc", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse inc block" << std::endl;
            return nullptr;
        }
        if (entry->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in inc block. Expected Number but got " << entry->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(((NumberEntry*) entry)->getValue() + 1);
        return result;
    }),
    std::pair("dec", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse dec block" << std::endl;
            return nullptr;
        }
        if (entry->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in dec block. Expected Number but got " << entry->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* result = new NumberEntry();
        result->setValue(((NumberEntry*) entry)->getValue() - 1);
        return result;
    }),
    std::pair("exit", [](std::vector<Token> &tokens, int &i, ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack) -> ConfigEntry* {
        i++;
        ConfigEntry* entry = parser->parseValue(tokens, i, compoundStack);
        if (!entry) {
            LYNX_ERR << "Failed to parse exit block" << std::endl;
            return nullptr;
        }
        if (entry->getType() != EntryType::Number) {
            LYNX_ERR << "Invalid entry type in exit block. Expected Number but got " << entry->getType() << std::endl;
            return nullptr;
        }
        NumberEntry* number = (NumberEntry*) entry;
        std::exit(number->getValue());
    }),
};

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
    }
    for (int i = 1; i < argc; i++) {
        std::string file = argv[i];
        ConfigParser().parse(file);
    }
    return 0;
}
