#include "vodafone_device_control.h"
#include "../device_control.h"
#include <stdexcept>

namespace universal_loader {
namespace oem {
namespace vodafone {

VodafoneDeviceControl::VodafoneDeviceControl() = default;

VodafoneDeviceControl& VodafoneDeviceControl::instance() {
    static VodafoneDeviceControl s_instance;
    return s_instance;
}

int32_t VodafoneDeviceControl::getDeviceState(int32_t deviceNo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (deviceNo) {
        case DEVICE_BATTERY:
            return m_batteryLevel;
        case DEVICE_FIELD_INTENSITY:
            return m_fieldIntensity;
        case DEVICE_KEY_STATE:
            return static_cast<int32_t>(m_keyStatesMask);
        case DEVICE_EIGHT_DIRECTIONS:
            return m_eightDirectionsActive ? 1 : 0;
        default:
            throw std::runtime_error("Invalid Vodafone device number: " + std::to_string(deviceNo));
    }
}

bool VodafoneDeviceControl::isDeviceActive(int32_t deviceNo) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (deviceNo) {
        case DEVICE_BACK_LIGHT:
            return m_backlightActive;
        case DEVICE_EIGHT_DIRECTIONS:
            return m_eightDirectionsActive;
        case DEVICE_VIBRATION:
            return m_vibrationActive;
        default:
            return false;
    }
}

bool VodafoneDeviceControl::setDeviceActive(int32_t deviceNo, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (deviceNo) {
        case DEVICE_BACK_LIGHT:
            m_backlightActive = active;
            DeviceControlManager::instance().setLights(0, active ? 100 : 0);
            return true;
        case DEVICE_EIGHT_DIRECTIONS:
            m_eightDirectionsActive = active;
            return true;
        case DEVICE_VIBRATION:
            m_vibrationActive = active;
            DeviceControlManager::instance().vibrate(active ? 2000 : 0);
            return true;
        default:
            throw std::runtime_error("Invalid Vodafone device number: " + std::to_string(deviceNo));
    }
}

void VodafoneDeviceControl::blink(int32_t lightingMs, int32_t extinctionMs, int32_t repeat) {
    (void)lightingMs;
    (void)extinctionMs;
    (void)repeat;
    // Delegate to system lights flasher
    DeviceControlManager::instance().flashLights(lightingMs > 0 ? lightingMs : 500);
}

void VodafoneDeviceControl::setKeyDown(int32_t keyCode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys.insert(keyCode);
    uint32_t bit = keyToVodafoneBit(keyCode);
    m_keyStatesMask |= bit;
}

void VodafoneDeviceControl::setKeyUp(int32_t keyCode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys.erase(keyCode);
    uint32_t bit = keyToVodafoneBit(keyCode);
    m_keyStatesMask &= ~bit;
}

void VodafoneDeviceControl::resetKeyStates() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys.clear();
    m_keyStatesMask = 0;
}

uint32_t VodafoneDeviceControl::getKeyStatesVodafone() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_keyStatesMask;
}

void VodafoneDeviceControl::setKeyStatesMask(uint32_t mask) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_keyStatesMask = mask;
}

uint32_t VodafoneDeviceControl::keyToVodafoneBit(int32_t keyCode) {
    switch (keyCode) {
        case '0': case 0:
            return VODAFONE_KEY_0;
        case '1':
            return VODAFONE_KEY_1;
        case '2':
            return VODAFONE_KEY_2;
        case '3':
            return VODAFONE_KEY_3;
        case '4':
            return VODAFONE_KEY_4;
        case '5':
            return VODAFONE_KEY_5;
        case '6':
            return VODAFONE_KEY_6;
        case '7':
            return VODAFONE_KEY_7;
        case '8':
            return VODAFONE_KEY_8;
        case '9':
            return VODAFONE_KEY_9;
        case '*':
            return VODAFONE_KEY_STAR;
        case '#':
            return VODAFONE_KEY_POUND;
        case -1: // KEY_UP
            return VODAFONE_KEY_UP;
        case -3: // KEY_LEFT
            return VODAFONE_KEY_LEFT;
        case -4: // KEY_RIGHT
            return VODAFONE_KEY_RIGHT;
        case -2: // KEY_DOWN
            return VODAFONE_KEY_DOWN;
        case -5: case 10: case 32: // KEY_FIRE / SELECT
            return VODAFONE_KEY_FIRE;
        case -6: case -21: // KEY_SOFT_LEFT
            return VODAFONE_KEY_SOFT_LEFT;
        case -7: case -22: // KEY_SOFT_RIGHT
            return VODAFONE_KEY_SOFT_RIGHT;
        case -8: case 'c': case 'C': // KEY_CLEAR
            return VODAFONE_KEY_CLEAR;
        case -17: case 18: // KEY_UP_RIGHT
            return VODAFONE_KEY_UP_RIGHT;
        case -16: // KEY_UP_LEFT
            return VODAFONE_KEY_UP_LEFT;
        case -19: case 23: // KEY_DOWN_RIGHT
            return VODAFONE_KEY_DOWN_RIGHT;
        case -18: case 21: // KEY_DOWN_LEFT
            return VODAFONE_KEY_DOWN_LEFT;
        default:
            return 0;
    }
}

void VodafoneDeviceControl::setBatteryLevel(int32_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batteryLevel = (level < 0 ? 0 : (level > 100 ? 100 : level));
}

void VodafoneDeviceControl::setFieldIntensity(int32_t level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fieldIntensity = (level < 0 ? 0 : (level > 100 ? 100 : level));
}

} // namespace vodafone
} // namespace oem
} // namespace universal_loader
