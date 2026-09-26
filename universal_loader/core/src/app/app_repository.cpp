#include "app_repository.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace universal_loader {
namespace app {

static std::string toLower(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

AppRepository::AppRepository(std::string appsRootDir)
    : m_appsRootDir(std::move(appsRootDir)) {
    load();
}

std::string AppRepository::getDatabaseFilePath() const {
    return (fs::path(m_appsRootDir) / "apps_db.json").string();
}

bool AppRepository::load() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_items.clear();
    m_nextId = 1;

    std::string dbPath = getDatabaseFilePath();
    if (!fs::exists(dbPath)) {
        return false;
    }

    std::ifstream ifs(dbPath);
    if (!ifs.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();

    // Read nextId
    size_t nidPos = content.find("\"nextId\"");
    if (nidPos != std::string::npos) {
        size_t colon = content.find(':', nidPos);
        if (colon != std::string::npos) {
            size_t valEnd = content.find_first_of(",}\n", colon + 1);
            std::string nVal = content.substr(colon + 1, valEnd - (colon + 1));
            try { m_nextId = std::stoi(nVal); } catch (...) {}
        }
    }

    // Split apps array items: find each '{' ... '}' inside "apps": [ ... ]
    size_t appsPos = content.find("\"apps\"");
    if (appsPos != std::string::npos) {
        size_t arrayStart = content.find('[', appsPos);
        size_t arrayEnd = content.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            size_t cur = arrayStart + 1;
            while (cur < arrayEnd) {
                size_t objStart = content.find('{', cur);
                if (objStart == std::string::npos || objStart >= arrayEnd) break;
                
                // Find matching closing brace
                int depth = 0;
                size_t objEnd = objStart;
                for (size_t p = objStart; p < arrayEnd; ++p) {
                    if (content[p] == '{') depth++;
                    else if (content[p] == '}') {
                        depth--;
                        if (depth == 0) {
                            objEnd = p;
                            break;
                        }
                    }
                }

                if (depth == 0) {
                    std::string itemJson = content.substr(objStart, objEnd - objStart + 1);
                    AppItem item;
                    if (item.deserializeJson(itemJson)) {
                        m_items.push_back(item);
                        if (item.id >= m_nextId) {
                            m_nextId = item.id + 1;
                        }
                    }
                    cur = objEnd + 1;
                } else {
                    break;
                }
            }
        }
    }

    return true;
}

bool AppRepository::save() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    try {
        fs::create_directories(m_appsRootDir);
    } catch (...) {}

    std::string dbPath = getDatabaseFilePath();
    std::ofstream ofs(dbPath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "{\n";
    ofs << "  \"nextId\": " << m_nextId << ",\n";
    ofs << "  \"apps\": [\n";

    for (size_t i = 0; i < m_items.size(); ++i) {
        std::string itemJson = m_items[i].serializeJson();
        // Indent lines
        std::istringstream iss(itemJson);
        std::string line;
        bool first = true;
        while (std::getline(iss, line)) {
            if (!first) ofs << "\n";
            ofs << "    " << line;
            first = false;
        }
        if (i + 1 < m_items.size()) {
            ofs << ",\n";
        } else {
            ofs << "\n";
        }
    }

    ofs << "  ]\n";
    ofs << "}\n";
    return true;
}

int AppRepository::insert(const AppItem& item) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    AppItem newItem = item;
    if (newItem.id <= 0) {
        newItem.id = m_nextId++;
    } else if (newItem.id >= m_nextId) {
        m_nextId = newItem.id + 1;
    }

    // Check if already in list
    for (auto& existing : m_items) {
        if (existing.id == newItem.id || existing.path == newItem.path) {
            existing = newItem;
            save();
            return existing.id;
        }
    }

    m_items.push_back(newItem);
    save();
    return newItem.id;
}

bool AppRepository::update(const AppItem& item) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& existing : m_items) {
        if (existing.id == item.id || existing.path == item.path) {
            existing = item;
            save();
            return true;
        }
    }
    return false;
}

bool AppRepository::remove(int id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = std::remove_if(m_items.begin(), m_items.end(), [id](const AppItem& a) {
        return a.id == id;
    });
    if (it != m_items.end()) {
        m_items.erase(it, m_items.end());
        save();
        return true;
    }
    return false;
}

bool AppRepository::removeByPath(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = std::remove_if(m_items.begin(), m_items.end(), [&path](const AppItem& a) {
        return a.path == path;
    });
    if (it != m_items.end()) {
        m_items.erase(it, m_items.end());
        save();
        return true;
    }
    return false;
}

bool AppRepository::getById(int id, AppItem& outItem) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& item : m_items) {
        if (item.id == id) {
            outItem = item;
            return true;
        }
    }
    return false;
}

bool AppRepository::getByPath(const std::string& path, AppItem& outItem) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& item : m_items) {
        if (item.path == path) {
            outItem = item;
            return true;
        }
    }
    return false;
}

bool AppRepository::getByTitleVendor(const std::string& title, const std::string& vendor, AppItem& outItem) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string lowerTitle = toLower(title);
    std::string lowerVendor = toLower(vendor);

    for (const auto& item : m_items) {
        if (toLower(item.title) == lowerTitle &&
            (vendor.empty() || toLower(item.author) == lowerVendor)) {
            outItem = item;
            return true;
        }
    }
    return false;
}

size_t AppRepository::getCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_items.size();
}

std::vector<AppItem> AppRepository::getAll(AppSortOrder order) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<AppItem> result = m_items;

    switch (order) {
        case AppSortOrder::TITLE_ASC:
            std::sort(result.begin(), result.end(), [](const AppItem& a, const AppItem& b) {
                return toLower(a.title) < toLower(b.title);
            });
            break;
        case AppSortOrder::TITLE_DESC:
            std::sort(result.begin(), result.end(), [](const AppItem& a, const AppItem& b) {
                return toLower(a.title) > toLower(b.title);
            });
            break;
        case AppSortOrder::INSTALLED_DESC:
            std::sort(result.begin(), result.end(), [](const AppItem& a, const AppItem& b) {
                return a.installedTimestamp > b.installedTimestamp;
            });
            break;
        case AppSortOrder::LAST_PLAYED_DESC:
            std::sort(result.begin(), result.end(), [](const AppItem& a, const AppItem& b) {
                return a.lastPlayedTimestamp > b.lastPlayedTimestamp;
            });
            break;
        case AppSortOrder::PLAY_COUNT_DESC:
            std::sort(result.begin(), result.end(), [](const AppItem& a, const AppItem& b) {
                return a.playCount > b.playCount;
            });
            break;
    }

    return result;
}

void AppRepository::clearAll() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_items.clear();
    m_nextId = 1;
    save();
}

} // namespace app
} // namespace universal_loader
