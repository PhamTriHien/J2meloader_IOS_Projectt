#include "file_system_registry.h"
#include "file_connection.h"
#include <algorithm>
#include <iostream>

namespace universal_loader {
namespace file {

FileSystemRegistry& FileSystemRegistry::instance() {
    static FileSystemRegistry s_instance;
    return s_instance;
}

FileSystemRegistry::FileSystemRegistry()
    : m_baseDir("./j2me_filesystem") {
    // Standard default roots matching upstream FileSystemFileConnection.java
    registerRoot("c:/", "fs_internal");
    registerRoot("e:/", "fs_external");
    registerRoot("photos/", "fs_photos");
    registerRoot("sounds/", "fs_sounds");
}

void FileSystemRegistry::setBaseDirectory(const std::string& baseDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseDir = baseDir;
    std::filesystem::create_directories(m_baseDir);

    // Re-register defaults with new base directory
    m_roots["c:/"] = std::filesystem::path(m_baseDir) / "fs_internal";
    m_roots["e:/"] = std::filesystem::path(m_baseDir) / "fs_external";
    m_roots["photos/"] = std::filesystem::path(m_baseDir) / "fs_photos";
    m_roots["sounds/"] = std::filesystem::path(m_baseDir) / "fs_sounds";

    for (const auto& [_, p] : m_roots) {
        std::filesystem::create_directories(p);
    }
}

void FileSystemRegistry::registerRoot(const std::string& rootName, const std::string& relativeOrAbsolutePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::filesystem::path p(relativeOrAbsolutePath);
    if (p.is_relative()) {
        p = std::filesystem::path(m_baseDir) / p;
    }
    std::filesystem::create_directories(p);
    m_roots[rootName] = p;
    notifyListeners(FileSystemListener::ROOT_ADDED, rootName);
}

void FileSystemRegistry::unregisterRoot(const std::string& rootName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_roots.find(rootName);
    if (it != m_roots.end()) {
        m_roots.erase(it);
        notifyListeners(FileSystemListener::ROOT_REMOVED, rootName);
    }
}

std::vector<std::string> FileSystemRegistry::listRoots() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> list;
    for (const auto& [name, _] : m_roots) {
        list.push_back(name);
    }
    return list;
}

bool FileSystemRegistry::resolve(const std::string& url, std::string& outRoot, std::filesystem::path& outPhysicalPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Expecting: file://... or file:///...
    std::string pathPart = url;
    if (pathPart.rfind("file://", 0) == 0) {
        pathPart = pathPart.substr(7);
    } else if (pathPart.rfind("file:/", 0) == 0) {
        pathPart = pathPart.substr(6);
    }

    // Strip optional host like "localhost/"
    if (pathPart.rfind("localhost/", 0) == 0) {
        pathPart = pathPart.substr(10);
    }

    // Strip leading slash
    while (!pathPart.empty() && (pathPart[0] == '/' || pathPart[0] == '\\')) {
        pathPart.erase(0, 1);
    }

    // Match against known roots (including aliases)
    std::string matchedRoot;
    std::string subPath;

    for (const auto& [rootName, _] : m_roots) {
        if (pathPart.size() >= rootName.size()) {
            std::string prefix = pathPart.substr(0, rootName.size());
            // Case-insensitive match for root
            std::string lowerPrefix = prefix;
            std::string lowerRoot = rootName;
            std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);
            std::transform(lowerRoot.begin(), lowerRoot.end(), lowerRoot.begin(), ::tolower);

            if (lowerPrefix == lowerRoot) {
                matchedRoot = rootName;
                subPath = pathPart.substr(rootName.size());
                break;
            }
        }
    }

    // Handle common aliases if no root matched yet:
    if (matchedRoot.empty()) {
        if (pathPart.rfind("root/", 0) == 0) {
            matchedRoot = "c:/";
            subPath = pathPart.substr(5);
        } else if (pathPart.rfind("sdcard/", 0) == 0) {
            matchedRoot = "e:/";
            subPath = pathPart.substr(7);
        }
    }

    if (matchedRoot.empty()) {
        return false;
    }

    auto it = m_roots.find(matchedRoot);
    if (it == m_roots.end()) {
        return false;
    }

    outRoot = matchedRoot;
    outPhysicalPath = it->second;

    // Prevent directory traversal attacks
    if (!subPath.empty()) {
        while (subPath.size() > 1 && (subPath.back() == '/' || subPath.back() == '\\')) {
            subPath.pop_back();
        }
        if (subPath != "/" && subPath != "\\") {
            std::filesystem::path rel(subPath);
            for (const auto& part : rel) {
                if (part == "..") {
                    return false; // Traversal beyond root prohibited
                }
            }
            outPhysicalPath /= rel;
        }
    }
    outPhysicalPath = outPhysicalPath.lexically_normal();

    return true;
}

std::shared_ptr<FileConnection> FileSystemRegistry::open(const std::string& url) {
    std::string root;
    std::filesystem::path physicalPath;
    if (!resolve(url, root, physicalPath)) {
        return nullptr;
    }

    // Parse path and file name
    std::string path = physicalPath.parent_path().string();
    std::string name = physicalPath.filename().string();

    bool isDirUrl = (!url.empty() && (url.back() == '/' || url.back() == '\\'));
    if (isDirUrl || std::filesystem::is_directory(physicalPath)) {
        if (!name.empty() && name.back() != '/') {
            name += '/';
        }
    }

    return std::make_shared<FileConnection>(url, root, path, name, physicalPath);
}

void FileSystemRegistry::addFileSystemListener(FileSystemListener* listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (listener) {
        m_listeners.push_back(listener);
    }
}

void FileSystemRegistry::removeFileSystemListener(FileSystemListener* listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(
        std::remove(m_listeners.begin(), m_listeners.end(), listener),
        m_listeners.end());
}

void FileSystemRegistry::notifyListeners(int state, const std::string& rootName) {
    for (auto* l : m_listeners) {
        if (l) {
            l->rootChanged(state, rootName);
        }
    }
}

} // namespace file
} // namespace universal_loader
