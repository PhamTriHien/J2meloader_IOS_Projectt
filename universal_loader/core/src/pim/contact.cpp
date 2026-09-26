#include "contact.h"
#include <sstream>
#include <algorithm>

namespace universal_loader {
namespace pim {

Contact::Contact(PIMList* list)
    : PIMItem(list)
{
}

int Contact::getDataType(int field) const {
    switch (field) {
        case CONTACT_ADDR:
        case CONTACT_NAME:
            return PIM_TYPE_STRING_ARRAY;

        case CONTACT_BIRTHDAY:
        case CONTACT_REVISION:
            return PIM_TYPE_DATE;

        case CONTACT_CLASS:
            return PIM_TYPE_INT;

        case CONTACT_PHOTO:
        case CONTACT_PUBLIC_KEY:
            return PIM_TYPE_BINARY;

        case CONTACT_EMAIL:
        case CONTACT_FORMATTED_ADDR:
        case CONTACT_FORMATTED_NAME:
        case CONTACT_NICKNAME:
        case CONTACT_NOTE:
        case CONTACT_ORG:
        case CONTACT_PHOTO_URL:
        case CONTACT_PUBLIC_KEY_STRING:
        case CONTACT_TEL:
        case CONTACT_TITLE:
        case CONTACT_UID:
        case CONTACT_URL:
        default:
            return PIM_TYPE_STRING;
    }
}

int Contact::getPreferredIndex(int field) const {
    int cnt = countValues(field);
    if (cnt <= 0) {
        return -1;
    }
    for (int i = 0; i < cnt; ++i) {
        if ((getAttributes(field, i) & ATTR_PREFERRED) != 0) {
            return i;
        }
    }
    return 0; // default to first value if none marked preferred
}

std::string Contact::getFormattedName() const {
    if (countValues(CONTACT_FORMATTED_NAME) > 0) {
        return getString(CONTACT_FORMATTED_NAME, 0);
    }
    if (countValues(CONTACT_NAME) > 0) {
        auto names = getStringArray(CONTACT_NAME, 0);
        std::string res;
        if (names.size() > NAME_GIVEN && !names[NAME_GIVEN].empty()) {
            res += names[NAME_GIVEN];
        }
        if (names.size() > NAME_FAMILY && !names[NAME_FAMILY].empty()) {
            if (!res.empty()) res += " ";
            res += names[NAME_FAMILY];
        }
        if (!res.empty()) return res;
    }
    if (countValues(CONTACT_NICKNAME) > 0) {
        return getString(CONTACT_NICKNAME, 0);
    }
    return "";
}

void Contact::setName(const std::string& family, const std::string& given,
                      const std::string& other, const std::string& prefix, const std::string& suffix) {
    std::vector<std::string> name(NAME_ARRAY_SIZE);
    name[NAME_FAMILY] = family;
    name[NAME_GIVEN]  = given;
    name[NAME_OTHER]  = other;
    name[NAME_PREFIX] = prefix;
    name[NAME_SUFFIX] = suffix;

    if (countValues(CONTACT_NAME) == 0) {
        addStringArray(CONTACT_NAME, ATTR_NONE, name);
    } else {
        setStringArray(CONTACT_NAME, 0, ATTR_NONE, name);
    }

    std::string formatted;
    if (!given.empty()) formatted += given;
    if (!family.empty()) {
        if (!formatted.empty()) formatted += " ";
        formatted += family;
    }
    if (!formatted.empty()) {
        if (countValues(CONTACT_FORMATTED_NAME) == 0) {
            addString(CONTACT_FORMATTED_NAME, ATTR_NONE, formatted);
        } else {
            setString(CONTACT_FORMATTED_NAME, 0, ATTR_NONE, formatted);
        }
    }
}

void Contact::setAddress(int index, int attributes,
                         const std::string& street, const std::string& locality,
                         const std::string& region, const std::string& postalCode,
                         const std::string& country, const std::string& poBox,
                         const std::string& extra) {
    std::vector<std::string> addr(ADDR_ARRAY_SIZE);
    addr[ADDR_POBOX]      = poBox;
    addr[ADDR_EXTRA]      = extra;
    addr[ADDR_STREET]     = street;
    addr[ADDR_LOCALITY]   = locality;
    addr[ADDR_REGION]     = region;
    addr[ADDR_POSTALCODE] = postalCode;
    addr[ADDR_COUNTRY]    = country;

    if (index < countValues(CONTACT_ADDR)) {
        setStringArray(CONTACT_ADDR, index, attributes, addr);
    } else {
        addStringArray(CONTACT_ADDR, attributes, addr);
    }
}

std::string Contact::toVCard() const {
    std::ostringstream ss;
    ss << "BEGIN:VCARD\r\n";
    ss << "VERSION:2.1\r\n";

    if (countValues(CONTACT_NAME) > 0) {
        auto n = getStringArray(CONTACT_NAME, 0);
        ss << "N:"
           << (n.size() > NAME_FAMILY ? n[NAME_FAMILY] : "") << ";"
           << (n.size() > NAME_GIVEN  ? n[NAME_GIVEN]  : "") << ";"
           << (n.size() > NAME_OTHER  ? n[NAME_OTHER]  : "") << ";"
           << (n.size() > NAME_PREFIX ? n[NAME_PREFIX] : "") << ";"
           << (n.size() > NAME_SUFFIX ? n[NAME_SUFFIX] : "") << "\r\n";
    }

    std::string fn = getFormattedName();
    if (!fn.empty()) {
        ss << "FN:" << fn << "\r\n";
    }

    for (int i = 0; i < countValues(CONTACT_TEL); ++i) {
        int attr = getAttributes(CONTACT_TEL, i);
        ss << "TEL";
        if (attr & ATTR_MOBILE) ss << ";CELL";
        if (attr & ATTR_WORK)   ss << ";WORK";
        if (attr & ATTR_HOME)   ss << ";HOME";
        if (attr & ATTR_FAX)    ss << ";FAX";
        if (attr & ATTR_PREFERRED) ss << ";PREF";
        ss << ";VOICE:" << getString(CONTACT_TEL, i) << "\r\n";
    }

    for (int i = 0; i < countValues(CONTACT_EMAIL); ++i) {
        int attr = getAttributes(CONTACT_EMAIL, i);
        ss << "EMAIL";
        if (attr & ATTR_WORK) ss << ";WORK";
        if (attr & ATTR_HOME) ss << ";HOME";
        if (attr & ATTR_PREFERRED) ss << ";PREF";
        ss << ";INTERNET:" << getString(CONTACT_EMAIL, i) << "\r\n";
    }

    for (int i = 0; i < countValues(CONTACT_ADDR); ++i) {
        int attr = getAttributes(CONTACT_ADDR, i);
        auto ad = getStringArray(CONTACT_ADDR, i);
        ss << "ADR";
        if (attr & ATTR_WORK) ss << ";WORK";
        if (attr & ATTR_HOME) ss << ";HOME";
        ss << ":";
        for (size_t a = 0; a < ADDR_ARRAY_SIZE; ++a) {
            if (a < ad.size()) ss << ad[a];
            if (a + 1 < ADDR_ARRAY_SIZE) ss << ";";
        }
        ss << "\r\n";
    }

    if (countValues(CONTACT_ORG) > 0) {
        ss << "ORG:" << getString(CONTACT_ORG, 0) << "\r\n";
    }
    if (countValues(CONTACT_TITLE) > 0) {
        ss << "TITLE:" << getString(CONTACT_TITLE, 0) << "\r\n";
    }
    if (countValues(CONTACT_NOTE) > 0) {
        ss << "NOTE:" << getString(CONTACT_NOTE, 0) << "\r\n";
    }
    if (countValues(CONTACT_URL) > 0) {
        ss << "URL:" << getString(CONTACT_URL, 0) << "\r\n";
    }
    if (!m_uid.empty()) {
        ss << "UID:" << m_uid << "\r\n";
    }

    ss << "END:VCARD\r\n";
    return ss.str();
}

std::shared_ptr<Contact> Contact::fromVCard(const std::string& vcardText, PIMList* list) {
    auto contact = std::make_shared<Contact>(list);
    std::istringstream stream(vcardText);
    std::string line;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string tag = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        // Uppercase tag for matching
        std::string tagUpper = tag;
        std::transform(tagUpper.begin(), tagUpper.end(), tagUpper.begin(), ::toupper);

        if (tagUpper.rfind("N", 0) == 0 && (tagUpper == "N" || tagUpper[1] == ';')) {
            // Split by ';'
            std::vector<std::string> parts;
            size_t c = 0;
            while (c < val.size()) {
                size_t semi = val.find(';', c);
                if (semi == std::string::npos) {
                    parts.push_back(val.substr(c));
                    break;
                }
                parts.push_back(val.substr(c, semi - c));
                c = semi + 1;
            }
            while (parts.size() < NAME_ARRAY_SIZE) parts.emplace_back("");
            contact->setName(parts[NAME_FAMILY], parts[NAME_GIVEN], parts[NAME_OTHER], parts[NAME_PREFIX], parts[NAME_SUFFIX]);
        } else if (tagUpper.rfind("FN", 0) == 0) {
            contact->addString(CONTACT_FORMATTED_NAME, ATTR_NONE, val);
        } else if (tagUpper.rfind("TEL", 0) == 0) {
            int attr = ATTR_NONE;
            if (tagUpper.find("CELL") != std::string::npos || tagUpper.find("MOBILE") != std::string::npos) attr |= ATTR_MOBILE;
            if (tagUpper.find("WORK") != std::string::npos) attr |= ATTR_WORK;
            if (tagUpper.find("HOME") != std::string::npos) attr |= ATTR_HOME;
            if (tagUpper.find("FAX") != std::string::npos)  attr |= ATTR_FAX;
            if (tagUpper.find("PREF") != std::string::npos) attr |= ATTR_PREFERRED;
            contact->addString(CONTACT_TEL, attr, val);
        } else if (tagUpper.rfind("EMAIL", 0) == 0) {
            int attr = ATTR_NONE;
            if (tagUpper.find("WORK") != std::string::npos) attr |= ATTR_WORK;
            if (tagUpper.find("HOME") != std::string::npos) attr |= ATTR_HOME;
            if (tagUpper.find("PREF") != std::string::npos) attr |= ATTR_PREFERRED;
            contact->addString(CONTACT_EMAIL, attr, val);
        } else if (tagUpper.rfind("ADR", 0) == 0) {
            int attr = ATTR_NONE;
            if (tagUpper.find("WORK") != std::string::npos) attr |= ATTR_WORK;
            if (tagUpper.find("HOME") != std::string::npos) attr |= ATTR_HOME;
            std::vector<std::string> parts;
            size_t c = 0;
            while (c < val.size()) {
                size_t semi = val.find(';', c);
                if (semi == std::string::npos) {
                    parts.push_back(val.substr(c));
                    break;
                }
                parts.push_back(val.substr(c, semi - c));
                c = semi + 1;
            }
            while (parts.size() < ADDR_ARRAY_SIZE) parts.emplace_back("");
            contact->setAddress(contact->countValues(CONTACT_ADDR), attr,
                                parts[ADDR_STREET], parts[ADDR_LOCALITY], parts[ADDR_REGION],
                                parts[ADDR_POSTALCODE], parts[ADDR_COUNTRY],
                                parts[ADDR_POBOX], parts[ADDR_EXTRA]);
        } else if (tagUpper.rfind("ORG", 0) == 0) {
            contact->addString(CONTACT_ORG, ATTR_NONE, val);
        } else if (tagUpper.rfind("TITLE", 0) == 0) {
            contact->addString(CONTACT_TITLE, ATTR_NONE, val);
        } else if (tagUpper.rfind("NOTE", 0) == 0) {
            contact->addString(CONTACT_NOTE, ATTR_NONE, val);
        } else if (tagUpper.rfind("URL", 0) == 0) {
            contact->addString(CONTACT_URL, ATTR_NONE, val);
        } else if (tagUpper.rfind("UID", 0) == 0) {
            contact->setUid(val);
        }
    }

    return contact;
}

} // namespace pim
} // namespace universal_loader
