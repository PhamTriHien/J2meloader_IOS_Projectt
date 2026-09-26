#ifndef UNIVERSAL_LOADER_LOCATION_H
#define UNIVERSAL_LOADER_LOCATION_H

#include "location_types.h"
#include "coordinates.h"
#include "address_info.h"
#include <string>
#include <memory>
#include <limits>

namespace universal_loader {
namespace location {

class J2ME_API Location {
public:
    Location(const QualifiedCoordinates& coords,
             float speed = std::numeric_limits<float>::quiet_NaN(),
             float course = std::numeric_limits<float>::quiet_NaN(),
             int64_t timestamp = 0,
             int32_t locationMethod = (MTE_SATELLITE | MTY_TERMINALBASED),
             std::string nmea = "",
             std::shared_ptr<AddressInfo> addressInfo = nullptr);
    ~Location() = default;

    bool isValid() const;
    int64_t getTimestamp() const;
    const QualifiedCoordinates& getQualifiedCoordinates() const;
    float getSpeed() const;
    float getCourse() const;
    int32_t getLocationMethod() const;
    std::shared_ptr<AddressInfo> getAddressInfo() const;
    std::string getExtraInfo(const std::string& mimetype) const;

    void setSpeed(float speed);
    void setCourse(float course);
    void setTimestamp(int64_t timestamp);
    void setNmea(const std::string& nmea);

private:
    QualifiedCoordinates m_coords;
    float m_speed;
    float m_course;
    int64_t m_timestamp;
    int32_t m_locationMethod;
    std::string m_nmea;
    std::shared_ptr<AddressInfo> m_addressInfo;
    bool m_valid{true};
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_LOCATION_H
