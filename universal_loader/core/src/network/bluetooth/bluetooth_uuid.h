#pragma once

#include "j2me_core.h"
#include <cstdint>
#include <string>
#include <array>
#include <functional>

namespace j2me {
namespace bluetooth {

class J2ME_API BluetoothUUID {
public:
    // Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB
    static const std::array<uint8_t, 16> BASE_UUID;

    // Default: all zeros
    BluetoothUUID();

    // From 16-bit or 32-bit short value
    explicit BluetoothUUID(uint32_t shortValue);

    // From hex string. If shortUUID is true, expands with Bluetooth Base UUID.
    // Supports with or without hyphens.
    BluetoothUUID(const std::string& uuidString, bool shortUUID = false);

    // From raw 16 bytes (big endian)
    explicit BluetoothUUID(const std::array<uint8_t, 16>& rawBytes);
    explicit BluetoothUUID(const uint8_t* rawBytes);

    // Check if matches Bluetooth Base UUID
    bool isShortUUID() const;

    // Get 32-bit value if short UUID, or first 4 bytes
    uint32_t getShortValue() const;

    // Upstream J2ME string representation (uppercase hex without hyphens, leading zeros trimmed)
    std::string toString() const;

    // Short hex string (e.g. "1101")
    std::string toShortHexString() const;

    // Standard canonical string: 8-4-4-4-12 hex with hyphens
    std::string toCanonicalString() const;

    // Full 32-character uppercase hex string (without hyphens)
    std::string toHex32() const;

    const std::array<uint8_t, 16>& getBytes() const { return bytes_; }

    bool operator==(const BluetoothUUID& other) const { return bytes_ == other.bytes_; }
    bool operator!=(const BluetoothUUID& other) const { return bytes_ != other.bytes_; }
    bool operator<(const BluetoothUUID& other) const { return bytes_ < other.bytes_; }

private:
    std::array<uint8_t, 16> bytes_;

    static uint8_t hexNibble(char c);
};

} // namespace bluetooth
} // namespace j2me

namespace std {
template<>
struct hash<j2me::bluetooth::BluetoothUUID> {
    size_t operator()(const j2me::bluetooth::BluetoothUUID& uuid) const noexcept {
        const auto& b = uuid.getBytes();
        size_t h = 0;
        for (size_t i = 0; i < 16; i += sizeof(size_t)) {
            size_t chunk = 0;
            for (size_t j = 0; j < sizeof(size_t) && (i + j) < 16; ++j) {
                chunk = (chunk << 8) | b[i + j];
            }
            h ^= chunk + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};
} // namespace std
