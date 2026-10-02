#ifndef J2ME_AUDIO_OUTPUT_H
#define J2ME_AUDIO_OUTPUT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace j2me {

// Anything that can be heard: MMAPI players, the tone generator, live MIDI.
class AudioSource {
public:
    virtual ~AudioSource() = default;
    // Adds frameCount interleaved stereo 44.1 kHz frames into acc. Returns true while it produced sound.
    // Called on the audio thread; must not call back into AudioOutput.
    virtual bool mixInto(int32_t* acc, size_t frameCount) = 0;
};

// Process-wide mixer feeding the platform device (waveOut / OpenSL ES / AudioQueue).
// The device is opened lazily when a source starts and closed again after a few seconds of silence.
// Set J2ME_AUDIO=off for a silent (null) device, J2ME_AUDIOLOG=1 to log output peak levels.
class AudioOutput {
public:
    static constexpr int kSampleRate = 44100;

    static AudioOutput& instance();

    // Registers src (weakly) and makes sure the device is running. Never call with a source lock held.
    void attach(const std::shared_ptr<AudioSource>& src);

    // Pulled by the backend: mixes every source into out (interleaved stereo s16).
    void render(int16_t* out, size_t frameCount);

    class Backend {
    public:
        virtual ~Backend() = default;
        virtual bool open() = 0;
        virtual void close() = 0;
    };

private:
    AudioOutput() = default;
    void ensureStarted();
    void monitorLoop();

    std::mutex m_srcMutex;
    std::vector<std::weak_ptr<AudioSource>> m_sources;

    std::mutex m_devMutex;
    std::unique_ptr<Backend> m_backend;
    bool m_monitorRunning{false};
    bool m_openFailed{false};
    std::atomic<int64_t> m_lastActiveMs{0};

    std::vector<int32_t> m_acc;
    std::vector<std::shared_ptr<AudioSource>> m_live;
    int m_logPeak{0};
    size_t m_logFrames{0};
};

} // namespace j2me

#endif // J2ME_AUDIO_OUTPUT_H
