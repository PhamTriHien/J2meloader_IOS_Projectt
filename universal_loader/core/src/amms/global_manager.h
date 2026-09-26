#ifndef UNIVERSAL_LOADER_AMMS_GLOBAL_MANAGER_H
#define UNIVERSAL_LOADER_AMMS_GLOBAL_MANAGER_H

#include "amms_types.h"
#include "sound_source_3d.h"
#include "audio_effects.h"
#include "camera_controls.h"
#include <memory>
#include <vector>
#include <mutex>

namespace universal_loader {
namespace amms {

class J2ME_API GlobalManager {
public:
    static GlobalManager& getInstance();

    Spectator& getSpectator() { return m_spectator; }
    const Spectator& getSpectator() const { return m_spectator; }

    std::shared_ptr<SoundSource3D> createSoundSource3D();
    std::shared_ptr<EffectModule> createEffectModule();

    CameraControl& getCameraControl() { return m_camera; }
    FlashControl& getFlashControl() { return m_flash; }
    ZoomControl& getZoomControl() { return m_zoom; }
    ImageTransformControl& getImageTransformControl() { return m_imageTransform; }

    std::vector<std::string> getSupportedSoundSource3DPlayerTypes() const;
    std::vector<std::string> getSupportedMediaProcessorInputTypes() const;

    SpatialAudioOutput processSoundSource(const std::shared_ptr<SoundSource3D>& source) const;

    void reset();

private:
    GlobalManager();
    ~GlobalManager() = default;

    mutable std::mutex m_mutex;
    Spectator m_spectator;
    CameraControl m_camera;
    FlashControl m_flash;
    ZoomControl m_zoom;
    ImageTransformControl m_imageTransform;
    std::vector<std::shared_ptr<SoundSource3D>> m_soundSources;
    std::vector<std::shared_ptr<EffectModule>> m_effectModules;
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_GLOBAL_MANAGER_H
