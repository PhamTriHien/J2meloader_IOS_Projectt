#ifndef UNIVERSAL_LOADER_AMMS_AUDIO3D_CONTROLS_H
#define UNIVERSAL_LOADER_AMMS_AUDIO3D_CONTROLS_H

#include "amms_types.h"
#include <cmath>
#include <functional>

namespace universal_loader {
namespace amms {

class J2ME_API LocationControl {
public:
    LocationControl();

    void setCartesian(int x, int y, int z);
    void getCartesian(int& out_x, int& out_y, int& out_z) const;

    void setSpherical(int azimuthDeg, int elevationDeg, int radius);
    void getSpherical(int& out_azimuthDeg, int& out_elevationDeg, int& out_radius) const;

    Vec3 getPositionMeters() const {
        return {m_x / 1000.0f, m_y / 1000.0f, m_z / 1000.0f};
    }

private:
    int m_x{0};
    int m_y{0};
    int m_z{0};
};

class J2ME_API OrientationControl {
public:
    OrientationControl();

    void setOrientation(int headingDeg, int pitchDeg, int rollDeg);
    void setOrientationVectors(const Vec3& front, const Vec3& up);

    void getOrientationVectors(Vec3& out_front, Vec3& out_up) const;
    void getEulerAngles(int& out_headingDeg, int& out_pitchDeg, int& out_rollDeg) const;

private:
    Vec3 m_front{0.0f, 0.0f, -1.0f};
    Vec3 m_up{0.0f, 1.0f, 0.0f};
    int m_heading{0};
    int m_pitch{0};
    int m_roll{0};

    void normalizeVectors();
};

class J2ME_API DopplerControl {
public:
    DopplerControl();

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setVelocityCartesian(int vx, int vy, int vz);
    void getVelocityCartesian(int& out_vx, int& out_vy, int& out_vz) const;

    void setVelocitySpherical(int azimuthDeg, int elevationDeg, int speed);

    Vec3 getVelocityMetersPerSecond() const {
        return {m_vx / 1000.0f, m_vy / 1000.0f, m_vz / 1000.0f};
    }

private:
    bool m_enabled{true};
    int m_vx{0};
    int m_vy{0};
    int m_vz{0};
};

class J2ME_API DistanceAttenuationControl {
public:
    DistanceAttenuationControl();

    void setParameters(int minDistance, int maxDistance, bool muteAfterMax, int rolloffFactor);

    int getMinDistance() const { return m_minDistance; }
    int getMaxDistance() const { return m_maxDistance; }
    bool getMuteAfterMax() const { return m_muteAfterMax; }
    int getRolloffFactor() const { return m_rolloffFactor; }

    // Evaluates gain [0.0f, 1.0f] given distance in mm
    float calculateGain(float distanceMm) const;

private:
    int m_minDistance{1000};    // 1 meter default
    int m_maxDistance{1000000}; // 1000 meters default
    bool m_muteAfterMax{false};
    int m_rolloffFactor{1000};  // 1.0 (in thousandths)
};

class J2ME_API CommitControl {
public:
    CommitControl() = default;

    bool isDeferred() const { return m_deferred; }
    void setDeferred(bool deferred) { m_deferred = deferred; }

    void setCommitCallback(std::function<void()> cb) { m_onCommit = cb; }
    void commit() {
        if (m_onCommit) m_onCommit();
    }

private:
    bool m_deferred{false};
    std::function<void()> m_onCommit;
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_AUDIO3D_CONTROLS_H
