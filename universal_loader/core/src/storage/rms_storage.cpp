#include "rms_storage.h"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace j2me {

// --- Big-Endian Byte Helpers (Chuẩn mạng & Chuẩn Java DataOutputStream) ---
static void write_be16(std::ostream& os, uint16_t v) {
    uint8_t b[2] = { (uint8_t)((v >> 8) & 0xFF), (uint8_t)(v & 0xFF) };
    os.write(reinterpret_cast<const char*>(b), 2);
}

static void write_be32(std::ostream& os, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)((v >> 24) & 0xFF), (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),  (uint8_t)(v & 0xFF)
    };
    os.write(reinterpret_cast<const char*>(b), 4);
}

static void write_be64(std::ostream& os, uint64_t v) {
    uint8_t b[8];
    for (int i = 7; i >= 0; --i) {
        b[7 - i] = (uint8_t)((v >> (i * 8)) & 0xFF);
    }
    os.write(reinterpret_cast<const char*>(b), 8);
}

static bool read_be16(std::istream& is, uint16_t& out) {
    uint8_t b[2];
    if (!is.read(reinterpret_cast<char*>(b), 2)) return false;
    out = ((uint16_t)b[0] << 8) | (uint16_t)b[1];
    return true;
}

static bool read_be32(std::istream& is, uint32_t& out) {
    uint8_t b[4];
    if (!is.read(reinterpret_cast<char*>(b), 4)) return false;
    out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
    return true;
}

static bool read_be64(std::istream& is, uint64_t& out) {
    uint8_t b[8];
    if (!is.read(reinterpret_cast<char*>(b), 8)) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | (uint64_t)b[i];
    }
    return true;
}

static void write_utf(std::ostream& os, const std::string& str) {
    write_be16(os, (uint16_t)str.size());
    if (!str.empty()) {
        os.write(str.data(), str.size());
    }
}

static bool read_utf(std::istream& is, std::string& out) {
    uint16_t len = 0;
    if (!read_be16(is, len)) return false;
    out.resize(len);
    if (len > 0) {
        if (!is.read(&out[0], len)) return false;
    }
    return true;
}

// ============================================================================
// RecordEnumeration Triển Khai
// ============================================================================

RecordEnumeration::RecordEnumeration(RecordStoreInstance* store,
                                     RecordFilterFunc filter,
                                     RecordComparatorFunc comparator,
                                     bool keepUpdated)
    : m_store(store), m_filter(filter), m_comparator(comparator), m_keepUpdated(keepUpdated) {
    rebuild();
}

RecordEnumeration::~RecordEnumeration() {
    destroy();
}

void RecordEnumeration::rebuild() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_store || m_destroyed) return;

    std::vector<int> allIds = m_store->getRecordIds();
    m_orderedIds.clear();

    // 1. Lọc bản ghi qua RecordFilter
    for (int id : allIds) {
        if (m_filter) {
            std::vector<uint8_t> data;
            if (m_store->getRecord(id, data)) {
                if (m_filter(data)) {
                    m_orderedIds.push_back(id);
                }
            }
        } else {
            m_orderedIds.push_back(id);
        }
    }

    // 2. Sắp xếp thứ tự qua RecordComparator
    if (m_comparator && m_orderedIds.size() > 1) {
        std::stable_sort(m_orderedIds.begin(), m_orderedIds.end(), [this](int a, int b) {
            std::vector<uint8_t> dataA, dataB;
            m_store->getRecord(a, dataA);
            m_store->getRecord(b, dataB);
            int res = m_comparator(dataA, dataB);
            return res == RMS_PRECEDES;
        });
    }

    m_currentIndex = 0;
}

int RecordEnumeration::numRecords() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (int)m_orderedIds.size();
}

bool RecordEnumeration::hasNextElement() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return !m_destroyed && (m_currentIndex < (int)m_orderedIds.size());
}

bool RecordEnumeration::hasPreviousElement() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return !m_destroyed && (m_currentIndex > 0);
}

int RecordEnumeration::nextRecordId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_destroyed || m_currentIndex >= (int)m_orderedIds.size()) return -1;
    return m_orderedIds[m_currentIndex++];
}

bool RecordEnumeration::nextRecord(std::vector<uint8_t>& outData) {
    int id = nextRecordId();
    if (id < 0) return false;
    return m_store->getRecord(id, outData);
}

int RecordEnumeration::previousRecordId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_destroyed || m_currentIndex <= 0) return -1;
    return m_orderedIds[--m_currentIndex];
}

bool RecordEnumeration::previousRecord(std::vector<uint8_t>& outData) {
    int id = previousRecordId();
    if (id < 0) return false;
    return m_store->getRecord(id, outData);
}

void RecordEnumeration::reset() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_currentIndex = 0;
}

void RecordEnumeration::keepUpdated(bool keepUpdated) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_keepUpdated = keepUpdated;
    if (m_keepUpdated) rebuild();
}

void RecordEnumeration::destroy() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_destroyed = true;
    m_orderedIds.clear();
}

void RecordEnumeration::notifyRecordChanged() {
    if (m_keepUpdated && !m_destroyed) {
        rebuild();
    }
}

// ============================================================================
// RecordStoreInstance Triển Khai (Chuẩn MIDRMS v3.0 Khớp 100% Gốc)
// ============================================================================

RecordStoreInstance::RecordStoreInstance(const std::string& suiteName, const std::string& storeName, const std::string& baseDir)
    : m_suiteName(suiteName), m_storeName(storeName) {
    fs::path dir(baseDir);
    dir /= suiteName;
    try {
        fs::create_directories(dir);
    } catch (...) {}
    m_filePath = (dir / (storeName + ".rms")).string();
}

RecordStoreInstance::~RecordStoreInstance() {
    close();
}

bool RecordStoreInstance::open(bool createIfNecessary) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_isOpen) return true;

    if (fs::exists(m_filePath)) {
        if (loadFromDisk()) {
            m_isOpen = true;
            return true;
        }
    }

    if (createIfNecessary) {
        m_version = 1;
        m_nextRecordId = 1;
        m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        m_records.clear();
        m_isOpen = true;
        saveToDisk();
        return true;
    }
    return false;
}

void RecordStoreInstance::close() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return;

    saveToDisk();
    m_isOpen = false;

    // Hủy các enumerations còn mở
    for (auto& weakEnum : m_activeEnumerations) {
        auto enumPtr = weakEnum.lock();
        if (enumPtr) enumPtr->destroy();
    }
    m_activeEnumerations.clear();
}

int RecordStoreInstance::addRecord(const uint8_t* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return -1;

    int newId = m_nextRecordId++;
    if (data && size > 0) {
        m_records[newId] = std::vector<uint8_t>(data, data + size);
    } else {
        m_records[newId] = std::vector<uint8_t>();
    }

    m_version++;
    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    saveToDisk();
    notifyListeners(0, newId);
    return newId;
}

bool RecordStoreInstance::setRecord(int recordId, const uint8_t* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return false;

    auto it = m_records.find(recordId);
    if (it == m_records.end()) return false;

    if (data && size > 0) {
        it->second.assign(data, data + size);
    } else {
        it->second.clear();
    }

    m_version++;
    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    saveToDisk();
    notifyListeners(1, recordId);
    return true;
}

bool RecordStoreInstance::getRecord(int recordId, std::vector<uint8_t>& outData) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return false;

    auto it = m_records.find(recordId);
    if (it == m_records.end()) return false;
    outData = it->second;
    return true;
}

int RecordStoreInstance::getRecordSize(int recordId) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return -1;

    auto it = m_records.find(recordId);
    if (it == m_records.end()) return -1;
    return (int)it->second.size();
}

bool RecordStoreInstance::deleteRecord(int recordId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_isOpen) return false;

    auto it = m_records.find(recordId);
    if (it == m_records.end()) return false;

    m_records.erase(it);
    m_version++;
    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    saveToDisk();
    notifyListeners(2, recordId);
    return true;
}

int RecordStoreInstance::getNumRecords() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return (int)m_records.size();
}

int RecordStoreInstance::getSize() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int total = 0;
    for (const auto& pair : m_records) {
        total += (int)pair.second.size();
    }
    return total;
}

int RecordStoreInstance::getSizeAvailable() const {
    // 32MB khả dụng chuẩn J2ME
    return 32 * 1024 * 1024 - getSize();
}

std::vector<int> RecordStoreInstance::getRecordIds() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<int> ids;
    ids.reserve(m_records.size());
    for (const auto& pair : m_records) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::shared_ptr<RecordEnumeration> RecordStoreInstance::enumerateRecords(RecordFilterFunc filter,
                                                                          RecordComparatorFunc comparator,
                                                                          bool keepUpdated) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto e = std::make_shared<RecordEnumeration>(this, filter, comparator, keepUpdated);
    m_activeEnumerations.push_back(e);
    return e;
}

void RecordStoreInstance::addRecordListener(RecordListener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (listener) {
        m_listeners.push_back(listener);
    }
}

void RecordStoreInstance::removeRecordListener(RecordListener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
}

void RecordStoreInstance::notifyListeners(int type, int recordId) {
    for (auto* l : m_listeners) {
        if (!l) continue;
        if (type == 0) l->recordAdded(m_storeName, recordId);
        else if (type == 1) l->recordChanged(m_storeName, recordId);
        else if (type == 2) l->recordDeleted(m_storeName, recordId);
    }

    // Thông báo cho các active RecordEnumerations cập nhật lại
    for (auto it = m_activeEnumerations.begin(); it != m_activeEnumerations.end();) {
        auto enumPtr = it->lock();
        if (enumPtr) {
            enumPtr->notifyRecordChanged();
            ++it;
        } else {
            it = m_activeEnumerations.erase(it);
        }
    }
}

// Lưu tệp theo đúng đặc tả nhị phân MIDRMS v3.0 của Android J2ME-Loader
void RecordStoreInstance::saveToDisk() {
    std::ofstream out(m_filePath, std::ios::binary);
    if (!out.is_open()) return;

    // 1. File Identifier: "MIDRMS" (6 bytes)
    const char magic[6] = {'M', 'I', 'D', 'R', 'M', 'S'};
    out.write(magic, 6);

    // 2. Version Major (0x03), Minor (0x00), Encrypted (0x00)
    uint8_t flags[3] = { 0x03, 0x00, 0x00 };
    out.write(reinterpret_cast<const char*>(flags), 3);

    // 3. RecordStoreName (DataOutputStream UTF-8)
    write_utf(out, m_storeName);

    // 4. LastModified (8 bytes int64 big-endian)
    write_be64(out, (uint64_t)m_lastModified);

    // 5. Version (4 bytes int32 big-endian)
    write_be32(out, (uint32_t)m_version);

    // 6. AuthMode (4 bytes = 0), Writable (1 byte = 0)
    write_be32(out, 0);
    uint8_t writable = 0;
    out.write(reinterpret_cast<const char*>(&writable), 1);

    // 7. Số lượng records & lastRecordId
    write_be32(out, (uint32_t)m_records.size());
    write_be32(out, (uint32_t)(m_nextRecordId - 1));

    // 8. Từng bản ghi
    for (const auto& pair : m_records) {
        write_be32(out, (uint32_t)pair.first); // recordId
        write_be32(out, 0);                    // tag
        write_be32(out, (uint32_t)pair.second.size()); // dataLength
        if (!pair.second.empty()) {
            out.write(reinterpret_cast<const char*>(pair.second.data()), pair.second.size());
        }
    }
}

// Đọc và thẩm định tệp MIDRMS v3.0 gốc từ disk
bool RecordStoreInstance::loadFromDisk() {
    std::ifstream in(m_filePath, std::ios::binary);
    if (!in.is_open()) return false;

    // Kiểm tra Magic "MIDRMS"
    char magic[6];
    if (!in.read(magic, 6) || std::memcmp(magic, "MIDRMS", 6) != 0) {
        return false;
    }

    // Đọc version numbers & encrypted
    uint8_t major = 0, minor = 0, enc = 0;
    if (!in.read(reinterpret_cast<char*>(&major), 1)) return false;
    if (!in.read(reinterpret_cast<char*>(&minor), 1)) return false;
    if (!in.read(reinterpret_cast<char*>(&enc), 1)) return false;

    if (major != 0x03) return false; // Chỉ chấp nhận MIDRMS v3.0

    // Đọc tên recordStoreName
    std::string loadedName;
    if (!read_utf(in, loadedName)) return false;

    // Đọc lastModified & version
    uint64_t lastMod = 0;
    uint32_t ver = 0;
    if (!read_be64(in, lastMod)) return false;
    if (!read_be32(in, ver)) return false;
    m_lastModified = (int64_t)lastMod;
    m_version = (int)ver;

    // Đọc AuthMode & Writable
    uint32_t authMode = 0;
    uint8_t writable = 0;
    if (!read_be32(in, authMode)) return false;
    if (!in.read(reinterpret_cast<char*>(&writable), 1)) return false;

    // Đọc count & lastRecordId
    uint32_t count = 0, lastRecId = 0;
    if (!read_be32(in, count)) return false;
    if (!read_be32(in, lastRecId)) return false;
    m_nextRecordId = (int)lastRecId + 1;

    // Đọc toàn bộ danh sách records
    m_records.clear();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id = 0, tag = 0, dataLen = 0;
        if (!read_be32(in, id)) return false;
        if (!read_be32(in, tag)) return false;
        if (!read_be32(in, dataLen)) return false;

        std::vector<uint8_t> data(dataLen);
        if (dataLen > 0) {
            if (!in.read(reinterpret_cast<char*>(data.data()), dataLen)) return false;
        }
        m_records[(int)id] = std::move(data);
    }
    return true;
}

// ============================================================================
// RmsManager Triển Khai
// ============================================================================

RmsManager::RmsManager() {}

RmsManager::~RmsManager() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : m_openStores) {
        delete pair.second;
    }
    m_openStores.clear();
}

void RmsManager::setStorageRoot(const std::string& rootDir) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rootDir = rootDir;
    try {
        fs::create_directories(m_rootDir);
    } catch (...) {}
}

std::string RmsManager::makeKey(const std::string& suite, const std::string& store) {
    return suite + "::" + store;
}

RecordStoreInstance* RmsManager::openRecordStore(const std::string& suiteName, const std::string& storeName, bool createIfNecessary) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string key = makeKey(suiteName, storeName);
    auto it = m_openStores.find(key);
    if (it != m_openStores.end()) {
        return it->second;
    }

    auto* store = new RecordStoreInstance(suiteName, storeName, m_rootDir);
    if (!store->open(createIfNecessary)) {
        delete store;
        return nullptr;
    }
    m_openStores[key] = store;
    return store;
}

void RmsManager::closeRecordStore(RecordStoreInstance* store) {
    if (!store) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto it = m_openStores.begin(); it != m_openStores.end(); ++it) {
        if (it->second == store) {
            store->close();
            delete store;
            m_openStores.erase(it);
            break;
        }
    }
}

bool RmsManager::deleteRecordStore(const std::string& suiteName, const std::string& storeName) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string key = makeKey(suiteName, storeName);
    auto it = m_openStores.find(key);
    if (it != m_openStores.end()) {
        it->second->close();
        delete it->second;
        m_openStores.erase(it);
    }

    fs::path p(m_rootDir);
    p = p / suiteName / (storeName + ".rms");
    if (fs::exists(p)) {
        return fs::remove(p);
    }
    return false;
}

std::vector<std::string> RmsManager::listRecordStores(const std::string& suiteName) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::string> names;
    fs::path p(m_rootDir);
    p /= suiteName;
    if (!fs::exists(p) || !fs::is_directory(p)) return names;

    for (const auto& entry : fs::directory_iterator(p)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rms") {
            names.push_back(entry.path().stem().string());
        }
    }
    return names;
}

} // namespace j2me
