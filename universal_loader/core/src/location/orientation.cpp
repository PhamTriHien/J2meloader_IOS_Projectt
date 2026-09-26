#include "orientation.h"

namespace universal_loader {
namespace location {

std::mutex Orientation::s_mutex;
float Orientation::s_globalAzimuth = 0.0f;
bool Orientation::s_globalIsMagnetic = false;
float Orientation::s_globalPitch = 0.0f;
float Orientation::s_globalRoll = 0.0f;

Orientation::Orientation(float azimuth, bool isMagnetic, float pitch, float roll)
    : m_azimuth(azimuth),
      m_isMagnetic(isMagnetic),
      m_pitch(pitch),
      m_roll(roll) {
}

float Orientation::getCompassAzimuth() const {
    return m_azimuth;
}

bool Orientation::isOrientationMagnetic() const {
    return m_isMagnetic;
}

float Orientation::getPitch() const {
    return m_pitch;
}

float Orientation::getRoll() const {
    return m_roll;
}

Orientation Orientation::getOrientation() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return Orientation(s_globalAzimuth, s_globalIsMagnetic, s_globalPitch, s_globalRoll);
}

void Orientation::setGlobalOrientation(float azimuth, bool isMagnetic, float pitch, float roll) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_globalAzimuth = azimuth;
    s_globalIsMagnetic = isMagnetic;
    s_globalPitch = pitch;
    s_globalRoll = roll;
}

} // namespace location
} // namespace universal_loader
