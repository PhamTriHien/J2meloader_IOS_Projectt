#ifndef UNIVERSAL_LOADER_LOCATION_TYPES_H
#define UNIVERSAL_LOADER_LOCATION_TYPES_H

#include "j2me_core.h"
#include <cstdint>
#include <cmath>

namespace universal_loader {
namespace location {

// Coordinate conversion format types (Coordinates.convert)
constexpr int32_t COORDINATE_FORMAT_DD_MM_SS = 1;
constexpr int32_t COORDINATE_FORMAT_DD_MM    = 2;

// Location Technology & Method Constants (Location.java)
constexpr int32_t MTE_SATELLITE        = 1;
constexpr int32_t MTE_TIMEDIFFERENCE   = 2;
constexpr int32_t MTE_TIMEOFARRIVAL    = 4;
constexpr int32_t MTE_CELLID           = 8;
constexpr int32_t MTE_SHORTRANGE       = 16;
constexpr int32_t MTE_ANGLEOFARRIVAL   = 32;
constexpr int32_t MTY_TERMINALBASED    = 65536;
constexpr int32_t MTY_NETWORKBASED     = 131072;
constexpr int32_t MTA_ASSISTED         = 262144;
constexpr int32_t MTA_UNASSISTED       = 524288;

// LocationProvider State Constants (LocationProvider.java)
constexpr int32_t LOCATION_PROVIDER_AVAILABLE               = 1;
constexpr int32_t LOCATION_PROVIDER_TEMPORARILY_UNAVAILABLE = 2;
constexpr int32_t LOCATION_PROVIDER_OUT_OF_SERVICE          = 3;

// Criteria Power Requirements (Criteria.java)
constexpr int32_t POWER_NO_REQUIREMENT = 0;
constexpr int32_t POWER_USAGE_LOW      = 1;
constexpr int32_t POWER_USAGE_MEDIUM   = 2;
constexpr int32_t POWER_USAGE_HIGH     = 3;

// Geodetic constants (WGS-84 Sphere approximation)
constexpr double WGS84_EARTH_RADIUS_METERS = 6371000.0;
constexpr double PI = 3.14159265358979323846;

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_LOCATION_TYPES_H
