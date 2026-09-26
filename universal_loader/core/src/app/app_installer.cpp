#include "app_installer.h"
#include "../../include/jar_resource_loader.h"
#include "../../include/app_profile_config.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace universal_loader {
namespace app {

static int64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string AppInstaller::sanitizePathName(const std::string& rawName) {
    std::string clean;
    clean.reserve(rawName.size());
    for (char c : rawName) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            clean += c;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            clean += '_';
        }
    }
    if (clean.empty()) {
        clean = "midlet_app";
    }
    return clean;
}

AppInstaller::AppInstaller(std::string rootDir, std::shared_ptr<AppRepository> repository)
    : m_rootDir(std::move(rootDir)), m_repository(std::move(repository)) {
}

std::string AppInstaller::getAppDir(const std::string& appPath) const {
    return (fs::path(m_rootDir) / "apps" / appPath).string();
}

std::string AppInstaller::getDataDir(const std::string& appPath) const {
    return (fs::path(m_rootDir) / "data" / appPath).string();
}

std::string AppInstaller::getConfigDir(const std::string& appPath) const {
    return (fs::path(m_rootDir) / "configs" / appPath).string();
}

InstallCheckResult AppInstaller::checkJar(const std::string& jarPath) {
    InstallCheckResult res;
    if (!fs::exists(jarPath)) {
        res.status = InstallStatus::STATUS_ERROR;
        return res;
    }

    jvm::JarResourceLoader loader;
    if (!loader.openFromFile(jarPath)) {
        res.status = InstallStatus::STATUS_ERROR;
        return res;
    }

    auto desc = loader.getDescriptor();
    if (!desc) {
        res.status = InstallStatus::STATUS_ERROR;
        return res;
    }

    res.title = desc->getName();
    res.vendor = desc->getVendor();
    res.version = desc->getVersion();
    res.icon = desc->getIcon();

    if (res.title.empty()) {
        res.title = fs::path(jarPath).stem().string();
    }

    if (!m_repository) {
        res.status = InstallStatus::STATUS_NEW;
        return res;
    }

    AppItem existingItem;
    if (m_repository->getByTitleVendor(res.title, res.vendor, existingItem)) {
        res.matchedAppId = existingItem.id;
        int cmp = midlet::AppDescriptor::compareVersions(res.version, existingItem.version);
        if (cmp > 0) {
            res.status = InstallStatus::STATUS_NEWEST;
        } else if (cmp < 0) {
            res.status = InstallStatus::STATUS_OLDEST;
        } else {
            res.status = InstallStatus::STATUS_EQUAL;
        }
    } else {
        res.status = InstallStatus::STATUS_NEW;
    }

    return res;
}

bool AppInstaller::installFromJar(const std::string& jarPath, bool forceUpdate, int& outAppId, std::string& outError) {
    outAppId = 0;
    outError.clear();

    if (!fs::exists(jarPath)) {
        outError = "JAR file not found: " + jarPath;
        return false;
    }

    jvm::JarResourceLoader loader;
    if (!loader.openFromFile(jarPath)) {
        outError = "Failed to open JAR file archive: " + jarPath;
        return false;
    }

    auto desc = loader.getDescriptor();
    if (!desc) {
        outError = "Failed to parse META-INF/MANIFEST.MF in JAR: " + jarPath;
        return false;
    }

    std::string title = desc->getName();
    if (title.empty()) {
        title = fs::path(jarPath).stem().string();
    }
    std::string vendor = desc->getVendor();
    std::string version = desc->getVersion();

    std::string appPathName;
    bool isUpdate = false;
    AppItem existingItem;

    if (m_repository && m_repository->getByTitleVendor(title, vendor, existingItem)) {
        int cmp = midlet::AppDescriptor::compareVersions(version, existingItem.version);
        if (cmp == 0 && !forceUpdate) {
            outError = "Application already installed with identical version: " + version;
            return false;
        }
        if (cmp < 0 && !forceUpdate) {
            outError = "Application already installed with newer version: " + existingItem.version;
            return false;
        }
        isUpdate = true;
        appPathName = existingItem.path;
    } else {
        // Generate unique path name
        std::string baseName = sanitizePathName(title);
        appPathName = baseName;
        int counter = 1;
        while (fs::exists(getAppDir(appPathName))) {
            appPathName = baseName + "_" + std::to_string(counter++);
        }
    }

    std::string appsDir = (fs::path(m_rootDir) / "apps").string();
    std::string tmpDir = (fs::path(appsDir) / (".tmp_" + appPathName)).string();
    std::string finalAppDir = getAppDir(appPathName);

    try {
        fs::create_directories(appsDir);
        if (fs::exists(tmpDir)) {
            fs::remove_all(tmpDir);
        }
        fs::create_directories(tmpDir);

        // 1. Copy JAR file
        fs::copy_file(jarPath, fs::path(tmpDir) / "app.jar", fs::copy_options::overwrite_existing);

        // 2. Write Manifest
        if (loader.hasResource("META-INF/MANIFEST.MF")) {
            auto mfBytes = loader.getResourceBytes("META-INF/MANIFEST.MF");
            std::ofstream mfs(fs::path(tmpDir) / "MANIFEST.MF", std::ios::binary);
            mfs.write(reinterpret_cast<const char*>(mfBytes.data()), mfBytes.size());
        }

        // 3. Extract Icon
        std::string iconResource = desc->getIcon();
        if (iconResource.empty()) {
            const auto& midlets = desc->getMidlets();
            if (!midlets.empty() && !midlets[0].icon.empty()) {
                iconResource = midlets[0].icon;
            }
        }

        bool hasIcon = false;
        if (!iconResource.empty() && loader.hasResource(iconResource)) {
            auto iconBytes = loader.getResourceBytes(iconResource);
            if (!iconBytes.empty()) {
                std::ofstream ifs(fs::path(tmpDir) / "icon.png", std::ios::binary);
                ifs.write(reinterpret_cast<const char*>(iconBytes.data()), iconBytes.size());
                hasIcon = true;
            }
        }

        // 4. Create default configuration profile if not exists
        std::string cfgDir = getConfigDir(appPathName);
        fs::create_directories(cfgDir);
        std::string cfgFile = (fs::path(cfgDir) / "config.json").string();
        if (!fs::exists(cfgFile)) {
            config::ProfileModel defaultProfile;
            config::ProfilesManager::saveConfig(cfgFile, defaultProfile);
        }

        // 5. Ensure data (RMS) directory exists
        std::string dataDir = getDataDir(appPathName);
        fs::create_directories(dataDir);

        // 6. Move tmp directory to final target directory atomically
        if (fs::exists(finalAppDir)) {
            fs::remove_all(finalAppDir);
        }
        fs::rename(tmpDir, finalAppDir);

        // 7. Register / update in AppRepository
        AppItem appItem;
        if (isUpdate) {
            appItem = existingItem;
        }
        appItem.path = appPathName;
        appItem.title = title;
        appItem.author = vendor;
        appItem.version = version;
        appItem.imagePath = hasIcon ? "icon.png" : "";
        if (!isUpdate) {
            appItem.installedTimestamp = getCurrentTimeMs();
            appItem.lastPlayedTimestamp = 0;
            appItem.playCount = 0;
        }

        if (m_repository) {
            if (isUpdate) {
                m_repository->update(appItem);
                outAppId = appItem.id;
            } else {
                outAppId = m_repository->insert(appItem);
            }
            m_repository->save();
        }

        return true;
    } catch (const std::exception& ex) {
        outError = std::string("Installation exception: ") + ex.what();
        try {
            if (fs::exists(tmpDir)) fs::remove_all(tmpDir);
        } catch (...) {}
        return false;
    }
}

bool AppInstaller::uninstall(int appId, std::string& outError) {
    outError.clear();
    if (!m_repository) {
        outError = "Repository is null";
        return false;
    }

    AppItem item;
    if (!m_repository->getById(appId, item)) {
        outError = "App with ID " + std::to_string(appId) + " not found";
        return false;
    }

    return uninstallByPath(item.path, outError);
}

bool AppInstaller::uninstallByPath(const std::string& appPath, std::string& outError) {
    outError.clear();
    try {
        std::string appDir = getAppDir(appPath);
        if (fs::exists(appDir)) {
            fs::remove_all(appDir);
        }

        std::string dataDir = getDataDir(appPath);
        if (fs::exists(dataDir)) {
            fs::remove_all(dataDir);
        }

        std::string cfgDir = getConfigDir(appPath);
        if (fs::exists(cfgDir)) {
            fs::remove_all(cfgDir);
        }

        if (m_repository) {
            m_repository->removeByPath(appPath);
            m_repository->save();
        }

        return true;
    } catch (const std::exception& ex) {
        outError = std::string("Uninstall exception: ") + ex.what();
        return false;
    }
}

} // namespace app
} // namespace universal_loader
