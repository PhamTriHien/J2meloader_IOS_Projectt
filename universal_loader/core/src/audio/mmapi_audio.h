#ifndef J2ME_MMAPI_AUDIO_H
#define J2ME_MMAPI_AUDIO_H

#include "../../include/j2me_core.h"
#include "audio_output.h"
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
    virtual int64_t getDuration() const = 0; // -1 (TIME_UNKNOWN) when not known yet
    virtual std::string getContentType() const = 0;

    virtual VolumeControl* getVolumeControl() = 0;
    virtual MidiControl* getMidiControl() = 0;

    // Incremented on the audio thread each time playback reaches the end of the media (END_OF_MEDIA)
    virtual uint32_t getEndOfMediaCount() const { return 0; }
};

// --- 5. Bộ tổng hợp âm thanh Sonivox EAS Engine ---
// Shared sources that are not tied to a Player: Manager.playTone and live MIDI events.
// playMidiData/stopMidi keep the old single-track C API working on top of MidiAudioPlayer.
class J2ME_API SonivoxAudioEngine : public AudioSource {
public:
    static SonivoxAudioEngine& instance();

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

    bool mixInto(int32_t* acc, size_t frameCount) override;

private:
    SonivoxAudioEngine();
    void openLiveStreamLocked();
    std::shared_ptr<AudioSource> selfRef();

    mutable std::mutex m_mutex;
    std::atomic<int> m_volumePercent{100};

    // Máy phát Tone độc lập
    double m_tonePhase{0.0};
    double m_tonePhaseInc{0.0};
    int m_toneFramesLeft{0};
    float m_toneVolume{0.8f};

    // Live MIDI: own EAS instance, rendered while events keep arriving
    void* m_liveEas{nullptr};
    void* m_liveStream{nullptr};
    int64_t m_liveUntilMs{0};
    std::vector<int16_t> m_liveOut;
    size_t m_liveOutPos{0};
    int16_t m_livePrevL{0};
    int16_t m_livePrevR{0};

    std::shared_ptr<Player> m_legacyPlayer;
};

// --- 6. Triển khai MidiAudioPlayer thực tế ---
// SMF / iMelody / RTTTL / OTA player backed by its own Sonivox instance, so any number of players can
// play, pause and loop independently. Must be owned by a shared_ptr (MmapiManager::createPlayer).
class J2ME_API MidiAudioPlayer : public Player, public VolumeControl, public MidiControl, public AudioSource,
                                 public std::enable_shared_from_this<MidiAudioPlayer> {
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

    MmapiPlayerState getState() const override;
    void setLoopCount(int count) override;
    int64_t setMediaTime(int64_t nowUsec) override;
    int64_t getMediaTime() const override;
    int64_t getDuration() const override;
    std::string getContentType() const override { return m_contentType; }
    uint32_t getEndOfMediaCount() const override { return m_endOfMedia.load(); }

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

    bool mixInto(int32_t* acc, size_t frameCount) override;

    struct MemFile {
        const uint8_t* data{nullptr};
        int size{0};
    };

private:
    bool openLocked();
    void closeStreamLocked();
    void releaseLocked();
    bool renderChunkLocked();

    std::vector<uint8_t> m_data;
    MemFile m_file;
    std::string m_contentType;
    std::string m_locator;

    mutable std::mutex m_mutex;
    MmapiPlayerState m_state{PLAYER_UNREALIZED};
    int m_loopCount{1};
    int m_loopsLeft{1};
    std::atomic<int> m_volume{100};
    std::atomic<bool> m_muted{false};
    std::atomic<uint32_t> m_endOfMedia{0};

    void* m_eas{nullptr};      // EAS_DATA_HANDLE
    void* m_stream{nullptr};   // EAS_HANDLE
    bool m_streamDone{false};  // parser reached the end; the output FIFO still holds the tail
    bool m_atEnd{false};       // finished; the next start() rewinds
    int64_t m_durationMs{-1};

    std::vector<int16_t> m_out; // 44.1 kHz stereo produced from the last 128-frame EAS chunk
    size_t m_outPos{0};
    int16_t m_prevL{0};
    int16_t m_prevR{0};
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
