#pragma once

#include "bluetooth_types.h"
#include "bluetooth_uuid.h"
#include "bluetooth_device.h"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace j2me {
namespace bluetooth {

struct J2ME_API SPPUrlParams {
    std::string host; // "localhost" or MAC/IP
    BluetoothUUID uuid;
    std::string serviceName;
    bool authenticate = false;
    bool encrypt = false;
    bool master = false;
    bool skipAfterWrite = false;
    uint16_t port = 0;
    bool isServer = false;

    static SPPUrlParams parse(const std::string& url);
};

class J2ME_API SPPConnectionImpl {
public:
    explicit SPPConnectionImpl(uintptr_t rawSocket, bool skipAfterWrite = false);
    ~SPPConnectionImpl();

    int read(uint8_t* buffer, size_t maxLen, int timeoutMs = 1000);
    int write(const uint8_t* buffer, size_t len);
    int available();
    bool readFully(uint8_t* buffer, size_t len, int timeoutMs = 3000);
    void close();

    bool isConnected() const { return connected_; }
    bool isSkipAfterWrite() const { return skipAfterWrite_; }

private:
    uintptr_t sock_{static_cast<uintptr_t>(~0)};
    bool skipAfterWrite_ = false;
    std::atomic<bool> connected_{false};
    mutable std::mutex mutex_;
};

class J2ME_API SPPConnectionNotifier {
public:
    explicit SPPConnectionNotifier(const SPPUrlParams& params);
    ~SPPConnectionNotifier();

    bool start();
    std::shared_ptr<SPPConnectionImpl> acceptAndOpen(int timeoutMs = 5000);
    void close();

    uint16_t getPort() const { return port_; }
    const BluetoothUUID& getUUID() const { return params_.uuid; }
    const std::string& getServiceName() const { return params_.serviceName; }
    bool isListening() const { return listening_; }
    std::string getConnectionURL() const;

private:
    SPPUrlParams params_;
    uintptr_t listenSock_{static_cast<uintptr_t>(~0)};
    uint16_t port_ = 0;
    std::atomic<bool> listening_{false};
    mutable std::mutex mutex_;
};

// Factory helper
J2ME_API std::shared_ptr<SPPConnectionNotifier> openBtsppServer(const std::string& url);
J2ME_API std::shared_ptr<SPPConnectionImpl> openBtsppClient(const std::string& url, int timeoutMs = 5000);

} // namespace bluetooth
} // namespace j2me
