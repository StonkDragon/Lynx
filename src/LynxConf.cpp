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

#include "LynxConf.hpp"

using namespace std;

ConfigEntry* ConfigEntry::Null = nullptr;

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
        case EntryType::Function:
            out << "Function";
            break;
        case EntryType::Type:
            out << "Type";
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
    return (this->values[index]->getType() == EntryType::String ? reinterpret_cast<StringEntry*>(this->values[index]) : nullptr);
}
CompoundEntry* ListEntry::getCompound(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::Compound ? reinterpret_cast<CompoundEntry*>(this->values[index]) : nullptr);
}
ListEntry* ListEntry::getList(unsigned long index) {
    if (index >= this->length) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::List ? reinterpret_cast<ListEntry*>(this->values[index]) : nullptr);
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
        return reinterpret_cast<StringEntry*>(x);
    }
    return nullptr;
}
StringEntry* CompoundEntry::getStringOrDefault(const std::string& key, const std::string& defaultValue) {
    StringEntry* entry = getString(key);
    if (entry) {
        return entry;
    }
    entry = new StringEntry();
    entry->setValue(defaultValue);
    return entry;
}
ListEntry* CompoundEntry::getList(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::List) {
        return reinterpret_cast<ListEntry*>(x);
    }
    return nullptr;
}
CompoundEntry* CompoundEntry::getCompound(const std::string& key) {
    auto x = get(key);
    if (x && x->getType() == EntryType::Compound) {
        return reinterpret_cast<CompoundEntry*>(x);
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
    add(entry);
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
    add(newEntry);
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
    add(newEntry);
}
void CompoundEntry::addList(const std::string& key, ConfigEntry* value) {
    if (this->hasMember(key)) {
        std::cerr << "List with key '" << key << "' already exists!" << std::endl;
        return;
    }
    ListEntry* newEntry = new ListEntry();
    newEntry->setKey(key);
    newEntry->add(value);
    add(newEntry);
}
void CompoundEntry::addList(ListEntry* value) {
    if (this->hasMember(value->getKey())) {
        std::cerr << "List with key '" << value->getKey() << "' already exists!" << std::endl;
        return;
    }
    add(reinterpret_cast<ConfigEntry*>(value));
}
void CompoundEntry::addCompound(CompoundEntry* value) {
    if (this->hasMember(value->getKey())) {
        std::cerr << "Compound with key '" << value->getKey() << "' already exists!" << std::endl;
        return;
    }
    add(reinterpret_cast<ConfigEntry*>(value));
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

FunctionEntry::FunctionEntry() {
    this->setType(EntryType::Function);
    this->isDotCallable = false;
}

bool FunctionEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != this->getType()) return false;
    FunctionEntry* otherFunc = (FunctionEntry*) &other;
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
    stream << "func ( ";
    for (auto&& arg : this->args) {
        stream << arg.key << " ";
    }
    stream << ")" << std::endl;
}
ConfigEntry* FunctionEntry::clone() {
    FunctionEntry* newFunc = new FunctionEntry();
    newFunc->body = this->body;
    newFunc->args = this->args;
    newFunc->compoundStack = this->compoundStack;
    newFunc->isDotCallable = this->isDotCallable;
    return newFunc;
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
        } else if (c == ':') {
            token.type = Token::Assign;
            token.value = ":";
        } else if (c == '=') {
            token.type = Token::Is;
            token.value = "=";
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
    std::vector<CompoundEntry*> compoundStack;
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
    return entry;
}

ConfigEntry* NumberEntry::clone() {
    NumberEntry* entry = new NumberEntry();
    entry->setKey(this->getKey());
    entry->setValue(this->value);
    return entry;
}

ConfigEntry* ListEntry::clone() {
    ListEntry* entry = new ListEntry();
    entry->setKey(this->getKey());
    entry->setListType(this->listType);
    entry->merge(this);
    return entry;
}

ConfigEntry* CompoundEntry::clone() {
    CompoundEntry* entry = new CompoundEntry();
    entry->setKey(this->getKey());
    entry->merge(this);
    return entry;
}

bool Type::validate(ConfigEntry* what, const std::vector<std::string>& flags, std::ostream& out) {
    bool hasError = false;

    bool optional = std::find(flags.begin(), flags.end(), "optional") != flags.end();
    if (what->getType() != this->type) {
        LYNX_RT_ERR_TO(out) << "Invalid entry type. Expected " << this->type << " but got " << what->getType() << std::endl;
        return false;
    }
    
    if (what->getType() == EntryType::List) {
        ListEntry* list = (ListEntry*) what;
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
        CompoundEntry* compound = (CompoundEntry*) what;
        for (size_t i = 0; i < this->compoundTypes->size(); i++) {
            Type::CompoundType compoundType = this->compoundTypes->at(i);
            if (!compound->hasMember(compoundType.key)) {
                if (optional) {
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
    }
    return !hasError;
}

bool Type::operator==(const Type& other) const {
    if (this->type != other.type) return false;
    switch (this->type) {
        case EntryType::List: return this->listType->operator==(*other.listType);
        case EntryType::Compound: [[fallthrough]];
        case EntryType::Function: {
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
    return entry;
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
        list->add(entry);
        i++;
    }
    return list;
}

extern std::unordered_map<std::string, Command> commands;

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
            return entry;
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
            return entry;
        }

        case Token::Identifier: {
            auto x = commands.find(tokens[i].value);
            if (x == commands.end()) {
                std::string path = makePath(tokens, i);
                if (path.empty()) {
                    return nullptr;
                }
                ConfigEntry* parent = nullptr;
                ConfigEntry* entry = byPath(path, &parent);
                if (!entry) {
                    LYNX_ERR << "Failed to find entry by path '" << path << "'" << std::endl;
                    return nullptr;
                }
                if (entry->getType() == EntryType::Function) {
                    FunctionEntry* func = (FunctionEntry*) entry;
                    int tmp = 0;
                    CompoundEntry* args = new CompoundEntry();
                    if (!parent) {
                        LYNX_ERR << "Unable to find parent of '" << path << "'" << std::endl;
                        return nullptr;
                    }
                    bool isMethod = func->isDotCallable && func->args.size() != 0;
                    if (isMethod) {
                        std::stringstream sink;
                        if (!func->args.front().type->validate(parent, {}, sink)) {
                            isMethod = false;
                        } else {
                            parent->setKey(func->args.front().key);
                            args->add(parent);
                        }
                    }
                    for (size_t n = (size_t) isMethod; n < func->args.size(); n++) {
                        i++;
                        ConfigEntry* arg = parseValue(tokens, i, compoundStack);
                        if (!arg) {
                            LYNX_ERR << "Failed to parse argument '" << func->args[n].key << "'" << std::endl;
                            return nullptr;
                        }
                        if (!func->args[n].type->validate(arg, {})) {
                            LYNX_ERR << "Invalid argument type" << std::endl;
                            return nullptr;
                        }
                        arg->setKey(func->args[n].key);
                        args->add(arg);
                    }
                    auto stack = func->compoundStack;
                    stack.push_back(args);
                    int x = 0;
                    ConfigEntry* result = this->parseValue(func->body, x, stack);
                    return result;
                }
                return entry;
            }
            ConfigEntry* entry = x->second(tokens, i, this, compoundStack);
            return entry;
        }
        case Token::BlockStart: {
            i++;
            ConfigEntry* finalEntry = parseValue(tokens, i, compoundStack);
            if (!finalEntry) {
                LYNX_ERR << "Failed to parse value" << std::endl;
                return nullptr;
            }
            i++;

            bool add(ConfigEntry* finalEntry, ConfigEntry* entry);
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
                            if (!add(finalEntry, list->get(i))) {
                                return nullptr;
                            }
                        }
                    } else if (entry->getType() == EntryType::Number && finalEntry->getType() == EntryType::String) {
                        double value = ((NumberEntry*) entry)->getValue();
                        ((StringEntry*) finalEntry)->setValue(((StringEntry*) finalEntry)->getValue() + std::to_string(value));
                    } else if (finalEntry->getType() == EntryType::Number && entry->getType() == EntryType::String) {
                        std::string value = std::to_string(((NumberEntry*) finalEntry)->getValue());
                        finalEntry = new StringEntry();
                        ((StringEntry*) finalEntry)->setValue(value + ((StringEntry*) entry)->getValue());
                    } else {
                        LYNX_ERR << "Invalid entry type. Expected " << finalEntry->getType() << " but got " << entry->getType() << std::endl;
                        return nullptr;
                    }
                } else if (!add(finalEntry, entry)) {
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
    } else if (tokens[i].value == "func") {
        i++;
        if (i >= tokens.size() || tokens[i].type != Token::BlockStart) {
            LYNX_ERR << "Expected arguments but got " << tokens[i].value << std::endl;
            return nullptr;
        }
        t->type = EntryType::Function;
        i++;
        t->compoundTypes = new std::vector<Type::CompoundType>();
        if (i < tokens.size() && tokens[i].type != Token::BlockEnd) {
            int blockDepth = 1;
            while (i < tokens.size() && tokens[i].type != Token::BlockEnd) {
                if (tokens[i].type != Token::Identifier) {
                    LYNX_ERR << "Expected Identifier but got " << tokens[i].value << std::endl;
                    return nullptr;
                }
                std::string key = tokens[i].value;
                i++;
                if (i >= tokens.size() || tokens[i].type != Token::Is) {
                    LYNX_ERR << "Expected type assignment but got " << tokens[i].value << std::endl;
                    return nullptr;
                }
                i++;
                Type* t = parseType(tokens, i, compoundStack);
                t->compoundTypes->push_back({key, t});
                i++;
            }
        }
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
        TypeEntry* entry = (TypeEntry*) typeEntry;
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
        if (tokens[i].type == Token::Assign) {
            i++;
            ConfigEntry* entry = parseValue(tokens, i, compoundStack);
            if (!entry) {
                LYNX_ERR << "Failed to parse value for merge" << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }
            if (entry->getType() != EntryType::Compound) {
                LYNX_ERR << "Expected Compound for merge but got " << entry->getType() << std::endl;
                compoundStack.pop_back();
                return nullptr;
            }
            compound->merge((CompoundEntry*) entry);
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
            compound->add(entry);
            i++;
            continue;
        }
        if (i >= tokens.size() || tokens[i].type != Token::Assign) {
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
        compound->add(entry);
        i++;
    }
    compoundStack.pop_back();
    return compound;
}
