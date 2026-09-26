#ifndef UNIVERSAL_LOADER_AMMS_TYPES_H
#define UNIVERSAL_LOADER_AMMS_TYPES_H

#include "j2me_core.h"
#include <cstdint>
#include <string>
#include <vector>

namespace universal_loader {
namespace amms {

// EffectControl Scope (EffectControl.java)
constexpr int32_t SCOPE_LIVE_ONLY        = 1;
constexpr int32_t SCOPE_RECORD_ONLY      = 2;
constexpr int32_t SCOPE_LIVE_AND_RECORD  = 3;

// CameraControl Constants (CameraControl.java)
constexpr int32_t ROTATE_NONE  = 1;
constexpr int32_t ROTATE_LEFT  = 2;
constexpr int32_t ROTATE_RIGHT = 3;
constexpr int32_t UNKNOWN      = -1004;

// FlashControl Constants (FlashControl.java)
constexpr int32_t FLASH_OFF                    = 1;
constexpr int32_t FLASH_AUTO                   = 2;
constexpr int32_t FLASH_AUTO_WITH_REDEYEREDUCE = 3;
constexpr int32_t FLASH_FORCE                  = 4;
constexpr int32_t FLASH_FORCE_WITH_REDEYEREDUCE= 5;
constexpr int32_t FLASH_FILLIN                 = 6;

// FocusControl Constants
constexpr int32_t FOCUS_AUTO        = 1;
constexpr int32_t FOCUS_AUTO_LOCK   = 2;
constexpr int32_t FOCUS_MANUAL      = 3;
constexpr int32_t FOCUS_MACRO       = 4;

// ZoomControl Constants (ZoomControl.java)
constexpr int32_t ZOOM_NEXT     = -1001;
constexpr int32_t ZOOM_PREVIOUS = -1002;

// Speed of sound in dry air at 20 C (in m/s)
constexpr float SPEED_OF_SOUND = 343.0f;

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

struct Dimension2D {
    int32_t width{0};
    int32_t height{0};
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_TYPES_H
