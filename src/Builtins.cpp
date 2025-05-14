#include <LynxConf.hpp>

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
                bool sumEntries(ConfigEntry* finalEntry, ConfigEntry* entry);
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
