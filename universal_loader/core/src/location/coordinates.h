#ifndef UNIVERSAL_LOADER_COORDINATES_H
#define UNIVERSAL_LOADER_COORDINATES_H

#include "location_types.h"
#include <string>
#include <limits>

namespace universal_loader {
namespace location {

class J2ME_API Coordinates {
public:
    Coordinates(double latitude, double longitude, float altitude = std::numeric_limits<float>::quiet_NaN());
    virtual ~Coordinates() = default;

    double getLatitude() const;
    void setLatitude(double latitude);

    double getLongitude() const;
    void setLongitude(double longitude);

    float getAltitude() const;
    void setAltitude(float altitude);

    // Geodetic calculations
    float distance(const Coordinates& to) const;
    float azimuthTo(const Coordinates& to) const;

    // Formatting & Parsing
    static std::string convert(double coordinate, int32_t outputType);
    static double convert(const std::string& coordinate);

    bool equals(const Coordinates& other) const;

protected:
    double m_latitude;
    double m_longitude;
    float m_altitude;
};

class J2ME_API QualifiedCoordinates : public Coordinates {
public:
    QualifiedCoordinates(double latitude, double longitude, float altitude,
                         float horizontalAccuracy = std::numeric_limits<float>::quiet_NaN(),
                         float verticalAccuracy = std::numeric_limits<float>::quiet_NaN());
    ~QualifiedCoordinates() override = default;

    float getHorizontalAccuracy() const;
    void setHorizontalAccuracy(float accuracy);

    float getVerticalAccuracy() const;
    void setVerticalAccuracy(float accuracy);

private:
    float m_horizontalAccuracy;
    float m_verticalAccuracy;
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_COORDINATES_H
