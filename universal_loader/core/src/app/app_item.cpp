#include "app_item.h"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace universal_loader {
namespace app {

static std::string escapeJson(const std::string& input) {
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

static std::string unescapeJson(const std::string& input) {
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
                default:   out += input[i]; break;
            }
        } else {
            out += input[i];
        }
    }
    return out;
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

std::string AppItem::serializeJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": " << id << ",\n";
    ss << "  \"path\": \"" << escapeJson(path) << "\",\n";
    ss << "  \"title\": \"" << escapeJson(title) << "\",\n";
    ss << "  \"author\": \"" << escapeJson(author) << "\",\n";
    ss << "  \"version\": \"" << escapeJson(version) << "\",\n";
    ss << "  \"imagePath\": \"" << escapeJson(imagePath) << "\",\n";
    ss << "  \"installedTimestamp\": " << installedTimestamp << ",\n";
    ss << "  \"lastPlayedTimestamp\": " << lastPlayedTimestamp << ",\n";
    ss << "  \"playCount\": " << playCount << "\n";
    ss << "}";
    return ss.str();
}

bool AppItem::deserializeJson(const std::string& json) {
    size_t pos = 0;
    auto findNextToken = [&](size_t& outKeyStart, size_t& outKeyEnd,
                             size_t& outValStart, size_t& outValEnd) -> bool {
        size_t quote1 = json.find('\"', pos);
        if (quote1 == std::string::npos) return false;
        size_t quote2 = json.find('\"', quote1 + 1);
        if (quote2 == std::string::npos) return false;

        outKeyStart = quote1 + 1;
        outKeyEnd = quote2;

        size_t colon = json.find(':', quote2 + 1);
        if (colon == std::string::npos) return false;

        size_t vStart = colon + 1;
        while (vStart < json.size() && std::isspace(static_cast<unsigned char>(json[vStart]))) {
            vStart++;
        }
        if (vStart >= json.size()) return false;

        size_t vEnd = vStart;
        if (json[vStart] == '\"') {
            vStart++;
            size_t endQ = vStart;
            while (endQ < json.size()) {
                if (json[endQ] == '\"' && json[endQ - 1] != '\\') {
                    break;
                }
                endQ++;
            }
            vEnd = endQ;
            pos = (endQ < json.size()) ? endQ + 1 : json.size();
        } else {
            while (vEnd < json.size() && json[vEnd] != ',' && json[vEnd] != '}' && json[vEnd] != '\n') {
                vEnd++;
            }
            pos = vEnd;
        }

        outValStart = vStart;
        outValEnd = vEnd;
        return true;
    };

    size_t ks = 0, ke = 0, vs = 0, ve = 0;
    while (findNextToken(ks, ke, vs, ve)) {
        std::string key = json.substr(ks, ke - ks);
        std::string rawVal = json.substr(vs, ve - vs);
        std::string val = trim(rawVal);

        if (key == "id") {
            try { id = std::stoi(val); } catch (...) {}
        } else if (key == "path") {
            path = unescapeJson(val);
        } else if (key == "title") {
            title = unescapeJson(val);
        } else if (key == "author") {
            author = unescapeJson(val);
        } else if (key == "version") {
            version = unescapeJson(val);
        } else if (key == "imagePath") {
            imagePath = unescapeJson(val);
        } else if (key == "installedTimestamp") {
            try { installedTimestamp = std::stoll(val); } catch (...) {}
        } else if (key == "lastPlayedTimestamp") {
            try { lastPlayedTimestamp = std::stoll(val); } catch (...) {}
        } else if (key == "playCount") {
            try { playCount = std::stoi(val); } catch (...) {}
        }
    }
    return true;
}

} // namespace app
} // namespace universal_loader
