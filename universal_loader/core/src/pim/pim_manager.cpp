#include "pim_manager.h"
#include <filesystem>
#include <fstream>

namespace universal_loader {
namespace pim {

PIMManager& PIMManager::getInstance() {
    static PIMManager instance;
    return instance;
}

PIMManager::PIMManager() {
    ensureDefaults();
}

void PIMManager::ensureDefaults() {
    if (m_contactLists.empty()) {
        m_contactLists["Contacts"] = std::make_shared<ContactList>("Contacts", PIM_READ_WRITE);
    }
    if (m_eventLists.empty()) {
        m_eventLists["Events"] = std::make_shared<EventList>("Events", PIM_READ_WRITE);
    }
    if (m_todoLists.empty()) {
        m_todoLists["ToDos"] = std::make_shared<ToDoList>("ToDos", PIM_READ_WRITE);
    }
}

std::string PIMManager::getListFilePath(int pimListType, const std::string& name) const {
    if (m_sandboxDir.empty()) return "";
    std::string prefix;
    switch (pimListType) {
        case PIM_CONTACT_LIST: prefix = "contacts_"; break;
        case PIM_EVENT_LIST:   prefix = "events_"; break;
        case PIM_TODO_LIST:    prefix = "todos_"; break;
        default:               prefix = "pim_"; break;
    }
    std::filesystem::path p(m_sandboxDir);
    p /= "pim";
    p /= (prefix + name + ".json");
    return p.string();
}

void PIMManager::init(const std::string& sandboxDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sandboxDir = sandboxDir;

    if (!m_sandboxDir.empty()) {
        std::filesystem::path pimDir(m_sandboxDir);
        pimDir /= "pim";
        std::error_code ec;
        std::filesystem::create_directories(pimDir, ec);

        // Load persisted defaults
        ensureDefaults();

        for (auto& kv : m_contactLists) {
            std::string path = getListFilePath(PIM_CONTACT_LIST, kv.first);
            if (std::filesystem::exists(path)) {
                kv.second->loadFromFile(path);
            }
        }
        for (auto& kv : m_eventLists) {
            std::string path = getListFilePath(PIM_EVENT_LIST, kv.first);
            if (std::filesystem::exists(path)) {
                kv.second->loadFromFile(path);
            }
        }
        for (auto& kv : m_todoLists) {
            std::string path = getListFilePath(PIM_TODO_LIST, kv.first);
            if (std::filesystem::exists(path)) {
                kv.second->loadFromFile(path);
            }
        }
    }
}

void PIMManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_contactLists.clear();
    m_eventLists.clear();
    m_todoLists.clear();
    ensureDefaults();
}

std::vector<std::string> PIMManager::listPIMLists(int pimListType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ensureDefaults();
    std::vector<std::string> res;
    switch (pimListType) {
        case PIM_CONTACT_LIST:
            for (const auto& kv : m_contactLists) res.push_back(kv.first);
            break;
        case PIM_EVENT_LIST:
            for (const auto& kv : m_eventLists) res.push_back(kv.first);
            break;
        case PIM_TODO_LIST:
            for (const auto& kv : m_todoLists) res.push_back(kv.first);
            break;
        default:
            break;
    }
    return res;
}

std::shared_ptr<PIMList> PIMManager::openPIMList(int pimListType, int mode, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ensureDefaults();

    std::string actualName = name;
    if (actualName.empty()) {
        if (pimListType == PIM_CONTACT_LIST) actualName = "Contacts";
        else if (pimListType == PIM_EVENT_LIST) actualName = "Events";
        else if (pimListType == PIM_TODO_LIST) actualName = "ToDos";
    }

    switch (pimListType) {
        case PIM_CONTACT_LIST: {
            auto it = m_contactLists.find(actualName);
            if (it != m_contactLists.end()) return it->second;
            auto list = std::make_shared<ContactList>(actualName, mode);
            std::string path = getListFilePath(PIM_CONTACT_LIST, actualName);
            if (!path.empty() && std::filesystem::exists(path)) {
                list->loadFromFile(path);
            }
            m_contactLists[actualName] = list;
            return list;
        }
        case PIM_EVENT_LIST: {
            auto it = m_eventLists.find(actualName);
            if (it != m_eventLists.end()) return it->second;
            auto list = std::make_shared<EventList>(actualName, mode);
            std::string path = getListFilePath(PIM_EVENT_LIST, actualName);
            if (!path.empty() && std::filesystem::exists(path)) {
                list->loadFromFile(path);
            }
            m_eventLists[actualName] = list;
            return list;
        }
        case PIM_TODO_LIST: {
            auto it = m_todoLists.find(actualName);
            if (it != m_todoLists.end()) return it->second;
            auto list = std::make_shared<ToDoList>(actualName, mode);
            std::string path = getListFilePath(PIM_TODO_LIST, actualName);
            if (!path.empty() && std::filesystem::exists(path)) {
                list->loadFromFile(path);
            }
            m_todoLists[actualName] = list;
            return list;
        }
        default:
            return nullptr;
    }
}

std::vector<std::string> PIMManager::getSupportedSerialFormats(int pimListType) {
    switch (pimListType) {
        case PIM_CONTACT_LIST:
            return {"VCARD/2.1", "VCARD/3.0"};
        case PIM_EVENT_LIST:
        case PIM_TODO_LIST:
            return {"VCALENDAR/1.0"};
        default:
            return {};
    }
}

std::vector<std::shared_ptr<PIMItem>> PIMManager::fromSerialFormat(int pimListType, const std::string& data, const std::string& /*encoding*/) {
    std::vector<std::shared_ptr<PIMItem>> result;
    if (pimListType == PIM_CONTACT_LIST) {
        auto c = Contact::fromVCard(data);
        if (c) result.push_back(c);
    } else if (pimListType == PIM_EVENT_LIST) {
        auto e = Event::fromVCalendar(data);
        if (e) result.push_back(e);
    } else if (pimListType == PIM_TODO_LIST) {
        auto t = ToDo::fromVCalendar(data);
        if (t) result.push_back(t);
    }
    return result;
}

std::string PIMManager::toSerialFormat(const std::shared_ptr<PIMItem>& item, const std::string& /*dataFormat*/, const std::string& /*encoding*/) {
    if (!item) return "";
    auto c = std::dynamic_pointer_cast<Contact>(item);
    if (c) return c->toVCard();

    auto e = std::dynamic_pointer_cast<Event>(item);
    if (e) return e->toVCalendar();

    auto t = std::dynamic_pointer_cast<ToDo>(item);
    if (t) return t->toVCalendar();

    return "";
}

void PIMManager::saveAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sandboxDir.empty()) return;

    for (const auto& kv : m_contactLists) {
        std::string path = getListFilePath(PIM_CONTACT_LIST, kv.first);
        kv.second->saveToFile(path);
    }
    for (const auto& kv : m_eventLists) {
        std::string path = getListFilePath(PIM_EVENT_LIST, kv.first);
        kv.second->saveToFile(path);
    }
    for (const auto& kv : m_todoLists) {
        std::string path = getListFilePath(PIM_TODO_LIST, kv.first);
        kv.second->saveToFile(path);
    }
}

} // namespace pim
} // namespace universal_loader
