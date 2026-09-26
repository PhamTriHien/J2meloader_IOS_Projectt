#include "pim_list.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace universal_loader {
namespace pim {

static std::string toLowerStr(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

PIMList::PIMList(const std::string& name, int type, int mode)
    : m_name(name)
    , m_type(type)
    , m_mode(mode)
    , m_open(true)
{
    m_categories.push_back("Personal");
    m_categories.push_back("Business");
}

bool PIMList::isModified() const {
    for (const auto& item : m_items) {
        if (item && item->isModified()) return true;
    }
    return false;
}

void PIMList::addCategory(const std::string& category) {
    if (category.empty()) return;
    if (std::find(m_categories.begin(), m_categories.end(), category) == m_categories.end()) {
        if (static_cast<int>(m_categories.size()) < maxCategories()) {
            m_categories.push_back(category);
        }
    }
}

void PIMList::deleteCategory(const std::string& category, bool deleteItems) {
    auto it = std::remove(m_categories.begin(), m_categories.end(), category);
    if (it != m_categories.end()) {
        m_categories.erase(it, m_categories.end());
    }

    if (deleteItems) {
        m_items.erase(std::remove_if(m_items.begin(), m_items.end(), [&](const std::shared_ptr<PIMItem>& item) {
            const auto& cats = item->getCategories();
            return std::find(cats.begin(), cats.end(), category) != cats.end();
        }), m_items.end());
    } else {
        for (auto& item : m_items) {
            item->removeFromCategory(category);
        }
    }
}

bool PIMList::isCategory(const std::string& category) const {
    return std::find(m_categories.begin(), m_categories.end(), category) != m_categories.end();
}

int PIMList::maxValues(int /*field*/) const {
    return 10;
}

int PIMList::getFieldDataType(int /*field*/) const {
    return PIM_TYPE_STRING;
}

std::string PIMList::getFieldLabel(int /*field*/) const {
    return "Unknown";
}

std::vector<int> PIMList::getSupportedFields() const {
    return {};
}

std::vector<int> PIMList::getSupportedAttributes(int /*field*/) const {
    return {ATTR_NONE, ATTR_PREFERRED, ATTR_HOME, ATTR_WORK, ATTR_MOBILE};
}

std::vector<int> PIMList::getSupportedArrayElements(int /*stringArrayField*/) const {
    return {};
}

std::vector<std::shared_ptr<PIMItem>> PIMList::items() const {
    return m_items;
}

std::vector<std::shared_ptr<PIMItem>> PIMList::items(const std::shared_ptr<PIMItem>& matchingItem) const {
    if (!matchingItem) return m_items;
    std::vector<std::shared_ptr<PIMItem>> results;
    std::vector<int> matchFields = matchingItem->getFields();

    for (const auto& item : m_items) {
        bool allMatch = true;
        for (int fid : matchFields) {
            int dType = item->getDataType(fid);
            if (item->countValues(fid) == 0) {
                allMatch = false;
                break;
            }
            if (dType == PIM_TYPE_STRING) {
                std::string targetVal = toLowerStr(matchingItem->getString(fid, 0));
                bool strFound = false;
                for (int v = 0; v < item->countValues(fid); ++v) {
                    if (toLowerStr(item->getString(fid, v)).find(targetVal) != std::string::npos) {
                        strFound = true;
                        break;
                    }
                }
                if (!strFound) { allMatch = false; break; }
            } else if (dType == PIM_TYPE_INT) {
                if (item->getInt(fid, 0) != matchingItem->getInt(fid, 0)) {
                    allMatch = false;
                    break;
                }
            } else if (dType == PIM_TYPE_DATE) {
                if (item->getDate(fid, 0) != matchingItem->getDate(fid, 0)) {
                    allMatch = false;
                    break;
                }
            }
        }
        if (allMatch) {
            results.push_back(item);
        }
    }
    return results;
}

std::vector<std::shared_ptr<PIMItem>> PIMList::items(const std::string& matchingValue) const {
    if (matchingValue.empty()) return m_items;
    std::string needle = toLowerStr(matchingValue);
    std::vector<std::shared_ptr<PIMItem>> results;

    for (const auto& item : m_items) {
        bool found = false;
        for (int fid : item->getFields()) {
            if (item->getDataType(fid) == PIM_TYPE_STRING) {
                for (int v = 0; v < item->countValues(fid); ++v) {
                    if (toLowerStr(item->getString(fid, v)).find(needle) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
            } else if (item->getDataType(fid) == PIM_TYPE_STRING_ARRAY) {
                for (int v = 0; v < item->countValues(fid); ++v) {
                    for (const auto& elem : item->getStringArray(fid, v)) {
                        if (toLowerStr(elem).find(needle) != std::string::npos) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }
            if (found) break;
        }
        if (found) {
            results.push_back(item);
        }
    }
    return results;
}

std::vector<std::shared_ptr<PIMItem>> PIMList::itemsByCategory(const std::string& category) const {
    std::vector<std::shared_ptr<PIMItem>> results;
    for (const auto& item : m_items) {
        const auto& cats = item->getCategories();
        if (category.empty()) {
            if (cats.empty()) results.push_back(item);
        } else {
            if (std::find(cats.begin(), cats.end(), category) != cats.end()) {
                results.push_back(item);
            }
        }
    }
    return results;
}

void PIMList::addItem(const std::shared_ptr<PIMItem>& item) {
    if (item) {
        item->setPIMList(this);
        if (std::find(m_items.begin(), m_items.end(), item) == m_items.end()) {
            m_items.push_back(item);
        }
    }
}

void PIMList::removeItem(const std::shared_ptr<PIMItem>& item) {
    auto it = std::remove(m_items.begin(), m_items.end(), item);
    if (it != m_items.end()) {
        item->setPIMList(nullptr);
        m_items.erase(it, m_items.end());
    }
}

void PIMList::saveToFile(const std::string& filePath) const {
    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"name\": \"" << m_name << "\",\n";
    out << "  \"type\": " << m_type << ",\n";
    out << "  \"categories\": [";
    for (size_t i = 0; i < m_categories.size(); ++i) {
        out << "\"" << m_categories[i] << "\"";
        if (i + 1 < m_categories.size()) out << ", ";
    }
    out << "],\n";
    out << "  \"items\": [\n";
    for (size_t i = 0; i < m_items.size(); ++i) {
        out << m_items[i]->toJson();
        if (i + 1 < m_items.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void PIMList::loadFromFile(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) return;

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    if (content.empty()) return;

    m_items.clear();

    // Parse categories
    size_t posCat = content.find("\"categories\":");
    if (posCat != std::string::npos) {
        size_t b1 = content.find('[', posCat);
        size_t b2 = content.find(']', b1);
        if (b1 != std::string::npos && b2 != std::string::npos) {
            m_categories.clear();
            std::string sub = content.substr(b1 + 1, b2 - b1 - 1);
            size_t cur = 0;
            while ((cur = sub.find('\"', cur)) != std::string::npos) {
                size_t next = sub.find('\"', cur + 1);
                if (next == std::string::npos) break;
                m_categories.push_back(sub.substr(cur + 1, next - cur - 1));
                cur = next + 1;
            }
        }
    }

    // Parse items array
    size_t posItems = content.find("\"items\":");
    if (posItems == std::string::npos) return;
    size_t arrStart = content.find('[', posItems);
    if (arrStart == std::string::npos) return;

    size_t cur = arrStart + 1;
    while (cur < content.size()) {
        size_t startObj = content.find('{', cur);
        if (startObj == std::string::npos) break;
        // Find matching closing brace taking nested braces into account
        int depth = 0;
        size_t endObj = std::string::npos;
        for (size_t p = startObj; p < content.size(); ++p) {
            if (content[p] == '{') depth++;
            else if (content[p] == '}') {
                depth--;
                if (depth == 0) {
                    endObj = p;
                    break;
                }
            }
        }
        if (endObj == std::string::npos) break;

        std::string itemJson = content.substr(startObj, endObj - startObj + 1);
        std::shared_ptr<PIMItem> newItem;
        if (m_type == PIM_CONTACT_LIST) {
            newItem = std::make_shared<Contact>(this);
        } else if (m_type == PIM_EVENT_LIST) {
            newItem = std::make_shared<Event>(this);
        } else if (m_type == PIM_TODO_LIST) {
            newItem = std::make_shared<ToDo>(this);
        }
        if (newItem) {
            newItem->fromJson(itemJson);
            newItem->commit();
            m_items.push_back(newItem);
        }

        cur = endObj + 1;
    }
}

// -------------------------------------------------------------
// ContactList
// -------------------------------------------------------------
ContactList::ContactList(const std::string& name, int mode)
    : PIMList(name, PIM_CONTACT_LIST, mode)
{
}

int ContactList::getFieldDataType(int field) const {
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
        default:
            return PIM_TYPE_STRING;
    }
}

std::string ContactList::getFieldLabel(int field) const {
    switch (field) {
        case CONTACT_ADDR:           return "Address";
        case CONTACT_BIRTHDAY:       return "Birthday";
        case CONTACT_CLASS:          return "Class";
        case CONTACT_EMAIL:          return "Email";
        case CONTACT_FORMATTED_ADDR: return "Formatted Address";
        case CONTACT_FORMATTED_NAME: return "Formatted Name";
        case CONTACT_NAME:           return "Name";
        case CONTACT_NICKNAME:       return "Nickname";
        case CONTACT_NOTE:           return "Note";
        case CONTACT_ORG:            return "Organization";
        case CONTACT_PHOTO:          return "Photo";
        case CONTACT_PHOTO_URL:      return "Photo URL";
        case CONTACT_PUBLIC_KEY:     return "Public Key";
        case CONTACT_REVISION:       return "Revision";
        case CONTACT_TEL:            return "Telephone";
        case CONTACT_TITLE:          return "Job Title";
        case CONTACT_UID:            return "UID";
        case CONTACT_URL:            return "URL";
        default:                     return "Unknown";
    }
}

std::vector<int> ContactList::getSupportedFields() const {
    return {
        CONTACT_ADDR, CONTACT_BIRTHDAY, CONTACT_CLASS, CONTACT_EMAIL,
        CONTACT_FORMATTED_ADDR, CONTACT_FORMATTED_NAME, CONTACT_NAME,
        CONTACT_NICKNAME, CONTACT_NOTE, CONTACT_ORG, CONTACT_PHOTO,
        CONTACT_PHOTO_URL, CONTACT_PUBLIC_KEY, CONTACT_REVISION,
        CONTACT_TEL, CONTACT_TITLE, CONTACT_UID, CONTACT_URL
    };
}

std::vector<int> ContactList::getSupportedAttributes(int field) const {
    if (field == CONTACT_TEL) {
        return {ATTR_NONE, ATTR_MOBILE, ATTR_WORK, ATTR_HOME, ATTR_PREFERRED, ATTR_FAX, ATTR_PAGER, ATTR_SMS};
    }
    if (field == CONTACT_EMAIL || field == CONTACT_ADDR) {
        return {ATTR_NONE, ATTR_WORK, ATTR_HOME, ATTR_PREFERRED};
    }
    return {ATTR_NONE};
}

std::vector<int> ContactList::getSupportedArrayElements(int stringArrayField) const {
    if (stringArrayField == CONTACT_NAME) {
        return {NAME_FAMILY, NAME_GIVEN, NAME_OTHER, NAME_PREFIX, NAME_SUFFIX};
    }
    if (stringArrayField == CONTACT_ADDR) {
        return {ADDR_POBOX, ADDR_EXTRA, ADDR_STREET, ADDR_LOCALITY, ADDR_REGION, ADDR_POSTALCODE, ADDR_COUNTRY};
    }
    return {};
}

std::shared_ptr<Contact> ContactList::createContact() {
    auto c = std::make_shared<Contact>(this);
    m_items.push_back(c);
    return c;
}

std::shared_ptr<Contact> ContactList::importContact(const std::shared_ptr<Contact>& contact) {
    if (!contact) return nullptr;
    auto copy = std::make_shared<Contact>(this);
    copy->fromJson(contact->toJson());
    m_items.push_back(copy);
    return copy;
}

void ContactList::removeContact(const std::shared_ptr<Contact>& contact) {
    removeItem(contact);
}

// -------------------------------------------------------------
// EventList
// -------------------------------------------------------------
EventList::EventList(const std::string& name, int mode)
    : PIMList(name, PIM_EVENT_LIST, mode)
{
}

int EventList::getFieldDataType(int field) const {
    switch (field) {
        case EVENT_ALARM:
        case EVENT_CLASS:
            return PIM_TYPE_INT;
        case EVENT_START:
        case EVENT_END:
        case EVENT_REVISION:
            return PIM_TYPE_DATE;
        default:
            return PIM_TYPE_STRING;
    }
}

std::string EventList::getFieldLabel(int field) const {
    switch (field) {
        case EVENT_ALARM:    return "Alarm";
        case EVENT_CLASS:    return "Class";
        case EVENT_END:      return "End Date";
        case EVENT_LOCATION: return "Location";
        case EVENT_NOTE:     return "Note";
        case EVENT_REVISION: return "Revision";
        case EVENT_START:    return "Start Date";
        case EVENT_SUMMARY:  return "Summary";
        case EVENT_UID:      return "UID";
        default:             return "Unknown";
    }
}

std::vector<int> EventList::getSupportedFields() const {
    return {
        EVENT_ALARM, EVENT_CLASS, EVENT_END, EVENT_LOCATION,
        EVENT_NOTE, EVENT_REVISION, EVENT_START, EVENT_SUMMARY, EVENT_UID
    };
}

std::vector<int> EventList::getSupportedAttributes(int /*field*/) const {
    return {ATTR_NONE};
}

std::shared_ptr<Event> EventList::createEvent() {
    auto e = std::make_shared<Event>(this);
    m_items.push_back(e);
    return e;
}

std::shared_ptr<Event> EventList::importEvent(const std::shared_ptr<Event>& event) {
    if (!event) return nullptr;
    auto copy = std::make_shared<Event>(this);
    copy->fromJson(event->toJson());
    if (event->hasRepeatRule()) {
        copy->setRepeatRule(event->getRepeatRule());
    }
    m_items.push_back(copy);
    return copy;
}

void EventList::removeEvent(const std::shared_ptr<Event>& event) {
    removeItem(event);
}

std::vector<std::shared_ptr<Event>> EventList::items(int searchType, int64_t startDate, int64_t endDate, bool /*initialEventOnly*/) const {
    std::vector<std::shared_ptr<Event>> results;
    for (const auto& item : m_items) {
        auto ev = std::dynamic_pointer_cast<Event>(item);
        if (!ev) continue;

        int64_t eStart = ev->countValues(EVENT_START) > 0 ? ev->getDate(EVENT_START, 0) : 0;
        int64_t eEnd   = ev->countValues(EVENT_END)   > 0 ? ev->getDate(EVENT_END, 0)   : eStart;

        bool matches = false;
        switch (searchType) {
            case EVENT_STARTING:
                matches = (eStart >= startDate && eStart <= endDate);
                break;
            case EVENT_ENDING:
                matches = (eEnd >= startDate && eEnd <= endDate);
                break;
            case EVENT_OCCURRING:
            default:
                matches = (eStart <= endDate && eEnd >= startDate);
                break;
        }

        // Also check repeat rule occurrences if event has one
        if (!matches && ev->hasRepeatRule()) {
            auto occ = ev->getRepeatRule().dates(eStart, startDate, endDate);
            if (!occ.empty()) {
                matches = true;
            }
        }

        if (matches) {
            results.push_back(ev);
        }
    }
    return results;
}

// -------------------------------------------------------------
// ToDoList
// -------------------------------------------------------------
ToDoList::ToDoList(const std::string& name, int mode)
    : PIMList(name, PIM_TODO_LIST, mode)
{
}

int ToDoList::getFieldDataType(int field) const {
    switch (field) {
        case TODO_CLASS:
        case TODO_PRIORITY:
            return PIM_TYPE_INT;
        case TODO_COMPLETED:
            return PIM_TYPE_BOOLEAN;
        case TODO_COMPLETION_DATE:
        case TODO_DUE:
        case TODO_REVISION:
            return PIM_TYPE_DATE;
        default:
            return PIM_TYPE_STRING;
    }
}

std::string ToDoList::getFieldLabel(int field) const {
    switch (field) {
        case TODO_CLASS:           return "Class";
        case TODO_COMPLETED:       return "Completed";
        case TODO_COMPLETION_DATE: return "Completion Date";
        case TODO_DUE:             return "Due Date";
        case TODO_NOTE:            return "Note";
        case TODO_PRIORITY:        return "Priority";
        case TODO_REVISION:        return "Revision";
        case TODO_SUMMARY:         return "Summary";
        case TODO_UID:             return "UID";
        default:                   return "Unknown";
    }
}

std::vector<int> ToDoList::getSupportedFields() const {
    return {
        TODO_CLASS, TODO_COMPLETED, TODO_COMPLETION_DATE, TODO_DUE,
        TODO_NOTE, TODO_PRIORITY, TODO_REVISION, TODO_SUMMARY, TODO_UID
    };
}

std::vector<int> ToDoList::getSupportedAttributes(int /*field*/) const {
    return {ATTR_NONE};
}

std::shared_ptr<ToDo> ToDoList::createToDo() {
    auto t = std::make_shared<ToDo>(this);
    m_items.push_back(t);
    return t;
}

std::shared_ptr<ToDo> ToDoList::importToDo(const std::shared_ptr<ToDo>& todo) {
    if (!todo) return nullptr;
    auto copy = std::make_shared<ToDo>(this);
    copy->fromJson(todo->toJson());
    m_items.push_back(copy);
    return copy;
}

void ToDoList::removeToDo(const std::shared_ptr<ToDo>& todo) {
    removeItem(todo);
}

std::vector<std::shared_ptr<ToDo>> ToDoList::items(int field, int64_t startDate, int64_t endDate) const {
    std::vector<std::shared_ptr<ToDo>> results;
    for (const auto& item : m_items) {
        auto td = std::dynamic_pointer_cast<ToDo>(item);
        if (!td) continue;

        if (td->countValues(field) > 0) {
            int64_t d = td->getDate(field, 0);
            if (d >= startDate && d <= endDate) {
                results.push_back(td);
            }
        }
    }
    return results;
}

} // namespace pim
} // namespace universal_loader
