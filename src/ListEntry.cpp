#include <LynxConf.hpp>

ListEntry::ListEntry() {
    this->setType(EntryType::List);
    this->values = {};
}

ConfigEntry* ListEntry::get(unsigned long index) const {
    if (index >= this->size()) {
        std::cerr << "Index out of bounds" << std::endl;
        return ConfigEntry::Null;
    }
    return this->values.at(index);
}

ConfigEntry*& ListEntry::operator[](unsigned long index) {
    return this->values[index];
}

StringEntry* ListEntry::getString(unsigned long index) const {
    if (index >= this->size()) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::String ? ((StringEntry*) this->values[index]) : nullptr);
}

CompoundEntry* ListEntry::getCompound(unsigned long index) const {
    if (index >= this->size()) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::Compound ? ((CompoundEntry*) this->values[index]) : nullptr);
}

ListEntry* ListEntry::getList(unsigned long index) const {
    if (index >= this->size()) {
        std::cerr << "Index out of bounds" << std::endl;
        return nullptr;
    }
    return (this->values[index]->getType() == EntryType::List ? ((ListEntry*) this->values[index]) : nullptr);
}

unsigned long ListEntry::size() const {
    return this->values.size();
}

void ListEntry::add(ConfigEntry* value) {
    if (value->getType() != this->listType && this->listType != EntryType::Invalid) {
        std::cerr << "Invalid type for list entry" << std::endl;
        return;
    }
    if (this->listType == EntryType::Invalid) {
        this->listType = value->getType();
    }
    this->values.push_back(value);
}

void ListEntry::remove(unsigned long index) {
    if (index >= this->size() || index < 0) {
        std::cerr << "Index out of bounds" << std::endl;
        return;
    }
    this->values.erase(this->values.begin() + index);
    if (this->values.size() == 0) {
        this->listType = EntryType::Invalid;
    }
}

void ListEntry::setListType(EntryType type) {
    this->listType = type;
}

EntryType ListEntry::getListType() const {
    return this->listType;
}

void ListEntry::clear() {
    this->values.clear();
    this->listType = EntryType::Invalid;
}

bool ListEntry::isEmpty() const {
    return this->size() == 0;
}

void ListEntry::merge(ListEntry* other) {
    if (this->listType != other->listType) {
        std::cerr << "Invalid type for list entry" << std::endl;
        return;
    }
    if (this->listType == EntryType::Invalid) {
        this->listType = other->getListType();
    }
    for (unsigned long i = 0; i < other->size(); i++) {
        this->add(other->get(i)->clone());
    }
}

bool ListEntry::operator==(const ConfigEntry& other) {
    if (other.getType() != EntryType::List) {
        return false;
    }
    const ListEntry& list = (const ListEntry&) other;
    if (this->listType != list.listType) {
        return false;
    }
    if (this->size() != list.size()) {
        return false;
    }
    for (unsigned long i = 0; i < this->size(); i++) {
        if (this->values[i]->operator!=(*list.values[i])) {
            return false;
        }
    }
    return true;
}

bool ListEntry::operator!=(const ConfigEntry& other) {
    return !operator==(other);
}

void ListEntry::print(std::ostream& stream, int indent) const {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "[" << std::endl;
    indent += 2;
    for (unsigned long i = 0; i < this->size(); i++) {
        this->values[i]->print(stream, indent);
    }
    indent -= 2;
    stream << std::string(indent, ' ') << "]" << std::endl;
}

ConfigEntry* ListEntry::clone() {
    ListEntry* entry = new ListEntry();
    entry->setKey(this->getKey());
    entry->setListType(this->listType);
    entry->merge(this);
    return ((ConfigEntry*) entry);
}
