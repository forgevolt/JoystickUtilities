// In-memory stand-in for the ESP32 Preferences library: enough to test saving and loading,
// including that a read-only begin() of a missing namespace fails instead of creating it.
#pragma once

#include <cstdint>
#include <map>
#include <string>

class Preferences
{
  public:
    using Namespace = std::map<std::string, int32_t>;
    static std::map<std::string, Namespace>& storage();

    bool begin(const char* name, bool readOnly = false)
    {
      if (readOnly && storage().count(name) == 0)
        return false;
      myName = name;
      myReadOnly = readOnly;
      storage()[myName];   // creates it when opened read-write
      myOpen = true;
      return true;
    }

    void end() { myOpen = false; }

    size_t putInt(const char* key, int32_t value)
    {
      if (myOpen == false || myReadOnly)
        return 0;
      storage()[myName][key] = value;
      return sizeof(value);
    }

    int32_t getInt(const char* key, int32_t defaultValue = 0)
    {
      if (myOpen == false)
        return defaultValue;
      const Namespace& ns = storage()[myName];
      auto it = ns.find(key);
      return (it == ns.end()) ? defaultValue : it->second;
    }

  private:
    std::string myName;
    bool myReadOnly = false;
    bool myOpen = false;
};
