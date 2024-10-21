#pragma once

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
typedef unsigned long u_long;
#endif

enum class EntryType {
    Invalid,
    String = 1,
    Number,
    List,
    Compound
};

struct ConfigEntry {
private:
    std::string key;
    EntryType type;

public:
    static ConfigEntry* Null;

    virtual std::string getKey() const;
    virtual EntryType getType() const;
    virtual void setKey(const std::string& key);
    virtual void setType(EntryType type);
    virtual bool operator==(const ConfigEntry& other) = 0;
    virtual bool operator!=(const ConfigEntry& other) = 0;
    virtual void print(std::ostream& out, int indent = 0);
};

struct StringEntry : public ConfigEntry {
private:
    std::string value;

public:
    StringEntry();
    std::string getValue();
    void setValue(std::string value);
    bool isEmpty();
    bool operator==(const ConfigEntry& other) override;
    bool operator!=(const ConfigEntry& other) override;
    void print(std::ostream& stream, int indent = 0) override;
};

struct NumberEntry : public ConfigEntry {
private:
    double value;

public:
    NumberEntry();
    double getValue();
    void setValue(double value);
    bool operator==(const ConfigEntry& other) override;
    bool operator!=(const ConfigEntry& other) override;
    void print(std::ostream& stream, int indent = 0) override;
};

struct CompoundEntry;

struct ListEntry : public ConfigEntry {
private:
    ConfigEntry** values;
    unsigned long length;
    unsigned long capacity;
    EntryType listType = EntryType::Invalid;

public:
    ListEntry();
    ConfigEntry*& get(unsigned long index);
    ConfigEntry*& operator[](unsigned long index);
    StringEntry* getString(unsigned long index);
    ListEntry* getList(unsigned long index);
    CompoundEntry* getCompound(unsigned long index);
    unsigned long size();
    void add(ConfigEntry* value);
    void remove(unsigned long index);
    void clear();
    void setListType(EntryType type);
    EntryType getListType();
    bool isEmpty();
    bool operator==(const ConfigEntry& other) override;
    bool operator!=(const ConfigEntry& other) override;
    void print(std::ostream& stream, int indent = 0) override;
};

struct CompoundEntry : public ConfigEntry {
private:
    ConfigEntry** entries;
    unsigned long length;
    unsigned long capacity;

public:
    CompoundEntry();
    bool hasMember(const std::string& key);
    ConfigEntry*& get(const std::string& key);
    ConfigEntry*& operator[](const std::string& key);
    StringEntry* getString(const std::string& key);
    StringEntry* getStringOrDefault(const std::string& key, const std::string& defaultValue);
    ListEntry* getList(const std::string& key);
    CompoundEntry* getCompound(const std::string& key);
    void add(ConfigEntry* entry);
    void setString(const std::string& key, const std::string& value);
    void addString(const std::string& key, const std::string& value);
    void addList(const std::string& key, const std::vector<ConfigEntry*>& value);
    void addList(const std::string& key, ConfigEntry* value);
    void addList(ListEntry* value);
    void addCompound(CompoundEntry* value);
    void remove(const std::string& key);
    void removeAll();
    bool isEmpty();
    bool operator==(const ConfigEntry& other) override;
    bool operator!=(const ConfigEntry& other) override;
    void print(std::ostream& stream, int indent = 0) override;
};

struct ConfigParser {
    CompoundEntry* parse(const std::string& configFile);
    
private:
    bool isValidIdentifier(char c);

    CompoundEntry* parseCompound(std::string& data, int& i);
    ListEntry* parseList(std::string& data, int& i);
    StringEntry* parseString(std::string& data, int& i);
    NumberEntry* parseNumber(std::string& data, int& i);
};
