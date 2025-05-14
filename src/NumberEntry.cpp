#include <LynxConf.hpp>

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

ConfigEntry* NumberEntry::clone() {
    NumberEntry* entry = new NumberEntry();
    entry->setKey(this->getKey());
    entry->setValue(this->value);
    return ((ConfigEntry*) entry);
}
