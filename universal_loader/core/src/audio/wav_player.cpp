#include "wav_player.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace j2me {

std::shared_ptr<WavPlayer> WavPlayer::createFromMemory(const uint8_t* data, size_t size) {
    if (!data || size < 44) return nullptr;
    auto player = std::make_shared<WavPlayer>(data, size);
    if (!player->isValid()) return nullptr;
    return player;
}

std::shared_ptr<WavPlayer> WavPlayer::createFromFile(const std::string& path) {
    auto player = std::make_shared<WavPlayer>(path);
    if (!player->isValid()) return nullptr;
    return player;
}

WavPlayer::WavPlayer(const uint8_t* data, size_t size) {
    if (data && size >= 44) {
        m_rawData.assign(data, data + size);
        if (parseRiffHeader()) {
            decodeSamplesToPcm16();
        }
    }
}

WavPlayer::WavPlayer(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        if (size >= 44) {
            file.seekg(0, std::ios::beg);
            m_rawData.resize(static_cast<size_t>(size));
            if (file.read(reinterpret_cast<char*>(m_rawData.data()), size)) {
                if (parseRiffHeader()) {
                    decodeSamplesToPcm16();
                }
            }
        }
    }
}

WavPlayer::~WavPlayer() {
    close();
}

static uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
          (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}

static uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
          (static_cast<uint16_t>(p[1]) << 8);
}

bool WavPlayer::parseRiffHeader() {
    if (m_rawData.size() < 44) return false;

    // Check "RIFF"
    if (std::memcmp(m_rawData.data(), "RIFF", 4) != 0) {
        return false;
    }

    // Check "WAVE"
    if (std::memcmp(m_rawData.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool fmtFound = false;
    bool dataFound = false;
    size_t offset = 12;

    while (offset + 8 <= m_rawData.size()) {
        char chunkId[5] = {0};
        std::memcpy(chunkId, m_rawData.data() + offset, 4);
        uint32_t chunkSize = readU32LE(m_rawData.data() + offset + 4);
        offset += 8;

        if (std::strcmp(chunkId, "fmt ") == 0 && offset + 16 <= m_rawData.size()) {
            m_format.audioFormat = readU16LE(m_rawData.data() + offset);
            m_format.numChannels = readU16LE(m_rawData.data() + offset + 2);
            m_format.sampleRate = readU32LE(m_rawData.data() + offset + 4);
            m_format.byteRate = readU32LE(m_rawData.data() + offset + 8);
            m_format.blockAlign = readU16LE(m_rawData.data() + offset + 12);
            m_format.bitsPerSample = readU16LE(m_rawData.data() + offset + 14);
            fmtFound = true;
        } else if (std::strcmp(chunkId, "data") == 0) {
            m_format.dataOffset = offset;
            m_format.dataSize = std::min<size_t>(chunkSize, m_rawData.size() - offset);
            dataFound = true;
            break; // Data chunk found
        }

        offset += chunkSize;
        if (chunkSize & 1) offset++; // Word alignment
    }

    if (!fmtFound || !dataFound) {
        return false;
    }

    // Supports PCM 8-bit or 16-bit, 1 or 2 channels
    if (m_format.audioFormat != 1 || (m_format.bitsPerSample != 8 && m_format.bitsPerSample != 16)) {
        return false;
    }
    if (m_format.numChannels != 1 && m_format.numChannels != 2) {
        return false;
    }
    if (m_format.sampleRate == 0) {
        return false;
    }

    size_t bytesPerFrame = m_format.numChannels * (m_format.bitsPerSample / 8);
    if (bytesPerFrame == 0) return false;

    m_format.totalFrames = m_format.dataSize / bytesPerFrame;
    m_valid = (m_format.totalFrames > 0);
    return m_valid;
}

void WavPlayer::decodeSamplesToPcm16() {
    if (!m_valid) return;

    size_t sampleCount = m_format.totalFrames * m_format.numChannels;
    m_decodedPcmMonoOrStereo.resize(sampleCount);

    const uint8_t* src = m_rawData.data() + m_format.dataOffset;

    if (m_format.bitsPerSample == 8) {
        for (size_t i = 0; i < sampleCount; ++i) {
            uint8_t u8 = src[i];
            m_decodedPcmMonoOrStereo[i] = static_cast<int16_t>((static_cast<int>(u8) - 128) << 8);
        }
    } else if (m_format.bitsPerSample == 16) {
        for (size_t i = 0; i < sampleCount; ++i) {
            m_decodedPcmMonoOrStereo[i] = static_cast<int16_t>(readU16LE(src + i * 2));
        }
    }
}

void WavPlayer::sampleAt(double frameIndex, int16_t& outLeft, int16_t& outRight) const {
    if (m_decodedPcmMonoOrStereo.empty() || m_format.totalFrames == 0) {
        outLeft = 0;
        outRight = 0;
        return;
    }

    size_t f0 = static_cast<size_t>(frameIndex);
    if (f0 >= m_format.totalFrames) f0 = m_format.totalFrames - 1;
    size_t f1 = std::min(f0 + 1, m_format.totalFrames - 1);
    double frac = frameIndex - static_cast<double>(f0);

    if (m_format.numChannels == 1) {
        double s0 = m_decodedPcmMonoOrStereo[f0];
        double s1 = m_decodedPcmMonoOrStereo[f1];
        int16_t s = static_cast<int16_t>(s0 + frac * (s1 - s0));
        outLeft = s;
        outRight = s;
    } else {
        double l0 = m_decodedPcmMonoOrStereo[f0 * 2];
        double l1 = m_decodedPcmMonoOrStereo[f1 * 2];
        double r0 = m_decodedPcmMonoOrStereo[f0 * 2 + 1];
        double r1 = m_decodedPcmMonoOrStereo[f1 * 2 + 1];
        outLeft = static_cast<int16_t>(l0 + frac * (l1 - l0));
        outRight = static_cast<int16_t>(r0 + frac * (r1 - r0));
    }
}

void WavPlayer::realize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_UNREALIZED) {
        m_state = PLAYER_REALIZED;
    }
}

void WavPlayer::prefetch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state < PLAYER_PREFETCHED) {
        m_state = PLAYER_PREFETCHED;
    }
}

MmapiPlayerState WavPlayer::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void WavPlayer::start() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == PLAYER_CLOSED || m_state == PLAYER_STARTED) return;
        m_state = PLAYER_STARTED;
    }
    if (auto self = weak_from_this().lock()) AudioOutput::instance().attach(self);
}

void WavPlayer::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == PLAYER_STARTED) {
        m_state = PLAYER_PREFETCHED;
    }
}

void WavPlayer::deallocate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != PLAYER_CLOSED && m_state > PLAYER_REALIZED) m_state = PLAYER_REALIZED;
}

void WavPlayer::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = PLAYER_CLOSED;
}

void WavPlayer::setLoopCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (count == 0) return;
    m_loopCount = count;
    m_currentLoop = 0;
}

int64_t WavPlayer::setMediaTime(int64_t nowUsec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid || m_format.sampleRate == 0) return 0;
    double targetSec = static_cast<double>(nowUsec) / 1000000.0;
    m_playbackFramePos = std::clamp(targetSec * m_format.sampleRate, 0.0, static_cast<double>(m_format.totalFrames));
    return static_cast<int64_t>(m_playbackFramePos * 1000000.0 / m_format.sampleRate);
}

int64_t WavPlayer::getMediaTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid || m_format.sampleRate == 0) return 0;
    return static_cast<int64_t>(m_playbackFramePos * 1000000.0 / m_format.sampleRate);
}

int64_t WavPlayer::getDuration() const {
    if (!m_valid || m_format.sampleRate == 0) return 0;
    return static_cast<int64_t>(m_format.totalFrames) * 1000000LL / m_format.sampleRate;
}

void WavPlayer::setLevel(int level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_volume = std::clamp(level, 0, 100);
}

void WavPlayer::setMute(bool mute) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_muted = mute;
}

template <typename Emit>
size_t WavPlayer::renderLocked(size_t frameCount, Emit emit) {
    if (m_state != PLAYER_STARTED || !m_valid || m_format.totalFrames == 0) return 0;

    double ratio = static_cast<double>(m_format.sampleRate) / 44100.0;
    float vol = m_muted ? 0.0f : (static_cast<float>(m_volume) / 100.0f);
    size_t rendered = 0;

    for (size_t i = 0; i < frameCount; ++i) {
        if (m_playbackFramePos >= static_cast<double>(m_format.totalFrames)) {
            if (m_loopCount == -1) {
                // Infinite loop
                m_playbackFramePos = std::fmod(m_playbackFramePos, static_cast<double>(m_format.totalFrames));
            } else if (m_currentLoop < m_loopCount - 1) {
                m_currentLoop++;
                m_playbackFramePos = std::fmod(m_playbackFramePos, static_cast<double>(m_format.totalFrames));
            } else {
                // Finished playing
                m_state = PLAYER_PREFETCHED;
                m_playbackFramePos = 0.0;
                m_currentLoop = 0;
                m_endOfMedia++;
                break;
            }
        }

        int16_t l = 0, r = 0;
        sampleAt(m_playbackFramePos, l, r);
        emit(i, static_cast<int32_t>(static_cast<float>(l) * vol), static_cast<int32_t>(static_cast<float>(r) * vol));

        m_playbackFramePos += ratio;
        rendered++;
    }

    return rendered;
}

size_t WavPlayer::renderAudio44100(int16_t* outStereoPcm, size_t frameCount) {
    if (!outStereoPcm || frameCount == 0) return 0;
    std::memset(outStereoPcm, 0, frameCount * 2 * sizeof(int16_t));
    std::lock_guard<std::mutex> lock(m_mutex);
    return renderLocked(frameCount, [&](size_t i, int32_t l, int32_t r) {
        outStereoPcm[i * 2] = static_cast<int16_t>(l);
        outStereoPcm[i * 2 + 1] = static_cast<int16_t>(r);
    });
}

bool WavPlayer::mixInto(int32_t* acc, size_t frameCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return renderLocked(frameCount, [&](size_t i, int32_t l, int32_t r) {
        acc[i * 2] += l;
        acc[i * 2 + 1] += r;
    }) > 0;
}

size_t WavPlayer::streamToRingBuffer(AudioRingBuffer& ringBuffer, size_t maxFrames) {
    if (m_state != PLAYER_STARTED || !m_valid) return 0;

    size_t toWrite = std::min(maxFrames, ringBuffer.availableWrite());
    if (toWrite == 0) return 0;

    std::vector<int16_t> tempBuffer(toWrite * 2);
    size_t rendered = renderAudio44100(tempBuffer.data(), toWrite);
    if (rendered > 0) {
        return ringBuffer.write(tempBuffer.data(), rendered);
    }
    return 0;
}

} // namespace j2me
