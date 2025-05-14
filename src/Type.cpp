#include <LynxConf.hpp>

#include <algorithm>
#include <sstream>

#pragma region Type
bool Type::validate(ConfigEntry* what, const std::vector<std::string>& flags, std::ostream& out) {
    if (this->type == EntryType::Any) {
        return true;
    }

    bool hasError = false;

    bool optional = this->isOptional || std::find(flags.begin(), flags.end(), "optional") != flags.end();
    if (what->getType() != this->type) {
        LYNX_RT_ERR_TO(out) << "Invalid entry type. Expected " << this->type << " but got " << what->getType() << std::endl;
        return false;
    }
    
    if (what->getType() == EntryType::List) {
        ListEntry* list = ((ListEntry*) what);
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
        CompoundEntry* compound = ((CompoundEntry*) what);
        for (size_t i = 0; i < this->compoundTypes->size(); i++) {
            Type::CompoundType compoundType = this->compoundTypes->at(i);
            if (!compound->hasMember(compoundType.key)) {
                if (optional || compoundType.type->isOptional) {
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
    } else if (what->getType() == EntryType::String) {
        return true;
    } else if (what->getType() == EntryType::Number) {
        return true;
    }
    return !hasError;
}

std::string Type::toString() const {
    std::ostringstream oss;
    if (this->isOptional) {
        oss << "optional ";
    }
    switch (this->type) {
        case EntryType::String: oss << "string"; break;
        case EntryType::Number: oss << "number"; break;
        case EntryType::List: {
            oss << "list[";
            if (this->listType) {
                oss << this->listType->toString();
            } else {
                oss << "any";
            }
            oss << "]";
            break;
        }
        case EntryType::Function: {
            oss << "func";
            if (this->compoundTypes) {
                oss << "(";
                for (size_t i = 0; i < this->compoundTypes->size(); i++) {
                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << this->compoundTypes->at(i).key << ": " << this->compoundTypes->at(i).type->toString();
                }
                oss << ")";
            }
            break;
        }
        case EntryType::Compound: {
            oss << "compound {";
            if (this->compoundTypes) {
                for (size_t i = 0; i < this->compoundTypes->size(); i++) {
                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << this->compoundTypes->at(i).key << ": " << this->compoundTypes->at(i).type->toString();
                }
            }
            oss << "}";
            break;
        } 
        case EntryType::Invalid: return "<invalid>";
        default: return "<unknown>";
    }
    return oss.str();
}

bool Type::operator==(const Type& other) const {
    if (this->type != other.type) return false;
    switch (this->type) {
        case EntryType::List: return (*(this->listType) == (*other.listType));
        case EntryType::Function: [[fallthrough]];
        case EntryType::Compound: {
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

Type* Type::String() {
    Type* type = new Type();
    type->type = EntryType::String;
    return type;
}

Type* Type::Number() {
    Type* type = new Type();
    type->type = EntryType::Number;
    return type;
}

Type* Type::List(Type* type) {
    Type* t = new Type();
    t->type = EntryType::List;
    t->listType = type;
    return t;
}

Type* Type::Compound(std::vector<CompoundType> types) {
    Type* type = new Type();
    type->type = EntryType::Compound;
    type->compoundTypes = new std::vector<CompoundType>();
    for (const auto& compoundType : types) {
        type->compoundTypes->push_back(compoundType);
    }
    return type;
}

Type* Type::Function(std::vector<CompoundType> types) {
    Type* type = Type::Compound(types);
    type->type = EntryType::Function;
    return type;
}

Type* Type::Optional(Type* type) {
    Type* t = type->clone();
    t->isOptional = true;
    return t;
}

Type* Type::Any() {
    Type* type = new Type();
    type->type = EntryType::Any;
    return type;
}

#pragma endregion

#pragma region TypeEntry
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
    return ((ConfigEntry*) entry);
}
#pragma endregion
