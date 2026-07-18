#ifndef MOCK_PREFERENCES_H
#define MOCK_PREFERENCES_H

#include <string>
#include <map>

class Preferences {
private:
    inline static std::map<std::string, std::string> _storage;
    bool _readOnly;
    std::string _namespace;

public:
    Preferences() : _readOnly(false) {}

    bool begin(const char * name, bool readOnly = false) {
        _namespace = name;
        _readOnly = readOnly;
        return true;
    }

    void end() {}

    bool clear() {
        _storage.clear();
        return true;
    }

    bool remove(const char * key) {
        std::string fullKey = _namespace + "_" + key;
        _storage.erase(fullKey);
        return true;
    }

    size_t putBool(const char* key, bool value) {
        std::string fullKey = _namespace + "_" + key;
        _storage[fullKey] = value ? "1" : "0";
        return 1;
    }

    size_t putUChar(const char* key, uint8_t value) {
        std::string fullKey = _namespace + "_" + key;
        _storage[fullKey] = std::to_string(value);
        return 1;
    }

    size_t putUInt(const char* key, uint32_t value) {
        std::string fullKey = _namespace + "_" + key;
        _storage[fullKey] = std::to_string(value);
        return 4;
    }

    size_t putULong(const char* key, uint32_t value) {
        std::string fullKey = _namespace + "_" + key;
        _storage[fullKey] = std::to_string(value);
        return 4;
    }

    bool getBool(const char* key, bool defaultValue = false) {
        std::string fullKey = _namespace + "_" + key;
        if (_storage.find(fullKey) == _storage.end()) {
            return defaultValue;
        }
        return _storage[fullKey] == "1";
    }

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) {
        std::string fullKey = _namespace + "_" + key;
        if (_storage.find(fullKey) == _storage.end()) {
            return defaultValue;
        }
        return (uint8_t)std::stoi(_storage[fullKey]);
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
        std::string fullKey = _namespace + "_" + key;
        if (_storage.find(fullKey) == _storage.end()) {
            return defaultValue;
        }
        return (uint32_t)std::stoul(_storage[fullKey]);
    }

    uint32_t getULong(const char* key, uint32_t defaultValue = 0) {
        std::string fullKey = _namespace + "_" + key;
        if (_storage.find(fullKey) == _storage.end()) {
            return defaultValue;
        }
        return (uint32_t)std::stoul(_storage[fullKey]);
    }
};

#endif // MOCK_PREFERENCES_H
