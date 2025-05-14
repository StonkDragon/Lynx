#include <LynxConf.hpp>

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

ConfigEntry* ListEntry::clone() {
    ListEntry* entry = new ListEntry();
    entry->setKey(this->getKey());
    entry->setListType(this->listType);
    entry->merge(this);
    return ((ConfigEntry*) entry);
}
