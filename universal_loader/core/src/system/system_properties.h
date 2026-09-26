#ifndef J2ME_SYSTEM_PROPERTIES_H
#define J2ME_SYSTEM_PROPERTIES_H

#include "../../include/j2me_core.h"
#include <string>
#include <map>
#include <mutex>

namespace j2me {

class J2ME_API SystemPropertiesManager {
public:
    static SystemPropertiesManager& instance();

    void resetToDefaults();
    std::string getProperty(const std::string& key, const std::string& defaultValue = "") const;
    void setProperty(const std::string& key, const std::string& value);
    bool hasProperty(const std::string& key) const;
    bool removeProperty(const std::string& key);

    bool loadProperties(const std::string& propContent);
    std::string exportProperties() const;
    std::map<std::string, std::string> getAllProperties() const;
    size_t getPropertyCount() const;

private:
    SystemPropertiesManager();
    ~SystemPropertiesManager() = default;

    mutable std::mutex m_mutex;
    std::map<std::string, std::string> m_properties;

    void populateDefaultsLocked();
};

} // namespace j2me

#endif // J2ME_SYSTEM_PROPERTIES_H
