#include "audio3d_controls.h"
#include <algorithm>

namespace universal_loader {
namespace amms {

static constexpr float PI = 3.14159265358979323846f;
static constexpr float DEG_TO_RAD = PI / 180.0f;
static constexpr float RAD_TO_DEG = 180.0f / PI;

// -------------------------------------------------------------
// LocationControl
// -------------------------------------------------------------
LocationControl::LocationControl()
    : m_x(0), m_y(0), m_z(0)
{
}

void LocationControl::setCartesian(int x, int y, int z) {
    m_x = x;
    m_y = y;
    m_z = z;
}

void LocationControl::getCartesian(int& out_x, int& out_y, int& out_z) const {
    out_x = m_x;
    out_y = m_y;
    out_z = m_z;
}

void LocationControl::setSpherical(int azimuthDeg, int elevationDeg, int radius) {
    float azimRad = azimuthDeg * DEG_TO_RAD;
    float elevRad = elevationDeg * DEG_TO_RAD;
    float r = static_cast<float>(radius);

    m_x = static_cast<int>(std::round(r * std::cos(elevRad) * std::sin(azimRad)));
    m_y = static_cast<int>(std::round(r * std::sin(elevRad)));
    m_z = static_cast<int>(std::round(-r * std::cos(elevRad) * std::cos(azimRad)));
}

void LocationControl::getSpherical(int& out_azimuthDeg, int& out_elevationDeg, int& out_radius) const {
    float fx = static_cast<float>(m_x);
    float fy = static_cast<float>(m_y);
    float fz = static_cast<float>(m_z);

    float r = std::sqrt(fx * fx + fy * fy + fz * fz);
    out_radius = static_cast<int>(std::round(r));

    if (r < 1e-4f) {
        out_azimuthDeg = 0;
        out_elevationDeg = 0;
        return;
    }

    float elevRad = std::asin(std::clamp(fy / r, -1.0f, 1.0f));
    float azimRad = std::atan2(fx, -fz);

    out_elevationDeg = static_cast<int>(std::round(elevRad * RAD_TO_DEG));
    out_azimuthDeg   = static_cast<int>(std::round(azimRad * RAD_TO_DEG));
}

// -------------------------------------------------------------
// OrientationControl
// -------------------------------------------------------------
OrientationControl::OrientationControl()
    : m_front(0.0f, 0.0f, -1.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_heading(0)
    , m_pitch(0)
    , m_roll(0)
{
}

void OrientationControl::normalizeVectors() {
    float fLen = std::sqrt(m_front.x * m_front.x + m_front.y * m_front.y + m_front.z * m_front.z);
    if (fLen > 1e-6f) {
        m_front.x /= fLen;
        m_front.y /= fLen;
        m_front.z /= fLen;
    } else {
        m_front = {0.0f, 0.0f, -1.0f};
    }

    // Project up onto plane perpendicular to front
    float dot = m_up.x * m_front.x + m_up.y * m_front.y + m_up.z * m_front.z;
    m_up.x -= dot * m_front.x;
    m_up.y -= dot * m_front.y;
    m_up.z -= dot * m_front.z;

    float uLen = std::sqrt(m_up.x * m_up.x + m_up.y * m_up.y + m_up.z * m_up.z);
    if (uLen > 1e-6f) {
        m_up.x /= uLen;
        m_up.y /= uLen;
        m_up.z /= uLen;
    } else {
        m_up = {0.0f, 1.0f, 0.0f};
    }
}

void OrientationControl::setOrientation(int headingDeg, int pitchDeg, int rollDeg) {
    m_heading = headingDeg;
    m_pitch = pitchDeg;
    m_roll = rollDeg;

    float hRad = headingDeg * DEG_TO_RAD;
    float pRad = pitchDeg * DEG_TO_RAD;
    float rRad = rollDeg * DEG_TO_RAD;

    // Front vector
    m_front.x = std::sin(hRad) * std::cos(pRad);
    m_front.y = std::sin(pRad);
    m_front.z = -std::cos(hRad) * std::cos(pRad);

    // Up vector
    m_up.x = -std::sin(hRad) * std::sin(pRad) * std::sin(rRad) - std::cos(hRad) * std::cos(rRad);
    m_up.y = std::cos(pRad) * std::sin(rRad);
    m_up.z = std::cos(hRad) * std::sin(pRad) * std::sin(rRad) - std::sin(hRad) * std::cos(rRad);

    normalizeVectors();
}

void OrientationControl::setOrientationVectors(const Vec3& front, const Vec3& up) {
    m_front = front;
    m_up = up;
    normalizeVectors();

    // Approximate Euler angles from front and up
    float pitchRad = std::asin(std::clamp(m_front.y, -1.0f, 1.0f));
    float headingRad = std::atan2(m_front.x, -m_front.z);
    m_pitch = static_cast<int>(std::round(pitchRad * RAD_TO_DEG));
    m_heading = static_cast<int>(std::round(headingRad * RAD_TO_DEG));
    m_roll = 0;
}

void OrientationControl::getOrientationVectors(Vec3& out_front, Vec3& out_up) const {
    out_front = m_front;
    out_up = m_up;
}

void OrientationControl::getEulerAngles(int& out_headingDeg, int& out_pitchDeg, int& out_rollDeg) const {
    out_headingDeg = m_heading;
    out_pitchDeg = m_pitch;
    out_rollDeg = m_roll;
}

// -------------------------------------------------------------
// DopplerControl
// -------------------------------------------------------------
DopplerControl::DopplerControl()
    : m_enabled(true)
    , m_vx(0), m_vy(0), m_vz(0)
{
}

void DopplerControl::setVelocityCartesian(int vx, int vy, int vz) {
    m_vx = vx;
    m_vy = vy;
    m_vz = vz;
}

void DopplerControl::getVelocityCartesian(int& out_vx, int& out_vy, int& out_vz) const {
    out_vx = m_vx;
    out_vy = m_vy;
    out_vz = m_vz;
}

void DopplerControl::setVelocitySpherical(int azimuthDeg, int elevationDeg, int speed) {
    float azimRad = azimuthDeg * DEG_TO_RAD;
    float elevRad = elevationDeg * DEG_TO_RAD;
    float s = static_cast<float>(speed);

    m_vx = static_cast<int>(std::round(s * std::cos(elevRad) * std::sin(azimRad)));
    m_vy = static_cast<int>(std::round(s * std::sin(elevRad)));
    m_vz = static_cast<int>(std::round(-s * std::cos(elevRad) * std::cos(azimRad)));
}

// -------------------------------------------------------------
// DistanceAttenuationControl
// -------------------------------------------------------------
DistanceAttenuationControl::DistanceAttenuationControl()
    : m_minDistance(1000)
    , m_maxDistance(1000000)
    , m_muteAfterMax(false)
    , m_rolloffFactor(1000)
{
}

void DistanceAttenuationControl::setParameters(int minDistance, int maxDistance, bool muteAfterMax, int rolloffFactor) {
    m_minDistance = (minDistance > 0) ? minDistance : 1;
    m_maxDistance = (maxDistance >= m_minDistance) ? maxDistance : m_minDistance;
    m_muteAfterMax = muteAfterMax;
    m_rolloffFactor = (rolloffFactor >= 0) ? rolloffFactor : 0;
}

float DistanceAttenuationControl::calculateGain(float distanceMm) const {
    if (distanceMm <= static_cast<float>(m_minDistance)) {
        return 1.0f;
    }
    if (m_muteAfterMax && distanceMm > static_cast<float>(m_maxDistance)) {
        return 0.0f;
    }
    float d = distanceMm;
    float dMin = static_cast<float>(m_minDistance);
    float factor = static_cast<float>(m_rolloffFactor) / 1000.0f;
    float denom = dMin + factor * (d - dMin);
    if (denom <= 0.0f) return 1.0f;

    float gain = dMin / denom;
    return std::clamp(gain, 0.0f, 1.0f);
}

} // namespace amms
} // namespace universal_loader
