#include <iostream>
#include <string>
#include <sstream>

#include <LynxConf.hpp>

#pragma region FunctionEntry
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

ConfigEntry* FunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = this->parseArgs(parser, tokens, i, compoundStack);
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

CompoundEntry* FunctionEntry::parseArgs(ConfigParser* parser, std::vector<Token>& tokens, int& i, std::vector<CompoundEntry*>& compoundStack) {
    CompoundEntry* args = new CompoundEntry();
    for (size_t n = 0; n < this->args.size(); n++) {
        i++;
        std::string key = this->args[n].key;
        Type* type = this->args[n].type;
        if (tokens[i].type == Token::Assign) {
            i++;
            key = tokens[i].value;
            bool found = false;
            for (size_t j = 0; j < this->args.size(); j++) {
                if (this->args[j].key == key) {
                    type = this->args[j].type;
                    found = true;
                    break;
                }
            }
            if (!found) {
                LYNX_ERR << "Invalid argument name '" << key << "'" << std::endl;
                return nullptr;
            }
            i++;
        }
        ConfigEntry* arg = parser->parseValue(tokens, i, compoundStack);
        if (!arg) {
            LYNX_ERR << "Failed to parse argument '" << key << "'" << std::endl;
            return nullptr;
        }
        arg = arg->clone();
        if (!type->validate(arg, {})) {
            LYNX_ERR << "Invalid argument type" << std::endl;
            return nullptr;
        }
        arg->setKey(key);
        args->add(arg);
    }
    return args;
}
#pragma endregion

#pragma region DeclaredFunctionEntry
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

ConfigEntry* DeclaredFunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = this->parseArgs(parser, tokens, i, compoundStack);
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
#pragma endregion

#pragma region NativeFunctionEntry
ConfigEntry* NativeFunctionEntry::call(ConfigParser* parser, std::vector<CompoundEntry*>& compoundStack, std::vector<Token>& tokens, int& i) {
    int tmp = 0;
    CompoundEntry* args = this->parseArgs(parser, tokens, i, compoundStack);
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
#pragma endregion
