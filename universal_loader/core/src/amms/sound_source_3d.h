#ifndef UNIVERSAL_LOADER_AMMS_SOUND_SOURCE_3D_H
#define UNIVERSAL_LOADER_AMMS_SOUND_SOURCE_3D_H

#include "amms_types.h"
#include "audio3d_controls.h"
#include <memory>

namespace universal_loader {
namespace amms {

class J2ME_API Spectator {
public:
    Spectator();

    LocationControl& getLocation() { return m_location; }
    const LocationControl& getLocation() const { return m_location; }

    OrientationControl& getOrientation() { return m_orientation; }
    const OrientationControl& getOrientation() const { return m_orientation; }

private:
    LocationControl m_location;
    OrientationControl m_orientation;
};

struct SpatialAudioOutput {
    float gain{1.0f};           // Volume attenuation [0.0f, 1.0f]
    int pan{0};                 // Stereo pan [-100 (left) .. 100 (right)]
    float dopplerFactor{1.0f};  // Frequency pitch multiplier (1.0 = normal)
    float distanceMm{0.0f};     // Distance to listener in mm
};

class J2ME_API SoundSource3D {
public:
    SoundSource3D();
    ~SoundSource3D() = default;

    LocationControl& getLocation() { return m_location; }
    const LocationControl& getLocation() const { return m_location; }

    OrientationControl& getOrientation() { return m_orientation; }
    const OrientationControl& getOrientation() const { return m_orientation; }

    DopplerControl& getDoppler() { return m_doppler; }
    const DopplerControl& getDoppler() const { return m_doppler; }

    DistanceAttenuationControl& getAttenuation() { return m_attenuation; }
    const DistanceAttenuationControl& getAttenuation() const { return m_attenuation; }

    CommitControl& getCommit() { return m_commit; }
    const CommitControl& getCommit() const { return m_commit; }

    // Evaluates current spatial parameters relative to the given spectator
    SpatialAudioOutput evaluate(const Spectator& spectator) const;

private:
    LocationControl m_location;
    OrientationControl m_orientation;
    DopplerControl m_doppler;
    DistanceAttenuationControl m_attenuation;
    CommitControl m_commit;
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_SOUND_SOURCE_3D_H
