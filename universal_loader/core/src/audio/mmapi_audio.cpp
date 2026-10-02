#include "mmapi_audio.h"
#include "wav_player.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cstdio>

// Include các định nghĩa C của Sonivox EAS
extern "C" {
#include "sonivox/eas.h"
#include "sonivox/eas_types.h"
}

namespace j2me {

// ============================================================================
// 1. Audio RingBuffer Implementation
// ============================================================================

AudioRingBuffer::AudioRingBuffer(size_t capacityFrames)
    : m_capacityFrames(capacityFrames) {
    m_buffer.resize(capacityFrames * 2, 0); // 2 kênh stereo
}

AudioRingBuffer::~AudioRingBuffer() = default;

size_t AudioRingBuffer::write(const int16_t* stereoPcm, size_t frames) {
    if (!stereoPcm || frames == 0) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t r = m_readPos.load(std::memory_order_relaxed);
    size_t w = m_writePos.load(std::memory_order_relaxed);

    size_t freeFrames = (r > w) ? (r - w - 1) : (m_capacityFrames - (w - r) - 1);
    size_t toWrite = std::min(frames, freeFrames);

    if (toWrite == 0) return 0;

    size_t firstChunk = std::min(toWrite, m_capacityFrames - w);
    std::memcpy(&m_buffer[w * 2], stereoPcm, firstChunk * 2 * sizeof(int16_t));

    size_t secondChunk = toWrite - firstChunk;
    if (secondChunk > 0) {
        std::memcpy(&m_buffer[0], stereoPcm + (firstChunk * 2), secondChunk * 2 * sizeof(int16_t));
    }

    m_writePos.store((w + toWrite) % m_capacityFrames, std::memory_order_release);
    return toWrite;
}

size_t AudioRingBuffer::read(int16_t* stereoPcm, size_t frames) {
    if (!stereoPcm || frames == 0) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t r = m_readPos.load(std::memory_order_relaxed);
    size_t w = m_writePos.load(std::memory_order_acquire);

    size_t avail = (w >= r) ? (w - r) : (m_capacityFrames - (r - w));
    size_t toRead = std::min(frames, avail);

    if (toRead == 0) {
        std::memset(stereoPcm, 0, frames * 2 * sizeof(int16_t));
        return 0;
    }

    size_t firstChunk = std::min(toRead, m_capacityFrames - r);
    std::memcpy(stereoPcm, &m_buffer[r * 2], firstChunk * 2 * sizeof(int16_t));

    size_t secondChunk = toRead - firstChunk;
    if (secondChunk > 0) {
        std::memcpy(stereoPcm + (firstChunk * 2), &m_buffer[0], secondChunk * 2 * sizeof(int16_t));
    }

    if (toRead < frames) {
        std::memset(stereoPcm + (toRead * 2), 0, (frames - toRead) * 2 * sizeof(int16_t));
    }

    m_readPos.store((r + toRead) % m_capacityFrames, std::memory_order_release);
    return toRead;
}

size_t AudioRingBuffer::availableRead() const {
    size_t r = m_readPos.load(std::memory_order_relaxed);
    size_t w = m_writePos.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (m_capacityFrames - (r - w));
}

size_t AudioRingBuffer::availableWrite() const {
    size_t r = m_readPos.load(std::memory_order_acquire);
    size_t w = m_writePos.load(std::memory_order_relaxed);
    return (r > w) ? (r - w - 1) : (m_capacityFrames - (w - r) - 1);
}

void AudioRingBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_readPos.store(0, std::memory_order_relaxed);
    m_writePos.store(0, std::memory_order_relaxed);
    std::fill(m_buffer.begin(), m_buffer.end(), 0);
}

// ============================================================================
// 2. Sonivox EAS helpers
// ============================================================================

namespace {

EAS_DATA_HANDLE easData(void* h) { return reinterpret_cast<EAS_DATA_HANDLE>(h); }
EAS_HANDLE easStream(void* h) { return reinterpret_cast<EAS_HANDLE>(h); }

int easMemReadAt(void* handle, void* buf, int offset, int size) {
    auto* m = static_cast<MidiAudioPlayer::MemFile*>(handle);
    if (!m || !m->data || offset < 0 || size <= 0 || offset >= m->size) return 0;
    int toCopy = std::min(size, m->size - offset);
    std::memcpy(buf, m->data + offset, toCopy);
    return toCopy;
}

int easMemSize(void* handle) {
    auto* m = static_cast<MidiAudioPlayer::MemFile*>(handle);
    return m ? m->size : 0;
}

int64_t steadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// EAS_Render only accepts exactly one mix buffer (128 frames at 22.05 kHz) per call.
// Renders one chunk and upsamples it 2x into out (interleaved stereo 44.1 kHz).
bool renderEasChunk(void* eas, std::vector<int16_t>& out, int16_t& prevL, int16_t& prevR) {
    static const EAS_I32 chunk = EAS_Config()->mixBufferSize;
    EAS_PCM buf[512 * 2];
    if (chunk <= 0 || chunk > 512) return false;
    EAS_I32 n = 0;
    if (EAS_Render(easData(eas), buf, chunk, &n) != EAS_SUCCESS || n <= 0) return false;
    out.resize(static_cast<size_t>(n) * 4);
    for (EAS_I32 k = 0; k < n; k++) {
        int16_t l = buf[k * 2];
        int16_t r = buf[k * 2 + 1];
        out[k * 4]     = static_cast<int16_t>((prevL + l) / 2);
        out[k * 4 + 1] = static_cast<int16_t>((prevR + r) / 2);
        out[k * 4 + 2] = l;
        out[k * 4 + 3] = r;
        prevL = l;
        prevR = r;
    }
    return true;
}

// Adds up to frames frames from out[pos..] into acc with gain 0..100; returns frames consumed
size_t drainInto(int32_t* acc, size_t frames, const std::vector<int16_t>& out, size_t& pos, int gain) {
    size_t n = std::min(frames, (out.size() - pos) / 2);
    const int16_t* src = out.data() + pos;
    for (size_t i = 0; i < n * 2; i++) acc[i] += src[i] * gain / 100;
    pos += n * 2;
    return n;
}

} // namespace

// ============================================================================
// 3. Sonivox Audio Engine (tone generator, live MIDI, legacy single-track API)
// ============================================================================

SonivoxAudioEngine& SonivoxAudioEngine::instance() {
    // Leaked: the audio thread can outlive static destruction
    static SonivoxAudioEngine* s_instance = new SonivoxAudioEngine();
    return *s_instance;
}

SonivoxAudioEngine::SonivoxAudioEngine() = default;

std::shared_ptr<AudioSource> SonivoxAudioEngine::selfRef() {
    static std::shared_ptr<AudioSource> s_ref(this, [](AudioSource*) {});
    return s_ref;
}

bool SonivoxAudioEngine::playMidiData(const uint8_t* data, size_t size, int loopCount) {
    if (!data || size == 0) return false;
    auto player = MmapiManager::createPlayer(data, size, "audio/midi");
    player->setLoopCount(loopCount);
    player->getVolumeControl()->setLevel(m_volumePercent);
    player->prefetch();
    if (player->getDuration() < 0) return false;
    std::shared_ptr<Player> old;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old = std::move(m_legacyPlayer);
        m_legacyPlayer = player;
    }
    if (old) old->close();
    player->start();
    return true;
}

void SonivoxAudioEngine::stopMidi() {
    std::shared_ptr<Player> old;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old = std::move(m_legacyPlayer);
        m_toneFramesLeft = 0;
    }
    if (old) old->close();
}

void SonivoxAudioEngine::pauseMidi() {
    std::shared_ptr<Player> p;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        p = m_legacyPlayer;
    }
    if (p) p->stop();
}

void SonivoxAudioEngine::resumeMidi() {
    std::shared_ptr<Player> p;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        p = m_legacyPlayer;
    }
    if (p) p->start();
}

bool SonivoxAudioEngine::isMidiPlaying() const {
    std::shared_ptr<Player> p;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        p = m_legacyPlayer;
    }
    return p && p->getState() == PLAYER_STARTED;
}

void SonivoxAudioEngine::openLiveStreamLocked() {
    if (!m_liveEas) {
        EAS_DATA_HANDLE h = nullptr;
        if (EAS_Init(&h) != EAS_SUCCESS || !h) return;
        m_liveEas = h;
    }
    if (!m_liveStream) {
        EAS_HANDLE stream = nullptr;
        if (EAS_OpenMIDIStream(easData(m_liveEas), &stream, nullptr) == EAS_SUCCESS) {
            m_liveStream = stream;
        }
    }
}

void SonivoxAudioEngine::sendShortMidi(int status, int data1, int data2) {
    uint8_t ev[3] = {
        static_cast<uint8_t>(status),
        static_cast<uint8_t>(data1),
        static_cast<uint8_t>(data2)
    };
    int cmd = status & 0xF0;
    sendLongMidi(ev, (cmd == 0xC0 || cmd == 0xD0) ? 2 : 3); // Program change & Channel pressure
}

void SonivoxAudioEngine::sendLongMidi(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        openLiveStreamLocked();
        if (!m_liveStream) return;
        EAS_WriteMIDIStream(easData(m_liveEas), easStream(m_liveStream),
                            const_cast<uint8_t*>(data), static_cast<EAS_I32>(length));
        // Keep rendering for a while so note releases are heard
        m_liveUntilMs = steadyMs() + 5000;
    }
    AudioOutput::instance().attach(selfRef());
}

void SonivoxAudioEngine::setMasterVolume(int volumePercent) {
    m_volumePercent = std::clamp(volumePercent, 0, 100);
    std::shared_ptr<Player> p;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        p = m_legacyPlayer;
    }
    if (p) p->getVolumeControl()->setLevel(m_volumePercent);
}

void SonivoxAudioEngine::playTone(int note, int durationMs, int volume) {
    if (note < 0 || note > 127 || durationMs <= 0) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Tần số chuẩn MIDI note: f = 440 * 2^((note - 69) / 12)
        double freq = 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
        m_tonePhaseInc = (2.0 * 3.14159265358979323846 * freq) / AudioOutput::kSampleRate;
        m_toneFramesLeft = static_cast<int>((static_cast<int64_t>(AudioOutput::kSampleRate) * durationMs) / 1000);
        m_toneVolume = std::clamp(volume, 0, 100) / 100.0f;
    }
    AudioOutput::instance().attach(selfRef());
}

bool SonivoxAudioEngine::mixInto(int32_t* acc, size_t frameCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const int vol = m_volumePercent;
    bool active = false;

    if (m_toneFramesLeft > 0) {
        active = true;
        constexpr int kFade = 220; // ~5 ms ramp to avoid clicks
        size_t n = std::min<size_t>(frameCount, static_cast<size_t>(m_toneFramesLeft));
        const double amp = 8000.0 * m_toneVolume * vol / 100.0;
        for (size_t i = 0; i < n; i++) {
            double env = std::min(1.0, m_toneFramesLeft / static_cast<double>(kFade));
            int32_t s = static_cast<int32_t>(std::sin(m_tonePhase) * amp * env);
            m_tonePhase += m_tonePhaseInc;
            if (m_tonePhase >= 2.0 * 3.14159265358979323846) m_tonePhase -= 2.0 * 3.14159265358979323846;
            acc[i * 2] += s;
            acc[i * 2 + 1] += s;
            m_toneFramesLeft--;
        }
    }

    if (m_liveStream && steadyMs() < m_liveUntilMs) {
        active = true;
        size_t done = 0;
        while (done < frameCount) {
            if (m_liveOutPos >= m_liveOut.size()) {
                if (!renderEasChunk(m_liveEas, m_liveOut, m_livePrevL, m_livePrevR)) break;
                m_liveOutPos = 0;
            }
            done += drainInto(acc + done * 2, frameCount - done, m_liveOut, m_liveOutPos, vol);
        }
    }
    return active;
}

// ============================================================================
// 4. MidiAudioPlayer Implementation
// ============================================================================

MidiAudioPlayer::MidiAudioPlayer(const std::vector<uint8_t>& data, const std::string& contentType)
    : m_data(data), m_contentType(contentType) {
    m_file.data = m_data.data();
    m_file.size = static_cast<int>(m_data.size());
}

MidiAudioPlayer::MidiAudioPlayer(const std::string& locator)
    : m_contentType("audio/midi"), m_locator(locator) {}

MidiAudioPlayer::~MidiAudioPlayer() {
    releaseLocked();
}

bool MidiAudioPlayer::openLocked() {
    if (m_stream) return true;
    if (m_data.empty()) return false;
    if (!m_eas) {
        EAS_DATA_HANDLE h = nullptr;
        EAS_RESULT res = EAS_Init(&h);
        if (res != EAS_SUCCESS || !h) {
            std::fprintf(stderr, "[audio] synthesizer init failed: EAS error %ld\n", static_cast<long>(res));
            return false;
        }
        m_eas = h;
    }
    EAS_FILE locator;
    locator.handle = &m_file;
    locator.readAt = easMemReadAt;
    locator.size = easMemSize;

    EAS_HANDLE stream = nullptr;
    EAS_RESULT res = EAS_OpenFile(easData(m_eas), &locator, &stream);
    if (res != EAS_SUCCESS || !stream) {
        std::fprintf(stderr, "[audio] cannot parse %s (%zu bytes): EAS error %ld\n",
                     m_contentType.empty() ? "media" : m_contentType.c_str(), m_data.size(), static_cast<long>(res));
        return false;
    }
    res = EAS_Prepare(easData(m_eas), stream);
    if (res != EAS_SUCCESS) {
        std::fprintf(stderr, "[audio] cannot prepare %s: EAS error %ld\n",
                     m_contentType.empty() ? "media" : m_contentType.c_str(), static_cast<long>(res));
        EAS_CloseFile(easData(m_eas), stream);
        return false;
    }
    if (m_durationMs < 0) {
        // Only valid once prepared; parses to the end and rewinds
        EAS_I32 length = 0;
        m_durationMs = EAS_ParseMetaData(easData(m_eas), stream, &length) == EAS_SUCCESS ? length : 0;
    }
    m_stream = stream;
    m_streamDone = false;
    m_atEnd = false;
    m_out.clear();
    m_outPos = 0;
    m_prevL = m_prevR = 0;
    return true;
}

void MidiAudioPlayer::closeStreamLocked() {
    if (m_stream && m_eas) EAS_CloseFile(easData(m_eas), easStream(m_stream));
    m_stream = nullptr;
    m_out.clear();
    m_outPos = 0;
}

void MidiAudioPlayer::releaseLocked() {
    closeStreamLocked();
    if (m_eas) EAS_Shutdown(easData(m_eas));
    m_eas = nullptr;
}

bool MidiAudioPlayer::renderChunkLocked() {
    if (!renderEasChunk(m_eas, m_out, m_prevL, m_prevR)) return false;
    m_outPos = 0;
    EAS_STATE state = EAS_STATE_ERROR;
    if (EAS_State(easData(m_eas), easStream(m_stream), &state) != EAS_SUCCESS ||
        state == EAS_STATE_STOPPED || state == EAS_STATE_ERROR || state == EAS_STATE_EMPTY) {
        m_streamDone = true;
    } else if (state == EAS_STATE_STOPPING && (m_loopsLeft < 0 || m_loopsLeft > 1)) {
        // Parser hit the end and only release tails remain: loop now instead of after the tail
        m_streamDone = true;
    }
    return true;
}

MmapiPlayerState MidiAudioPlayer::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void MidiAudioPlayer::realize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_UNREALIZED) m_state = PLAYER_REALIZED;
}

void MidiAudioPlayer::prefetch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_CLOSED || m_state >= PLAYER_PREFETCHED) return;
    // Unsupported or corrupt data stays a silent player instead of failing the game
    openLocked();
    m_state = PLAYER_PREFETCHED;
}

void MidiAudioPlayer::start() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == PLAYER_CLOSED || m_state == PLAYER_STARTED) return;
        if (m_atEnd) {
            closeStreamLocked();
            m_loopsLeft = m_loopCount;
        }
        openLocked();
        m_state = PLAYER_STARTED;
        if (!m_stream) return;
    }
    if (auto self = weak_from_this().lock()) AudioOutput::instance().attach(self);
}

void MidiAudioPlayer::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_STARTED) m_state = PLAYER_PREFETCHED;
}

void MidiAudioPlayer::deallocate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_CLOSED) return;
    releaseLocked();
    m_atEnd = false;
    if (m_state > PLAYER_REALIZED) m_state = PLAYER_REALIZED;
}

void MidiAudioPlayer::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    releaseLocked();
    m_state = PLAYER_CLOSED;
}

void MidiAudioPlayer::setLoopCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (count == 0) return;
    m_loopCount = count;
    m_loopsLeft = count;
}

int64_t MidiAudioPlayer::setMediaTime(int64_t nowUsec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_CLOSED) return 0;
    if (m_atEnd || m_streamDone) closeStreamLocked();
    if (!openLocked()) return 0;
    EAS_I32 ms = static_cast<EAS_I32>(std::clamp<int64_t>(nowUsec / 1000, 0, std::max<int64_t>(m_durationMs, 0)));
    if (EAS_Locate(easData(m_eas), easStream(m_stream), ms, EAS_FALSE) != EAS_SUCCESS) ms = 0;
    m_out.clear();
    m_outPos = 0;
    return static_cast<int64_t>(ms) * 1000;
}

int64_t MidiAudioPlayer::getMediaTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_stream || m_atEnd) return 0;
    EAS_I32 ms = 0;
    if (EAS_GetLocation(easData(m_eas), easStream(m_stream), &ms) != EAS_SUCCESS) return 0;
    return static_cast<int64_t>(ms) * 1000;
}

int64_t MidiAudioPlayer::getDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_durationMs < 0 ? -1 : m_durationMs * 1000;
}

bool MidiAudioPlayer::mixInto(int32_t* acc, size_t frameCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != PLAYER_STARTED || !m_stream) return false;
    const int gain = m_muted ? 0 : m_volume.load();
    size_t done = 0;
    while (done < frameCount) {
        if (m_outPos >= m_out.size()) {
            if (m_streamDone || !renderChunkLocked()) {
                if (m_loopsLeft < 0 || --m_loopsLeft > 0) {
                    closeStreamLocked();
                    if (openLocked()) continue;
                }
                m_state = PLAYER_PREFETCHED;
                m_atEnd = true;
                m_endOfMedia++;
                break;
            }
            continue;
        }
        done += drainInto(acc + done * 2, frameCount - done, m_out, m_outPos, gain);
    }
    return true;
}

void MidiAudioPlayer::setLevel(int level) {
    m_volume = std::clamp(level, 0, 100);
}

void MidiAudioPlayer::setMute(bool mute) {
    m_muted = mute;
}

void MidiAudioPlayer::shortMidiEvent(int type, int data1, int data2) {
    SonivoxAudioEngine::instance().sendShortMidi(type, data1, data2);
}

int MidiAudioPlayer::longMidiEvent(const uint8_t* data, size_t length) {
    SonivoxAudioEngine::instance().sendLongMidi(data, length);
    return static_cast<int>(length);
}

void MidiAudioPlayer::setProgram(int channel, int bank, int program) {
    if (bank >= 0) {
        SonivoxAudioEngine::instance().sendShortMidi(0xB0 | (channel & 0x0F), 0, (bank >> 7) & 0x7F);
        SonivoxAudioEngine::instance().sendShortMidi(0xB0 | (channel & 0x0F), 32, bank & 0x7F);
    }
    SonivoxAudioEngine::instance().sendShortMidi(0xC0 | (channel & 0x0F), program & 0x7F, 0);
}

void MidiAudioPlayer::setChannelVolume(int channel, int volume) {
    SonivoxAudioEngine::instance().sendShortMidi(0xB0 | (channel & 0x0F), 7, volume & 0x7F);
}

// ============================================================================
// 5. MMAPI Manager Implementation
// ============================================================================

std::shared_ptr<Player> MmapiManager::createPlayer(const std::string& locator) {
    std::string lowerLoc = locator;
    std::transform(lowerLoc.begin(), lowerLoc.end(), lowerLoc.begin(), ::tolower);
    if (lowerLoc.ends_with(".wav") || lowerLoc.ends_with(".wave")) {
        auto wav = WavPlayer::createFromFile(locator);
        if (wav) return wav;
    }
    // device://tone, device://midi and unsupported locators: a silent player
    return std::make_shared<MidiAudioPlayer>(locator);
}

std::shared_ptr<Player> MmapiManager::createPlayer(const uint8_t* data, size_t length, const std::string& contentType) {
    if (data && length >= 12) {
        if (contentType == "audio/wav" || contentType == "audio/x-wav" ||
            (std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0)) {
            auto wav = WavPlayer::createFromMemory(data, length);
            if (wav) return wav;
        }
    }
    std::vector<uint8_t> buffer;
    if (data && length > 0) {
        buffer.assign(data, data + length);
    }
    return std::make_shared<MidiAudioPlayer>(buffer, contentType);
}

void MmapiManager::playTone(int note, int durationMs, int volume) {
    SonivoxAudioEngine::instance().playTone(note, durationMs, volume);
}

std::vector<std::string> MmapiManager::getSupportedContentTypes() {
    return {
        "audio/midi",
        "audio/x-midi",
        "audio/mid",
        "audio/sp-midi",
        "audio/x-tone-seq",
        "audio/wav",
        "audio/x-wav"
    };
}

} // namespace j2me
