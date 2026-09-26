#pragma once

#include "j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cstdint>

namespace universal_loader {
namespace file {

class J2ME_API FileConnection {
public:
    FileConnection(std::string url, std::string root, std::string path, std::string name, std::filesystem::path physicalPath);
    ~FileConnection();

    bool isOpen() const { return m_isOpen; }
    void close();

    bool exists() const;
    bool isDirectory() const;
    bool isFile() const;
    bool isHidden() const;
    bool canRead() const;
    bool canWrite() const;

    void setReadable(bool readable);
    void setWritable(bool writable);
    void setHidden(bool hidden);

    void create();
    void mkdir();
    void deleteFile();
    void rename(const std::string& newName);
    void truncate(int64_t byteOffset);

    void setFileConnection(const std::string& fileName);

    const std::string& getName() const { return m_name; }
    const std::string& getPath() const { return m_path; }
    std::string getURL() const;

    int64_t fileSize() const;
    int64_t directorySize(bool includeSubDirs) const;
    int64_t totalSize() const;
    int64_t availableSize() const;
    int64_t usedSize() const;
    int64_t lastModified() const;

    std::vector<std::string> list(const std::string& filter = "*", bool includeHidden = false);

    std::vector<uint8_t> readAllBytes();
    void writeAllBytes(const uint8_t* data, size_t length);
    void writeBytesAt(int64_t byteOffset, const uint8_t* data, size_t length);

    const std::filesystem::path& getPhysicalPath() const { return m_physicalPath; }

private:
    void throwClosed() const;
    static bool matchPattern(const std::string& str, const std::string& pattern);

    std::string m_url;
    std::string m_root;
    std::string m_path;
    std::string m_name;
    std::filesystem::path m_physicalPath;
    bool m_isOpen;
};

} // namespace file
} // namespace universal_loader
