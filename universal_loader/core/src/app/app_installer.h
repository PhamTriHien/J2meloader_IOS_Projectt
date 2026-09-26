#ifndef UNIVERSAL_LOADER_APP_INSTALLER_H
#define UNIVERSAL_LOADER_APP_INSTALLER_H

#include <string>
#include <memory>
#include "app_repository.h"
#include "../../include/app_descriptor.h"

namespace universal_loader {
namespace app {

enum class InstallStatus {
    STATUS_OLDEST = -1, // An older version than currently installed
    STATUS_EQUAL = 0,   // Same version already installed
    STATUS_NEWEST = 1,  // Newer version than currently installed (update)
    STATUS_NEW = 2,     // New app, not installed yet
    STATUS_UNMATCHED = 3, // JAD/Manifest mismatch
    STATUS_ERROR = 4    // Parse or I/O error
};

struct InstallCheckResult {
    InstallStatus status{InstallStatus::STATUS_ERROR};
    std::string title;
    std::string vendor;
    std::string version;
    std::string icon;
    std::string jarUrl;
    int matchedAppId{0};
};

class J2ME_API AppInstaller {
public:
    AppInstaller(std::string rootDir, std::shared_ptr<AppRepository> repository);
    ~AppInstaller() = default;

    InstallCheckResult checkJar(const std::string& jarPath);

    bool installFromJar(const std::string& jarPath, bool forceUpdate, int& outAppId, std::string& outError);
    bool uninstall(int appId, std::string& outError);
    bool uninstallByPath(const std::string& appPath, std::string& outError);

    static std::string sanitizePathName(const std::string& rawName);

    std::string getAppDir(const std::string& appPath) const;
    std::string getDataDir(const std::string& appPath) const;
    std::string getConfigDir(const std::string& appPath) const;

private:
    std::string m_rootDir;
    std::shared_ptr<AppRepository> m_repository;
};

} // namespace app
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_APP_INSTALLER_H
