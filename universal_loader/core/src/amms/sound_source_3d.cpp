#include "sound_source_3d.h"
#include <algorithm>

namespace universal_loader {
namespace amms {

Spectator::Spectator() {
    m_location.setCartesian(0, 0, 0);
    m_orientation.setOrientation(0, 0, 0); // Facing -Z
}

SoundSource3D::SoundSource3D() {
    m_location.setCartesian(0, 0, 0);
    m_orientation.setOrientation(0, 0, 0);
}

SpatialAudioOutput SoundSource3D::evaluate(const Spectator& spectator) const {
    SpatialAudioOutput out;

    int sx = 0, sy = 0, sz = 0;
    m_location.getCartesian(sx, sy, sz);

    int lx = 0, ly = 0, lz = 0;
    spectator.getLocation().getCartesian(lx, ly, lz);

    float dx = static_cast<float>(sx - lx);
    float dy = static_cast<float>(sy - ly);
    float dz = static_cast<float>(sz - lz);

    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    out.distanceMm = dist;

    // 1. Gain attenuation
    out.gain = m_attenuation.calculateGain(dist);

    if (dist < 1e-4f) {
        out.pan = 0;
        out.dopplerFactor = 1.0f;
        return out;
    }

    // 2. Stereo Panning relative to Spectator coordinate axes
    Vec3 front, up;
    spectator.getOrientation().getOrientationVectors(front, up);

    // Right vector R = Front x Up (Right-handed system: Cross product)
    Vec3 right = {
        front.y * up.z - front.z * up.y,
        front.z * up.x - front.x * up.z,
        front.x * up.y - front.y * up.x
    };
    float rLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rLen > 1e-6f) {
        right.x /= rLen;
        right.y /= rLen;
        right.z /= rLen;
    }

    // Relative vector normalized
    Vec3 relDir = {dx / dist, dy / dist, dz / dist};

    // Dot with right vector gives horizontal stereo panning [-1.0 .. 1.0]
    float rightProj = relDir.x * right.x + relDir.y * right.y + relDir.z * right.z;
    out.pan = static_cast<int>(std::round(std::clamp(rightProj, -1.0f, 1.0f) * 100.0f));

    // 3. Doppler Effect
    if (m_doppler.isEnabled()) {
        Vec3 srcVel = m_doppler.getVelocityMetersPerSecond();
        // Radial velocity along line of sight (m/s)
        float vRadial = (srcVel.x * relDir.x + srcVel.y * relDir.y + srcVel.z * relDir.z);

        // Doppler shift: f' = f * (c / (c + v_radial))
        float denom = SPEED_OF_SOUND + vRadial;
        if (denom > 1.0f) {
            out.dopplerFactor = std::clamp(SPEED_OF_SOUND / denom, 0.1f, 10.0f);
        } else {
            out.dopplerFactor = 10.0f;
        }
    } else {
        out.dopplerFactor = 1.0f;
    }

    return out;
}

} // namespace amms
} // namespace universal_loader
