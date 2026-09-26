#pragma once

#include "j2me_core.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <filesystem>

namespace universal_loader {
namespace file {

class FileConnection;

class J2ME_API FileSystemListener {
public:
    static constexpr int ROOT_ADDED   = 1;
    static constexpr int ROOT_REMOVED = 2;

    virtual ~FileSystemListener() = default;
    virtual void rootChanged(int state, const std::string& rootName) = 0;
};

class J2ME_API FileSystemRegistry {
public:
    static FileSystemRegistry& instance();

    void setBaseDirectory(const std::string& baseDir);
    const std::string& getBaseDirectory() const { return m_baseDir; }

    void registerRoot(const std::string& rootName, const std::string& relativeOrAbsolutePath);
    void unregisterRoot(const std::string& rootName);

    std::vector<std::string> listRoots() const;

    bool resolve(const std::string& url, std::string& outRoot, std::filesystem::path& outPhysicalPath) const;
    std::shared_ptr<FileConnection> open(const std::string& url);

    void addFileSystemListener(FileSystemListener* listener);
    void removeFileSystemListener(FileSystemListener* listener);

private:
    FileSystemRegistry();
    ~FileSystemRegistry() = default;

    void notifyListeners(int state, const std::string& rootName);

    std::string m_baseDir;
    std::map<std::string, std::filesystem::path> m_roots;
    std::vector<FileSystemListener*> m_listeners;
    mutable std::mutex m_mutex;
};

} // namespace file
} // namespace universal_loader
