#include "landmark_store.h"
#include <algorithm>
#include <stdexcept>

namespace universal_loader {
namespace location {

// ==================== Landmark ====================

Landmark::Landmark(std::string name,
                   std::string description,
                   std::shared_ptr<QualifiedCoordinates> coords,
                   std::shared_ptr<AddressInfo> address)
    : m_name(std::move(name)),
      m_description(std::move(description)),
      m_coords(std::move(coords)),
      m_address(std::move(address)) {
}

const std::string& Landmark::getName() const {
    return m_name;
}

void Landmark::setName(const std::string& name) {
    m_name = name;
}

const std::string& Landmark::getDescription() const {
    return m_description;
}

void Landmark::setDescription(const std::string& description) {
    m_description = description;
}

std::shared_ptr<QualifiedCoordinates> Landmark::getQualifiedCoordinates() const {
    return m_coords;
}

void Landmark::setQualifiedCoordinates(std::shared_ptr<QualifiedCoordinates> coords) {
    m_coords = std::move(coords);
}

std::shared_ptr<AddressInfo> Landmark::getAddressInfo() const {
    return m_address;
}

void Landmark::setAddressInfo(std::shared_ptr<AddressInfo> address) {
    m_address = std::move(address);
}

// ==================== LandmarkStore ====================

std::mutex LandmarkStore::s_storesMutex;
std::unordered_map<std::string, std::unique_ptr<LandmarkStore>> LandmarkStore::s_stores;

LandmarkStore::LandmarkStore(std::string name)
    : m_name(std::move(name)) {
}

LandmarkStore* LandmarkStore::getInstance(const std::string& storeName) {
    std::lock_guard<std::mutex> lock(s_storesMutex);
    if (storeName.empty() || storeName == "default") {
        auto it = s_stores.find("default");
        if (it != s_stores.end()) {
            return it->second.get();
        }
        auto store = std::unique_ptr<LandmarkStore>(new LandmarkStore("default"));
        LandmarkStore* ptr = store.get();
        s_stores["default"] = std::move(store);
        return ptr;
    }
    auto it = s_stores.find(storeName);
    if (it != s_stores.end()) {
        return it->second.get();
    }
    return nullptr;
}

void LandmarkStore::createLandmarkStore(const std::string& storeName) {
    if (storeName.empty()) {
        throw std::invalid_argument("Store name cannot be empty");
    }
    std::lock_guard<std::mutex> lock(s_storesMutex);
    if (s_stores.find(storeName) != s_stores.end()) {
        throw std::runtime_error("Landmark store already exists: " + storeName);
    }
    s_stores[storeName] = std::unique_ptr<LandmarkStore>(new LandmarkStore(storeName));
}

void LandmarkStore::deleteLandmarkStore(const std::string& storeName) {
    if (storeName.empty()) {
        throw std::invalid_argument("Store name cannot be empty");
    }
    std::lock_guard<std::mutex> lock(s_storesMutex);
    auto it = s_stores.find(storeName);
    if (it == s_stores.end()) {
        throw std::runtime_error("Landmark store not found: " + storeName);
    }
    s_stores.erase(it);
}

std::vector<std::string> LandmarkStore::listLandmarkStores() {
    std::lock_guard<std::mutex> lock(s_storesMutex);
    std::vector<std::string> names;
    names.reserve(s_stores.size());
    for (const auto& pair : s_stores) {
        names.push_back(pair.first);
    }
    return names;
}

void LandmarkStore::resetAllStores() {
    std::lock_guard<std::mutex> lock(s_storesMutex);
    s_stores.clear();
}

void LandmarkStore::addCategory(const std::string& categoryName) {
    if (categoryName.empty()) {
        throw std::invalid_argument("Category name cannot be empty");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_categories.begin(), m_categories.end(), categoryName) == m_categories.end()) {
        m_categories.push_back(categoryName);
    }
}

void LandmarkStore::deleteCategory(const std::string& categoryName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find(m_categories.begin(), m_categories.end(), categoryName);
    if (it != m_categories.end()) {
        m_categories.erase(it);
    }
    // Remove category from all landmark entries
    for (auto& entry : m_entries) {
        auto catIt = std::find(entry.categories.begin(), entry.categories.end(), categoryName);
        if (catIt != entry.categories.end()) {
            entry.categories.erase(catIt);
        }
    }
}

std::vector<std::string> LandmarkStore::getCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_categories;
}

void LandmarkStore::addLandmark(const Landmark& landmark, const std::string& category) {
    std::lock_guard<std::mutex> lock(m_mutex);
    LandmarkEntry entry;
    entry.landmark = landmark;
    if (!category.empty()) {
        entry.categories.push_back(category);
        if (std::find(m_categories.begin(), m_categories.end(), category) == m_categories.end()) {
            m_categories.push_back(category);
        }
    }
    m_entries.push_back(std::move(entry));
}

void LandmarkStore::updateLandmark(const Landmark& landmark) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : m_entries) {
        if (entry.landmark.getName() == landmark.getName()) {
            entry.landmark = landmark;
            return;
        }
    }
    throw std::runtime_error("Landmark not found to update: " + landmark.getName());
}

void LandmarkStore::deleteLandmark(const std::string& landmarkName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_entries.begin(), m_entries.end(), [&](const LandmarkEntry& e) {
        return e.landmark.getName() == landmarkName;
    });
    m_entries.erase(it, m_entries.end());
}

void LandmarkStore::removeLandmarkFromCategory(const std::string& landmarkName, const std::string& category) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : m_entries) {
        if (entry.landmark.getName() == landmarkName) {
            auto catIt = std::find(entry.categories.begin(), entry.categories.end(), category);
            if (catIt != entry.categories.end()) {
                entry.categories.erase(catIt);
            }
        }
    }
}

std::vector<Landmark> LandmarkStore::getLandmarks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Landmark> list;
    list.reserve(m_entries.size());
    for (const auto& entry : m_entries) {
        list.push_back(entry.landmark);
    }
    return list;
}

std::vector<Landmark> LandmarkStore::getLandmarks(const std::string& category, const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Landmark> list;
    for (const auto& entry : m_entries) {
        if (!category.empty()) {
            if (std::find(entry.categories.begin(), entry.categories.end(), category) == entry.categories.end()) {
                continue;
            }
        }
        if (!name.empty()) {
            if (entry.landmark.getName().find(name) == std::string::npos) {
                continue;
            }
        }
        list.push_back(entry.landmark);
    }
    return list;
}

std::vector<Landmark> LandmarkStore::getLandmarks(const std::string& category, double minLat, double maxLat, double minLon, double maxLon) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Landmark> list;
    for (const auto& entry : m_entries) {
        if (!category.empty()) {
            if (std::find(entry.categories.begin(), entry.categories.end(), category) == entry.categories.end()) {
                continue;
            }
        }
        auto coords = entry.landmark.getQualifiedCoordinates();
        if (coords) {
            double lat = coords->getLatitude();
            double lon = coords->getLongitude();
            if (lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon) {
                list.push_back(entry.landmark);
            }
        }
    }
    return list;
}

} // namespace location
} // namespace universal_loader
