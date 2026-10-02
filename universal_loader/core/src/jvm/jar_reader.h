#ifndef J2ME_JAR_READER_H
#define J2ME_JAR_READER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <memory>

namespace j2me {

struct ZipEntryInfo {
    std::string name;
    uint32_t compressedSize{0};
    uint32_t uncompressedSize{0};
    uint32_t localHeaderOffset{0};
    uint16_t compressionMethod{0}; // 0 = Stored, 8 = Deflated
};

class JarReader {
public:
    JarReader();
    ~JarReader();

    bool openFromMemory(const uint8_t* data, size_t size);
    bool openFromFile(const std::string& filePath);
    void close();

    bool hasEntry(const std::string& name) const;
    bool extractEntry(const std::string& name, std::vector<uint8_t>& outData) const;
    std::vector<std::string> listEntries() const;

    // Manifest helpers
    std::map<std::string, std::string> parseManifest() const;
    std::string getManifestProperty(const std::string& key) const;
    std::string getMainMidletClass() const;
    // Decompression helper
    static bool inflateRaw(const uint8_t* compressed, size_t compLen, uint8_t* decompressed, size_t decompLen);

private:
    std::vector<uint8_t> m_rawJarData;
    std::map<std::string, ZipEntryInfo> m_entries;
    std::map<std::string, std::string> m_manifest;

    bool parseCentralDirectory();
};

} // namespace j2me

#endif // J2ME_JAR_READER_H
