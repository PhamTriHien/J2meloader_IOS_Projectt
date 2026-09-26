#include "file_connection.h"
#include <algorithm>
#include <stdexcept>
#include <chrono>

namespace universal_loader {
namespace file {

FileConnection::FileConnection(std::string url, std::string root, std::string path, std::string name, std::filesystem::path physicalPath)
    : m_url(std::move(url)),
      m_root(std::move(root)),
      m_path(std::move(path)),
      m_name(std::move(name)),
      m_physicalPath(std::move(physicalPath)),
      m_isOpen(true) {}

FileConnection::~FileConnection() {
    close();
}

void FileConnection::close() {
    m_isOpen = false;
}

void FileConnection::throwClosed() const {
    if (!m_isOpen) {
        throw std::runtime_error("FileConnection is closed");
    }
}

bool FileConnection::exists() const {
    throwClosed();
    std::error_code ec;
    return std::filesystem::exists(m_physicalPath, ec);
}

bool FileConnection::isDirectory() const {
    throwClosed();
    std::error_code ec;
    return std::filesystem::is_directory(m_physicalPath, ec);
}

bool FileConnection::isFile() const {
    throwClosed();
    std::error_code ec;
    return std::filesystem::is_regular_file(m_physicalPath, ec);
}

bool FileConnection::isHidden() const {
    throwClosed();
    std::string filename = m_physicalPath.filename().string();
    return !filename.empty() && filename[0] == '.';
}

bool FileConnection::canRead() const {
    throwClosed();
    std::error_code ec;
    auto perms = std::filesystem::status(m_physicalPath, ec).permissions();
    return (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none;
}

bool FileConnection::canWrite() const {
    throwClosed();
    std::error_code ec;
    auto perms = std::filesystem::status(m_physicalPath, ec).permissions();
    return (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
}

void FileConnection::setReadable(bool readable) {
    throwClosed();
    std::error_code ec;
    auto perm = readable ? std::filesystem::perm_options::add : std::filesystem::perm_options::remove;
    std::filesystem::permissions(m_physicalPath, std::filesystem::perms::owner_read, perm, ec);
}

void FileConnection::setWritable(bool writable) {
    throwClosed();
    std::error_code ec;
    auto perm = writable ? std::filesystem::perm_options::add : std::filesystem::perm_options::remove;
    std::filesystem::permissions(m_physicalPath, std::filesystem::perms::owner_write, perm, ec);
}

void FileConnection::setHidden(bool hidden) {
    throwClosed();
    (void)hidden;
}

void FileConnection::create() {
    throwClosed();
    if (exists()) {
        throw std::runtime_error("File already exists: " + m_physicalPath.string());
    }
    if (m_name.empty() || m_name.back() == '/' || m_name.back() == '\\') {
        throw std::runtime_error("Cannot create directory using create()");
    }

    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(m_physicalPath.parent_path(), ec);

    std::ofstream ofs(m_physicalPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to create file: " + m_physicalPath.string());
    }
    ofs.close();
}

void FileConnection::mkdir() {
    throwClosed();
    if (exists()) {
        throw std::runtime_error("Directory or file already exists: " + m_physicalPath.string());
    }
    std::error_code ec;
    std::filesystem::create_directories(m_physicalPath, ec);
    if (ec || !exists()) {
        throw std::runtime_error("Failed to mkdir: " + m_physicalPath.string() + " (" + ec.message() + ")");
    }
    if (!m_name.empty() && m_name.back() != '/') {
        m_name += '/';
    }
}

void FileConnection::deleteFile() {
    throwClosed();
    std::error_code ec;
    if (!std::filesystem::remove(m_physicalPath, ec)) {
        throw std::runtime_error("Unable to delete: " + m_physicalPath.string());
    }
}

void FileConnection::rename(const std::string& newName) {
    throwClosed();
    if (newName.find('/') != std::string::npos || newName.find('\\') != std::string::npos) {
        throw std::runtime_error("newName contains path separator: " + newName);
    }
    auto targetPath = m_physicalPath.parent_path() / newName;
    std::error_code ec;
    std::filesystem::rename(m_physicalPath, targetPath, ec);
    if (ec) {
        throw std::runtime_error("Rename failed: " + ec.message());
    }
    m_physicalPath = targetPath;
    m_name = newName;
}

void FileConnection::truncate(int64_t byteOffset) {
    throwClosed();
    std::error_code ec;
    std::filesystem::resize_file(m_physicalPath, static_cast<uintmax_t>(byteOffset), ec);
    if (ec) {
        throw std::runtime_error("Truncate failed: " + ec.message());
    }
}

void FileConnection::setFileConnection(const std::string& fileName) {
    throwClosed();
    if (fileName == "..") {
        m_physicalPath = m_physicalPath.parent_path();
        m_name = m_physicalPath.filename().string();
        if (isDirectory() && !m_name.empty() && m_name.back() != '/') {
            m_name += '/';
        }
    } else {
        m_physicalPath /= fileName;
        m_name = fileName;
        if (isDirectory() && !m_name.empty() && m_name.back() != '/') {
            m_name += '/';
        }
    }
}

std::string FileConnection::getURL() const {
    return m_url;
}

int64_t FileConnection::fileSize() const {
    throwClosed();
    if (!isFile()) {
        throw std::runtime_error("Not a regular file: " + m_physicalPath.string());
    }
    std::error_code ec;
    auto sz = std::filesystem::file_size(m_physicalPath, ec);
    return ec ? -1 : static_cast<int64_t>(sz);
}

int64_t FileConnection::directorySize(bool includeSubDirs) const {
    throwClosed();
    if (!isDirectory()) {
        throw std::runtime_error("Not a directory: " + m_physicalPath.string());
    }
    int64_t total = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_physicalPath, ec)) {
        if (entry.is_directory(ec)) {
            if (includeSubDirs) {
                FileConnection subConn("", "", "", "", entry.path());
                total += subConn.directorySize(true);
            }
        } else if (entry.is_regular_file(ec)) {
            total += static_cast<int64_t>(entry.file_size(ec));
        }
    }
    return total;
}

int64_t FileConnection::totalSize() const {
    throwClosed();
    std::error_code ec;
    auto spaceInfo = std::filesystem::space(m_physicalPath, ec);
    return ec ? -1 : static_cast<int64_t>(spaceInfo.capacity);
}

int64_t FileConnection::availableSize() const {
    throwClosed();
    std::error_code ec;
    auto spaceInfo = std::filesystem::space(m_physicalPath, ec);
    return ec ? -1 : static_cast<int64_t>(spaceInfo.available);
}

int64_t FileConnection::usedSize() const {
    if (isFile()) {
        return fileSize();
    } else if (isDirectory()) {
        return directorySize(false);
    }
    return -1;
}

int64_t FileConnection::lastModified() const {
    throwClosed();
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(m_physicalPath, ec);
    if (ec) return 0;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();
}

bool FileConnection::matchPattern(const std::string& str, const std::string& pattern) {
    if (pattern == "*" || pattern.empty()) return true;

    size_t s = 0, p = 0, starIdx = std::string::npos, match = 0;
    while (s < str.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
            s++;
            p++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starIdx = p;
            match = s;
            p++;
        } else if (starIdx != std::string::npos) {
            p = starIdx + 1;
            match++;
            s = match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        p++;
    }
    return p == pattern.size();
}

std::vector<std::string> FileConnection::list(const std::string& filter, bool includeHidden) {
    throwClosed();
    if (!isDirectory()) {
        throw std::runtime_error("Not a directory: " + m_physicalPath.string());
    }

    std::vector<std::string> result;
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator(m_physicalPath, ec)) {
        std::string fname = entry.path().filename().string();
        if (!includeHidden && !fname.empty() && fname[0] == '.') {
            continue;
        }

        bool isDir = entry.is_directory(ec);
        std::string matchTarget = fname;

        if (matchPattern(matchTarget, filter)) {
            if (isDir) {
                result.push_back(fname + "/");
            } else {
                result.push_back(fname);
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<uint8_t> FileConnection::readAllBytes() {
    throwClosed();
    std::ifstream ifs(m_physicalPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file for read: " + m_physicalPath.string());
    }
    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0) {
        ifs.read(reinterpret_cast<char*>(buffer.data()), size);
    }
    return buffer;
}

void FileConnection::writeAllBytes(const uint8_t* data, size_t length) {
    throwClosed();
    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(m_physicalPath.parent_path(), ec);

    std::ofstream ofs(m_physicalPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for write: " + m_physicalPath.string());
    }
    if (length > 0 && data != nullptr) {
        ofs.write(reinterpret_cast<const char*>(data), length);
    }
}

void FileConnection::writeBytesAt(int64_t byteOffset, const uint8_t* data, size_t length) {
    throwClosed();
    // Ensure file exists
    if (!exists()) {
        create();
    }

    std::fstream fs(m_physicalPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open file for seek-write: " + m_physicalPath.string());
    }
    fs.seekp(byteOffset);
    if (length > 0 && data != nullptr) {
        fs.write(reinterpret_cast<const char*>(data), length);
    }
}

} // namespace file
} // namespace universal_loader
