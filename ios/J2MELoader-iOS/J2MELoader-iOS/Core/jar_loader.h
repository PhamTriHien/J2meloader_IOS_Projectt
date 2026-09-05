#ifndef JAR_LOADER_H
#define JAR_LOADER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct JarEntry {
    std::string name;
    uint32_t uncompressedSize;
    uint32_t compressedSize;
    uint32_t offset;
    uint16_t compressionMethod;
};

class JarLoader {
public:
    JarLoader();
    ~JarLoader();

    bool open(const std::string& filePath);
    void close();

    std::map<std::string, std::string> parseManifest();
    bool extractEntry(const std::string& entryName, std::vector<uint8_t>& outData);
    bool extractEntryToFile(const std::string& entryName, const std::string& outputPath);
    std::vector<std::string> listEntries() const;
    bool hasEntry(const std::string& entryName) const;
    const std::string& getFilePath() const { return m_filePath; }

private:
    std::string m_filePath;
    std::map<std::string, JarEntry> m_entries;
    bool readCentralDirectory();
};

#endif // JAR_LOADER_H