#ifndef J2ME_MMAPI_AUDIO_H
#define J2ME_MMAPI_AUDIO_H

#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace j2me {

// --- 1. J2ME Player Trạng thái & Sự kiện ---
enum MmapiPlayerState {
    PLAYER_CLOSED     = 0,
    PLAYER_UNREALIZED = 100,
    PLAYER_REALIZED   = 200,
    PLAYER_PREFETCHED = 300,
    PLAYER_STARTED    = 400
};

// --- 2. RingBuffer Luồng Âm thanh Khóa An toàn (Audio Ring Buffer) ---
class J2ME_API AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacityFrames = 16384);
    ~AudioRingBuffer();

    size_t write(const int16_t* stereoPcm, size_t frames);
    size_t read(int16_t* stereoPcm, size_t frames);
    size_t availableRead() const;
    size_t availableWrite() const;
    void clear();

private:
    std::vector<int16_t> m_buffer; // Stereo: frames * 2 mẫu
    size_t m_capacityFrames;
    std::atomic<size_t> m_readPos{0};
    std::atomic<size_t> m_writePos{0};
    mutable std::mutex m_mutex;
};

// --- 3. Giao diện VolumeControl & MIDIControl ---
class J2ME_API VolumeControl {
public:
    virtual ~VolumeControl() = default;
    virtual void setLevel(int level) = 0; // 0..100
    virtual int getLevel() const = 0;
    virtual void setMute(bool mute) = 0;
    virtual bool isMuted() const = 0;
};

class J2ME_API MidiControl {
public:
    virtual ~MidiControl() = default;
    virtual void shortMidiEvent(int type, int data1, int data2) = 0;
    virtual int longMidiEvent(const uint8_t* data, size_t length) = 0;
    virtual void setProgram(int channel, int bank, int program) = 0;
    virtual void setChannelVolume(int channel, int volume) = 0;
};

// --- 4. Giao diện Player Cơ sở ---
class J2ME_API Player {
public:
    virtual ~Player() = default;

    virtual void realize() = 0;
    virtual void prefetch() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void deallocate() = 0;
    virtual void close() = 0;

    virtual MmapiPlayerState getState() const = 0;
    virtual void setLoopCount(int count) = 0;
    virtual int64_t setMediaTime(int64_t nowUsec) = 0;
    virtual int64_t getMediaTime() const = 0;
    virtual int64_t getDuration() const = 0;
    virtual std::string getContentType() const = 0;

    virtual VolumeControl* getVolumeControl() = 0;
    virtual MidiControl* getMidiControl() = 0;
};

// --- 5. Bộ tổng hợp âm thanh Sonivox EAS Engine ---
class J2ME_API SonivoxAudioEngine {
public:
    static SonivoxAudioEngine& instance();

    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Nạp phát file nhạc SMF / MIDI / OTA / iMelody từ bộ nhớ
    bool playMidiData(const uint8_t* data, size_t size, int loopCount = 1);
    void stopMidi();
    void pauseMidi();
    void resumeMidi();
    bool isMidiPlaying() const;

    // Live MIDI Stream (Cho MidiPlayer & MIDIControl)
    void sendShortMidi(int status, int data1, int data2);
    void sendLongMidi(const uint8_t* data, size_t length);

    // Âm lượng & Tone
    void setMasterVolume(int volumePercent); // 0..100
    int getMasterVolume() const { return m_volumePercent; }
    void playTone(int note, int durationMs, int volume);

    // Xuất mẫu PCM 16-bit Stereo (resample 22050 -> 44100Hz)
    size_t renderAudio44100(int16_t* outStereoPcm, size_t frameCount);

    int64_t getMediaTimeMs() const;
    int64_t getMediaDurationMs() const;

private:
    SonivoxAudioEngine();
    ~SonivoxAudioEngine();

    bool m_initialized{false};
    int m_volumePercent{80};
    int m_loopCount{1};
    mutable std::mutex m_mutex;

    void* m_easHandle{nullptr};        // EAS_DATA_HANDLE
    void* m_streamHandle{nullptr};     // EAS_HANDLE (file)
    void* m_midiStreamHandle{nullptr}; // EAS_HANDLE (interactive stream)
    std::vector<uint8_t> m_currentMidiData;

    // Máy phát Tone độc lập
    double m_tonePhase{0.0};
    double m_tonePhaseInc{0.0};
    int m_toneFramesLeft{0};
    float m_toneVolume{0.8f};

    // Bộ nhớ đệm mẫu thô 22050Hz từ EAS
    std::vector<int16_t> m_raw22050Buffer;
    int16_t m_lastSampleLeft{0};
    int16_t m_lastSampleRight{0};

    void closeStreamLocked();
    void openMidiStreamLocked();
};

// --- 6. Triển khai MidiAudioPlayer thực tế ---
class J2ME_API MidiAudioPlayer : public Player, public VolumeControl, public MidiControl {
public:
    MidiAudioPlayer(const std::vector<uint8_t>& data, const std::string& contentType);
    explicit MidiAudioPlayer(const std::string& locator);
    ~MidiAudioPlayer() override;

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
    std::string getContentType() const override { return m_contentType; }

    VolumeControl* getVolumeControl() override { return this; }
    MidiControl* getMidiControl() override { return this; }

    // VolumeControl Methods
    void setLevel(int level) override;
    int getLevel() const override { return m_volume; }
    void setMute(bool mute) override;
    bool isMuted() const override { return m_muted; }

    // MidiControl Methods
    void shortMidiEvent(int type, int data1, int data2) override;
    int longMidiEvent(const uint8_t* data, size_t length) override;
    void setProgram(int channel, int bank, int program) override;
    void setChannelVolume(int channel, int volume) override;

private:
    std::vector<uint8_t> m_data;
    std::string m_contentType;
    std::string m_locator;
    MmapiPlayerState m_state{PLAYER_UNREALIZED};
    int m_loopCount{1};
    int m_volume{80};
    bool m_muted{false};
};

// --- 7. Bộ quản lý MMAPI Manager ---
class J2ME_API MmapiManager {
public:
    static std::shared_ptr<Player> createPlayer(const std::string& locator);
    static std::shared_ptr<Player> createPlayer(const uint8_t* data, size_t length, const std::string& contentType);
    static void playTone(int note, int durationMs, int volume);
    static std::vector<std::string> getSupportedContentTypes();
};

} // namespace j2me

#endif // J2ME_MMAPI_AUDIO_H
