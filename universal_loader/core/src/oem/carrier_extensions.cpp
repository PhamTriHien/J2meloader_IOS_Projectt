#include "carrier_extensions.h"
#include "vodafone/vodafone_device_control.h"
#include <algorithm>

namespace universal_loader {
namespace oem {
namespace carrier {

// ==================== KDDI PhoneSystem ====================

int32_t KDDISystem::getKeyState(bool eightDirections) {
    (void)eightDirections;
    return static_cast<int32_t>(vodafone::VodafoneDeviceControl::instance().getKeyStatesVodafone());
}

// ==================== Motorola FunLight ====================

MotorolaFunLight::MotorolaFunLight() {
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        m_regionColors[i] = 0;
    }
}

MotorolaFunLight& MotorolaFunLight::instance() {
    static MotorolaFunLight s_instance;
    return s_instance;
}

void MotorolaFunLight::setColor(int32_t region, uint32_t rgb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (region >= 1 && region <= MAX_REGIONS) {
        m_regionColors[region - 1] = rgb & 0x00FFFFFFu;
    }
}

uint32_t MotorolaFunLight::getColor(int32_t region) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (region >= 1 && region <= MAX_REGIONS) {
        return m_regionColors[region - 1];
    }
    return 0;
}

int32_t MotorolaFunLight::getRegionCount() const {
    return MAX_REGIONS;
}

void MotorolaFunLight::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        m_regionColors[i] = 0;
    }
}

// ==================== Sony Ericsson Accelerometer ====================

SonyEricssonAccelerometer::SonyEricssonAccelerometer() = default;

SonyEricssonAccelerometer& SonyEricssonAccelerometer::instance() {
    static SonyEricssonAccelerometer s_instance;
    return s_instance;
}

void SonyEricssonAccelerometer::setAcceleration(float x, float y, float z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_accelX = x;
    m_accelY = y;
    m_accelZ = z;
}

void SonyEricssonAccelerometer::getAcceleration(float& outX, float& outY, float& outZ) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outX = m_accelX;
    outY = m_accelY;
    outZ = m_accelZ;
}

void SonyEricssonAccelerometer::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_accelX = 0.0f;
    m_accelY = 0.0f;
    m_accelZ = 9.81f;
}

// ==================== Sprint PCS Media Player ====================

SprintPCSPlayer::SprintPCSPlayer() = default;

SprintPCSPlayer& SprintPCSPlayer::instance() {
    static SprintPCSPlayer s_instance;
    return s_instance;
}

void SprintPCSPlayer::playClip(const std::string& clipName, int32_t loopCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentClip = clipName;
    m_loopCount = loopCount;
    m_isPlaying = true;
}

void SprintPCSPlayer::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isPlaying = false;
}

bool SprintPCSPlayer::isPlaying() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isPlaying;
}

std::string SprintPCSPlayer::getCurrentClip() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentClip;
}

int32_t SprintPCSPlayer::getLoopCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loopCount;
}

} // namespace carrier
} // namespace oem
} // namespace universal_loader
