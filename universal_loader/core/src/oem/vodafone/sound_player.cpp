#include "sound_player.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace universal_loader {
namespace oem {
namespace vodafone {

// ==================== VodafoneSound ====================

VodafoneSound::VodafoneSound(std::vector<uint8_t> data)
    : m_data(std::move(data)) {
}

const std::vector<uint8_t>& VodafoneSound::getData() const {
    return m_data;
}

size_t VodafoneSound::getSize() const {
    return m_data.size();
}

std::string VodafoneSound::detectMimeType() const {
    if (m_data.size() >= 4) {
        if (std::memcmp(m_data.data(), "MMMD", 4) == 0) {
            return "audio/x-smaf";
        }
        if (std::memcmp(m_data.data(), "MThd", 4) == 0) {
            return "audio/midi";
        }
        if (std::memcmp(m_data.data(), "RIFF", 4) == 0) {
            return "audio/x-wav";
        }
        if (std::memcmp(m_data.data(), "XMF_", 4) == 0) {
            return "audio/xmf";
        }
    }
    // Default upstream fallback format for Vodafone carrier games
    return "audio/xmf";
}

// ==================== VodafoneSoundTrack ====================

VodafoneSoundTrack::VodafoneSoundTrack()
    : m_state(SOUND_STATE_NO_DATA),
      m_volume(SOUND_MAX_VOLUME),
      m_loopCount(0) {
}

void VodafoneSoundTrack::setSound(std::shared_ptr<VodafoneSound> sound) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sound = sound;
    if (m_sound) {
        m_state = SOUND_STATE_READY;
    } else {
        m_state = SOUND_STATE_NO_DATA;
    }
}

std::shared_ptr<VodafoneSound> VodafoneSoundTrack::getSound() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sound;
}

void VodafoneSoundTrack::removeSound() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sound = nullptr;
    m_state = SOUND_STATE_NO_DATA;
}

int32_t VodafoneSoundTrack::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void VodafoneSoundTrack::setVolume(int32_t volume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (volume < 0) {
        m_volume = 0;
    } else if (volume > SOUND_MAX_VOLUME) {
        m_volume = SOUND_MAX_VOLUME;
    } else {
        m_volume = volume;
    }
}

int32_t VodafoneSoundTrack::getVolume() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_volume;
}

void VodafoneSoundTrack::play(int32_t loop) {
    EventListener listenerCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_sound || m_state == SOUND_STATE_NO_DATA) {
            return;
        }
        m_loopCount = (loop == 0) ? -1 : loop;
        m_state = SOUND_STATE_PLAYING;
        listenerCopy = m_listener;
    }
    if (listenerCopy) {
        listenerCopy(EV_LOOP);
    }
}

void VodafoneSoundTrack::stop() {
    EventListener listenerCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == SOUND_STATE_PLAYING || m_state == SOUND_STATE_PAUSED) {
            m_state = SOUND_STATE_READY;
            listenerCopy = m_listener;
        }
    }
    if (listenerCopy) {
        listenerCopy(EV_END);
    }
}

void VodafoneSoundTrack::pause() {
    EventListener listenerCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == SOUND_STATE_PLAYING) {
            m_state = SOUND_STATE_PAUSED;
            listenerCopy = m_listener;
        }
    }
    if (listenerCopy) {
        listenerCopy(EV_PAUSE);
    }
}

void VodafoneSoundTrack::resume() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == SOUND_STATE_PAUSED) {
        m_state = SOUND_STATE_PLAYING;
    }
}

int32_t VodafoneSoundTrack::getLoopCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loopCount;
}

void VodafoneSoundTrack::setEventListener(EventListener listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listener = std::move(listener);
}

void VodafoneSoundTrack::postEvent(int32_t event) {
    EventListener listenerCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        listenerCopy = m_listener;
    }
    if (listenerCopy) {
        listenerCopy(event);
    }
}

// ==================== VodafoneSoundPlayer ====================

VodafoneSoundPlayer::VodafoneSoundPlayer() {
}

VodafoneSoundPlayer& VodafoneSoundPlayer::instance() {
    static VodafoneSoundPlayer s_instance;
    return s_instance;
}

std::shared_ptr<VodafoneSoundTrack> VodafoneSoundPlayer::getTrack() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < SOUND_MAX_TRACKS; ++i) {
        if (!m_tracks[i]) {
            m_tracks[i] = std::make_shared<VodafoneSoundTrack>();
            return m_tracks[i];
        }
    }
    throw std::runtime_error("no more tracks available!");
}

std::shared_ptr<VodafoneSoundTrack> VodafoneSoundPlayer::getTrack(int32_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < SOUND_MAX_TRACKS) {
        if (!m_tracks[index]) {
            m_tracks[index] = std::make_shared<VodafoneSoundTrack>();
        }
        return m_tracks[index];
    }
    return nullptr;
}

void VodafoneSoundPlayer::resetAllTracks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < SOUND_MAX_TRACKS; ++i) {
        if (m_tracks[i]) {
            m_tracks[i]->stop();
            m_tracks[i].reset();
        }
    }
}

} // namespace vodafone
} // namespace oem
} // namespace universal_loader
