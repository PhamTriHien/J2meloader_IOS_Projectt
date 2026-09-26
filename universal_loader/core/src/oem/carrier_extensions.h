#ifndef UNIVERSAL_LOADER_CARRIER_EXTENSIONS_H
#define UNIVERSAL_LOADER_CARRIER_EXTENSIONS_H

#include "j2me_core.h"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

namespace universal_loader {
namespace oem {
namespace carrier {

// ==================== KDDI PhoneSystem (com.kddi.system.PhoneSystem) ====================
class J2ME_API KDDISystem {
public:
    static int32_t getKeyState(bool eightDirections);
};

// ==================== Motorola FunLight (com.motorola.funlight.FunLight) ====================
class J2ME_API MotorolaFunLight {
public:
    static constexpr int32_t MAX_REGIONS = 8;

    static MotorolaFunLight& instance();

    void setColor(int32_t region, uint32_t rgb);
    uint32_t getColor(int32_t region) const;
    int32_t getRegionCount() const;
    void reset();

private:
    MotorolaFunLight();
    ~MotorolaFunLight() = default;

    mutable std::mutex m_mutex;
    uint32_t m_regionColors[MAX_REGIONS];
};

// ==================== Sony Ericsson Accelerometer (com.sonyericsson.accelerometer) ====================
class J2ME_API SonyEricssonAccelerometer {
public:
    static SonyEricssonAccelerometer& instance();

    void setAcceleration(float x, float y, float z);
    void getAcceleration(float& outX, float& outY, float& outZ) const;
    void reset();

private:
    SonyEricssonAccelerometer();
    ~SonyEricssonAccelerometer() = default;

    mutable std::mutex m_mutex;
    float m_accelX{0.0f};
    float m_accelY{0.0f};
    float m_accelZ{9.81f}; // Standard resting 1G on Z axis
};

// ==================== Sprint PCS Media Player (com.sprintpcs.media.Player) ====================
class J2ME_API SprintPCSPlayer {
public:
    static SprintPCSPlayer& instance();

    void playClip(const std::string& clipName, int32_t loopCount);
    void stop();
    bool isPlaying() const;
    std::string getCurrentClip() const;
    int32_t getLoopCount() const;

private:
    SprintPCSPlayer();
    ~SprintPCSPlayer() = default;

    mutable std::mutex m_mutex;
    std::string m_currentClip;
    int32_t m_loopCount{0};
    bool m_isPlaying{false};
};

} // namespace carrier
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CARRIER_EXTENSIONS_H
