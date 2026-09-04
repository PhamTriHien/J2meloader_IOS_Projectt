#include "rms_storage.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

RmsStorage::RmsStorage() : m_baseDir("./RMS") {}
RmsStorage::~RmsStorage() {}

RmsStorage& RmsStorage::getInstance() {
    static RmsStorage instance;
    return instance;
}

void RmsStorage::setBaseDirectory(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseDir = path;
    mkdir(m_baseDir.c_str(), 0755);
}

std::string RmsStorage::getStoreFilePath(const std::string& suiteName, const std::string& storeName) {
    return m_baseDir + "/" + suiteName + "_" + storeName + ".rms";
}

void RmsStorage::loadFromDisk(const std::string& suiteName, const std::string& storeName) {
    std::string path = getStoreFilePath(suiteName, storeName);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    auto& records = m_openStores[storeName];
    records.clear();
    int maxId = 0;

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        int32_t id = 0;
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&id), sizeof(id));
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        std::vector<uint8_t> data(size);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(data.data()), size);
        }
        records[id] = std::move(data);
        if (id > maxId) maxId = id;
    }

    m_nextRecordIds[storeName] = maxId + 1;
}

void RmsStorage::saveToDisk(const std::string& suiteName, const std::string& storeName) {
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return;

    std::string path = getStoreFilePath(suiteName, storeName);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    const auto& records = it->second;
    uint32_t count = (uint32_t)records.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& pair : records) {
        int32_t id = pair.first;
        uint32_t size = (uint32_t)pair.second.size();
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (size > 0) {
            file.write(reinterpret_cast<const char*>(pair.second.data()), size);
        }
    }
}

bool RmsStorage::openRecordStore(const std::string& suiteName, const std::string& storeName, bool createIfNecessary) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_openStores.find(storeName) != m_openStores.end()) return true;

    std::string path = getStoreFilePath(suiteName, storeName);
    bool fileExists = std::ifstream(path).good();
    if (!fileExists && !createIfNecessary) return false;

    loadFromDisk(suiteName, storeName);
    if (m_nextRecordIds.find(storeName) == m_nextRecordIds.end()) {
        m_nextRecordIds[storeName] = 1;
    }
    return true;
}

void RmsStorage::closeRecordStore(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    saveToDisk("Default", storeName);
    m_openStores.erase(storeName);
}

int RmsStorage::addRecord(const std::string& storeName, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return -1;

    int recordId = m_nextRecordIds[storeName]++;
    it->second[recordId] = std::vector<uint8_t>(data, data + size);
    saveToDisk("Default", storeName);
    return recordId;
}

bool RmsStorage::getRecord(const std::string& storeName, int recordId, std::vector<uint8_t>& outData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return false;

    auto recIt = it->second.find(recordId);
    if (recIt == it->second.end()) return false;

    outData = recIt->second;
    return true;
}

bool RmsStorage::setRecord(const std::string& storeName, int recordId, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return false;

    it->second[recordId] = std::vector<uint8_t>(data, data + size);
    saveToDisk("Default", storeName);
    return true;
}

bool RmsStorage::deleteRecord(const std::string& storeName, int recordId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return false;

    bool erased = it->second.erase(recordId) > 0;
    if (erased) saveToDisk("Default", storeName);
    return erased;
}

int RmsStorage::getNumRecords(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return 0;
    return (int)it->second.size();
}