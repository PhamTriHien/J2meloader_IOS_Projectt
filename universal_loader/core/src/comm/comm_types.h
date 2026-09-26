#ifndef UNIVERSAL_LOADER_COMM_TYPES_H
#define UNIVERSAL_LOADER_COMM_TYPES_H

#include "j2me_core.h"
#include <string>
#include <vector>
#include <cstdint>

namespace universal_loader {
namespace comm {

enum class CommParity {
    None,
    Even,
    Odd
};

struct CommConfig {
    std::string port{"0"};       // "0", "COM1", "/dev/ttyS0", etc.
    int baudRate{9600};          // Default 9600
    int bitsPerChar{8};          // 7 or 8, default 8
    int stopBits{1};             // 1 or 2, default 1
    CommParity parity{CommParity::None};
    bool autoCts{false};
    bool autoRts{false};
};

class J2ME_API CommUrlParser {
public:
    static bool parse(const std::string& url, CommConfig& outConfig);
};

} // namespace comm
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_COMM_TYPES_H
