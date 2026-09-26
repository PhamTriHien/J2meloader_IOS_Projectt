#ifndef UNIVERSAL_LOADER_CRITERIA_H
#define UNIVERSAL_LOADER_CRITERIA_H

#include "location_types.h"

namespace universal_loader {
namespace location {

class J2ME_API Criteria {
public:
    Criteria() = default;
    ~Criteria() = default;

    int32_t getPreferredPowerConsumption() const;
    void setPreferredPowerConsumption(int32_t level);

    bool isAllowedToCost() const;
    void setCostAllowed(bool costAllowed);

    int32_t getVerticalAccuracy() const;
    void setVerticalAccuracy(int32_t accuracy);

    int32_t getHorizontalAccuracy() const;
    void setHorizontalAccuracy(int32_t accuracy);

    int32_t getPreferredResponseTime() const;
    void setPreferredResponseTime(int32_t time);

    bool isSpeedAndCourseRequired() const;
    void setSpeedAndCourseRequired(bool speedAndCourseRequired);

    bool isAltitudeRequired() const;
    void setAltitudeRequired(bool altitudeRequired);

    bool isAddressInfoRequired() const;
    void setAddressInfoRequired(bool addressInfoRequired);

private:
    int32_t m_horizontalAccuracy{0};
    int32_t m_verticalAccuracy{0};
    int32_t m_maxResponseTime{0};
    int32_t m_powerConsumption{POWER_NO_REQUIREMENT};
    bool m_costAllowed{true};
    bool m_speedRequired{false};
    bool m_altitudeRequired{false};
    bool m_addressInfoRequired{false};
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CRITERIA_H
