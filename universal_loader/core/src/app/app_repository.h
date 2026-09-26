#ifndef UNIVERSAL_LOADER_APP_REPOSITORY_H
#define UNIVERSAL_LOADER_APP_REPOSITORY_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "app_item.h"

namespace universal_loader {
namespace app {

enum class AppSortOrder {
    TITLE_ASC,
    TITLE_DESC,
    INSTALLED_DESC,
    LAST_PLAYED_DESC,
    PLAY_COUNT_DESC
};

class J2ME_API AppRepository {
public:
    explicit AppRepository(std::string appsRootDir);
    ~AppRepository() = default;

    bool load();
    bool save();

    int insert(const AppItem& item);
    bool update(const AppItem& item);
    bool remove(int id);
    bool removeByPath(const std::string& path);

    bool getById(int id, AppItem& outItem) const;
    bool getByPath(const std::string& path, AppItem& outItem) const;
    bool getByTitleVendor(const std::string& title, const std::string& vendor, AppItem& outItem) const;

    size_t getCount() const;
    std::vector<AppItem> getAll(AppSortOrder order = AppSortOrder::TITLE_ASC) const;

    void clearAll();

    const std::string& getAppsRootDir() const { return m_appsRootDir; }
    std::string getDatabaseFilePath() const;

private:
    std::string m_appsRootDir;
    mutable std::recursive_mutex m_mutex;
    std::vector<AppItem> m_items;
    int m_nextId{1};
};

} // namespace app
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_APP_REPOSITORY_H
