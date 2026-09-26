#ifndef J2ME_WAV_PLAYER_H
#define J2ME_WAV_PLAYER_H

#include "mmapi_audio.h"
#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>

namespace j2me {

struct WavFormatInfo {
    uint16_t audioFormat{1};    // 1 = PCM
    uint16_t numChannels{1};    // 1 = Mono, 2 = Stereo
    uint32_t sampleRate{44100}; // Sample rate in Hz
    uint32_t byteRate{88200};
    uint16_t blockAlign{2};
    uint16_t bitsPerSample{16}; // 8 or 16
    size_t dataOffset{0};
    size_t dataSize{0};
    size_t totalFrames{0};
};

class J2ME_API WavPlayer : public Player, public VolumeControl {
public:
    static std::shared_ptr<WavPlayer> createFromMemory(const uint8_t* data, size_t size);
    static std::shared_ptr<WavPlayer> createFromFile(const std::string& path);

    WavPlayer(const uint8_t* data, size_t size);
    explicit WavPlayer(const std::string& filePath);
    ~WavPlayer() override;

    // Player Methods
    void realize() override;
    void prefetch() override;
    void start() override;
    void stop() override;
    void deallocate() override;
    void close() override;

    MmapiPlayerState getState() const override { return m_state; }
    void setLoopCount(int count) override;
    int64_t setMediaTime(int64_t nowUsec) override;
    int64_t getMediaTime() const override;
    int64_t getDuration() const override;
    std::string getContentType() const override { return "audio/x-wav"; }

    VolumeControl* getVolumeControl() override { return this; }
    MidiControl* getMidiControl() override { return nullptr; }

    // VolumeControl Methods
    void setLevel(int level) override;
    int getLevel() const override { return m_volume; }
    void setMute(bool mute) override;
    bool isMuted() const override { return m_muted; }

    // Direct Audio Render (Resampled to 44.1kHz 16-bit Stereo PCM)
    size_t renderAudio44100(int16_t* outStereoPcm, size_t frameCount);
    size_t streamToRingBuffer(AudioRingBuffer& ringBuffer, size_t maxFrames);

    const WavFormatInfo& getFormatInfo() const { return m_format; }
    bool isValid() const { return m_valid; }

private:
    std::vector<uint8_t> m_rawData;
    std::vector<int16_t> m_decodedPcmMonoOrStereo; // Normalized to 16-bit signed
    WavFormatInfo m_format;
    bool m_valid{false};

    MmapiPlayerState m_state{PLAYER_UNREALIZED};
    int m_loopCount{1};
    int m_currentLoop{0};
    int m_volume{80};
    bool m_muted{false};

    double m_playbackFramePos{0.0};
    mutable std::mutex m_mutex;

    bool parseRiffHeader();
    void decodeSamplesToPcm16();
    void sampleAt(double frameIndex, int16_t& outLeft, int16_t& outRight) const;
};

} // namespace j2me

#endif // J2ME_WAV_PLAYER_H
