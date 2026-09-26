#ifndef UNIVERSAL_LOADER_ORIENTATION_H
#define UNIVERSAL_LOADER_ORIENTATION_H

#include "location_types.h"
#include <mutex>

namespace universal_loader {
namespace location {

class J2ME_API Orientation {
public:
    Orientation(float azimuth, bool isMagnetic, float pitch, float roll);
    ~Orientation() = default;

    float getCompassAzimuth() const;
    bool isOrientationMagnetic() const;
    float getPitch() const;
    float getRoll() const;

    // Singleton / Global current orientation tracker
    static Orientation getOrientation();
    static void setGlobalOrientation(float azimuth, bool isMagnetic, float pitch, float roll);

private:
    float m_azimuth{0.0f};
    bool m_isMagnetic{false};
    float m_pitch{0.0f};
    float m_roll{0.0f};

    static std::mutex s_mutex;
    static float s_globalAzimuth;
    static bool s_globalIsMagnetic;
    static float s_globalPitch;
    static float s_globalRoll;
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_ORIENTATION_H
