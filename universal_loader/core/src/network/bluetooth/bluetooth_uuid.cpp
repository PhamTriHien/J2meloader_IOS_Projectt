#include "bluetooth_uuid.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace j2me {
namespace bluetooth {

// Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB
const std::array<uint8_t, 16> BluetoothUUID::BASE_UUID = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x10, 0x00,
    0x80, 0x00,
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
};

BluetoothUUID::BluetoothUUID() {
    bytes_.fill(0);
}

BluetoothUUID::BluetoothUUID(uint32_t shortValue) {
    bytes_ = BASE_UUID;
    bytes_[0] = static_cast<uint8_t>((shortValue >> 24) & 0xFF);
    bytes_[1] = static_cast<uint8_t>((shortValue >> 16) & 0xFF);
    bytes_[2] = static_cast<uint8_t>((shortValue >> 8) & 0xFF);
    bytes_[3] = static_cast<uint8_t>(shortValue & 0xFF);
}

uint8_t BluetoothUUID::hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    return 0;
}

BluetoothUUID::BluetoothUUID(const std::string& uuidString, bool shortUUID) {
    std::string clean;
    for (char c : uuidString) {
        if (c != '-') clean.push_back(c);
    }

    if (clean.empty()) {
        bytes_.fill(0);
        return;
    }

    if (shortUUID) {
        if (clean.length() > 8) {
            clean = clean.substr(0, 8);
        }
        std::string padded(8 - clean.length(), '0');
        padded += clean;

        bytes_ = BASE_UUID;
        for (size_t i = 0; i < 4; ++i) {
            bytes_[i] = (hexNibble(padded[i * 2]) << 4) | hexNibble(padded[i * 2 + 1]);
        }
    } else {
        if (clean.length() < 32) {
            std::string padded(32 - clean.length(), '0');
            clean = padded + clean;
        } else if (clean.length() > 32) {
            clean = clean.substr(0, 32);
        }

        for (size_t i = 0; i < 16; ++i) {
            bytes_[i] = (hexNibble(clean[i * 2]) << 4) | hexNibble(clean[i * 2 + 1]);
        }
    }
}

BluetoothUUID::BluetoothUUID(const std::array<uint8_t, 16>& rawBytes)
    : bytes_(rawBytes) {}

BluetoothUUID::BluetoothUUID(const uint8_t* rawBytes) {
    if (rawBytes) {
        std::memcpy(bytes_.data(), rawBytes, 16);
    } else {
        bytes_.fill(0);
    }
}

bool BluetoothUUID::isShortUUID() const {
    for (size_t i = 4; i < 16; ++i) {
        if (bytes_[i] != BASE_UUID[i]) return false;
    }
    return true;
}

uint32_t BluetoothUUID::getShortValue() const {
    return (static_cast<uint32_t>(bytes_[0]) << 24) |
           (static_cast<uint32_t>(bytes_[1]) << 16) |
           (static_cast<uint32_t>(bytes_[2]) << 8) |
           static_cast<uint32_t>(bytes_[3]);
}

std::string BluetoothUUID::toHex32() const {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < 16; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes_[i]);
    }
    return ss.str();
}

std::string BluetoothUUID::toString() const {
    std::string hex = toHex32();
    // Trim leading zeros, but keep at least 1 digit
    size_t firstNonZero = hex.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return "0";
    }
    return hex.substr(firstNonZero);
}

std::string BluetoothUUID::toShortHexString() const {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << getShortValue();
    return ss.str();
}

std::string BluetoothUUID::toCanonicalString() const {
    std::string hex = toHex32();
    // Format 8-4-4-4-12
    return hex.substr(0, 8) + "-" +
           hex.substr(8, 4) + "-" +
           hex.substr(12, 4) + "-" +
           hex.substr(16, 4) + "-" +
           hex.substr(20, 12);
}

} // namespace bluetooth
} // namespace j2me
