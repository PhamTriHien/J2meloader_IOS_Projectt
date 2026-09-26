#include "mmapi_audio.h"
#include "wav_player.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>

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
// 2. In-Memory File Reader Adapter for Sonivox EAS
// ============================================================================

struct MemFileStream {
    const uint8_t* data{nullptr};
    int size{0};
};

static int easMemReadAt(void* handle, void* buf, int offset, int size) {
    auto* m = reinterpret_cast<MemFileStream*>(handle);
    if (!m || !m->data || offset < 0 || size <= 0) return 0;
    if (offset >= m->size) return 0;
    int avail = m->size - offset;
    int toCopy = (size < avail) ? size : avail;
    std::memcpy(buf, m->data + offset, toCopy);
    return toCopy;
}

static int easMemSize(void* handle) {
    auto* m = reinterpret_cast<MemFileStream*>(handle);
    return m ? m->size : 0;
}

// ============================================================================
// 3. Sonivox Audio Engine Implementation
// ============================================================================

SonivoxAudioEngine& SonivoxAudioEngine::instance() {
    static SonivoxAudioEngine s_instance;
    return s_instance;
}

SonivoxAudioEngine::SonivoxAudioEngine() {
    m_raw22050Buffer.resize(4096 * 2, 0); // Đệm tạm stereo 22050Hz
    initialize();
}

SonivoxAudioEngine::~SonivoxAudioEngine() {
    shutdown();
}

bool SonivoxAudioEngine::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    EAS_DATA_HANDLE easData = nullptr;
    EAS_RESULT r = EAS_Init(&easData);
    if (r != EAS_SUCCESS || !easData) {
        return false;
    }

    m_easHandle = easData;
    m_initialized = true;
    return true;
}

void SonivoxAudioEngine::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    closeStreamLocked();

    if (m_midiStreamHandle && m_easHandle) {
        EAS_CloseMIDIStream(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                            reinterpret_cast<EAS_HANDLE>(m_midiStreamHandle));
        m_midiStreamHandle = nullptr;
    }

    if (m_easHandle) {
        EAS_Shutdown(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle));
        m_easHandle = nullptr;
    }

    m_initialized = false;
}

void SonivoxAudioEngine::closeStreamLocked() {
    if (m_streamHandle && m_easHandle) {
        EAS_CloseFile(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                      reinterpret_cast<EAS_HANDLE>(m_streamHandle));
        m_streamHandle = nullptr;
    }
    m_currentMidiData.clear();
}

void SonivoxAudioEngine::openMidiStreamLocked() {
    if (!m_initialized || !m_easHandle) return;
    if (m_midiStreamHandle) return;

    EAS_HANDLE stream = nullptr;
    if (EAS_OpenMIDIStream(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                           &stream, nullptr) == EAS_SUCCESS) {
        m_midiStreamHandle = stream;
    }
}

bool SonivoxAudioEngine::playMidiData(const uint8_t* data, size_t size, int loopCount) {
    if (!data || size == 0) return false;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        if (!initialize()) return false;
    }

    closeStreamLocked();

    m_currentMidiData.assign(data, data + size);
    m_loopCount = loopCount;

    static MemFileStream memStream;
    memStream.data = m_currentMidiData.data();
    memStream.size = static_cast<int>(m_currentMidiData.size());

    EAS_FILE locator;
    locator.handle = &memStream;
    locator.readAt = easMemReadAt;
    locator.size = easMemSize;

    EAS_HANDLE stream = nullptr;
    EAS_RESULT r = EAS_OpenFile(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle), &locator, &stream);
    if (r != EAS_SUCCESS || !stream) {
        m_currentMidiData.clear();
        return false;
    }

    r = EAS_Prepare(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle), stream);
    if (r != EAS_SUCCESS) {
        EAS_CloseFile(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle), stream);
        m_currentMidiData.clear();
        return false;
    }

    EAS_SetVolume(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle), stream, m_volumePercent);
    m_streamHandle = stream;
    return true;
}

void SonivoxAudioEngine::stopMidi() {
    std::lock_guard<std::mutex> lock(m_mutex);
    closeStreamLocked();
    m_toneFramesLeft = 0;
}

void SonivoxAudioEngine::pauseMidi() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_easHandle && m_streamHandle) {
        EAS_Pause(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                  reinterpret_cast<EAS_HANDLE>(m_streamHandle));
    }
}

void SonivoxAudioEngine::resumeMidi() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_easHandle && m_streamHandle) {
        EAS_Resume(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                   reinterpret_cast<EAS_HANDLE>(m_streamHandle));
    }
}

bool SonivoxAudioEngine::isMidiPlaying() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_easHandle || !m_streamHandle) return false;

    EAS_STATE state = EAS_STATE_STOPPED;
    EAS_State(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
              reinterpret_cast<EAS_HANDLE>(m_streamHandle), &state);
    return (state == EAS_STATE_PLAY || state == EAS_STATE_READY);
}

void SonivoxAudioEngine::sendShortMidi(int status, int data1, int data2) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) initialize();
    openMidiStreamLocked();

    if (m_easHandle && m_midiStreamHandle) {
        uint8_t ev[3] = {
            static_cast<uint8_t>(status),
            static_cast<uint8_t>(data1),
            static_cast<uint8_t>(data2)
        };
        int len = 3;
        int cmd = status & 0xF0;
        if (cmd == 0xC0 || cmd == 0xD0) len = 2; // Program change & Channel pressure
        EAS_WriteMIDIStream(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                            reinterpret_cast<EAS_HANDLE>(m_midiStreamHandle),
                            ev, len);
    }
}

void SonivoxAudioEngine::sendLongMidi(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) initialize();
    openMidiStreamLocked();

    if (m_easHandle && m_midiStreamHandle) {
        EAS_WriteMIDIStream(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                            reinterpret_cast<EAS_HANDLE>(m_midiStreamHandle),
                            const_cast<uint8_t*>(data), static_cast<EAS_I32>(length));
    }
}

void SonivoxAudioEngine::setMasterVolume(int volumePercent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_volumePercent = std::clamp(volumePercent, 0, 100);
    if (m_easHandle && m_streamHandle) {
        EAS_SetVolume(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                      reinterpret_cast<EAS_HANDLE>(m_streamHandle), m_volumePercent);
    }
}

void SonivoxAudioEngine::playTone(int note, int durationMs, int volume) {
    if (note < 0 || note > 127 || durationMs <= 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Tính tần số chuẩn MIDI note: f = 440 * 2^((note - 69) / 12)
    double freq = 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
    m_tonePhaseInc = (2.0 * 3.14159265358979323846 * freq) / 44100.0;
    m_toneFramesLeft = static_cast<int>((44100LL * durationMs) / 1000LL);
    m_toneVolume = std::clamp(volume, 0, 100) / 100.0f;
}

size_t SonivoxAudioEngine::renderAudio44100(int16_t* outStereoPcm, size_t frameCount) {
    if (!outStereoPcm || frameCount == 0) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Xóa bộ nhớ đệm đích
    std::memset(outStereoPcm, 0, frameCount * 2 * sizeof(int16_t));

    int rendered22050 = 0;
    size_t req22050 = (frameCount + 1) / 2;

    if (m_raw22050Buffer.size() < req22050 * 2) {
        m_raw22050Buffer.resize(req22050 * 2);
    }

    if (m_easHandle && (m_streamHandle || m_midiStreamHandle)) {
        EAS_I32 numGenerated = 0;
        EAS_RESULT r = EAS_Render(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                                  reinterpret_cast<EAS_PCM*>(m_raw22050Buffer.data()),
                                  static_cast<EAS_I32>(req22050),
                                  &numGenerated);

        if (r == EAS_SUCCESS && numGenerated > 0) {
            rendered22050 = static_cast<int>(numGenerated);

            // Kiểm tra trạng thái bài hát
            if (m_streamHandle) {
                EAS_STATE state = EAS_STATE_PLAY;
                EAS_State(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                          reinterpret_cast<EAS_HANDLE>(m_streamHandle), &state);

                if (state == EAS_STATE_STOPPED || state == EAS_STATE_EMPTY) {
                    if (m_loopCount > 1 || m_loopCount < 0) {
                        if (m_loopCount > 1) m_loopCount--;
                        EAS_Locate(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                                   reinterpret_cast<EAS_HANDLE>(m_streamHandle), 0, EAS_FALSE);
                        EAS_Resume(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                                   reinterpret_cast<EAS_HANDLE>(m_streamHandle));
                    } else {
                        closeStreamLocked();
                    }
                }
            }
        }
    }

    // Resample 2x (22050Hz Stereo -> 44100Hz Stereo) qua Nội suy tuyến tính
    float volFactor = m_volumePercent / 100.0f;
    for (size_t i = 0; i < frameCount; i++) {
        size_t srcIdx = i / 2;
        int16_t curL = 0, curR = 0;

        if (srcIdx < static_cast<size_t>(rendered22050)) {
            if ((i % 2) == 0) {
                curL = m_raw22050Buffer[srcIdx * 2];
                curR = m_raw22050Buffer[srcIdx * 2 + 1];
            } else {
                size_t nextIdx = (srcIdx + 1 < static_cast<size_t>(rendered22050)) ? (srcIdx + 1) : srcIdx;
                int32_t interpL = (static_cast<int32_t>(m_raw22050Buffer[srcIdx * 2]) + m_raw22050Buffer[nextIdx * 2]) / 2;
                int32_t interpR = (static_cast<int32_t>(m_raw22050Buffer[srcIdx * 2 + 1]) + m_raw22050Buffer[nextIdx * 2 + 1]) / 2;
                curL = static_cast<int16_t>(interpL);
                curR = static_cast<int16_t>(interpR);
            }
            curL = static_cast<int16_t>(curL * volFactor);
            curR = static_cast<int16_t>(curR * volFactor);
        }

        // Hòa âm Máy phát Tone độc lập
        if (m_toneFramesLeft > 0) {
            int16_t toneSample = static_cast<int16_t>(std::sin(m_tonePhase) * 8000.0 * m_toneVolume * volFactor);
            m_tonePhase += m_tonePhaseInc;
            if (m_tonePhase >= 2.0 * 3.14159265358979323846) {
                m_tonePhase -= 2.0 * 3.14159265358979323846;
            }
            int32_t mixL = curL + toneSample;
            int32_t mixR = curR + toneSample;
            curL = static_cast<int16_t>(std::clamp(mixL, -32768, 32767));
            curR = static_cast<int16_t>(std::clamp(mixR, -32768, 32767));
            m_toneFramesLeft--;
        }

        outStereoPcm[i * 2]     = curL;
        outStereoPcm[i * 2 + 1] = curR;
    }

    return frameCount;
}

int64_t SonivoxAudioEngine::getMediaTimeMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_easHandle || !m_streamHandle) return 0;
    EAS_I32 timeMs = 0;
    if (EAS_GetLocation(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                        reinterpret_cast<EAS_HANDLE>(m_streamHandle), &timeMs) == EAS_SUCCESS) {
        return timeMs;
    }
    return 0;
}

int64_t SonivoxAudioEngine::getMediaDurationMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_easHandle || !m_streamHandle) return 0;
    EAS_I32 playLength = 0;
    if (EAS_ParseMetaData(reinterpret_cast<EAS_DATA_HANDLE>(m_easHandle),
                          reinterpret_cast<EAS_HANDLE>(m_streamHandle), &playLength) == EAS_SUCCESS) {
        return playLength;
    }
    return 0;
}

// ============================================================================
// 4. MidiAudioPlayer Implementation
// ============================================================================

MidiAudioPlayer::MidiAudioPlayer(const std::vector<uint8_t>& data, const std::string& contentType)
    : m_data(data), m_contentType(contentType) {}

MidiAudioPlayer::MidiAudioPlayer(const std::string& locator)
    : m_locator(locator) {}

MidiAudioPlayer::~MidiAudioPlayer() {
    close();
}

void MidiAudioPlayer::realize() {
    if (m_state == PLAYER_CLOSED) return;
    m_state = PLAYER_REALIZED;
}

void MidiAudioPlayer::prefetch() {
    if (m_state == PLAYER_CLOSED) return;
    if (m_state < PLAYER_REALIZED) realize();
    m_state = PLAYER_PREFETCHED;
}

void MidiAudioPlayer::start() {
    if (m_state == PLAYER_CLOSED) return;
    if (m_state < PLAYER_PREFETCHED) prefetch();

    if (!m_data.empty()) {
        SonivoxAudioEngine::instance().playMidiData(m_data.data(), m_data.size(), m_loopCount);
    }
    m_state = PLAYER_STARTED;
}

void MidiAudioPlayer::stop() {
    if (m_state == PLAYER_STARTED) {
        SonivoxAudioEngine::instance().pauseMidi();
        m_state = PLAYER_PREFETCHED;
    }
}

void MidiAudioPlayer::deallocate() {
    stop();
    m_state = PLAYER_REALIZED;
}

void MidiAudioPlayer::close() {
    stop();
    SonivoxAudioEngine::instance().stopMidi();
    m_state = PLAYER_CLOSED;
}

void MidiAudioPlayer::setLoopCount(int count) {
    m_loopCount = count;
}

int64_t MidiAudioPlayer::setMediaTime(int64_t nowUsec) {
    int timeMs = static_cast<int>(nowUsec / 1000);
    // Seek to timeMs
    return nowUsec;
}

int64_t MidiAudioPlayer::getMediaTime() const {
    return SonivoxAudioEngine::instance().getMediaTimeMs() * 1000LL;
}

int64_t MidiAudioPlayer::getDuration() const {
    return SonivoxAudioEngine::instance().getMediaDurationMs() * 1000LL;
}

void MidiAudioPlayer::setLevel(int level) {
    m_volume = std::clamp(level, 0, 100);
    if (!m_muted) {
        SonivoxAudioEngine::instance().setMasterVolume(m_volume);
    }
}

void MidiAudioPlayer::setMute(bool mute) {
    m_muted = mute;
    SonivoxAudioEngine::instance().setMasterVolume(m_muted ? 0 : m_volume);
}

void MidiAudioPlayer::shortMidiEvent(int type, int data1, int data2) {
    SonivoxAudioEngine::instance().sendShortMidi(type, data1, data2);
}

int MidiAudioPlayer::longMidiEvent(const uint8_t* data, size_t length) {
    SonivoxAudioEngine::instance().sendLongMidi(data, length);
    return static_cast<int>(length);
}

void MidiAudioPlayer::setProgram(int channel, int bank, int program) {
    (void)bank;
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
