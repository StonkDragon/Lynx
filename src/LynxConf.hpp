#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>

#ifdef _WIN32
typedef unsigned long u_long;
#endif

#define LYNX_LOG std::cout << "[Lynx Config] " << tokens[i].file << ":" << tokens[i].line << ":" << tokens[i].column << ": "
#define LYNX_ERR std::cerr << "[Lynx Config] " << tokens[i].file << ":" << tokens[i].line << ":" << tokens[i].column << ": "
#define LYNX_RT_ERR std::cerr << "[Lynx Config] "

enum class EntryType {
    Invalid,
    String = 1,
    Number,
    List,
    Compound
};

std::ostream& operator<<(std::ostream& out, EntryType type);

struct ConfigEntry {
private:
    std::string key;
    EntryType type;

public:
    static ConfigEntry* Null;

    /**
     * Returns the key of this entry.
     */
    virtual std::string getKey() const;
    /**
     * Returns the type of this entry.
     */
    virtual EntryType getType() const;
    /**
     * Sets the key of this entry.
     * @param key The key to set.
     */
    virtual void setKey(const std::string& key);
    /**
     * Sets the type of this entry.
     * @param type The type to set.
     */
    virtual void setType(EntryType type);
    /**
     * Creates a copy of this entry.
     * @return A copy of this entry.
     */
    virtual ConfigEntry* clone() = 0;
    /**
     * Compares this entry with another entry.
     */
    virtual bool operator==(const ConfigEntry& other) = 0;
    /**
     * Compares this entry with another entry.
     */
    virtual bool operator!=(const ConfigEntry& other) = 0;
    /**
     * Prints this entry to the output stream.
     * @param out The output stream to print to.
     * @param indent The indentation level.
     */
    virtual void print(std::ostream& out, int indent = 0);
};

struct StringEntry : public ConfigEntry {
private:
    std::string value;

public:
    /**
     * Creates a new string entry.
     */
    StringEntry();
    /**
     * Returns the value of this entry.
     * @return The value of this entry.
     */
    std::string getValue();
    /**
     * Sets the value of this entry.
     * @param value The value to set.
     */
    void setValue(std::string value);
    /**
     * Checks if this entry is empty.
     * @return True if this entry is empty, false otherwise.
     */
    bool isEmpty();
    /**
     * Creates a copy of this entry.
     * @return A copy of this entry.
     */
    ConfigEntry* clone() override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are equal, false otherwise.
     */
    bool operator==(const ConfigEntry& other) override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are not equal, false otherwise.
     */
    bool operator!=(const ConfigEntry& other) override;
    /**
     * Prints this entry to the output stream.
     * @param stream The output stream to print to.
     * @param indent The indentation level.
     */
    void print(std::ostream& stream, int indent = 0) override;
};

struct NumberEntry : public ConfigEntry {
private:
    double value;

public:
    /**
     * Creates a new number entry.
     */
    NumberEntry();
    /**
     * Returns the value of this entry.
     * @return The value of this entry.
     */
    double getValue();
    /**
     * Sets the value of this entry.
     * @param value The value to set.
     */
    void setValue(double value);
    /**
     * Creates a copy of this entry.
     * @return A copy of this entry.
     */
    ConfigEntry* clone() override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are equal, false otherwise.
     */
    bool operator==(const ConfigEntry& other) override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are not equal, false otherwise.
     */
    bool operator!=(const ConfigEntry& other) override;
    /**
     * Prints this entry to the output stream.
     * @param stream The output stream to print to.
     * @param indent The indentation level.
     */
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
    /**
     * Creates a new list entry.
     */
    ListEntry();
    /**
     * Returns the value at the specified index.
     * @param index The index of the value to get.
     * @return The value at the specified index.
     */
    ConfigEntry*& get(unsigned long index);
    /**
     * Returns the value at the specified index.
     * @param index The index of the value to get.
     * @return The value at the specified index.
     */
    ConfigEntry*& operator[](unsigned long index);
    /**
     * Returns the string value at the specified index.
     * @param index The index of the value to get.
     * @return The string value at the specified index.
     */
    StringEntry* getString(unsigned long index);
    /**
     * Returns the number value at the specified index.
     * @param index The index of the value to get.
     * @return The number value at the specified index.
     */
    ListEntry* getList(unsigned long index);
    /**
     * Returns the compound value at the specified index.
     * @param index The index of the value to get.
     * @return The compound value at the specified index.
     */
    CompoundEntry* getCompound(unsigned long index);
    /**
     * Returns the size of the list.
     */
    unsigned long size();
    /**
     * Adds a value to the list.
     * @param value The value to add.
     */
    void add(ConfigEntry* value);
    /**
     * Removes the value at the specified index.
     * @param index The index of the value to remove.
     */
    void remove(unsigned long index);
    /**
     * Removes all values from the list.
     */
    void clear();
    /**
     * Sets the type of the list.
     * @param type The type to set.
     */
    void setListType(EntryType type);
    /**
     * Returns the type of the list.
     */
    EntryType getListType();
    /**
     * Checks if the list is empty.
     * @return True if the list is empty, false otherwise.
     */
    bool isEmpty();
    /**
     * Merges the entries of another list entry into this list entry.
     * @param other The list entry to merge.
     */
    void merge(ListEntry* other);
    /**
     * Creates a copy of this entry.
     * @return A copy of this entry.
     */
    ConfigEntry* clone() override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are equal, false otherwise.
     */
    bool operator==(const ConfigEntry& other) override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are not equal, false otherwise.
     */
    bool operator!=(const ConfigEntry& other) override;
    /**
     * Prints this entry to the output stream.
     * @param stream The output stream to print to.
     * @param indent The indentation level.
     */
    void print(std::ostream& stream, int indent = 0) override;
};

struct CompoundEntry : public ConfigEntry {
private:
    ConfigEntry** entries;
    unsigned long length;
    unsigned long capacity;

public:
    /**
     * Creates a new compound entry.
     */
    CompoundEntry();
    /**
     * Returns the entry with the specified key.
     * @param key The key of the entry to get.
     * @return The entry with the specified key.
     */
    bool hasMember(const std::string& key);
    /**
     * Returns the entry with the specified key.
     * @param key The key of the entry to get.
     * @return The entry with the specified key.
     */
    ConfigEntry*& get(const std::string& key);
    /**
     * Returns the entry with the specified dot-separated path.
     * @param path The path of the entry to get.
     * @return The entry with the specified path.
     */
    ConfigEntry* getByPath(const std::string& path);
    /**
     * Returns the entry with the specified key.
     * @param key The key of the entry to get.
     * @return The entry with the specified key.
     */
    ConfigEntry*& operator[](const std::string& key);
    /**
     * Returns the string entry with the specified key.
     * @param key The key of the entry to get.
     * @return The string entry with the specified key.
     */
    StringEntry* getString(const std::string& key);
    /**
     * Returns the string entry with the specified key or the default value if the entry does not exist.
     * @param key The key of the entry to get.
     * @param defaultValue The default value to return if the entry does not exist.
     * @return The string entry with the specified key or the default value if the entry does not exist.
     */
    StringEntry* getStringOrDefault(const std::string& key, const std::string& defaultValue);
    /**
     * Returns the list entry with the specified key.
     * @param key The key of the entry to get.
     * @return The list entry with the specified key.
     */
    ListEntry* getList(const std::string& key);
    /**
     * Returns the compound entry with the specified key.
     * @param key The key of the entry to get.
     * @return The compound entry with the specified key.
     */
    CompoundEntry* getCompound(const std::string& key);
    /**
     * Adds an entry to the compound entry.
     * If an entry with the same key already exists, it will be replaced.
     * @param entry The entry to add.
     */
    void add(ConfigEntry* entry);
    /**
     * Sets the string entry with the specified key.
     * @param key The key of the entry to set.
     * @param value The value to set.
     */
    void setString(const std::string& key, const std::string& value);
    /**
     * Adds a string entry with the specified key.
     * @param key The key of the entry to add.
     * @param value The value to add.
     */
    void addString(const std::string& key, const std::string& value);
    /**
     * Adds a list entry with the specified key.
     * @param key The key of the entry to add.
     * @param value The value to add.
     */
    void addList(const std::string& key, const std::vector<ConfigEntry*>& value);
    /**
     * Adds a list entry with the specified key.
     * @param key The key of the entry to add.
     * @param value The value to add.
     */
    void addList(const std::string& key, ConfigEntry* value);
    /**
     * Adds a list entry with the specified key.
     * @param key The key of the entry to add.
     * @param value The value to add.
     */
    void addList(ListEntry* value);
    /**
     * Adds a compound entry with the specified key.
     * @param key The key of the entry to add.
     * @param value The value to add.
     */
    void addCompound(CompoundEntry* value);
    /**
     * Removes the entry with the specified key.
     * @param key The key of the entry to remove.
     */
    void remove(const std::string& key);
    /**
     * Removes all entries from the compound entry.
     */
    void removeAll();
    /**
     * Checks if the compound entry is empty.
     * @return True if the compound entry is empty, false otherwise.
     */
    bool isEmpty();
    /**
     * Merges the entries of another compound entry into this compound entry.
     * @param other The compound entry to merge.
     * @return The number of entries that were merged.
     */
    size_t merge(CompoundEntry* other);
    /**
     * Creates a copy of this entry.
     * @return A copy of this entry.
     */
    ConfigEntry* clone() override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are equal, false otherwise.
     */
    bool operator==(const ConfigEntry& other) override;
    /**
     * Compares this entry with another entry.
     * @param other The entry to compare with.
     * @return True if the entries are not equal, false otherwise.
     */
    bool operator!=(const ConfigEntry& other) override;
    /**
     * Prints this entry to the output stream.
     * @param stream The output stream to print to.
     * @param indent The indentation level.
     */
    void print(std::ostream& stream, int indent = 0) override;
};

struct Token {
    enum {
        Invalid,
        String,         // "string"
        Number,         // 123.456
        ListStart,      // [
        ListEnd,        // ]
        CompoundStart,  // {
        CompoundEnd,    // }
        BlockStart,     // (
        BlockEnd,       // )
        Identifier,     // identifier
        Dot,            // .
        Assign,         // :
        Is,             // =
    } type;
    std::string value;
    std::string file;
    int line;
    int column;
};

struct Type {
    struct CompoundType {
        std::string key;
        Type* type;
    };
    
    EntryType type;
    Type* listType;
    std::vector<CompoundType>* compoundTypes;
};

struct ConfigParser {
    /**
     * Parses the specified configuration file.
     * @param configFile The path to the configuration file.
     * @return The root entry of the configuration file.
     */
    CompoundEntry* parse(const std::string& configFile);
    
    Type* parseType(std::vector<Token>& tokens, int& i);
    bool checkTypes(ConfigEntry* entry, Type* type, const std::vector<std::string>& flags);
    std::vector<Type::CompoundType>* parseCompoundTypes(std::vector<Token>& tokens, int& i);
    CompoundEntry* parseCompound(std::vector<Token>& tokens, int& i);
    ListEntry* parseList(std::vector<Token>& tokens, int& i);
    ConfigEntry* parseValue(std::vector<Token>& tokens, int& i);

    std::vector<CompoundEntry*> compoundStack;
};

using Command = std::function<ConfigEntry*(std::vector<Token>&, int&, ConfigParser*)>;
