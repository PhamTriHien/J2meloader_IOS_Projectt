#include "app_descriptor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace universal_loader {
namespace midlet {

namespace {

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> splitString(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

} // anonymous namespace

AppDescriptor::AppDescriptor(const std::string& source, bool isJad) {
    loadFromString(source, isJad);
}

bool AppDescriptor::loadFromString(const std::string& source, bool isJad) {
    m_isJad = isJad;
    m_attributes.clear();
    m_midlets.clear();

    if (source.empty()) return false;

    parseInternal(source);
    extractMidletEntries();
    verifyAttrs();
    return true;
}

bool AppDescriptor::loadFromFile(const std::string& filePath, bool isJad) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    return loadFromString(ss.str(), isJad);
}

void AppDescriptor::parseInternal(const std::string& source) {
    std::vector<std::string> lines;
    std::string currentLine;
    for (size_t i = 0; i < source.length(); ++i) {
        char ch = source[i];
        if (ch == '\r' || ch == '\n') {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine.clear();
            }
        } else {
            currentLine += ch;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    if (lines.empty()) return;

    // Check and strip UTF-8 BOM
    std::string& firstLine = lines[0];
    if (firstLine.size() >= 3 &&
        static_cast<unsigned char>(firstLine[0]) == 0xEF &&
        static_cast<unsigned char>(firstLine[1]) == 0xBB &&
        static_cast<unsigned char>(firstLine[2]) == 0xBF) {
        firstLine = firstLine.substr(3);
    }

    std::string currentKey = "Manifest-Version";
    std::string currentValue = "1.0";

    for (const auto& rawLine : lines) {
        if (rawLine.empty()) continue;

        size_t colon = rawLine.find(':');
        if (colon == std::string::npos) {
            // Folded continuation line (starts with space)
            if (rawLine[0] == ' ') {
                currentValue.append(rawLine.substr(1));
            } else {
                currentValue.append(rawLine);
            }
        } else {
            if (!currentKey.empty()) {
                m_attributes[currentKey] = trim(currentValue);
            }
            currentKey = trim(rawLine.substr(0, colon));
            size_t valStart = colon + 1;
            if (valStart < rawLine.length() && rawLine[valStart] == ' ') {
                valStart++;
            }
            currentValue = rawLine.substr(valStart);
        }
    }

    if (!currentKey.empty()) {
        m_attributes[currentKey] = trim(currentValue);
    }
}

void AppDescriptor::extractMidletEntries() {
    for (int i = 1; ; ++i) {
        std::string key = std::string(ATTR_MIDLET_N_PREFIX) + std::to_string(i);
        auto it = m_attributes.find(key);
        if (it == m_attributes.end()) {
            break;
        }

        const std::string& val = it->second;
        MidletEntry entry;
        entry.index = i;

        size_t firstComma = val.find(',');
        size_t lastComma = val.rfind(',');

        if (firstComma != std::string::npos && lastComma != std::string::npos && firstComma != lastComma) {
            entry.name = trim(val.substr(0, firstComma));
            entry.icon = trim(val.substr(firstComma + 1, lastComma - firstComma - 1));
            entry.className = trim(val.substr(lastComma + 1));
        } else if (firstComma != std::string::npos) {
            entry.name = trim(val.substr(0, firstComma));
            entry.className = trim(val.substr(firstComma + 1));
        } else {
            entry.name = trim(val);
            entry.className = trim(val);
        }

        // Clean leading slash from icon path
        while (!entry.icon.empty() && entry.icon[0] == '/') {
            entry.icon = entry.icon.substr(1);
        }

        m_midlets.push_back(entry);
    }
}

void AppDescriptor::verifyAttrs() {
    // If JAD, verify MIDlet-Jar-URL and MIDlet-Jar-Size
    if (m_isJad) {
        auto itSize = m_attributes.find(ATTR_MIDLET_JAR_SIZE);
        if (itSize != m_attributes.end()) {
            itSize->second = trim(itSize->second);
        }
        auto itUrl = m_attributes.find(ATTR_MIDLET_JAR_URL);
        if (itUrl != m_attributes.end()) {
            itUrl->second = trim(itUrl->second);
        }
    }
}

std::string AppDescriptor::get(const std::string& key) const {
    auto it = m_attributes.find(key);
    return (it != m_attributes.end()) ? it->second : "";
}

void AppDescriptor::set(const std::string& key, const std::string& value) {
    m_attributes[key] = value;
    if (key.rfind(ATTR_MIDLET_N_PREFIX, 0) == 0) {
        m_midlets.clear();
        extractMidletEntries();
    }
}

bool AppDescriptor::has(const std::string& key) const {
    return m_attributes.find(key) != m_attributes.end();
}

std::string AppDescriptor::getName() const {
    return get(ATTR_MIDLET_NAME);
}

std::string AppDescriptor::getVersion() const {
    return get(ATTR_MIDLET_VERSION);
}

std::string AppDescriptor::getVendor() const {
    return get(ATTR_MIDLET_VENDOR);
}

std::string AppDescriptor::getJarUrl() const {
    return get(ATTR_MIDLET_JAR_URL);
}

int64_t AppDescriptor::getJarSize() const {
    std::string sz = get(ATTR_MIDLET_JAR_SIZE);
    if (sz.empty()) return 0;
    try {
        return std::stoll(sz);
    } catch (...) {
        return 0;
    }
}

std::string AppDescriptor::getIcon() const {
    std::string icon = get(ATTR_MIDLET_ICON);
    if (!icon.empty()) {
        while (!icon.empty() && icon[0] == '/') {
            icon = icon.substr(1);
        }
        return icon;
    }
    if (!m_midlets.empty() && !m_midlets[0].icon.empty()) {
        return m_midlets[0].icon;
    }
    return "";
}

std::string AppDescriptor::getDescription() const {
    return get(ATTR_MIDLET_DESCRIPTION);
}

std::string AppDescriptor::getProfile() const {
    return get(ATTR_MICROEDITION_PROFILE);
}

std::string AppDescriptor::getConfiguration() const {
    return get(ATTR_MICROEDITION_CONFIGURATION);
}

const MidletEntry* AppDescriptor::getMidlet(int index) const {
    for (const auto& entry : m_midlets) {
        if (entry.index == index) return &entry;
    }
    return nullptr;
}

int AppDescriptor::compareVersion(const std::string& otherVersion) const {
    return compareVersions(getVersion(), otherVersion);
}

int AppDescriptor::compareVersions(const std::string& v1, const std::string& v2) {
    auto mv = splitString(v1, '.');
    auto ov = splitString(v2, '.');
    size_t len = std::max(mv.size(), ov.size());

    for (size_t i = 0; i < len; ++i) {
        int m = 0;
        if (i < mv.size()) {
            try { m = std::stoi(trim(mv[i])); } catch (...) {}
        }
        int o = 0;
        if (i < ov.size()) {
            try { o = std::stoi(trim(ov[i])); } catch (...) {}
        }
        if (m != o) {
            return (m > o) ? 1 : -1;
        }
    }
    return 0;
}

void AppDescriptor::merge(const AppDescriptor& other) {
    for (const auto& kv : other.m_attributes) {
        m_attributes[kv.first] = kv.second;
    }
    m_midlets.clear();
    extractMidletEntries();
}

std::string AppDescriptor::serialize() const {
    std::ostringstream ss;
    for (const auto& kv : m_attributes) {
        ss << kv.first << ": " << kv.second << "\r\n";
    }
    return ss.str();
}

bool AppDescriptor::writeToFile(const std::string& filePath) const {
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    std::string data = serialize();
    file.write(data.data(), data.size());
    return file.good();
}

} // namespace midlet
} // namespace universal_loader
