#pragma once

#include "bluetooth_types.h"
#include "bluetooth_uuid.h"
#include "bluetooth_device.h"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

namespace j2me {
namespace bluetooth {

struct J2ME_API L2CAPUrlParams {
    std::string host; // "localhost" or MAC/IP
    BluetoothUUID uuid;
    std::string serviceName;
    int receiveMtu = L2CAP_DEFAULT_MTU;
    int transmitMtu = L2CAP_DEFAULT_MTU;
    bool authenticate = false;
    bool encrypt = false;
    bool master = false;
    uint16_t port = 0;
    bool isServer = false;

    static L2CAPUrlParams parse(const std::string& url);
};

class J2ME_API L2CAPConnectionImpl {
public:
    explicit L2CAPConnectionImpl(uintptr_t rawSocket, int receiveMtu = L2CAP_DEFAULT_MTU, int transmitMtu = L2CAP_DEFAULT_MTU);
    ~L2CAPConnectionImpl();

    int getReceiveMTU() const { return receiveMtu_; }
    int getTransmitMTU() const { return transmitMtu_; }

    // Send L2CAP packet (framed with 2-byte length header)
    bool send(const uint8_t* data, size_t len);

    // Receive L2CAP packet into inBuf. Returns number of bytes placed in inBuf, or -1 on error.
    int receive(uint8_t* inBuf, size_t inBufLen, int timeoutMs = 1000);

    bool ready();
    void close();
    bool isConnected() const { return connected_; }

private:
    uintptr_t sock_{static_cast<uintptr_t>(~0)};
    int receiveMtu_;
    int transmitMtu_;
    std::atomic<bool> connected_{false};
    mutable std::mutex mutex_;
};

class J2ME_API L2CAPConnectionNotifier {
public:
    explicit L2CAPConnectionNotifier(const L2CAPUrlParams& params);
    ~L2CAPConnectionNotifier();

    bool start();
    std::shared_ptr<L2CAPConnectionImpl> acceptAndOpen(int timeoutMs = 5000);
    void close();

    uint16_t getPort() const { return port_; }
    const BluetoothUUID& getUUID() const { return params_.uuid; }
    bool isListening() const { return listening_; }
    std::string getConnectionURL() const;

private:
    L2CAPUrlParams params_;
    uintptr_t listenSock_{static_cast<uintptr_t>(~0)};
    uint16_t port_ = 0;
    std::atomic<bool> listening_{false};
    mutable std::mutex mutex_;
};

// Factory helpers
J2ME_API std::shared_ptr<L2CAPConnectionNotifier> openBtl2capServer(const std::string& url);
J2ME_API std::shared_ptr<L2CAPConnectionImpl> openBtl2capClient(const std::string& url, int timeoutMs = 5000);

} // namespace bluetooth
} // namespace j2me
