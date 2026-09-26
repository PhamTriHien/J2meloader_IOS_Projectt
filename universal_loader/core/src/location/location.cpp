#include "location.h"
#include <chrono>

namespace universal_loader {
namespace location {

Location::Location(const QualifiedCoordinates& coords,
                   float speed,
                   float course,
                   int64_t timestamp,
                   int32_t locationMethod,
                   std::string nmea,
                   std::shared_ptr<AddressInfo> addressInfo)
    : m_coords(coords),
      m_speed(speed),
      m_course(course),
      m_timestamp(timestamp),
      m_locationMethod(locationMethod),
      m_nmea(std::move(nmea)),
      m_addressInfo(std::move(addressInfo)),
      m_valid(true) {
    if (m_timestamp == 0) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        m_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }
}

bool Location::isValid() const {
    return m_valid;
}

int64_t Location::getTimestamp() const {
    return m_timestamp;
}

const QualifiedCoordinates& Location::getQualifiedCoordinates() const {
    return m_coords;
}

float Location::getSpeed() const {
    return m_speed;
}

float Location::getCourse() const {
    return m_course;
}

int32_t Location::getLocationMethod() const {
    return m_locationMethod;
}

std::shared_ptr<AddressInfo> Location::getAddressInfo() const {
    return m_addressInfo;
}

std::string Location::getExtraInfo(const std::string& mimetype) const {
    if (mimetype == "application/X-jsr179-location-nmea") {
        return m_nmea;
    }
    return "";
}

void Location::setSpeed(float speed) {
    m_speed = speed;
}

void Location::setCourse(float course) {
    m_course = course;
}

void Location::setTimestamp(int64_t timestamp) {
    m_timestamp = timestamp;
}

void Location::setNmea(const std::string& nmea) {
    m_nmea = nmea;
}

} // namespace location
} // namespace universal_loader
