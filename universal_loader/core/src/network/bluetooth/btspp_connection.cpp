#include "btspp_connection.h"
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

// Global registry of active server ports by UUID for local loopback dispatch
static std::mutex s_portMutex;
static std::map<BluetoothUUID, uint16_t> s_activeServerPorts;

void registerActiveServerPort(const BluetoothUUID& uuid, uint16_t port) {
    std::lock_guard<std::mutex> lock(s_portMutex);
    s_activeServerPorts[uuid] = port;
}

void unregisterActiveServerPort(const BluetoothUUID& uuid) {
    std::lock_guard<std::mutex> lock(s_portMutex);
    s_activeServerPorts.erase(uuid);
}

uint16_t getActiveServerPort(const BluetoothUUID& uuid) {
    std::lock_guard<std::mutex> lock(s_portMutex);
    auto it = s_activeServerPorts.find(uuid);
    if (it != s_activeServerPorts.end()) {
        return it->second;
    }
    return 0;
}

// SPPUrlParams
SPPUrlParams SPPUrlParams::parse(const std::string& url) {
    SPPUrlParams params;
    std::string prefix = "btspp://";
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

    // Parse options
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
                } else if (key == "authenticate") {
                    params.authenticate = (val == "true");
                } else if (key == "encrypt") {
                    params.encrypt = (val == "true");
                } else if (key == "master") {
                    params.master = (val == "true");
                } else if (key == "skipAfterWrite") {
                    params.skipAfterWrite = (val == "true");
                } else if (key == "port") {
                    params.port = static_cast<uint16_t>(std::stoi(val));
                }
            }
        }
    }

    if (params.port == 0) {
        // Derive port deterministically in range 25000-55000 from UUID
        params.port = static_cast<uint16_t>(25000 + (std::hash<BluetoothUUID>{}(params.uuid) % 30000));
    }

    return params;
}

// SPPConnectionImpl
SPPConnectionImpl::SPPConnectionImpl(uintptr_t rawSocket, bool skipAfterWrite)
    : sock_(rawSocket), skipAfterWrite_(skipAfterWrite), connected_(rawSocket != static_cast<uintptr_t>(~0)) {
    ensurePlatformNetInit();
}

SPPConnectionImpl::~SPPConnectionImpl() {
    close();
}

void SPPConnectionImpl::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) {
        connected_ = false;
        if (sock_ != static_cast<uintptr_t>(~0)) {
            CLOSE_SOCK(static_cast<socket_t>(sock_));
            sock_ = static_cast<uintptr_t>(~0);
        }
    }
}

int SPPConnectionImpl::available() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return 0;

    u_long bytesAvail = 0;
#if defined(_WIN32) || defined(_WIN64)
    ioctlsocket(static_cast<socket_t>(sock_), FIONREAD, &bytesAvail);
#else
    ioctl(static_cast<socket_t>(sock_), FIONREAD, &bytesAvail);
#endif
    return static_cast<int>(bytesAvail);
}

int SPPConnectionImpl::read(uint8_t* buffer, size_t maxLen, int timeoutMs) {
    if (!buffer || maxLen == 0) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return -1;

    socket_t s = static_cast<socket_t>(sock_);
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(s, &readFds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(static_cast<int>(s + 1), &readFds, nullptr, nullptr, (timeoutMs >= 0) ? &tv : nullptr);
    if (sel <= 0) {
        return 0; // Timeout or nothing ready
    }

    int bytesRead = ::recv(s, reinterpret_cast<char*>(buffer), static_cast<int>(maxLen), 0);
    if (bytesRead <= 0) {
        connected_ = false;
        return -1; // Connection closed or error
    }

    return bytesRead;
}

int SPPConnectionImpl::write(const uint8_t* buffer, size_t len) {
    if (!buffer || len == 0) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || sock_ == static_cast<uintptr_t>(~0)) return -1;

    socket_t s = static_cast<socket_t>(sock_);
    int totalSent = 0;
    while (totalSent < static_cast<int>(len)) {
        int sent = ::send(s, reinterpret_cast<const char*>(buffer + totalSent), static_cast<int>(len - totalSent), 0);
        if (sent <= 0) {
            connected_ = false;
            return -1;
        }
        totalSent += sent;
    }

    if (skipAfterWrite_) {
        // Discard any echo data if skipAfterWrite is requested
        u_long bytesAvail = 0;
#if defined(_WIN32) || defined(_WIN64)
        ioctlsocket(s, FIONREAD, &bytesAvail);
#else
        ioctl(s, FIONREAD, &bytesAvail);
#endif
        if (bytesAvail > 0) {
            std::vector<char> discardBuf(bytesAvail);
            ::recv(s, discardBuf.data(), static_cast<int>(bytesAvail), 0);
        }
    }

    return totalSent;
}

bool SPPConnectionImpl::readFully(uint8_t* buffer, size_t len, int timeoutMs) {
    size_t total = 0;
    auto start = std::chrono::steady_clock::now();
    while (total < len && connected_) {
        int r = read(buffer + total, len - total, 100);
        if (r > 0) {
            total += static_cast<size_t>(r);
        } else if (r < 0) {
            return false;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) {
            break;
        }
    }
    return total == len;
}

// SPPConnectionNotifier
SPPConnectionNotifier::SPPConnectionNotifier(const SPPUrlParams& params)
    : params_(params), port_(params.port) {
    ensurePlatformNetInit();
}

SPPConnectionNotifier::~SPPConnectionNotifier() {
    close();
}

void SPPConnectionNotifier::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (listening_) {
        listening_ = false;
        unregisterActiveServerPort(params_.uuid);
        LocalDevice::getInstance().unregisterService(params_.uuid);
        if (listenSock_ != static_cast<uintptr_t>(~0)) {
            CLOSE_SOCK(static_cast<socket_t>(listenSock_));
            listenSock_ = static_cast<uintptr_t>(~0);
        }
    }
}

bool SPPConnectionNotifier::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (listening_) return true;

    socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCK) {
        return false;
    }

    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCK_ERR) {
        // Try fallback port binding on 0 (ephemeral port)
        addr.sin_port = 0;
        if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCK_ERR) {
            CLOSE_SOCK(s);
            return false;
        }
    }

    // Retrieve actual bound port
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

    // Register active port in registry and local device service records
    registerActiveServerPort(params_.uuid, port_);
    BluetoothServiceRecord srvRec("localhost", params_.uuid, false, params_.skipAfterWrite);
    if (!params_.serviceName.empty()) {
        srvRec.setServiceName(params_.serviceName);
    }
    LocalDevice::getInstance().registerService(srvRec);

    return true;
}

std::shared_ptr<SPPConnectionImpl> SPPConnectionNotifier::acceptAndOpen(int timeoutMs) {
    if (!listening_ || listenSock_ == static_cast<uintptr_t>(~0)) {
        return nullptr;
    }

    socket_t s = static_cast<socket_t>(listenSock_);
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(s, &readFds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(static_cast<int>(s + 1), &readFds, nullptr, nullptr, (timeoutMs > 0) ? &tv : nullptr);
    if (sel <= 0) {
        return nullptr; // Timeout or error
    }

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    socket_t clientSock = ::accept(s, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientSock == INVALID_SOCK) {
        return nullptr;
    }

    int nodelay = 1;
    setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    return std::make_shared<SPPConnectionImpl>(static_cast<uintptr_t>(clientSock), params_.skipAfterWrite);
}

std::string SPPConnectionNotifier::getConnectionURL() const {
    BluetoothServiceRecord rec("localhost", params_.uuid, false, params_.skipAfterWrite);
    int sec = NOAUTHENTICATE_NOENCRYPT;
    if (params_.authenticate && params_.encrypt) {
        sec = AUTHENTICATE_ENCRYPT;
    } else if (params_.authenticate) {
        sec = AUTHENTICATE_NOENCRYPT;
    }
    return rec.getConnectionURL(sec, params_.master);
}

// Helpers
std::shared_ptr<SPPConnectionNotifier> openBtsppServer(const std::string& url) {
    SPPUrlParams params = SPPUrlParams::parse(url);
    if (!params.isServer) return nullptr;

    auto notifier = std::make_shared<SPPConnectionNotifier>(params);
    if (!notifier->start()) {
        return nullptr;
    }
    return notifier;
}

std::shared_ptr<SPPConnectionImpl> openBtsppClient(const std::string& url, int timeoutMs) {
    ensurePlatformNetInit();
    SPPUrlParams params = SPPUrlParams::parse(url);
    if (params.host.empty()) return nullptr;

    uint16_t targetPort = params.port;

    // Check if host is loopback or local device MAC
    std::string localMac = LocalDevice::getInstance().getBluetoothAddress();
    std::string targetHost = "127.0.0.1";

    if (params.host == "localhost" || params.host == localMac || params.host == "127.0.0.1") {
        uint16_t activePort = getActiveServerPort(params.uuid);
        if (activePort != 0) {
            targetPort = activePort;
        }
    } else {
        // If it's a numeric IP or domain, use it; otherwise loopback
        targetHost = params.host;
    }

    socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCK) return nullptr;

    // Set non-blocking for connect timeout
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

    // Resolve address
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

    // Set back to blocking
#if defined(_WIN32) || defined(_WIN64)
    nonblock = 0;
    ioctlsocket(s, FIONBIO, &nonblock);
#else
    fcntl(s, F_SETFL, flags);
#endif

    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    return std::make_shared<SPPConnectionImpl>(static_cast<uintptr_t>(s), params.skipAfterWrite);
}

} // namespace bluetooth
} // namespace j2me
