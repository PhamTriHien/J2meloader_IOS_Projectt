#pragma once

#include "j2me_core.h"
#include "app_descriptor.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>

namespace universal_loader {
namespace jvm {

/**
 * @brief High-performance In-Memory Asset Streamer and JAR Archive Loader.
 * Directly corresponds to javax.microedition.shell.AppClassLoader in upstream J2ME-Loader.
 */
class J2ME_API JarResourceLoader {
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

public:
    JarResourceLoader();
    ~JarResourceLoader();

    // Opening JAR Archives
    bool openFromMemory(const uint8_t* data, size_t size);
    bool openFromFile(const std::string& filePath);
    void close();
    bool isOpen() const;

    // Resource Access (Directly matching AppClassLoader.getResourceAsStream)
    bool hasResource(const std::string& path) const;
    std::vector<uint8_t> getResourceBytes(const std::string& path);
    size_t getResourceSize(const std::string& path) const;
    std::vector<std::string> listResources() const;

    // Descriptor Integration
    std::shared_ptr<midlet::AppDescriptor> getDescriptor() const;

    // Path Normalization utility (matching AppClassLoader.getResourceAsStream)
    static std::string normalizePath(const std::string& path);

    // Cache management
    void clearCache();
};

} // namespace jvm
} // namespace universal_loader
