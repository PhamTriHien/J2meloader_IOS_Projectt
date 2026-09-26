#ifndef UNIVERSAL_LOADER_VODAFONE_TYPES_H
#define UNIVERSAL_LOADER_VODAFONE_TYPES_H

#include "j2me_core.h"
#include <cstdint>
#include <cstddef>

namespace universal_loader {
namespace oem {
namespace vodafone {

// Vodafone Device Control Constants (com.vodafone.v10.system.device.DeviceControl)
constexpr int32_t DEVICE_BATTERY          = 1;
constexpr int32_t DEVICE_FIELD_INTENSITY  = 2;
constexpr int32_t DEVICE_KEY_STATE        = 3;
constexpr int32_t DEVICE_VIBRATION        = 4;
constexpr int32_t DEVICE_BACK_LIGHT       = 5;
constexpr int32_t DEVICE_EIGHT_DIRECTIONS = 6;

// Vodafone 24-bit Key State Bitmask (VirtualKeyboard.java / DeviceControl.java / PhoneSystem.java)
constexpr uint32_t VODAFONE_KEY_0          = 1u << 0;  // 0 Key
constexpr uint32_t VODAFONE_KEY_1          = 1u << 1;  // 1 Key
constexpr uint32_t VODAFONE_KEY_2          = 1u << 2;  // 2 Key
constexpr uint32_t VODAFONE_KEY_3          = 1u << 3;  // 3 Key
constexpr uint32_t VODAFONE_KEY_4          = 1u << 4;  // 4 Key
constexpr uint32_t VODAFONE_KEY_5          = 1u << 5;  // 5 Key
constexpr uint32_t VODAFONE_KEY_6          = 1u << 6;  // 6 Key
constexpr uint32_t VODAFONE_KEY_7          = 1u << 7;  // 7 Key
constexpr uint32_t VODAFONE_KEY_8          = 1u << 8;  // 8 Key
constexpr uint32_t VODAFONE_KEY_9          = 1u << 9;  // 9 Key
constexpr uint32_t VODAFONE_KEY_STAR       = 1u << 10; // * Key
constexpr uint32_t VODAFONE_KEY_POUND      = 1u << 11; // # Key
constexpr uint32_t VODAFONE_KEY_UP         = 1u << 12; // Up Key
constexpr uint32_t VODAFONE_KEY_LEFT       = 1u << 13; // Left Key
constexpr uint32_t VODAFONE_KEY_RIGHT      = 1u << 14; // Right Key
constexpr uint32_t VODAFONE_KEY_DOWN       = 1u << 15; // Down Key
constexpr uint32_t VODAFONE_KEY_FIRE       = 1u << 16; // Select / Fire Key
constexpr uint32_t VODAFONE_KEY_SOFT_LEFT  = 1u << 17; // Softkey 1 (Left)
constexpr uint32_t VODAFONE_KEY_SOFT_RIGHT = 1u << 18; // Softkey 2 (Right)
constexpr uint32_t VODAFONE_KEY_CLEAR      = 1u << 19; // Softkey 3 / Clear (KEY_C)
constexpr uint32_t VODAFONE_KEY_UP_RIGHT   = 1u << 20; // Upper Right (Diagonal)
constexpr uint32_t VODAFONE_KEY_UP_LEFT    = 1u << 21; // Upper Left (Diagonal)
constexpr uint32_t VODAFONE_KEY_DOWN_RIGHT = 1u << 22; // Lower Right (Diagonal)
constexpr uint32_t VODAFONE_KEY_DOWN_LEFT  = 1u << 23; // Lower Left (Diagonal)

// Vodafone SoundTrack States (com.vodafone.v10.sound.SoundTrack)
constexpr int32_t SOUND_STATE_NO_DATA = 0;
constexpr int32_t SOUND_STATE_READY   = 1;
constexpr int32_t SOUND_STATE_PLAYING = 2;
constexpr int32_t SOUND_STATE_PAUSED  = 3;
constexpr int32_t SOUND_MAX_VOLUME    = 127;
constexpr int32_t SOUND_MAX_TRACKS    = 16;

// Vodafone Sound Event Types (com.vodafone.v10.sound.SoundEventType)
constexpr int32_t EV_END     = -1;
constexpr int32_t EV_LOOP    = -2;
constexpr int32_t EV_PAUSE   = -3;
constexpr int32_t EV_USER_0  = 0;
constexpr int32_t EV_USER_1  = 1;
constexpr int32_t EV_USER_2  = 2;
constexpr int32_t EV_USER_3  = 3;
constexpr int32_t EV_USER_4  = 4;
constexpr int32_t EV_USER_5  = 5;
constexpr int32_t EV_USER_6  = 6;
constexpr int32_t EV_USER_7  = 7;
constexpr int32_t EV_USER_8  = 8;
constexpr int32_t EV_USER_9  = 9;
constexpr int32_t EV_USER_10 = 10;
constexpr int32_t EV_USER_11 = 11;
constexpr int32_t EV_USER_12 = 12;
constexpr int32_t EV_USER_13 = 13;
constexpr int32_t EV_USER_14 = 14;
constexpr int32_t EV_USER_15 = 15;

// Vodafone ImageEncoder Formats (com.vodafone.util.ImageEncoder)
constexpr int32_t IMAGE_FORMAT_PNG  = 0;
constexpr int32_t IMAGE_FORMAT_JPEG = 1;

// Vodafone Sprite Canvas Rotation Constants
constexpr int32_t SPRITE_ROT_NONE = 0;
constexpr int32_t SPRITE_ROT_90   = 1;
constexpr int32_t SPRITE_ROT_180  = 2;
constexpr int32_t SPRITE_ROT_270  = 3;

// 8x8 Character Pattern Size
constexpr size_t SPRITE_PATTERN_SIZE = 64; // 8 * 8 bytes

} // namespace vodafone
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_VODAFONE_TYPES_H
