#include "coordinates.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace universal_loader {
namespace location {

// ==================== Coordinates ====================

Coordinates::Coordinates(double latitude, double longitude, float altitude)
    : m_latitude(0.0), m_longitude(0.0), m_altitude(altitude) {
    setLatitude(latitude);
    setLongitude(longitude);
}

double Coordinates::getLatitude() const {
    return m_latitude;
}

void Coordinates::setLatitude(double latitude) {
    if (std::isnan(latitude) || latitude < -90.0 || latitude > 90.0) {
        throw std::invalid_argument("Latitude must be between -90.0 and 90.0 inclusive");
    }
    m_latitude = latitude;
}

double Coordinates::getLongitude() const {
    return m_longitude;
}

void Coordinates::setLongitude(double longitude) {
    if (std::isnan(longitude) || longitude < -180.0 || longitude >= 180.0) {
        throw std::invalid_argument("Longitude must be between -180.0 inclusive and 180.0 exclusive");
    }
    m_longitude = longitude;
}

float Coordinates::getAltitude() const {
    return m_altitude;
}

void Coordinates::setAltitude(float altitude) {
    m_altitude = altitude;
}

float Coordinates::distance(const Coordinates& to) const {
    if (std::isnan(m_latitude) || std::isnan(m_longitude) ||
        std::isnan(to.m_latitude) || std::isnan(to.m_longitude)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (m_latitude == to.m_latitude && m_longitude == to.m_longitude) {
        return 0.0f;
    }

    // Great-Circle Haversine Formula
    double lat1 = m_latitude * PI / 180.0;
    double lon1 = m_longitude * PI / 180.0;
    double lat2 = to.m_latitude * PI / 180.0;
    double lon2 = to.m_longitude * PI / 180.0;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
               std::cos(lat1) * std::cos(lat2) * std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return static_cast<float>(WGS84_EARTH_RADIUS_METERS * c);
}

float Coordinates::azimuthTo(const Coordinates& to) const {
    if (std::isnan(m_latitude) || std::isnan(m_longitude) ||
        std::isnan(to.m_latitude) || std::isnan(to.m_longitude)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (m_latitude == to.m_latitude && m_longitude == to.m_longitude) {
        return 0.0f;
    }

    // Initial Geodetic Bearing / Forward Azimuth Formula
    double lat1 = m_latitude * PI / 180.0;
    double lon1 = m_longitude * PI / 180.0;
    double lat2 = to.m_latitude * PI / 180.0;
    double lon2 = to.m_longitude * PI / 180.0;

    double dlon = lon2 - lon1;
    double y = std::sin(dlon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon);

    double theta = std::atan2(y, x) * 180.0 / PI;
    theta = std::fmod(theta + 360.0, 360.0);

    return static_cast<float>(theta);
}

std::string Coordinates::convert(double coordinate, int32_t outputType) {
    if (std::isnan(coordinate) || coordinate < -180.0 || coordinate >= 180.0) {
        throw std::invalid_argument("Coordinate value is out of range");
    }

    bool negative = (coordinate < 0.0);
    double val = std::abs(coordinate);

    if (outputType == COORDINATE_FORMAT_DD_MM) {
        int dd = static_cast<int>(val);
        double mm = (val - dd) * 60.0;

        std::ostringstream ss;
        if (negative) ss << '-';
        ss << dd << ':';
        ss << std::fixed << std::setprecision(5);
        if (mm < 10.0) ss << '0';
        ss << mm;
        return ss.str();
    } else if (outputType == COORDINATE_FORMAT_DD_MM_SS) {
        int dd = static_cast<int>(val);
        double mmTotal = (val - dd) * 60.0;
        int mm = static_cast<int>(mmTotal);
        double ssVal = (mmTotal - mm) * 60.0;

        // Rounding to 3 decimal places
        double rss = std::floor(ssVal * 1000.0 + 0.5) / 1000.0;
        if (rss >= 60.0) {
            mm++;
            rss -= 60.0;
        }
        if (mm >= 60) {
            dd++;
            mm -= 60;
        }

        std::ostringstream ss;
        if (negative) ss << '-';
        ss << dd << ':';
        if (mm < 10) ss << '0';
        ss << mm << ':';
        ss << std::fixed << std::setprecision(3);
        if (rss < 10.0) ss << '0';
        ss << rss;
        return ss.str();
    }

    throw std::invalid_argument("Invalid coordinate format output type");
}

double Coordinates::convert(const std::string& coordinate) {
    if (coordinate.empty()) {
        throw std::invalid_argument("Empty coordinate string");
    }

    // Trim whitespace
    size_t first = coordinate.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        throw std::invalid_argument("Whitespace only coordinate string");
    }
    size_t last = coordinate.find_last_not_of(" \t\r\n");
    std::string s = coordinate.substr(first, last - first + 1);

    int sign = 1;
    if (s[0] == '-') {
        sign = -1;
        s = s.substr(1);
    }

    size_t colon1 = s.find(':');
    if (colon1 == std::string::npos) {
        throw std::invalid_argument("Missing degree delimiter ':'");
    }

    int dd = std::stoi(s.substr(0, colon1));
    std::string rest = s.substr(colon1 + 1);

    size_t colon2 = rest.find(':');
    if (colon2 != std::string::npos) {
        // Format DD:MM:SS
        int mm = std::stoi(rest.substr(0, colon2));
        double ss = std::stod(rest.substr(colon2 + 1));
        return sign * (dd + mm / 60.0 + ss / 3600.0);
    } else {
        // Format DD:MM.mmmmm
        double mm = std::stod(rest);
        return sign * (dd + mm / 60.0);
    }
}

bool Coordinates::equals(const Coordinates& other) const {
    if (m_latitude != other.m_latitude || m_longitude != other.m_longitude) {
        return false;
    }
    if (std::isnan(m_altitude) && std::isnan(other.m_altitude)) {
        return true;
    }
    return m_altitude == other.m_altitude;
}

// ==================== QualifiedCoordinates ====================

QualifiedCoordinates::QualifiedCoordinates(double latitude, double longitude, float altitude,
                                           float horizontalAccuracy, float verticalAccuracy)
    : Coordinates(latitude, longitude, altitude),
      m_horizontalAccuracy(horizontalAccuracy),
      m_verticalAccuracy(verticalAccuracy) {
}

float QualifiedCoordinates::getHorizontalAccuracy() const {
    return m_horizontalAccuracy;
}

void QualifiedCoordinates::setHorizontalAccuracy(float accuracy) {
    m_horizontalAccuracy = accuracy;
}

float QualifiedCoordinates::getVerticalAccuracy() const {
    return m_verticalAccuracy;
}

void QualifiedCoordinates::setVerticalAccuracy(float accuracy) {
    m_verticalAccuracy = accuracy;
}

} // namespace location
} // namespace universal_loader
