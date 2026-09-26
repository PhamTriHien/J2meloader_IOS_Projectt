#include "global_manager.h"

namespace universal_loader {
namespace amms {

GlobalManager& GlobalManager::getInstance() {
    static GlobalManager instance;
    return instance;
}

GlobalManager::GlobalManager() {
}

std::shared_ptr<SoundSource3D> GlobalManager::createSoundSource3D() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto src = std::make_shared<SoundSource3D>();
    m_soundSources.push_back(src);
    return src;
}

std::shared_ptr<EffectModule> GlobalManager::createEffectModule() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto mod = std::make_shared<EffectModule>();
    m_effectModules.push_back(mod);
    return mod;
}

std::vector<std::string> GlobalManager::getSupportedSoundSource3DPlayerTypes() const {
    return {"audio/midi", "audio/x-wav", "audio/mpeg", "audio/x-tone-seq", "audio/amr"};
}

std::vector<std::string> GlobalManager::getSupportedMediaProcessorInputTypes() const {
    return {"image/raw", "image/png", "image/jpeg"};
}

SpatialAudioOutput GlobalManager::processSoundSource(const std::shared_ptr<SoundSource3D>& source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!source) return {};
    return source->evaluate(m_spectator);
}

void GlobalManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_soundSources.clear();
    m_effectModules.clear();
    m_spectator = Spectator();
    m_camera = CameraControl();
    m_flash = FlashControl();
    m_zoom = ZoomControl();
    m_imageTransform = ImageTransformControl();
}

} // namespace amms
} // namespace universal_loader
