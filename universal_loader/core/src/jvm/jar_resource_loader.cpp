#include "jar_resource_loader.h"
#include "jar_reader.h"
#include <mutex>
#include <algorithm>

namespace universal_loader {
namespace jvm {

class JarResourceLoader::Impl {
public:
    mutable std::mutex mutex;
    j2me::JarReader reader;
    std::shared_ptr<midlet::AppDescriptor> descriptor;
    std::unordered_map<std::string, std::vector<uint8_t>> cache;
    bool opened{false};

    void parseManifest() {
        std::vector<uint8_t> manifestData;
        if (reader.extractEntry("META-INF/MANIFEST.MF", manifestData) ||
            reader.extractEntry("meta-inf/manifest.mf", manifestData)) {
            std::string content(manifestData.begin(), manifestData.end());
            descriptor = std::make_shared<midlet::AppDescriptor>(content, false);
        } else {
            descriptor = nullptr;
        }
    }
};

JarResourceLoader::JarResourceLoader()
    : m_impl(std::make_unique<Impl>()) {
}

JarResourceLoader::~JarResourceLoader() = default;

std::string JarResourceLoader::normalizePath(const std::string& path) {
    if (path.empty()) return "";

    std::string norm = path;
    // Replace backslashes
    for (char& ch : norm) {
        if (ch == '\\') ch = '/';
    }

    // Collapse multiple consecutive slashes
    std::string clean;
    clean.reserve(norm.size());
    bool lastWasSlash = false;
    for (char ch : norm) {
        if (ch == '/') {
            if (!lastWasSlash) {
                clean += ch;
                lastWasSlash = true;
            }
        } else {
            clean += ch;
            lastWasSlash = false;
        }
    }

    // Strip leading "./"
    while (clean.rfind("./", 0) == 0) {
        clean = clean.substr(2);
    }

    // Strip leading slash
    while (!clean.empty() && clean[0] == '/') {
        clean = clean.substr(1);
    }

    return clean;
}

bool JarResourceLoader::openFromMemory(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->cache.clear();
    m_impl->opened = m_impl->reader.openFromMemory(data, size);
    if (m_impl->opened) {
        m_impl->parseManifest();
    }
    return m_impl->opened;
}

bool JarResourceLoader::openFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->cache.clear();
    m_impl->opened = m_impl->reader.openFromFile(filePath);
    if (m_impl->opened) {
        m_impl->parseManifest();
    }
    return m_impl->opened;
}

void JarResourceLoader::close() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->reader.close();
    m_impl->cache.clear();
    m_impl->descriptor = nullptr;
    m_impl->opened = false;
}

bool JarResourceLoader::isOpen() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->opened;
}

bool JarResourceLoader::hasResource(const std::string& path) const {
    std::string norm = normalizePath(path);
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->opened) return false;
    if (m_impl->cache.find(norm) != m_impl->cache.end()) return true;
    return m_impl->reader.hasEntry(norm);
}

std::vector<uint8_t> JarResourceLoader::getResourceBytes(const std::string& path) {
    std::string norm = normalizePath(path);
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->opened) return {};

    auto it = m_impl->cache.find(norm);
    if (it != m_impl->cache.end()) {
        return it->second;
    }

    std::vector<uint8_t> data;
    if (m_impl->reader.extractEntry(norm, data)) {
        m_impl->cache[norm] = data;
        return data;
    }

    return {};
}

size_t JarResourceLoader::getResourceSize(const std::string& path) const {
    std::string norm = normalizePath(path);
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->opened) return 0;

    auto it = m_impl->cache.find(norm);
    if (it != m_impl->cache.end()) {
        return it->second.size();
    }

    std::vector<uint8_t> temp;
    if (m_impl->reader.extractEntry(norm, temp)) {
        return temp.size();
    }
    return 0;
}

std::vector<std::string> JarResourceLoader::listResources() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_impl->opened) return {};
    return m_impl->reader.listEntries();
}

std::shared_ptr<midlet::AppDescriptor> JarResourceLoader::getDescriptor() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->descriptor;
}

void JarResourceLoader::clearCache() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->cache.clear();
}

} // namespace jvm
} // namespace universal_loader
