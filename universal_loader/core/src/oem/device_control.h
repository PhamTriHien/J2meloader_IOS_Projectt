#ifndef UNIVERSAL_LOADER_DEVICE_CONTROL_H
#define UNIVERSAL_LOADER_DEVICE_CONTROL_H

#include <cstdint>
#include <mutex>
#include <chrono>
#include <functional>
#include "../../include/j2me_core.h"

namespace universal_loader {
namespace oem {

using DeviceVibrationCallback = void (*)(int32_t duration_ms, int32_t frequency, void* user_data);

class J2ME_API DeviceControlManager {
public:
    static DeviceControlManager& instance();

    // Callback wiring to Frontend (Flutter / iOS / Android / Desktop)
    void setVibrationCallback(DeviceVibrationCallback callback, void* userData);

    // Nokia DeviceControl & Siemens / Samsung Vibration API
    void startVibra(int32_t frequency, int64_t durationMs);
    void stopVibra();
    void vibrate(int32_t durationMs, int32_t strength = 100);

    // Lights
    void setLights(int32_t num, int32_t level);
    void flashLights(int64_t durationMs);

    // Inactivity Tracking
    int32_t getUserInactivityTime();
    void resetUserInactivityTime();

    // Inspection for verification
    int32_t getLastVibrationDuration() const;
    int32_t getLastVibrationFrequency() const;
    bool isVibrating() const;

private:
    DeviceControlManager();
    ~DeviceControlManager() = default;

    mutable std::mutex m_mutex;
    DeviceVibrationCallback m_vibrationCallback{nullptr};
    void* m_vibrationUserData{nullptr};

    int32_t m_lastDurationMs{0};
    int32_t m_lastFrequency{0};
    bool m_isVibrating{false};

    std::chrono::steady_clock::time_point m_lastActivityTime;
};

} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_DEVICE_CONTROL_H
