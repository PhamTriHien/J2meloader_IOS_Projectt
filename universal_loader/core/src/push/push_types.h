#ifndef UNIVERSAL_LOADER_PUSH_TYPES_H
#define UNIVERSAL_LOADER_PUSH_TYPES_H

#include "j2me_core.h"
#include <string>
#include <vector>
#include <cstdint>

namespace universal_loader {
namespace push {

struct PushConnectionRegistration {
    std::string connection; // e.g. "socket://:5000", "sms://:5000", "datagram://:5000"
    std::string midlet;     // class name of MIDlet to launch
    std::string filter;     // allowed sender pattern or "*"
    bool available{false};  // whether incoming connection/message is currently waiting

    bool matchesFilter(const std::string& senderAddress) const {
        if (filter.empty() || filter == "*") {
            return true;
        }
        if (senderAddress.empty()) {
            return false;
        }
        // Prefix wildcard matching: e.g. "192.168.1.*" or "+84*"
        if (filter.back() == '*') {
            std::string prefix = filter.substr(0, filter.size() - 1);
            return senderAddress.rfind(prefix, 0) == 0;
        }
        return senderAddress == filter;
    }
};

struct PushAlarmRegistration {
    std::string midlet;
    int64_t alarmTimeMs{0};
    bool fired{false};
};

} // namespace push
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PUSH_TYPES_H
