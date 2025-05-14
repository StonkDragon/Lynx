#include <LynxConf.hpp>

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

ConfigEntry* CompoundEntry::clone() {
    CompoundEntry* entry = new CompoundEntry();
    entry->setKey(this->getKey());
    entry->merge(this);
    return ((ConfigEntry*) entry);
}
