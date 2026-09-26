#include "btl2cap_connection.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
constexpr int SOCK_ERR = SOCKET_ERROR;
#define CLOSE_SOCK(s) ::closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
constexpr int SOCK_ERR = -1;
#define CLOSE_SOCK(s) ::close(s)
#endif

namespace j2me {
namespace bluetooth {

static void ensurePlatformNetInit() {
#if defined(_WIN32) || defined(_WIN64)
    static bool s_initialized = false;
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        s_initialized = true;
    }
#endif
}

// Global registry of active L2CAP server ports by UUID
static std::mutex s_l2capPortMutex;
static std::map<BluetoothUUID, uint16_t> s_activeL2capServerPorts;

void registerActiveL2capPort(const BluetoothUUID& uuid, uint16_t port) {
    std::lock_guard<std::mutex> lock(s_l2capPortMutex);
    s_activeL2capServerPorts[uuid] = port;
}

void unregisterActiveL2capPort(const BluetoothUUID& uuid) {
    std::lock_guard<std::mutex> lock(s_l2capPortMutex);
    s_activeL2capServerPorts.erase(uuid);
}

uint16_t getActiveL2capPort(const BluetoothUUID& uuid) {
    std::lock_guard<std::mutex> lock(s_l2capPortMutex);
    auto it = s_activeL2capServerPorts.find(uuid);
    if (it != s_activeL2capServerPorts.end()) {
        return it->second;
    }
    return 0;
}

// L2CAPUrlParams
L2CAPUrlParams L2CAPUrlParams::parse(const std::string& url) {
    L2CAPUrlParams params;
    std::string prefix = "btl2cap://";
    if (url.rfind(prefix, 0) != 0) {
        return params;
    }

    std::string rest = url.substr(prefix.length());
    size_t colonPos = rest.find(':');
    if (colonPos == std::string::npos) {
        return params;
    }

    params.host = rest.substr(0, colonPos);
    params.isServer = (params.host == "localhost");

    std::string afterColon = rest.substr(colonPos + 1);
    size_t semiPos = afterColon.find(';');

    std::string uuidStr;
    if (semiPos == std::string::npos) {
        uuidStr = afterColon;
    } else {
        uuidStr = afterColon.substr(0, semiPos);
    }

    params.uuid = BluetoothUUID(uuidStr, uuidStr.length() <= 8);

    if (semiPos != std::string::npos) {
        std::string options = afterColon.substr(semiPos + 1);
        std::istringstream optStream(options);
        std::string token;
        while (std::getline(optStream, token, ';')) {
            size_t eqPos = token.find('=');
            if (eqPos != std::string::npos) {
                std::string key = token.substr(0, eqPos);
                std::string val = token.substr(eqPos + 1);

                if (key == "name") {
                    params.serviceName = val;
                } else if (key == "ReceiveMTU") {
                    params.receiveMtu = std::stoi(val);
                } else if (key == "TransmitMTU") {
                    params.transmitMtu = std::stoi(val);
                } else if (key == "authenticate") {
                    params.authenticate = (val == "true");
                } else if (key == "encrypt") {
                    params.encrypt = (val == "true");
                } else if (key == "master") {
                    params.master = (val == "true");
                } else if (key == "port") {
                    params.port = static_cast<uint16_t>(std::stoi(val));
                }
            }
        }
    }

    if (params.port == 0) {
        // Derive port in range 30000-58000
        params.port = static_cast<uint16_t>(30000 + (std::hash<BluetoothUUID>{}(params.uuid) % 28000));
    }

    return params;
}

// L2CAPConnectionImpl
L2CAPConnectionImpl::L2CAPConnectionImpl(uintptr_t rawSocket, int receiveMtu, int transmitMtu)
    : sock_(rawSocket), receiveMtu_(receiveMtu), transmitMtu_(transmitMtu),
      connected_(rawSocket != static_cast<uintptr_t>(~0)) {
    ensurePlatformNetInit();
}

L2CAPConnectionImpl::~L2CAPConnectionImpl() {
    close();
}

void L2CAPConnectionImpl::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) {
        connected_ = false;
        if (sock_ != static_cast<uintptr_t>(~0)) {
            CLOSE_SOCK(static_cast<socket_t>(sock_));
            sock_ = static_cast<uintptr_t>(~0);
        }
    }
}

bool L2CAPConnectionImpl::ready() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return false;

    u_long bytesAvail = 0;
#if defined(_WIN32) || defined(_WIN64)
    ioctlsocket(static_cast<socket_t>(sock_), FIONREAD, &bytesAvail);
#else
    ioctl(static_cast<socket_t>(sock_), FIONREAD, &bytesAvail);
#endif
    return bytesAvail >= 2;
}

bool L2CAPConnectionImpl::send(const uint8_t* data, size_t len) {
    if (!data && len > 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return false;

    // Truncate to transmit MTU as per specification
    if (static_cast<int>(len) > transmitMtu_) {
        len = static_cast<size_t>(transmitMtu_);
    }

    // Packet framing: 2 bytes length (big endian) + payload
    uint16_t payloadLen = static_cast<uint16_t>(len);
    uint8_t header[2];
    header[0] = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);
    header[1] = static_cast<uint8_t>(payloadLen & 0xFF);

    socket_t s = static_cast<socket_t>(sock_);
    if (::send(s, reinterpret_cast<const char*>(header), 2, 0) != 2) {
        connected_ = false;
        return false;
    }

    if (len > 0) {
        size_t total = 0;
        while (total < len) {
            int sent = ::send(s, reinterpret_cast<const char*>(data + total), static_cast<int>(len - total), 0);
            if (sent <= 0) {
                connected_ = false;
                return false;
            }
            total += static_cast<size_t>(sent);
        }
    }
    return true;
}

int L2CAPConnectionImpl::receive(uint8_t* inBuf, size_t inBufLen, int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return -1;

    socket_t s = static_cast<socket_t>(sock_);

    // Read 2-byte header
    uint8_t header[2];
    size_t headerBytes = 0;
    auto start = std::chrono::steady_clock::now();

    while (headerBytes < 2) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(s, &readFds);

        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int sel = select(static_cast<int>(s + 1), &readFds, nullptr, nullptr, (timeoutMs >= 0) ? &tv : nullptr);
        if (sel <= 0) return 0; // Timeout

        int r = ::recv(s, reinterpret_cast<char*>(header + headerBytes), static_cast<int>(2 - headerBytes), 0);
        if (r <= 0) {
            connected_ = false;
            return -1;
        }
        headerBytes += static_cast<size_t>(r);
    }

    uint16_t packetLen = (static_cast<uint16_t>(header[0]) << 8) | static_cast<uint16_t>(header[1]);
    if (packetLen == 0) {
        return 0;
    }

    // Read payload
    std::vector<uint8_t> tempBuf(packetLen);
    size_t payloadRead = 0;

    while (payloadRead < packetLen) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(s, &readFds);

        timeval tv{};
        tv.tv_sec = 2; // 2 seconds timeout for remaining payload
        tv.tv_usec = 0;

        int sel = select(static_cast<int>(s + 1), &readFds, nullptr, nullptr, &tv);
        if (sel <= 0) break;

        int r = ::recv(s, reinterpret_cast<char*>(tempBuf.data() + payloadRead), static_cast<int>(packetLen - payloadRead), 0);
        if (r <= 0) {
            connected_ = false;
            return -1;
        }
        payloadRead += static_cast<size_t>(r);
    }

    // Truncate to inBufLen as per JSR-82 spec
    size_t copyBytes = (static_cast<size_t>(packetLen) < inBufLen) ? static_cast<size_t>(packetLen) : inBufLen;
    if (inBuf && copyBytes > 0) {
        std::memcpy(inBuf, tempBuf.data(), copyBytes);
    }

    return static_cast<int>(copyBytes);
}

// L2CAPConnectionNotifier
L2CAPConnectionNotifier::L2CAPConnectionNotifier(const L2CAPUrlParams& params)
    : params_(params), port_(params.port) {
    ensurePlatformNetInit();
}

L2CAPConnectionNotifier::~L2CAPConnectionNotifier() {
    close();
}

void L2CAPConnectionNotifier::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (listening_) {
        listening_ = false;
        unregisterActiveL2capPort(params_.uuid);
        LocalDevice::getInstance().unregisterService(params_.uuid);
        if (listenSock_ != static_cast<uintptr_t>(~0)) {
            CLOSE_SOCK(static_cast<socket_t>(listenSock_));
            listenSock_ = static_cast<uintptr_t>(~0);
        }
    }
}

bool L2CAPConnectionNotifier::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (listening_) return true;

    socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCK) return false;

    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCK_ERR) {
        addr.sin_port = 0;
        if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCK_ERR) {
            CLOSE_SOCK(s);
            return false;
        }
    }

    sockaddr_in boundAddr{};
    socklen_t addrLen = sizeof(boundAddr);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&boundAddr), &addrLen) == 0) {
        port_ = ntohs(boundAddr.sin_port);
    }

    if (::listen(s, 5) == SOCK_ERR) {
        CLOSE_SOCK(s);
        return false;
    }

    listenSock_ = static_cast<uintptr_t>(s);
    listening_ = true;

    registerActiveL2capPort(params_.uuid, port_);
    BluetoothServiceRecord srvRec("localhost", params_.uuid, true, false);
    if (!params_.serviceName.empty()) {
        srvRec.setServiceName(params_.serviceName);
    }
    LocalDevice::getInstance().registerService(srvRec);

    return true;
}

std::shared_ptr<L2CAPConnectionImpl> L2CAPConnectionNotifier::acceptAndOpen(int timeoutMs) {
    if (!listening_ || listenSock_ == static_cast<uintptr_t>(~0)) return nullptr;

    socket_t s = static_cast<socket_t>(listenSock_);
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(s, &readFds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(static_cast<int>(s + 1), &readFds, nullptr, nullptr, (timeoutMs > 0) ? &tv : nullptr);
    if (sel <= 0) return nullptr;

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    socket_t clientSock = ::accept(s, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientSock == INVALID_SOCK) return nullptr;

    int nodelay = 1;
    setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    return std::make_shared<L2CAPConnectionImpl>(static_cast<uintptr_t>(clientSock), params_.receiveMtu, params_.transmitMtu);
}

std::string L2CAPConnectionNotifier::getConnectionURL() const {
    BluetoothServiceRecord rec("localhost", params_.uuid, true, false);
    int sec = NOAUTHENTICATE_NOENCRYPT;
    if (params_.authenticate && params_.encrypt) {
        sec = AUTHENTICATE_ENCRYPT;
    } else if (params_.authenticate) {
        sec = AUTHENTICATE_NOENCRYPT;
    }
    return rec.getConnectionURL(sec, params_.master);
}

// Helpers
std::shared_ptr<L2CAPConnectionNotifier> openBtl2capServer(const std::string& url) {
    L2CAPUrlParams params = L2CAPUrlParams::parse(url);
    if (!params.isServer) return nullptr;

    auto notifier = std::make_shared<L2CAPConnectionNotifier>(params);
    if (!notifier->start()) return nullptr;
    return notifier;
}

std::shared_ptr<L2CAPConnectionImpl> openBtl2capClient(const std::string& url, int timeoutMs) {
    ensurePlatformNetInit();
    L2CAPUrlParams params = L2CAPUrlParams::parse(url);
    if (params.host.empty()) return nullptr;

    uint16_t targetPort = params.port;
    std::string localMac = LocalDevice::getInstance().getBluetoothAddress();
    std::string targetHost = "127.0.0.1";

    if (params.host == "localhost" || params.host == localMac || params.host == "127.0.0.1") {
        uint16_t activePort = getActiveL2capPort(params.uuid);
        if (activePort != 0) {
            targetPort = activePort;
        }
    } else {
        targetHost = params.host;
    }

    socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCK) return nullptr;

#if defined(_WIN32) || defined(_WIN64)
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(targetPort);

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(targetHost.c_str(), nullptr, &hints, &res) == 0 && res) {
        sockaddr_in* in = reinterpret_cast<sockaddr_in*>(res->ai_addr);
        serverAddr.sin_addr = in->sin_addr;
        freeaddrinfo(res);
    } else {
        serverAddr.sin_addr.s_addr = inet_addr(targetHost.c_str());
    }

    int connRes = ::connect(s, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (connRes == SOCK_ERR) {
        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(s, &writeFds);

        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int sel = select(static_cast<int>(s + 1), nullptr, &writeFds, nullptr, &tv);
        if (sel <= 0) {
            CLOSE_SOCK(s);
            return nullptr;
        }

        int sockError = 0;
        socklen_t len = sizeof(sockError);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&sockError), &len);
        if (sockError != 0) {
            CLOSE_SOCK(s);
            return nullptr;
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    nonblock = 0;
    ioctlsocket(s, FIONBIO, &nonblock);
#else
    fcntl(s, F_SETFL, flags);
#endif

    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    return std::make_shared<L2CAPConnectionImpl>(static_cast<uintptr_t>(s), params.receiveMtu, params.transmitMtu);
}

} // namespace bluetooth
} // namespace j2me
