#ifndef J2ME_RMS_STORAGE_H
#define J2ME_RMS_STORAGE_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <functional>
#include <memory>
#include "../../include/j2me_core.h"

namespace j2me {

// --- Interface Lọc bản ghi (RecordFilter) ---
using RecordFilterFunc = std::function<bool(const std::vector<uint8_t>& recordData)>;

// --- Interface So sánh bản ghi (RecordComparator) ---
// Trả về: -1 (PRECEDES), 0 (EQUIVALENT), 1 (FOLLOWS)
enum RecordCompareResult {
    RMS_PRECEDES   = -1,
    RMS_EQUIVALENT = 0,
    RMS_FOLLOWS    = 1
};
using RecordComparatorFunc = std::function<int(const std::vector<uint8_t>& rec1, const std::vector<uint8_t>& rec2)>;

// --- Interface Lắng nghe thay đổi (RecordListener) ---
class J2ME_API RecordListener {
public:
    virtual ~RecordListener() = default;
    virtual void recordAdded(const std::string& storeName, int recordId) = 0;
    virtual void recordChanged(const std::string& storeName, int recordId) = 0;
    virtual void recordDeleted(const std::string& storeName, int recordId) = 0;
};

class RecordStoreInstance;

// --- Con trỏ duyệt bản ghi (RecordEnumeration) ---
class J2ME_API RecordEnumeration {
public:
    RecordEnumeration(RecordStoreInstance* store,
                      RecordFilterFunc filter,
                      RecordComparatorFunc comparator,
                      bool keepUpdated);
    ~RecordEnumeration();

    int numRecords() const;
    bool hasNextElement() const;
    bool hasPreviousElement() const;
    int nextRecordId();
    bool nextRecord(std::vector<uint8_t>& outData);
    int previousRecordId();
    bool previousRecord(std::vector<uint8_t>& outData);

    void reset();
    void rebuild();
    void keepUpdated(bool keepUpdated);
    void destroy();

    bool isDestroyed() const { return m_destroyed; }

private:
    RecordStoreInstance* m_store;
    RecordFilterFunc m_filter;
    RecordComparatorFunc m_comparator;
    bool m_keepUpdated;
    bool m_destroyed{false};

    std::vector<int> m_orderedIds;
    int m_currentIndex{0};
    mutable std::recursive_mutex m_mutex;

    void notifyRecordChanged();
    friend class RecordStoreInstance;
};

// --- Thực thể RecordStore chuẩn MIDRMS v3.0 ---
class J2ME_API RecordStoreInstance {
public:
    RecordStoreInstance(const std::string& suiteName, const std::string& storeName, const std::string& baseDir);
    ~RecordStoreInstance();

    bool open(bool createIfNecessary);
    void close();
    bool isOpen() const { return m_isOpen; }

    const std::string& getName() const { return m_storeName; }
    const std::string& getSuiteName() const { return m_suiteName; }
    int getVersion() const { return m_version; }
    int64_t getLastModified() const { return m_lastModified; }
    int getNumRecords() const;
    int getSize() const;
    int getSizeAvailable() const;
    int getNextRecordId() const { return m_nextRecordId; }

    int addRecord(const uint8_t* data, size_t size);
    bool setRecord(int recordId, const uint8_t* data, size_t size);
    bool getRecord(int recordId, std::vector<uint8_t>& outData) const;
    int getRecordSize(int recordId) const;
    bool deleteRecord(int recordId);

    std::vector<int> getRecordIds() const;

    // Enumeration & Listeners
    std::shared_ptr<RecordEnumeration> enumerateRecords(RecordFilterFunc filter = nullptr,
                                                        RecordComparatorFunc comparator = nullptr,
                                                        bool keepUpdated = false);
    void addRecordListener(RecordListener* listener);
    void removeRecordListener(RecordListener* listener);

private:
    std::string m_suiteName;
    std::string m_storeName;
    std::string m_filePath;

    bool m_isOpen{false};
    int m_version{0};
    int64_t m_lastModified{0};
    int m_nextRecordId{1};

    std::map<int, std::vector<uint8_t>> m_records;
    std::vector<RecordListener*> m_listeners;
    std::vector<std::weak_ptr<RecordEnumeration>> m_activeEnumerations;

    mutable std::recursive_mutex m_mutex;

    void notifyListeners(int type, int recordId); // 0 = add, 1 = change, 2 = delete
    void saveToDisk();
    bool loadFromDisk();
};

// --- Quản lý toàn cục RMS (RmsManager) ---
class J2ME_API RmsManager {
public:
    static RmsManager& instance();

    void setStorageRoot(const std::string& rootDir);
    const std::string& getStorageRoot() const { return m_rootDir; }

    RecordStoreInstance* openRecordStore(const std::string& suiteName, const std::string& storeName, bool createIfNecessary);
    void closeRecordStore(RecordStoreInstance* store);
    bool deleteRecordStore(const std::string& suiteName, const std::string& storeName);
    std::vector<std::string> listRecordStores(const std::string& suiteName);

private:
    RmsManager();
    ~RmsManager();

    std::string m_rootDir{"./rms_data"};
    std::map<std::string, RecordStoreInstance*> m_openStores;
    std::recursive_mutex m_mutex;

    std::string makeKey(const std::string& suite, const std::string& store);
};

} // namespace j2me

#endif // J2ME_RMS_STORAGE_H
