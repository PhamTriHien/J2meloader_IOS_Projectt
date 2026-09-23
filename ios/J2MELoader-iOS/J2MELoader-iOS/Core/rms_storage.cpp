#include "rms_storage.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

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

static const uint32_t RMS_MAGIC_V2 = 0x524D5332; // "RMS2"

void RmsStorage::loadFromDisk(const std::string& suiteName, const std::string& storeName) {
    std::string path = getStoreFilePath(suiteName, storeName);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    auto& records = m_openStores[storeName];
    records.clear();
    int maxId = 0;
    int persistedNextId = -1;

    uint32_t firstWord = 0;
    if (!file.read(reinterpret_cast<char*>(&firstWord), sizeof(firstWord))) return;

    uint32_t count = 0;
    if (firstWord == RMS_MAGIC_V2) {
        int32_t nId = 1;
        file.read(reinterpret_cast<char*>(&nId), sizeof(nId));
        persistedNextId = nId;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
    } else {
        count = firstWord;
    }

    for (uint32_t i = 0; i < count; ++i) {
        int32_t id = 0;
        uint32_t size = 0;
        if (!file.read(reinterpret_cast<char*>(&id), sizeof(id))) break;
        if (!file.read(reinterpret_cast<char*>(&size), sizeof(size))) break;

        std::vector<uint8_t> data(size);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(data.data()), size);
        }
        records[id] = std::move(data);
        if (id > maxId) maxId = id;
    }

    if (persistedNextId > maxId) {
        m_nextRecordIds[storeName] = persistedNextId;
    } else {
        m_nextRecordIds[storeName] = maxId + 1;
    }
}

void RmsStorage::saveToDisk(const std::string& suiteName, const std::string& storeName) {
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return;

    std::string path = getStoreFilePath(suiteName, storeName);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    const auto& records = it->second;
    uint32_t magic = RMS_MAGIC_V2;
    int32_t nextId = m_nextRecordIds[storeName];
    if (nextId <= 0) nextId = 1;
    uint32_t count = (uint32_t)records.size();

    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&nextId), sizeof(nextId));
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

std::string RmsStorage::getSuite(const std::string& storeName) {
    auto it = m_storeSuites.find(storeName);
    return (it != m_storeSuites.end() && !it->second.empty()) ? it->second : "J2MEApp";
}

bool RmsStorage::openRecordStore(const std::string& suiteName, const std::string& storeName, bool createIfNecessary) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string suite = suiteName.empty() ? "J2MEApp" : suiteName;
    m_storeSuites[storeName] = suite;
    if (m_openStores.find(storeName) != m_openStores.end()) return true;

    std::string path = getStoreFilePath(suite, storeName);
    bool fileExists = std::ifstream(path).good();
    if (!fileExists && !createIfNecessary) return false;

    loadFromDisk(suite, storeName);
    if (m_nextRecordIds.find(storeName) == m_nextRecordIds.end()) {
        m_nextRecordIds[storeName] = 1;
    }
    return true;
}

void RmsStorage::closeRecordStore(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string suite = getSuite(storeName);
    saveToDisk(suite, storeName);
    m_openStores.erase(storeName);
    m_storeSuites.erase(storeName);
}

int RmsStorage::addRecord(const std::string& storeName, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return -1;

    int recordId = m_nextRecordIds[storeName]++;
    it->second[recordId] = std::vector<uint8_t>(data, data + size);
    std::string suite = getSuite(storeName);
    saveToDisk(suite, storeName);
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
    std::string suite = getSuite(storeName);
    saveToDisk(suite, storeName);
    return true;
}

bool RmsStorage::deleteRecord(const std::string& storeName, int recordId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return false;

    bool erased = it->second.erase(recordId) > 0;
    if (erased) {
        std::string suite = getSuite(storeName);
        saveToDisk(suite, storeName);
    }
    return erased;
}

int RmsStorage::getNumRecords(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return 0;
    return (int)it->second.size();
}

std::vector<int> RmsStorage::getRecordIds(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<int> ids;
    auto it = m_openStores.find(storeName);
    if (it != m_openStores.end()) {
        ids.reserve(it->second.size());
        for (const auto& pair : it->second) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

int RmsStorage::getNextRecordID(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_nextRecordIds.find(storeName);
    return (it != m_nextRecordIds.end()) ? it->second : 1;
}

int RmsStorage::getRecordSize(const std::string& storeName, int recordId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return 0;
    auto recIt = it->second.find(recordId);
    return (recIt != it->second.end()) ? (int)recIt->second.size() : 0;
}

int RmsStorage::getSize(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openStores.find(storeName);
    if (it == m_openStores.end()) return 0;
    int total = 0;
    for (const auto& pair : it->second) {
        total += (int)pair.second.size();
    }
    return total;
}

bool RmsStorage::deleteRecordStore(const std::string& suiteName, const std::string& storeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string suite = suiteName.empty() ? getSuite(storeName) : suiteName;
    m_openStores.erase(storeName);
    m_storeSuites.erase(storeName);
    m_nextRecordIds.erase(storeName);
    std::string path = getStoreFilePath(suite, storeName);
    return (remove(path.c_str()) == 0);
}

std::vector<std::string> RmsStorage::listRecordStores(const std::string& suiteName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> stores;
    std::string targetSuite = suiteName.empty() ? "J2MEApp" : suiteName;
    for (const auto& pair : m_storeSuites) {
        if (pair.second == targetSuite || targetSuite.empty()) {
            stores.push_back(pair.first);
        }
    }
    return stores;
}