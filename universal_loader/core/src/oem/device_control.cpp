#include "device_control.h"
#include <algorithm>

namespace universal_loader {
namespace oem {

DeviceControlManager& DeviceControlManager::instance() {
    static DeviceControlManager s_instance;
    return s_instance;
}

DeviceControlManager::DeviceControlManager() {
    m_lastActivityTime = std::chrono::steady_clock::now();
}

void DeviceControlManager::setVibrationCallback(DeviceVibrationCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vibrationCallback = callback;
    m_vibrationUserData = userData;
}

void DeviceControlManager::startVibra(int32_t frequency, int64_t durationMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastFrequency = std::clamp(frequency, 0, 100);
    m_lastDurationMs = static_cast<int32_t>(std::max(int64_t(0), durationMs));
    m_isVibrating = (m_lastDurationMs > 0);

    if (m_vibrationCallback) {
        m_vibrationCallback(m_lastDurationMs, m_lastFrequency, m_vibrationUserData);
    }
}

void DeviceControlManager::stopVibra() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastDurationMs = 0;
    m_isVibrating = false;

    if (m_vibrationCallback) {
        m_vibrationCallback(0, 0, m_vibrationUserData);
    }
}

void DeviceControlManager::vibrate(int32_t durationMs, int32_t strength) {
    startVibra(strength, durationMs);
}

void DeviceControlManager::setLights(int32_t num, int32_t level) {
    // Hardware lights control hook
    (void)num;
    (void)level;
}

void DeviceControlManager::flashLights(int64_t durationMs) {
    // Hardware flashlight flash hook
    (void)durationMs;
}

int32_t DeviceControlManager::getUserInactivityTime() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastActivityTime).count();
    return static_cast<int32_t>(elapsedSec);
}

void DeviceControlManager::resetUserInactivityTime() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastActivityTime = std::chrono::steady_clock::now();
}

int32_t DeviceControlManager::getLastVibrationDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastDurationMs;
}

int32_t DeviceControlManager::getLastVibrationFrequency() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastFrequency;
}

bool DeviceControlManager::isVibrating() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isVibrating;
}

} // namespace oem
} // namespace universal_loader
