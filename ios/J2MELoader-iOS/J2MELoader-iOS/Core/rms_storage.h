#ifndef RMS_STORAGE_H
#define RMS_STORAGE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

class RmsStorage {
public:
    static RmsStorage& getInstance();

    void setBaseDirectory(const std::string& path);
    
    bool openRecordStore(const std::string& suiteName, const std::string& storeName, bool createIfNecessary);
    void closeRecordStore(const std::string& storeName);
    bool deleteRecordStore(const std::string& suiteName, const std::string& storeName);

    int addRecord(const std::string& storeName, const uint8_t* data, size_t size);
    bool getRecord(const std::string& storeName, int recordId, std::vector<uint8_t>& outData);
    bool setRecord(const std::string& storeName, int recordId, const uint8_t* data, size_t size);
    bool deleteRecord(const std::string& storeName, int recordId);
    int getNumRecords(const std::string& storeName);

    std::vector<std::string> listRecordStores(const std::string& suiteName);

private:
    RmsStorage();
    ~RmsStorage();

    std::string m_baseDir;
    std::mutex m_mutex;
    std::map<std::string, std::map<int, std::vector<uint8_t>>> m_openStores;
    std::map<std::string, int> m_nextRecordIds;

    std::string getStoreFilePath(const std::string& suiteName, const std::string& storeName);
    void loadFromDisk(const std::string& suiteName, const std::string& storeName);
    void saveToDisk(const std::string& suiteName, const std::string& storeName);
};

#endif // RMS_STORAGE_H