#include <LynxConf.hpp>

CompoundEntry::CompoundEntry() {
    this->setType(EntryType::Compound);
    this->entriesMap = {};
}

bool CompoundEntry::hasMember(const std::string& key) const {
    return this->entriesMap.find(key) != this->entriesMap.end();
}

StringEntry* CompoundEntry::getString(const std::string& key) const {
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

NumberEntry* CompoundEntry::getNumber(const std::string& key) const {
    auto x = get(key);
    if (x && x->getType() == EntryType::Number) {
        return ((NumberEntry*) x);
    }
    return nullptr;
}

ListEntry* CompoundEntry::getList(const std::string& key) const {
    auto x = get(key);
    if (x && x->getType() == EntryType::List) {
        return ((ListEntry*) x);
    }
    return nullptr;
}

CompoundEntry* CompoundEntry::getCompound(const std::string& key) const {
    auto x = get(key);
    if (x && x->getType() == EntryType::Compound) {
        return ((CompoundEntry*) x);
    }
    return nullptr;
}

ConfigEntry* CompoundEntry::get(const std::string& key) const {
    auto it = this->entriesMap.find(key);
    if (it != this->entriesMap.end()) {
        return it->second;
    }
    return ConfigEntry::Null;
}

ConfigEntry* CompoundEntry::getByPath(const std::string& path) const {
    std::string key = "";
    const CompoundEntry* current = this;
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
    return this->entriesMap[key];
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
    this->entriesMap[entry->getKey()] = entry;
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
    for (auto& entry : other->entriesMap) {
        this->add(entry.second);
        count++;
    }
    return count;
}

void CompoundEntry::remove(const std::string& key) {
    auto it = this->entriesMap.find(key);
    if (it != this->entriesMap.end()) {
        this->entriesMap.erase(it);
    } else {
        std::cerr << "Entry with key '" << key << "' not found!" << std::endl;
    }
}

void CompoundEntry::removeAll() {
    this->entriesMap.clear();
}

bool CompoundEntry::isEmpty() const {
    return this->entriesMap.empty();
}

bool CompoundEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::Compound) {
        return false;
    }
    const CompoundEntry& otherCompound = (const CompoundEntry&) other;
    if (this->entriesMap.size() != otherCompound.entriesMap.size()) {
        return false;
    }
    for (const auto& entry : this->entriesMap) {
        auto it = otherCompound.entriesMap.find(entry.first);
        if (it == otherCompound.entriesMap.end()) {
            return false;
        }
        if (!entry.second->operator==(*(it->second))) {
            return false;
        }
    }
    for (const auto& entry : otherCompound.entriesMap) {
        auto it = this->entriesMap.find(entry.first);
        if (it == this->entriesMap.end()) {
            return false;
        }
        if (!entry.second->operator==(*(it->second))) {
            return false;
        }
    }
    return true;
}

bool CompoundEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}

void CompoundEntry::print(std::ostream& stream, int indent) const {
    if (this->getKey() == ".root") {
        for (auto& entry : this->entriesMap) {
            entry.second->print(stream, indent);
        }
    } else {
        stream << std::string(indent, ' ');
        if (this->getKey().size()) {
            stream << this->getKey() << ": ";
        } 
        stream << "{" << std::endl;
        indent += 2;
        for (auto& entry : this->entriesMap) {
            entry.second->print(stream, indent);
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
