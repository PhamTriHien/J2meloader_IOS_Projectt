#ifndef UNIVERSAL_LOADER_LANDMARK_STORE_H
#define UNIVERSAL_LOADER_LANDMARK_STORE_H

#include "location_types.h"
#include "coordinates.h"
#include "address_info.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace universal_loader {
namespace location {

class J2ME_API Landmark {
public:
    Landmark() = default;
    Landmark(std::string name,
             std::string description = "",
             std::shared_ptr<QualifiedCoordinates> coords = nullptr,
             std::shared_ptr<AddressInfo> address = nullptr);
    ~Landmark() = default;

    const std::string& getName() const;
    void setName(const std::string& name);

    const std::string& getDescription() const;
    void setDescription(const std::string& description);

    std::shared_ptr<QualifiedCoordinates> getQualifiedCoordinates() const;
    void setQualifiedCoordinates(std::shared_ptr<QualifiedCoordinates> coords);

    std::shared_ptr<AddressInfo> getAddressInfo() const;
    void setAddressInfo(std::shared_ptr<AddressInfo> address);

private:
    std::string m_name;
    std::string m_description;
    std::shared_ptr<QualifiedCoordinates> m_coords;
    std::shared_ptr<AddressInfo> m_address;
};

class J2ME_API LandmarkStore {
public:
    static LandmarkStore* getInstance(const std::string& storeName = "");
    static void createLandmarkStore(const std::string& storeName);
    static void deleteLandmarkStore(const std::string& storeName);
    static std::vector<std::string> listLandmarkStores();
    static void resetAllStores();

    // Category Management
    void addCategory(const std::string& categoryName);
    void deleteCategory(const std::string& categoryName);
    std::vector<std::string> getCategories() const;

    // Landmark Operations
    void addLandmark(const Landmark& landmark, const std::string& category = "");
    void updateLandmark(const Landmark& landmark);
    void deleteLandmark(const std::string& landmarkName);
    void removeLandmarkFromCategory(const std::string& landmarkName, const std::string& category);

    // Query Operations
    std::vector<Landmark> getLandmarks() const;
    std::vector<Landmark> getLandmarks(const std::string& category, const std::string& name = "") const;
    std::vector<Landmark> getLandmarks(const std::string& category, double minLat, double maxLat, double minLon, double maxLon) const;

    ~LandmarkStore() = default;

private:
    explicit LandmarkStore(std::string name);

    struct LandmarkEntry {
        Landmark landmark;
        std::vector<std::string> categories;
    };

    mutable std::mutex m_mutex;
    std::string m_name;
    std::vector<std::string> m_categories;
    std::vector<LandmarkEntry> m_entries;

    static std::mutex s_storesMutex;
    static std::unordered_map<std::string, std::unique_ptr<LandmarkStore>> s_stores;
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_LANDMARK_STORE_H
