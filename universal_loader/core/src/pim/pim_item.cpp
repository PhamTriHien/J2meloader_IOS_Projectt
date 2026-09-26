#include "pim_item.h"
#include "pim_list.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace universal_loader {
namespace pim {

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

static size_t findMatchingBracket(const std::string& str, size_t openPos, char openChar, char closeChar) {
    int depth = 0;
    for (size_t i = openPos; i < str.size(); ++i) {
        if (str[i] == openChar) depth++;
        else if (str[i] == closeChar) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

PIMItem::PIMItem(PIMList* list)
    : m_list(list)
    , m_modified(false)
{
}

void PIMItem::commit() {
    m_modified = false;
    if (m_list) {
        // Parent list will be marked modified or saved by caller/manager
    }
}

std::vector<int> PIMItem::getFields() const {
    std::vector<int> res;
    for (const auto& kv : m_fields) {
        if (!kv.second.values.empty()) {
            res.push_back(kv.first);
        }
    }
    return res;
}

int PIMItem::getDataType(int /*field*/) const {
    return PIM_TYPE_STRING;
}

int PIMItem::countValues(int field) const {
    auto it = m_fields.find(field);
    if (it == m_fields.end()) {
        return 0;
    }
    return static_cast<int>(it->second.values.size());
}

int PIMItem::getAttributes(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->attributes : ATTR_NONE;
}

void PIMItem::removeValue(int field, int index) {
    auto it = m_fields.find(field);
    if (it != m_fields.end()) {
        if (index >= 0 && index < static_cast<int>(it->second.values.size())) {
            it->second.values.erase(it->second.values.begin() + index);
            m_modified = true;
        }
    }
}

PIMValue* PIMItem::getOrCreateValue(int field, int index) {
    auto& fData = m_fields[field];
    fData.fieldId = field;
    fData.dataType = getDataType(field);
    if (index >= 0 && index < static_cast<int>(fData.values.size())) {
        return &fData.values[index];
    }
    if (index == static_cast<int>(fData.values.size())) {
        fData.values.emplace_back();
        return &fData.values.back();
    }
    return nullptr;
}

const PIMValue* PIMItem::getValue(int field, int index) const {
    auto it = m_fields.find(field);
    if (it == m_fields.end()) return nullptr;
    if (index < 0 || index >= static_cast<int>(it->second.values.size())) return nullptr;
    return &it->second.values[index];
}

std::string PIMItem::getString(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->stringVal : "";
}

int PIMItem::getInt(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->intVal : 0;
}

int64_t PIMItem::getDate(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->dateVal : 0;
}

bool PIMItem::getBoolean(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->boolVal : false;
}

std::vector<uint8_t> PIMItem::getBinary(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->binaryVal : std::vector<uint8_t>();
}

std::vector<std::string> PIMItem::getStringArray(int field, int index) const {
    const PIMValue* val = getValue(field, index);
    return val ? val->stringArrayVal : std::vector<std::string>();
}

void PIMItem::setString(int field, int index, int attributes, const std::string& value) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        val->stringVal = value;
        m_modified = true;
    }
}

void PIMItem::setInt(int field, int index, int attributes, int value) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        val->intVal = value;
        m_modified = true;
    }
}

void PIMItem::setDate(int field, int index, int attributes, int64_t value) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        val->dateVal = value;
        m_modified = true;
    }
}

void PIMItem::setBoolean(int field, int index, int attributes, bool value) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        val->boolVal = value;
        m_modified = true;
    }
}

void PIMItem::setBinary(int field, int index, int attributes, const std::vector<uint8_t>& value, int offset, int length) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        if (offset >= 0 && length >= 0 && offset + length <= static_cast<int>(value.size())) {
            val->binaryVal.assign(value.begin() + offset, value.begin() + offset + length);
        } else {
            val->binaryVal = value;
        }
        m_modified = true;
    }
}

void PIMItem::setStringArray(int field, int index, int attributes, const std::vector<std::string>& value) {
    PIMValue* val = getOrCreateValue(field, index);
    if (val) {
        val->attributes = attributes;
        val->stringArrayVal = value;
        m_modified = true;
    }
}

void PIMItem::addString(int field, int attributes, const std::string& value) {
    int idx = countValues(field);
    setString(field, idx, attributes, value);
}

void PIMItem::addInt(int field, int attributes, int value) {
    int idx = countValues(field);
    setInt(field, idx, attributes, value);
}

void PIMItem::addDate(int field, int attributes, int64_t value) {
    int idx = countValues(field);
    setDate(field, idx, attributes, value);
}

void PIMItem::addBoolean(int field, int attributes, bool value) {
    int idx = countValues(field);
    setBoolean(field, idx, attributes, value);
}

void PIMItem::addBinary(int field, int attributes, const std::vector<uint8_t>& value, int offset, int length) {
    int idx = countValues(field);
    setBinary(field, idx, attributes, value, offset, length);
}

void PIMItem::addStringArray(int field, int attributes, const std::vector<std::string>& value) {
    int idx = countValues(field);
    setStringArray(field, idx, attributes, value);
}

void PIMItem::addToCategory(const std::string& category) {
    if (category.empty()) return;
    if (std::find(m_categories.begin(), m_categories.end(), category) == m_categories.end()) {
        if (static_cast<int>(m_categories.size()) < maxCategories()) {
            m_categories.push_back(category);
            m_modified = true;
        }
    }
}

void PIMItem::removeFromCategory(const std::string& category) {
    auto it = std::remove(m_categories.begin(), m_categories.end(), category);
    if (it != m_categories.end()) {
        m_categories.erase(it, m_categories.end());
        m_modified = true;
    }
}

std::string PIMItem::toJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"uid\": \"" << escapeJson(m_uid) << "\",\n";
    ss << "  \"categories\": [";
    for (size_t i = 0; i < m_categories.size(); ++i) {
        ss << "\"" << escapeJson(m_categories[i]) << "\"";
        if (i + 1 < m_categories.size()) ss << ", ";
    }
    ss << "],\n";
    ss << "  \"fields\": [\n";
    bool firstField = true;
    for (const auto& kv : m_fields) {
        int fid = kv.first;
        const auto& fData = kv.second;
        if (fData.values.empty()) continue;
        if (!firstField) ss << ",\n";
        firstField = false;

        ss << "    {\n";
        ss << "      \"id\": " << fid << ",\n";
        ss << "      \"type\": " << fData.dataType << ",\n";
        ss << "      \"values\": [\n";
        for (size_t v = 0; v < fData.values.size(); ++v) {
            const auto& val = fData.values[v];
            ss << "        {\"attr\": " << val.attributes;
            switch (fData.dataType) {
                case PIM_TYPE_STRING:
                    ss << ", \"str\": \"" << escapeJson(val.stringVal) << "\"}";
                    break;
                case PIM_TYPE_INT:
                    ss << ", \"int\": " << val.intVal << "}";
                    break;
                case PIM_TYPE_DATE:
                    ss << ", \"date\": " << val.dateVal << "}";
                    break;
                case PIM_TYPE_BOOLEAN:
                    ss << ", \"bool\": " << (val.boolVal ? "true" : "false") << "}";
                    break;
                case PIM_TYPE_STRING_ARRAY: {
                    ss << ", \"arr\": [";
                    for (size_t a = 0; a < val.stringArrayVal.size(); ++a) {
                        ss << "\"" << escapeJson(val.stringArrayVal[a]) << "\"";
                        if (a + 1 < val.stringArrayVal.size()) ss << ", ";
                    }
                    ss << "]}";
                    break;
                }
                case PIM_TYPE_BINARY:
                    // base64 or hex
                    ss << ", \"bin\": \"\"}";
                    break;
                default:
                    ss << "}";
                    break;
            }
            if (v + 1 < fData.values.size()) ss << ",";
            ss << "\n";
        }
        ss << "      ]\n";
        ss << "    }";
    }
    ss << "\n  ]\n";
    ss << "}";
    return ss.str();
}

void PIMItem::fromJson(const std::string& json) {
    m_fields.clear();
    m_categories.clear();

    // Extract uid
    size_t posUid = json.find("\"uid\":");
    if (posUid != std::string::npos) {
        size_t q1 = json.find('\"', posUid + 6);
        if (q1 != std::string::npos) {
            size_t q2 = json.find('\"', q1 + 1);
            if (q2 != std::string::npos) {
                m_uid = unescapeJson(json.substr(q1 + 1, q2 - q1 - 1));
            }
        }
    }

    // Extract categories
    size_t posCat = json.find("\"categories\":");
    if (posCat != std::string::npos) {
        size_t b1 = json.find('[', posCat);
        size_t b2 = json.find(']', b1);
        if (b1 != std::string::npos && b2 != std::string::npos) {
            std::string catStr = json.substr(b1 + 1, b2 - b1 - 1);
            size_t cur = 0;
            while ((cur = catStr.find('\"', cur)) != std::string::npos) {
                size_t next = catStr.find('\"', cur + 1);
                if (next == std::string::npos) break;
                m_categories.push_back(unescapeJson(catStr.substr(cur + 1, next - cur - 1)));
                cur = next + 1;
            }
        }
    }

    // Parse fields
    size_t posFields = json.find("\"fields\":");
    if (posFields == std::string::npos) return;

    size_t curField = posFields;
    while ((curField = json.find("\"id\":", curField)) != std::string::npos) {
        size_t idStart = json.find(':', curField);
        if (idStart == std::string::npos) break;
        int fid = std::atoi(json.c_str() + idStart + 1);

        size_t typePos = json.find("\"type\":", idStart);
        if (typePos == std::string::npos) break;
        size_t typeStart = json.find(':', typePos);
        int ftype = std::atoi(json.c_str() + typeStart + 1);

        size_t valPos = json.find("\"values\":", typeStart);
        if (valPos == std::string::npos) break;
        size_t arrStart = json.find('[', valPos);
        if (arrStart == std::string::npos) break;
        size_t arrEnd = findMatchingBracket(json, arrStart, '[', ']');
        if (arrEnd == std::string::npos) break;

        // Parse individual value objects within [arrStart, arrEnd]
        std::string valsBlock = json.substr(arrStart, arrEnd - arrStart + 1);
        size_t vObj = 0;
        while ((vObj = valsBlock.find('{', vObj)) != std::string::npos) {
            size_t vEnd = findMatchingBracket(valsBlock, vObj, '{', '}');
            if (vEnd == std::string::npos) break;
            std::string itemObj = valsBlock.substr(vObj, vEnd - vObj + 1);

            int attr = ATTR_NONE;
            size_t attrPos = itemObj.find("\"attr\":");
            if (attrPos != std::string::npos) {
                attr = std::atoi(itemObj.c_str() + attrPos + 7);
            }

            if (ftype == PIM_TYPE_STRING) {
                size_t strPos = itemObj.find("\"str\":");
                if (strPos != std::string::npos) {
                    size_t q1 = itemObj.find('\"', strPos + 6);
                    size_t q2 = itemObj.find('\"', q1 + 1);
                    std::string sVal = (q1 != std::string::npos && q2 != std::string::npos)
                                           ? unescapeJson(itemObj.substr(q1 + 1, q2 - q1 - 1))
                                           : "";
                    addString(fid, attr, sVal);
                }
            } else if (ftype == PIM_TYPE_INT) {
                size_t intPos = itemObj.find("\"int\":");
                if (intPos != std::string::npos) {
                    int iVal = std::atoi(itemObj.c_str() + intPos + 6);
                    addInt(fid, attr, iVal);
                }
            } else if (ftype == PIM_TYPE_DATE) {
                size_t datePos = itemObj.find("\"date\":");
                if (datePos != std::string::npos) {
                    int64_t dVal = std::strtoll(itemObj.c_str() + datePos + 7, nullptr, 10);
                    addDate(fid, attr, dVal);
                }
            } else if (ftype == PIM_TYPE_BOOLEAN) {
                size_t boolPos = itemObj.find("\"bool\":");
                if (boolPos != std::string::npos) {
                    bool bVal = (itemObj.find("true", boolPos) != std::string::npos);
                    addBoolean(fid, attr, bVal);
                }
            } else if (ftype == PIM_TYPE_STRING_ARRAY) {
                size_t arrP = itemObj.find("\"arr\":");
                if (arrP != std::string::npos) {
                    size_t bStart = itemObj.find('[', arrP);
                    size_t bEnd = (bStart != std::string::npos) ? findMatchingBracket(itemObj, bStart, '[', ']') : std::string::npos;
                    std::vector<std::string> arrElems;
                    if (bStart != std::string::npos && bEnd != std::string::npos) {
                        std::string sub = itemObj.substr(bStart + 1, bEnd - bStart - 1);
                        size_t c = 0;
                        while ((c = sub.find('\"', c)) != std::string::npos) {
                            size_t n = sub.find('\"', c + 1);
                            if (n == std::string::npos) break;
                            arrElems.push_back(unescapeJson(sub.substr(c + 1, n - c - 1)));
                            c = n + 1;
                        }
                    }
                    addStringArray(fid, attr, arrElems);
                }
            }

            vObj = vEnd + 1;
        }

        curField = arrEnd + 1;
    }
}

} // namespace pim
} // namespace universal_loader
