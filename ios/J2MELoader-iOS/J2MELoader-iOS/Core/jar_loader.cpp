#include "jar_loader.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <zlib.h>

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};

struct ZipCentralDirectoryHeader {
    uint32_t signature;
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumberStart;
    uint16_t internalFileAttributes;
    uint32_t externalFileAttributes;
    uint32_t localHeaderOffset;
};

struct ZipEndOfCentralDirectory {
    uint32_t signature;
    uint16_t diskNumber;
    uint16_t diskWithCD;
    uint16_t totalEntriesDisk;
    uint16_t totalEntries;
    uint32_t sizeOfCD;
    uint32_t offsetOfCD;
    uint16_t commentLength;
};
#pragma pack(pop)

JarLoader::JarLoader() {}
JarLoader::~JarLoader() { close(); }

void JarLoader::close() {
    m_entries.clear();
    m_filePath.clear();
}

bool JarLoader::open(const std::string& filePath) {
    close();
    m_filePath = filePath;
    return readCentralDirectory();
}

bool JarLoader::readCentralDirectory() {
    std::ifstream file(m_filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    if (fileSize < sizeof(ZipEndOfCentralDirectory)) return false;

    // Search for End of Central Directory signature 0x06054b50 in the last 65KB
    size_t searchSize = std::min<size_t>(fileSize, 65536 + sizeof(ZipEndOfCentralDirectory));
    file.seekg(fileSize - searchSize);
    std::vector<uint8_t> buffer(searchSize);
    file.read(reinterpret_cast<char*>(buffer.data()), searchSize);

    size_t eocdOffset = 0;
    bool found = false;
    for (int i = (int)searchSize - (int)sizeof(ZipEndOfCentralDirectory); i >= 0; --i) {
        uint32_t sig = 0;
        std::memcpy(&sig, &buffer[i], sizeof(sig));
        if (sig == 0x06054b50) {
            eocdOffset = fileSize - searchSize + i;
            found = true;
            break;
        }
    }
    if (!found) return false;

    file.seekg(eocdOffset);
    ZipEndOfCentralDirectory eocd;
    file.read(reinterpret_cast<char*>(&eocd), sizeof(eocd));

    file.seekg(eocd.offsetOfCD);
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        ZipCentralDirectoryHeader cd;
        file.read(reinterpret_cast<char*>(&cd), sizeof(cd));
        if (cd.signature != 0x02014b50) break;

        std::string fileName(cd.fileNameLength, '\0');
        file.read(&fileName[0], cd.fileNameLength);

        if (cd.extraFieldLength > 0) file.seekg(cd.extraFieldLength, std::ios::cur);
        if (cd.fileCommentLength > 0) file.seekg(cd.fileCommentLength, std::ios::cur);

        JarEntry entry;
        entry.name = fileName;
        entry.uncompressedSize = cd.uncompressedSize;
        entry.compressedSize = cd.compressedSize;
        entry.offset = cd.localHeaderOffset;
        entry.compressionMethod = cd.compressionMethod;

        m_entries[fileName] = entry;
    }

    return true;
}

bool JarLoader::extractEntry(const std::string& entryName, std::vector<uint8_t>& outData) {
    auto it = m_entries.find(entryName);
    if (it == m_entries.end()) return false;

    const JarEntry& entry = it->second;
    std::ifstream file(m_filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(entry.offset);
    ZipLocalHeader localHeader;
    file.read(reinterpret_cast<char*>(&localHeader), sizeof(localHeader));
    if (localHeader.signature != 0x04034b50) return false;

    file.seekg(entry.offset + sizeof(ZipLocalHeader) + localHeader.fileNameLength + localHeader.extraFieldLength);

    std::vector<uint8_t> compressed(entry.compressedSize);
    file.read(reinterpret_cast<char*>(compressed.data()), entry.compressedSize);

    if (entry.compressionMethod == 0) { // Stored (Uncompressed)
        outData = std::move(compressed);
        return true;
    } else if (entry.compressionMethod == 8) { // Deflated
        outData.resize(entry.uncompressedSize);
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));
        strm.next_in = compressed.data();
        strm.avail_in = (uInt)compressed.size();
        strm.next_out = outData.data();
        strm.avail_out = (uInt)outData.size();

        if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        return (ret == Z_STREAM_END || ret == Z_OK);
    }

    return false;
}

bool JarLoader::extractEntryToFile(const std::string& entryName, const std::string& outputPath) {
    std::vector<uint8_t> data;
    if (!extractEntry(entryName, data)) return false;

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

std::map<std::string, std::string> JarLoader::parseManifest() {
    std::map<std::string, std::string> manifest;
    std::vector<uint8_t> manifestData;
    if (!extractEntry("META-INF/MANIFEST.MF", manifestData)) return manifest;

    std::string text(manifestData.begin(), manifestData.end());
    std::istringstream stream(text);
    std::string line;
    std::string currentKey, currentValue;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;
        if (line.back() == '\r') line.pop_back();

        if (line[0] == ' ' && !currentKey.empty()) {
            currentValue += line.substr(1);
        } else {
            if (!currentKey.empty()) {
                manifest[currentKey] = currentValue;
            }
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                currentKey = line.substr(0, colon);
                currentValue = line.substr(colon + 1);
                if (!currentValue.empty() && currentValue[0] == ' ') {
                    currentValue.erase(0, 1);
                }
            }
        }
    }
    if (!currentKey.empty()) {
        manifest[currentKey] = currentValue;
    }

    return manifest;
}

std::vector<std::string> JarLoader::listEntries() const {
    std::vector<std::string> list;
    for (const auto& pair : m_entries) {
        list.push_back(pair.first);
    }
    return list;
}

bool JarLoader::hasEntry(const std::string& entryName) const {
    return m_entries.find(entryName) != m_entries.end();
}