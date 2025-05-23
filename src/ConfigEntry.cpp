#include <LynxConf.hpp>

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
void ConfigEntry::print(std::ostream& out, int indent) const {
    (void) out; (void) indent;
}
