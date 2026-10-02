#include "app_profile_config.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace universal_loader::config {

static std::string escapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

static std::string unescapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            ++i;
            switch (input[i]) {
                case '\"': out += '\"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u':
                    if (i + 4 < input.size()) {
                        std::string hexStr = input.substr(i + 1, 4);
                        char* end = nullptr;
                        long val = strtol(hexStr.c_str(), &end, 16);
                        if (val > 0 && val < 128) {
                            out += static_cast<char>(val);
                        }
                        i += 4;
                    }
                    break;
                default:
                    out += input[i];
                    break;
            }
        } else {
            out += input[i];
        }
    }
    return out;
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

ProfileModel::ProfileModel() {
    systemProperties = ProfilesManager::getDefaultSystemProperties();
}

std::string ProfileModel::serializeJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"Version\": " << version << ",\n";
    ss << "  \"ScreenWidth\": " << screenWidth << ",\n";
    ss << "  \"ScreenHeight\": " << screenHeight << ",\n";
    ss << "  \"ScreenBackgroundColor\": " << screenBackgroundColor << ",\n";
    ss << "  \"ScreenScaleRatio\": " << screenScaleRatio << ",\n";
    ss << "  \"Orientation\": " << orientation << ",\n";
    ss << "  \"ScreenScaleToFit\": " << (screenScaleToFit ? "true" : "false") << ",\n";
    ss << "  \"ScreenKeepAspectRatio\": " << (screenKeepAspectRatio ? "true" : "false") << ",\n";
    ss << "  \"ScreenScaleType\": " << screenScaleType << ",\n";
    ss << "  \"ScreenGravity\": " << screenGravity << ",\n";
    ss << "  \"ScreenFilter\": " << (screenFilter ? "true" : "false") << ",\n";
    ss << "  \"ImmediateMode\": " << (immediateMode ? "true" : "false") << ",\n";
    ss << "  \"HwAcceleration\": " << (hwAcceleration ? "true" : "false") << ",\n";
    ss << "  \"GraphicsMode\": " << graphicsMode << ",\n";
    ss << "  \"ParallelRedrawScreen\": " << (parallelRedrawScreen ? "true" : "false") << ",\n";
    ss << "  \"ShowFps\": " << (showFps ? "true" : "false") << ",\n";
    ss << "  \"FpsLimit\": " << fpsLimit << ",\n";
    ss << "  \"ForceFullscreen\": " << (forceFullscreen ? "true" : "false") << ",\n";
    ss << "  \"FontSizeSmall\": " << fontSizeSmall << ",\n";
    ss << "  \"FontSizeMedium\": " << fontSizeMedium << ",\n";
    ss << "  \"FontSizeLarge\": " << fontSizeLarge << ",\n";
    ss << "  \"FontApplyDimensions\": " << (fontApplyDimensions ? "true" : "false") << ",\n";
    ss << "  \"FontAntiAlias\": " << (fontAA ? "true" : "false") << ",\n";
    ss << "  \"TouchInput\": " << (touchInput ? "true" : "false") << ",\n";
    ss << "  \"ShowKeyboard\": " << (showKeyboard ? "true" : "false") << ",\n";
    ss << "  \"VirtualKeyboardType\": " << vkType << ",\n";
    ss << "  \"ButtonShape\": " << vkButtonShape << ",\n";
    ss << "  \"VirtualKeyboardAlpha\": " << vkAlpha << ",\n";
    ss << "  \"VirtualKeyboardForceOpacity\": " << (vkForceOpacity ? "true" : "false") << ",\n";
    ss << "  \"VirtualKeyboardFeedback\": " << (vkFeedback ? "true" : "false") << ",\n";
    ss << "  \"VirtualKeyboardDelay\": " << vkHideDelay << ",\n";
    ss << "  \"VirtualKeyboardColorBackground\": " << vkBgColor << ",\n";
    ss << "  \"VirtualKeyboardColorBackgroundSelected\": " << vkBgColorSelected << ",\n";
    ss << "  \"VirtualKeyboardColorForeground\": " << vkFgColor << ",\n";
    ss << "  \"VirtualKeyboardColorForegroundSelected\": " << vkFgColorSelected << ",\n";
    ss << "  \"VirtualKeyboardColorOutline\": " << vkOutlineColor << ",\n";
    ss << "  \"Layout\": " << keyCodesLayout << ",\n";

    // KeyCodeMap
    ss << "  \"KeyCodeMap\": {";
    bool first = true;
    for (const auto& [k, v] : keyCodeMap) {
        if (!first) ss << ", ";
        ss << "\"" << k << "\": " << v;
        first = false;
    }
    ss << "},\n";

    // KeyMappings
    ss << "  \"KeyMappings\": {";
    first = true;
    for (const auto& [k, v] : keyMappings) {
        if (!first) ss << ", ";
        ss << "\"" << k << "\": " << v;
        first = false;
    }
    ss << "},\n";

    ss << "  \"SystemProperties\": \"" << escapeJsonString(systemProperties) << "\"\n";
    ss << "}";
    return ss.str();
}

bool ProfileModel::deserializeJson(const std::string& json) {
    if (json.empty()) return false;

    // Simple robust tokenizer for top-level JSON fields
    size_t i = 0;
    size_t n = json.size();

    auto skipWhitespace = [&]() {
        while (i < n && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
    };

    skipWhitespace();
    if (i >= n || json[i] != '{') return false;
    ++i; // skip '{'

    while (i < n) {
        skipWhitespace();
        if (i >= n || json[i] == '}') break;
        if (json[i] == ',') { ++i; continue; }

        if (json[i] != '\"') { ++i; continue; }
        ++i; // skip opening quote of key

        size_t keyStart = i;
        while (i < n && json[i] != '\"') ++i;
        if (i >= n) break;
        std::string key = json.substr(keyStart, i - keyStart);
        ++i; // skip closing quote

        skipWhitespace();
        if (i >= n || json[i] != ':') break;
        ++i; // skip ':'
        skipWhitespace();

        if (i >= n) break;

        if (json[i] == '\"') {
            // String value
            ++i;
            size_t valStart = i;
            bool escaped = false;
            while (i < n) {
                if (escaped) {
                    escaped = false;
                } else if (json[i] == '\\') {
                    escaped = true;
                } else if (json[i] == '\"') {
                    break;
                }
                ++i;
            }
            std::string rawVal = json.substr(valStart, i - valStart);
            if (i < n && json[i] == '\"') ++i;
            std::string val = unescapeJsonString(rawVal);

            if (key == "SystemProperties") systemProperties = val;
        } else if (json[i] == '{') {
            // Nested object (e.g. KeyCodeMap, KeyMappings)
            ++i; // skip '{'
            std::map<int, int> subMap;
            while (i < n && json[i] != '}') {
                skipWhitespace();
                if (json[i] == '}') break;
                if (json[i] == ',') { ++i; continue; }
                if (json[i] == '\"') {
                    ++i;
                    size_t mkStart = i;
                    while (i < n && json[i] != '\"') ++i;
                    std::string mk = json.substr(mkStart, i - mkStart);
                    if (i < n && json[i] == '\"') ++i;
                    skipWhitespace();
                    if (i < n && json[i] == ':') ++i;
                    skipWhitespace();
                    size_t mvStart = i;
                    while (i < n && (std::isdigit(static_cast<unsigned char>(json[i])) || json[i] == '-')) ++i;
                    std::string mv = json.substr(mvStart, i - mvStart);
                    if (!mk.empty() && !mv.empty()) {
                        subMap[std::stoi(mk)] = std::stoi(mv);
                    }
                } else {
                    ++i;
                }
            }
            if (i < n && json[i] == '}') ++i;
            if (key == "KeyCodeMap") keyCodeMap = subMap;
            else if (key == "KeyMappings") keyMappings = subMap;
        } else {
            // Primitive value (bool, int, float)
            size_t valStart = i;
            while (i < n && json[i] != ',' && json[i] != '}' && !std::isspace(static_cast<unsigned char>(json[i]))) {
                ++i;
            }
            std::string val = json.substr(valStart, i - valStart);

            if (key == "Version") version = std::stoi(val);
            else if (key == "ScreenWidth") screenWidth = std::stoi(val);
            else if (key == "ScreenHeight") screenHeight = std::stoi(val);
            else if (key == "ScreenBackgroundColor") screenBackgroundColor = static_cast<uint32_t>(std::stoul(val));
            else if (key == "ScreenScaleRatio") screenScaleRatio = std::stoi(val);
            else if (key == "Orientation") orientation = std::stoi(val);
            else if (key == "ScreenScaleToFit") screenScaleToFit = (val == "true");
            else if (key == "ScreenKeepAspectRatio") screenKeepAspectRatio = (val == "true");
            else if (key == "ScreenScaleType") screenScaleType = std::stoi(val);
            else if (key == "ScreenGravity") screenGravity = std::stoi(val);
            else if (key == "ScreenFilter") screenFilter = (val == "true");
            else if (key == "ImmediateMode") immediateMode = (val == "true");
            else if (key == "HwAcceleration") hwAcceleration = (val == "true");
            else if (key == "GraphicsMode") graphicsMode = std::stoi(val);
            else if (key == "ParallelRedrawScreen") parallelRedrawScreen = (val == "true");
            else if (key == "ShowFps") showFps = (val == "true");
            else if (key == "FpsLimit") fpsLimit = std::stoi(val);
            else if (key == "ForceFullscreen") forceFullscreen = (val == "true");
            else if (key == "FontSizeSmall") fontSizeSmall = std::stoi(val);
            else if (key == "FontSizeMedium") fontSizeMedium = std::stoi(val);
            else if (key == "FontSizeLarge") fontSizeLarge = std::stoi(val);
            else if (key == "FontApplyDimensions") fontApplyDimensions = (val == "true");
            else if (key == "FontAntiAlias") fontAA = (val == "true");
            else if (key == "TouchInput") touchInput = (val == "true");
            else if (key == "ShowKeyboard") showKeyboard = (val == "true");
            else if (key == "VirtualKeyboardType") vkType = std::stoi(val);
            else if (key == "ButtonShape") vkButtonShape = std::stoi(val);
            else if (key == "VirtualKeyboardAlpha") vkAlpha = std::stoi(val);
            else if (key == "VirtualKeyboardForceOpacity") vkForceOpacity = (val == "true");
            else if (key == "VirtualKeyboardFeedback") vkFeedback = (val == "true");
            else if (key == "VirtualKeyboardDelay") vkHideDelay = std::stoi(val);
            else if (key == "VirtualKeyboardColorBackground") vkBgColor = static_cast<uint32_t>(std::stoul(val));
            else if (key == "VirtualKeyboardColorBackgroundSelected") vkBgColorSelected = static_cast<uint32_t>(std::stoul(val));
            else if (key == "VirtualKeyboardColorForeground") vkFgColor = static_cast<uint32_t>(std::stoul(val));
            else if (key == "VirtualKeyboardColorForegroundSelected") vkFgColorSelected = static_cast<uint32_t>(std::stoul(val));
            else if (key == "VirtualKeyboardColorOutline") vkOutlineColor = static_cast<uint32_t>(std::stoul(val));
            else if (key == "Layout") keyCodesLayout = std::stoi(val);
        }
    }

    migrateVersion();
    return true;
}

void ProfileModel::migrateVersion() {
    if (version < 1) {
        if (hwAcceleration) {
            graphicsMode = 2;
        }
        std::string defaults = ProfilesManager::getDefaultSystemProperties();
        if (systemProperties.empty()) {
            systemProperties = defaults;
        }
    }
    if (version < 2) {
        fontAA = true;
    }
    if (version < 3) {
        if (screenScaleToFit) {
            screenScaleType = screenKeepAspectRatio ? 1 : 2;
        } else {
            screenScaleType = 0;
        }
        screenGravity = 1;
        version = VERSION;
    }
}

void ConfigDirs::init(const std::string& rootDir) {
    emulatorDir = rootDir;
    dataDir = rootDir + "/data/";
    configsDir = rootDir + "/configs/";
    profilesDir = rootDir + "/templates/";
    shadersDir = rootDir + "/shaders/";
    fsInternalDir = rootDir + "/fs/c/";
    fsExternalDir = rootDir + "/fs/e/";

    try {
        fs::create_directories(dataDir);
        fs::create_directories(configsDir);
        fs::create_directories(profilesDir);
        fs::create_directories(shadersDir);
        fs::create_directories(fsInternalDir);
        fs::create_directories(fsExternalDir);
    } catch (...) {}
}

std::string ProfilesManager::getDefaultSystemProperties() {
    return "microedition.configuration: CDLC-1.1\n"
           "microedition.profiles: MIDP-2.0\n"
           "microedition.encoding: ISO-8859-1\n"
           "microedition.platform: Nokia6233/05.10\n"
           "microedition.io.file.FileConnection.version: 1.0\n"
           "microedition.sensor.version: 1\n"
           "microedition.m3g.version: 1.1\n"
           "microedition.media.version: 1.0\n"
           "microedition.pim.version: 1.0\n"
           "microedition.location.version: 1.0\n"
           "supports.mixing: true\n"
           "supports.audio.capture: true\n"
           "supports.video.capture: true\n"
           "supports.recording: true\n"
           "com.siemens.mp.systemfolder.ringingtone: fs/MyStuff/Ringtones\n"
           "com.siemens.mp.systemfolder.pictures: fs/MyStuff/Pictures\n"
           "com.siemens.OSVersion: 11\n"
           "device.imei: 000000000000000\n"
           "com.siemens.IMEI: 000000000000000\n"
           "com.sonyericsson.imei: IMEI 00460101-501594-5-00\n"
           "com.nokia.mid.impl.isa.visual_radio_operator_id: 0\n"
           "com.nokia.mid.impl.isa.visual_radio_channel_freq: 0\n"
           "com.nokia.mid.ui.DirectGraphics.PIXEL_FORMAT: 565\n"
           "wireless.messaging.sms.smsc: +8613800010000\n";
}

bool ProfilesManager::loadConfig(const std::string& filePath, ProfileModel& outModel) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    return outModel.deserializeJson(buffer.str());
}

bool ProfilesManager::saveConfig(const std::string& filePath, const ProfileModel& model) {
    try {
        fs::path p(filePath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    } catch (...) {}

    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << model.serializeJson();
    return f.good();
}

std::vector<std::string> ProfilesManager::getProfilesList(const std::string& profilesDir) {
    std::vector<std::string> list;
    try {
        if (fs::exists(profilesDir) && fs::is_directory(profilesDir)) {
            for (const auto& entry : fs::directory_iterator(profilesDir)) {
                if (entry.is_directory()) {
                    list.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (...) {}
    std::sort(list.begin(), list.end());
    return list;
}

} // namespace universal_loader::config
