#include "system_properties.h"
#include <sstream>
#include <algorithm>

namespace j2me {

SystemPropertiesManager& SystemPropertiesManager::instance() {
    static SystemPropertiesManager s_instance;
    return s_instance;
}

SystemPropertiesManager::SystemPropertiesManager() {
    resetToDefaults();
}

void SystemPropertiesManager::populateDefaultsLocked() {
    m_properties.clear();

    // 24 exact properties from upstream/app/src/main/assets/defaults/system.props
    m_properties["microedition.configuration"] = "CLDC-1.1";
    m_properties["microedition.profiles"] = "MIDP-2.0";
    m_properties["microedition.encoding"] = "ISO-8859-1";
    m_properties["microedition.platform"] = "Nokia6233/05.10";
    m_properties["microedition.io.file.FileConnection.version"] = "1.0";
    m_properties["microedition.sensor.version"] = "1";
    m_properties["microedition.m3g.version"] = "1.1";
    m_properties["microedition.media.version"] = "1.0";
    m_properties["microedition.pim.version"] = "1.0";
    m_properties["microedition.location.version"] = "1.0";
    m_properties["supports.mixing"] = "true";
    m_properties["supports.audio.capture"] = "true";
    m_properties["supports.video.capture"] = "true";
    m_properties["supports.recording"] = "true";
    m_properties["com.siemens.mp.systemfolder.ringingtone"] = "fs/MyStuff/Ringtones";
    m_properties["com.siemens.mp.systemfolder.pictures"] = "fs/MyStuff/Pictures";
    m_properties["com.siemens.OSVersion"] = "11";
    m_properties["device.imei"] = "000000000000000";
    m_properties["com.siemens.IMEI"] = "000000000000000";
    m_properties["com.sonyericsson.imei"] = "IMEI 00460101-501594-5-00";
    m_properties["com.nokia.mid.impl.isa.visual_radio_operator_id"] = "0";
    m_properties["com.nokia.mid.impl.isa.visual_radio_channel_freq"] = "0";
    m_properties["com.nokia.mid.ui.DirectGraphics.PIXEL_FORMAT"] = "565";
    m_properties["wireless.messaging.sms.smsc"] = "+8613800010000";

    // Standard J2ME runtime properties
    m_properties["microedition.locale"] = "en-US";
    m_properties["microedition.hostname"] = "localhost";
    m_properties["microedition.commports"] = "COM1,COM2";
    m_properties["microedition.jtwi.version"] = "1.0";
}

void SystemPropertiesManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    populateDefaultsLocked();
}

std::string SystemPropertiesManager::getProperty(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_properties.find(key);
    if (it != m_properties.end()) {
        return it->second;
    }
    // Handle CLDC / CDLC alias if requested
    if (key == "microedition.configuration") {
        auto itAlt = m_properties.find("microedition.configuration");
        if (itAlt != m_properties.end()) return itAlt->second;
    }
    return defaultValue;
}

void SystemPropertiesManager::setProperty(const std::string& key, const std::string& value) {
    if (key.empty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_properties[key] = value;
}

bool SystemPropertiesManager::hasProperty(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_properties.find(key) != m_properties.end();
}

bool SystemPropertiesManager::removeProperty(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_properties.erase(key) > 0;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool SystemPropertiesManager::loadProperties(const std::string& propContent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::istringstream stream(propContent);
    std::string line;
    bool anyLoaded = false;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == '!') {
            continue;
        }

        size_t sep = trimmed.find(':');
        if (sep == std::string::npos) {
            sep = trimmed.find('=');
        }

        if (sep != std::string::npos) {
            std::string key = trim(trimmed.substr(0, sep));
            std::string val = trim(trimmed.substr(sep + 1));
            if (!key.empty()) {
                m_properties[key] = val;
                anyLoaded = true;
            }
        }
    }

    return anyLoaded;
}

std::string SystemPropertiesManager::exportProperties() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;
    for (const auto& [k, v] : m_properties) {
        ss << k << ": " << v << "\n";
    }
    return ss.str();
}

std::map<std::string, std::string> SystemPropertiesManager::getAllProperties() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_properties;
}

size_t SystemPropertiesManager::getPropertyCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_properties.size();
}

} // namespace j2me
