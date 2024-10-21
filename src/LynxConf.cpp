#if defined(_WIN32)
#define OS_NAME "windows"
#elif defined(__APPLE__)
#define OS_NAME "macos"
#elif defined(__linux__)
#define OS_NAME "linux"
#else
#define OS_NAME "unknown"
#endif

#include "LynxConf.hpp"

using namespace std;

#define LYNX_LOG std::cout << "[Lynx Config] "
#define LYNX_ERR std::cerr << "[Lynx Config] "

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
            LYNX_ERR << "Failed to reserve " << this->capacity << " elements" << std::endl;
            LYNX_ERR << e.what() << std::endl;
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
            LYNX_ERR << "Failed to reserve " << this->capacity << " elements" << std::endl;
            LYNX_ERR << e.what() << std::endl;
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
        if (buf[i] == '#') {
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
    CompoundEntry* rootEntry = parseCompound(config, i);
    rootEntry->setKey(".root");
    return rootEntry;
}

bool ConfigParser::isValidIdentifier(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static bool isnumber(char c) {
    return (c >= '0' && c <= '9');
}

#define skipWhitespace() while (i < data.size() && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) { i++; }
#define next() ({ i++; if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return nullptr; } data[i]; })
#define nextChar() ({ i++; skipWhitespace(); if (i >= data.size()) { LYNX_ERR << "Unexpected end of file" << std::endl; return nullptr; } data[i]; })
#define peek() ({ skipWhitespace(); data[i]; })

ListEntry* ConfigParser::parseList(std::string& data, int& i) {
    ListEntry* list = new ListEntry();
    if (data[i] != '[') {
        LYNX_ERR << "Invalid list" << std::endl;
        return nullptr;
    }
    char c = nextChar();
    while (c != ']') {
        if (c == '[') {
            if (list->getListType() != EntryType::Invalid && list->getListType() != EntryType::List) {
                LYNX_ERR << "Invalid element in list, expected list" << std::endl;
            }
            list->setListType(EntryType::List);
            list->add(this->parseList(data, i));
        } else if (c == '{') {
            if (list->getListType() != EntryType::Invalid && list->getListType() != EntryType::Compound) {
                LYNX_ERR << "Invalid element in list, expected compound" << std::endl;
            }
            list->setListType(EntryType::Compound);
            list->add(this->parseCompound(data, i));
        } else if (c == '"') {
            if (list->getListType() != EntryType::Invalid && list->getListType() != EntryType::String) {
                LYNX_ERR << "Invalid element in list, expected string" << std::endl;
            }
            list->setListType(EntryType::String);
            list->add(this->parseString(data, i));
        } else if (isnumber(c)) {
            if (list->getListType() != EntryType::Invalid && list->getListType() != EntryType::Number) {
                LYNX_ERR << "Invalid element in list, expected number" << std::endl;
            }
            list->setListType(EntryType::Number);
            list->add(this->parseNumber(data, i));
        } else {
            LYNX_ERR << "Invalid entry in list: " << data.substr(i) << std::endl;
            return nullptr;
        }
        c = nextChar();
    }
    return list;
}

StringEntry* ConfigParser::parseString(std::string& data, int& i) {
    std::string value = "";
    if (data[i] != '"') {
        LYNX_ERR << "Invalid string" << std::endl;
        return nullptr;
    }
    char c = next();
    bool escaped = false;
    while (true) {
        if (c == '"' && !escaped) break;
        if (c == '\\') {
            escaped = !escaped;
        } else {
            escaped = false;
        }
        value += c;
        c = next();
    }
    StringEntry* entry = new StringEntry();
    entry->setValue(value);
    return entry;
}

NumberEntry* ConfigParser::parseNumber(std::string& data, int& i) {
    std::string value = "";
    char c = peek();
    while (isnumber(c) || c == '.') {
        value += c;
        c = next();
    }
    NumberEntry* entry = new NumberEntry();
    entry->setValue(std::stod(value));
    return entry;
}

CompoundEntry* ConfigParser::parseCompound(std::string& data, int& i) {
    CompoundEntry* compound = new CompoundEntry();
    if (data[i] != '{') {
        LYNX_ERR << "Invalid compound" << std::endl;
        return nullptr;
    }
    char c = nextChar();
    bool lastConditionWasTrue = false;
    while (c != '}') {
        skipWhitespace();
        std::string key = "";
        while (isValidIdentifier(c)) {
            key += c;
            c = next();
        }
        if (c != ':') {
            c = nextChar();
        }
        if (c != ':') {
            LYNX_ERR << "Missing value: '" << key << "'" << std::endl;
            return nullptr;
        }
        c = nextChar();
        if (c == '[') {
            ListEntry* entry = this->parseList(data, i);
            if (!entry) {
                return nullptr;
            }
            entry->setKey(key);
            compound->add(entry);
        } else if (c == '{') {
            CompoundEntry* entry = parseCompound(data, i);
            if (!entry) {
                return nullptr;
            }
            entry->setKey(key);
            compound->add(entry);
        } else if (c == '"') {
            StringEntry* entry = this->parseString(data, i);
            if (!entry) {
                return nullptr;
            }
            entry->setKey(key);
            compound->add(entry);
        } else if (isnumber(c)) {
            NumberEntry* entry = parseNumber(data, i);
            if (!entry) {
                return nullptr;
            }
            entry->setKey(key);
            compound->add(entry);
        } else {
            LYNX_ERR << "Invalid entry: '" << key << "'" << std::endl;
            return nullptr;
        }
        c = nextChar();
    }
    return compound;
}
