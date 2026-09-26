#pragma once

#include "j2me_core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace universal_loader {
namespace midlet {

/**
 * @brief Represents an individual MIDlet defined in the manifest/JAD (MIDlet-<n>).
 */
struct J2ME_API MidletEntry {
    int index{1};
    std::string name;
    std::string icon;
    std::string className;
};

/**
 * @brief Parser and container for J2ME Application Descriptors (JAD and MANIFEST.MF).
 * Ported directly from ru.woesss.j2me.jar.Descriptor in upstream J2ME-Loader.
 */
class J2ME_API AppDescriptor {
public:
    // Standard JAD & Manifest Attribute Keys
    static constexpr const char* ATTR_MIDLET_NAME = "MIDlet-Name";
    static constexpr const char* ATTR_MIDLET_VERSION = "MIDlet-Version";
    static constexpr const char* ATTR_MIDLET_VENDOR = "MIDlet-Vendor";
    static constexpr const char* ATTR_MIDLET_JAR_URL = "MIDlet-Jar-URL";
    static constexpr const char* ATTR_MIDLET_JAR_SIZE = "MIDlet-Jar-Size";
    static constexpr const char* ATTR_MIDLET_N_PREFIX = "MIDlet-";
    static constexpr const char* ATTR_MICROEDITION_PROFILE = "MicroEdition-Profile";
    static constexpr const char* ATTR_MICROEDITION_CONFIGURATION = "MicroEdition-Configuration";
    static constexpr const char* ATTR_MIDLET_ICON = "MIDlet-Icon";
    static constexpr const char* ATTR_MIDLET_DESCRIPTION = "MIDlet-Description";
    static constexpr const char* ATTR_MIDLET_INFO_URL = "MIDlet-Info-URL";
    static constexpr const char* ATTR_MIDLET_DATA_SIZE = "MIDlet-Data-Size";
    static constexpr const char* ATTR_MIDLET_PERMISSIONS = "MIDlet-Permissions";
    static constexpr const char* ATTR_MIDLET_PERMISSIONS_OPT = "MIDlet-Permissions-Opt";
    static constexpr const char* ATTR_MIDLET_PUSH_N = "MIDlet-Push-";
    static constexpr const char* ATTR_NOKIA_MIDLET_UID_N = "Nokia-MIDlet-UID-";
    static constexpr const char* ATTR_NOKIA_UI_ENHANCEMENT = "Nokia-UI-Enhancement";

private:
    bool m_isJad{false};
    std::unordered_map<std::string, std::string> m_attributes;
    std::vector<MidletEntry> m_midlets;

    void parseInternal(const std::string& source);
    void extractMidletEntries();
    void verifyAttrs();

public:
    AppDescriptor() = default;
    explicit AppDescriptor(const std::string& source, bool isJad = false);

    bool loadFromString(const std::string& source, bool isJad = false);
    bool loadFromFile(const std::string& filePath, bool isJad = false);

    // Attribute Accessors
    std::string get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    bool has(const std::string& key) const;
    const std::unordered_map<std::string, std::string>& getAttributes() const { return m_attributes; }

    // Helper getters for standard fields
    std::string getName() const;
    std::string getVersion() const;
    std::string getVendor() const;
    std::string getJarUrl() const;
    int64_t getJarSize() const;
    std::string getIcon() const;
    std::string getDescription() const;
    std::string getProfile() const;
    std::string getConfiguration() const;

    const std::vector<MidletEntry>& getMidlets() const { return m_midlets; }
    const MidletEntry* getMidlet(int index) const;

    // Version Comparison Algorithm (matches upstream Descriptor.java)
    int compareVersion(const std::string& otherVersion) const;
    static int compareVersions(const std::string& v1, const std::string& v2);

    // Merge JAD with MANIFEST.MF
    void merge(const AppDescriptor& other);

    // Serialization
    std::string serialize() const;
    bool writeToFile(const std::string& filePath) const;
};

} // namespace midlet
} // namespace universal_loader
