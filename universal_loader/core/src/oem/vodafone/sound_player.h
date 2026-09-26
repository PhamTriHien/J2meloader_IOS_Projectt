#ifndef UNIVERSAL_LOADER_VODAFONE_SOUND_PLAYER_H
#define UNIVERSAL_LOADER_VODAFONE_SOUND_PLAYER_H

#include "vodafone_types.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <mutex>

namespace universal_loader {
namespace oem {
namespace vodafone {

class J2ME_API VodafoneSound {
public:
    explicit VodafoneSound(std::vector<uint8_t> data);
    ~VodafoneSound() = default;

    const std::vector<uint8_t>& getData() const;
    size_t getSize() const;
    std::string detectMimeType() const;

private:
    std::vector<uint8_t> m_data;
};

class J2ME_API VodafoneSoundTrack {
public:
    using EventListener = std::function<void(int32_t event)>;

    VodafoneSoundTrack();
    ~VodafoneSoundTrack() = default;

    void setSound(std::shared_ptr<VodafoneSound> sound);
    std::shared_ptr<VodafoneSound> getSound() const;
    void removeSound();

    int32_t getState() const;
    void setVolume(int32_t volume);
    int32_t getVolume() const;

    void play(int32_t loop = 1);
    void stop();
    void pause();
    void resume();

    int32_t getLoopCount() const;
    void setEventListener(EventListener listener);
    void postEvent(int32_t event);

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<VodafoneSound> m_sound;
    int32_t m_state{SOUND_STATE_NO_DATA};
    int32_t m_volume{SOUND_MAX_VOLUME};
    int32_t m_loopCount{0};
    EventListener m_listener;
};

class J2ME_API VodafoneSoundPlayer {
public:
    VodafoneSoundPlayer();
    ~VodafoneSoundPlayer() = default;

    static VodafoneSoundPlayer& instance();

    std::shared_ptr<VodafoneSoundTrack> getTrack();
    std::shared_ptr<VodafoneSoundTrack> getTrack(int32_t index);
    void resetAllTracks();

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<VodafoneSoundTrack> m_tracks[SOUND_MAX_TRACKS];
};

} // namespace vodafone
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_VODAFONE_SOUND_PLAYER_H
