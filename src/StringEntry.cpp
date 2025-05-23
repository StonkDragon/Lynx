#include <LynxConf.hpp>

StringEntry::StringEntry() {
    this->setType(EntryType::String);
}

std::string StringEntry::getValue() const {
    return this->value;
}

void StringEntry::setValue(std::string value) {
    this->value = value;
}

bool StringEntry::isEmpty() const {
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

void StringEntry::print(std::ostream& stream, int indent) const {
    stream << std::string(indent, ' ');
    if (this->getKey().size()) {
        stream << this->getKey() << ": ";
    }
    stream << "\"" << this->value << "\"" << std::endl;
}

ConfigEntry* StringEntry::clone() {
    StringEntry* entry = new StringEntry();
    entry->setKey(this->getKey());
    entry->setValue(this->value);
    return ((ConfigEntry*) entry);
}
