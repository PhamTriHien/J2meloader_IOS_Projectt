#ifndef UNIVERSAL_LOADER_VODAFONE_DEVICE_CONTROL_H
#define UNIVERSAL_LOADER_VODAFONE_DEVICE_CONTROL_H

#include "vodafone_types.h"
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace universal_loader {
namespace oem {
namespace vodafone {

class J2ME_API VodafoneDeviceControl {
public:
    static VodafoneDeviceControl& instance();

    // Query device status
    int32_t getDeviceState(int32_t deviceNo);
    bool isDeviceActive(int32_t deviceNo) const;
    bool setDeviceActive(int32_t deviceNo, bool active);

    // Illumination / Blink
    void blink(int32_t lightingMs, int32_t extinctionMs, int32_t repeat);

    // Keypad integration and Vodafone 24-bit key states
    void setKeyDown(int32_t keyCode);
    void setKeyUp(int32_t keyCode);
    void resetKeyStates();
    uint32_t getKeyStatesVodafone() const;

    // Direct key bitmask manipulation for testing / fast bridge
    void setKeyStatesMask(uint32_t mask);

    // Static mapping from standard J2ME key codes to Vodafone bitmask
    static uint32_t keyToVodafoneBit(int32_t keyCode);

    // Battery & Field intensity configuration
    void setBatteryLevel(int32_t level);
    void setFieldIntensity(int32_t level);

private:
    VodafoneDeviceControl();
    ~VodafoneDeviceControl() = default;

    mutable std::mutex m_mutex;
    int32_t m_batteryLevel{100};
    int32_t m_fieldIntensity{100};
    bool m_backlightActive{true};
    bool m_eightDirectionsActive{false};
    bool m_vibrationActive{false};

    uint32_t m_keyStatesMask{0};
    std::unordered_set<int32_t> m_pressedKeys;
};

} // namespace vodafone
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_VODAFONE_DEVICE_CONTROL_H
