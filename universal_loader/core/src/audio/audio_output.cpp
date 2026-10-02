#include "audio_output.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#elif defined(__ANDROID__)
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#elif defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace j2me {

namespace {

constexpr size_t kBufferFrames = 1024;   // ~23 ms per device buffer
constexpr int64_t kIdleCloseMs = 3000;   // close the device after this much silence

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool envIs(const char* name, const char* value) {
    const char* v = std::getenv(name);
    return v && std::strcmp(v, value) == 0;
}

// Silent device: pulls at real-time rate and discards (J2ME_AUDIO=off, headless tests)
class NullBackend : public AudioOutput::Backend {
public:
    bool open() override {
        m_stop = false;
        m_thread = std::thread([this] {
            std::vector<int16_t> buf(kBufferFrames * 2);
            auto next = std::chrono::steady_clock::now();
            const auto period = std::chrono::microseconds(kBufferFrames * 1000000LL / AudioOutput::kSampleRate);
            while (!m_stop) {
                AudioOutput::instance().render(buf.data(), kBufferFrames);
                next += period;
                std::this_thread::sleep_until(next);
            }
        });
        return true;
    }
    void close() override {
        m_stop = true;
        if (m_thread.joinable()) m_thread.join();
    }
private:
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

#if defined(_WIN32)

class WaveOutBackend : public AudioOutput::Backend {
public:
    bool open() override {
        m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_event) return false;
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = AudioOutput::kSampleRate;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = AudioOutput::kSampleRate * 4;
        if (waveOutOpen(&m_wave, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(m_event), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
            CloseHandle(m_event);
            m_event = nullptr;
            return false;
        }
        for (int i = 0; i < kBuffers; i++) {
            m_data[i].assign(kBufferFrames * 2, 0);
            m_hdr[i] = WAVEHDR{};
            m_hdr[i].lpData = reinterpret_cast<LPSTR>(m_data[i].data());
            m_hdr[i].dwBufferLength = static_cast<DWORD>(kBufferFrames * 4);
            waveOutPrepareHeader(m_wave, &m_hdr[i], sizeof(WAVEHDR));
        }
        m_stop = false;
        m_thread = std::thread([this] { feed(); });
        return true;
    }

    void close() override {
        m_stop = true;
        SetEvent(m_event);
        if (m_thread.joinable()) m_thread.join();
        waveOutReset(m_wave);
        for (int i = 0; i < kBuffers; i++) waveOutUnprepareHeader(m_wave, &m_hdr[i], sizeof(WAVEHDR));
        waveOutClose(m_wave);
        CloseHandle(m_event);
        m_wave = nullptr;
        m_event = nullptr;
    }

private:
    static constexpr int kBuffers = 4;

    void feed() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        while (!m_stop) {
            for (int i = 0; i < kBuffers && !m_stop; i++) {
                if (m_hdr[i].dwFlags & WHDR_INQUEUE) continue;
                AudioOutput::instance().render(m_data[i].data(), kBufferFrames);
                waveOutWrite(m_wave, &m_hdr[i], sizeof(WAVEHDR));
            }
            WaitForSingleObject(m_event, 100);
        }
    }

    HWAVEOUT m_wave{nullptr};
    HANDLE m_event{nullptr};
    WAVEHDR m_hdr[kBuffers]{};
    std::vector<int16_t> m_data[kBuffers];
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

using PlatformBackend = WaveOutBackend;

#elif defined(__ANDROID__)

class OpenSlBackend : public AudioOutput::Backend {
public:
    bool open() override {
        if (slCreateEngine(&m_engineObj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
        if ((*m_engineObj)->Realize(m_engineObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS ||
            (*m_engineObj)->GetInterface(m_engineObj, SL_IID_ENGINE, &m_engine) != SL_RESULT_SUCCESS ||
            (*m_engine)->CreateOutputMix(m_engine, &m_mixObj, 0, nullptr, nullptr) != SL_RESULT_SUCCESS ||
            (*m_mixObj)->Realize(m_mixObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
            close();
            return false;
        }

        SLDataLocator_AndroidSimpleBufferQueue loc{SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, kBuffers};
        SLDataFormat_PCM fmt{SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                             SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                             SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
        SLDataSource src{&loc, &fmt};
        SLDataLocator_OutputMix outLoc{SL_DATALOCATOR_OUTPUTMIX, m_mixObj};
        SLDataSink sink{&outLoc, nullptr};
        const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        if ((*m_engine)->CreateAudioPlayer(m_engine, &m_playerObj, &src, &sink, 1, ids, req) != SL_RESULT_SUCCESS ||
            (*m_playerObj)->Realize(m_playerObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS ||
            (*m_playerObj)->GetInterface(m_playerObj, SL_IID_PLAY, &m_play) != SL_RESULT_SUCCESS ||
            (*m_playerObj)->GetInterface(m_playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_queue) != SL_RESULT_SUCCESS ||
            (*m_queue)->RegisterCallback(m_queue, onBufferDone, this) != SL_RESULT_SUCCESS) {
            close();
            return false;
        }
        m_next = 0;
        for (int i = 0; i < kBuffers; i++) enqueueNext();
        (*m_play)->SetPlayState(m_play, SL_PLAYSTATE_PLAYING);
        return true;
    }

    void close() override {
        if (m_play) (*m_play)->SetPlayState(m_play, SL_PLAYSTATE_STOPPED);
        if (m_playerObj) (*m_playerObj)->Destroy(m_playerObj);
        if (m_mixObj) (*m_mixObj)->Destroy(m_mixObj);
        if (m_engineObj) (*m_engineObj)->Destroy(m_engineObj);
        m_playerObj = m_mixObj = m_engineObj = nullptr;
        m_engine = nullptr;
        m_play = nullptr;
        m_queue = nullptr;
    }

private:
    static constexpr int kBuffers = 3;

    static void onBufferDone(SLAndroidSimpleBufferQueueItf, void* ctx) {
        static_cast<OpenSlBackend*>(ctx)->enqueueNext();
    }

    void enqueueNext() {
        int16_t* buf = m_data[m_next];
        m_next = (m_next + 1) % kBuffers;
        AudioOutput::instance().render(buf, kBufferFrames);
        (*m_queue)->Enqueue(m_queue, buf, kBufferFrames * 4);
    }

    SLObjectItf m_engineObj{nullptr};
    SLEngineItf m_engine{nullptr};
    SLObjectItf m_mixObj{nullptr};
    SLObjectItf m_playerObj{nullptr};
    SLPlayItf m_play{nullptr};
    SLAndroidSimpleBufferQueueItf m_queue{nullptr};
    int16_t m_data[kBuffers][kBufferFrames * 2]{};
    int m_next{0};
};

using PlatformBackend = OpenSlBackend;

#elif defined(__APPLE__)

class AudioQueueBackend : public AudioOutput::Backend {
public:
    bool open() override {
        AudioStreamBasicDescription fmt{};
        fmt.mSampleRate = AudioOutput::kSampleRate;
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
        fmt.mBytesPerPacket = 4;
        fmt.mFramesPerPacket = 1;
        fmt.mBytesPerFrame = 4;
        fmt.mChannelsPerFrame = 2;
        fmt.mBitsPerChannel = 16;
        // NULL run loop: callbacks arrive on the queue's own internal thread
        if (AudioQueueNewOutput(&fmt, onBufferDone, this, nullptr, nullptr, 0, &m_queue) != noErr) {
            m_queue = nullptr;
            return false;
        }
        for (int i = 0; i < kBuffers; i++) {
            AudioQueueBufferRef buf = nullptr;
            if (AudioQueueAllocateBuffer(m_queue, kBufferFrames * 4, &buf) != noErr) {
                close();
                return false;
            }
            onBufferDone(this, m_queue, buf);
        }
        if (AudioQueueStart(m_queue, nullptr) != noErr) {
            close();
            return false;
        }
        return true;
    }

    void close() override {
        if (!m_queue) return;
        AudioQueueStop(m_queue, true);
        AudioQueueDispose(m_queue, true);
        m_queue = nullptr;
    }

private:
    static constexpr int kBuffers = 3;

    static void onBufferDone(void*, AudioQueueRef queue, AudioQueueBufferRef buf) {
        AudioOutput::instance().render(static_cast<int16_t*>(buf->mAudioData), kBufferFrames);
        buf->mAudioDataByteSize = kBufferFrames * 4;
        AudioQueueEnqueueBuffer(queue, buf, 0, nullptr);
    }

    AudioQueueRef m_queue{nullptr};
};

using PlatformBackend = AudioQueueBackend;

#else

using PlatformBackend = NullBackend;

#endif

} // namespace

AudioOutput& AudioOutput::instance() {
    // Leaked on purpose: the audio thread may still be running while the library unloads
    static AudioOutput* s_instance = new AudioOutput();
    return *s_instance;
}

void AudioOutput::attach(const std::shared_ptr<AudioSource>& src) {
    if (!src) return;
    {
        std::lock_guard<std::mutex> lock(m_srcMutex);
        bool present = false;
        m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [&](const std::weak_ptr<AudioSource>& w) {
            auto s = w.lock();
            if (s == src) present = true;
            return !s;
        }), m_sources.end());
        if (!present) m_sources.push_back(src);
    }
    ensureStarted();
}

void AudioOutput::ensureStarted() {
    m_lastActiveMs = nowMs();
    std::lock_guard<std::mutex> lock(m_devMutex);
    if (!m_backend) {
        std::unique_ptr<Backend> backend;
        if (envIs("J2ME_AUDIO", "off")) {
            backend = std::make_unique<NullBackend>();
        } else {
            backend = std::make_unique<PlatformBackend>();
        }
        if (!backend->open()) {
            // No usable device (e.g. none plugged in): keep timing-dependent games going with the null device
            if (!m_openFailed) std::fprintf(stderr, "[audio] output device unavailable, audio is muted\n");
            m_openFailed = true;
            backend = std::make_unique<NullBackend>();
            backend->open();
        }
        m_backend = std::move(backend);
    }
    if (!m_monitorRunning) {
        m_monitorRunning = true;
        std::thread([this] { monitorLoop(); }).detach();
    }
}

void AudioOutput::monitorLoop() {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (nowMs() - m_lastActiveMs.load() < kIdleCloseMs) continue;
        std::unique_ptr<Backend> idle;
        {
            std::lock_guard<std::mutex> lock(m_devMutex);
            if (nowMs() - m_lastActiveMs.load() < kIdleCloseMs) continue;
            idle = std::move(m_backend);
            m_monitorRunning = false;
            // Closed while holding the lock so a concurrent attach() waits and then reopens a fresh device
            if (idle) idle->close();
        }
        return;
    }
}

void AudioOutput::render(int16_t* out, size_t frameCount) {
    const size_t samples = frameCount * 2;
    if (m_acc.size() < samples) m_acc.resize(samples);
    std::fill(m_acc.begin(), m_acc.begin() + samples, 0);

    m_live.clear();
    {
        std::lock_guard<std::mutex> lock(m_srcMutex);
        for (auto& w : m_sources) {
            if (auto s = w.lock()) m_live.push_back(std::move(s));
        }
    }
    bool active = false;
    for (auto& s : m_live) {
        if (s->mixInto(m_acc.data(), frameCount)) active = true;
    }
    // Last references to closed players may drop here; that only frees memory
    m_live.clear();
    if (active) m_lastActiveMs = nowMs();

    int peak = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t v = std::clamp<int32_t>(m_acc[i], -32768, 32767);
        out[i] = static_cast<int16_t>(v);
        peak = std::max(peak, v < 0 ? -v : v);
    }

    static const bool logPeaks = envIs("J2ME_AUDIOLOG", "1");
    if (logPeaks) {
        m_logPeak = std::max(m_logPeak, peak);
        m_logFrames += frameCount;
        if (m_logFrames >= static_cast<size_t>(kSampleRate)) {
            std::fprintf(stderr, "[audio] peak=%d\n", m_logPeak);
            std::fflush(stderr);
            m_logPeak = 0;
            m_logFrames = 0;
        }
    }
}

} // namespace j2me
