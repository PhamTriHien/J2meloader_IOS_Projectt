#include "criteria.h"

namespace universal_loader {
namespace location {

int32_t Criteria::getPreferredPowerConsumption() const {
    return m_powerConsumption;
}

void Criteria::setPreferredPowerConsumption(int32_t level) {
    m_powerConsumption = level;
}

bool Criteria::isAllowedToCost() const {
    return m_costAllowed;
}

void Criteria::setCostAllowed(bool costAllowed) {
    m_costAllowed = costAllowed;
}

int32_t Criteria::getVerticalAccuracy() const {
    return m_verticalAccuracy;
}

void Criteria::setVerticalAccuracy(int32_t accuracy) {
    m_verticalAccuracy = accuracy;
}

int32_t Criteria::getHorizontalAccuracy() const {
    return m_horizontalAccuracy;
}

void Criteria::setHorizontalAccuracy(int32_t accuracy) {
    m_horizontalAccuracy = accuracy;
}

int32_t Criteria::getPreferredResponseTime() const {
    return m_maxResponseTime;
}

void Criteria::setPreferredResponseTime(int32_t time) {
    m_maxResponseTime = time;
}

bool Criteria::isSpeedAndCourseRequired() const {
    return m_speedRequired;
}

void Criteria::setSpeedAndCourseRequired(bool speedAndCourseRequired) {
    m_speedRequired = speedAndCourseRequired;
}

bool Criteria::isAltitudeRequired() const {
    return m_altitudeRequired;
}

void Criteria::setAltitudeRequired(bool altitudeRequired) {
    m_altitudeRequired = altitudeRequired;
}

bool Criteria::isAddressInfoRequired() const {
    return m_addressInfoRequired;
}

void Criteria::setAddressInfoRequired(bool addressInfoRequired) {
    m_addressInfoRequired = addressInfoRequired;
}

} // namespace location
} // namespace universal_loader
