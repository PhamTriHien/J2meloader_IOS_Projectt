#include "gcf_network.h"
#include "datagram_connection.h"
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <limits>
#include <cerrno>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#endif

namespace j2me {

void NetworkSocket::ensurePlatformNetInit() {
#if defined(_WIN32) || defined(_WIN64)
    static bool s_initialized = false;
    static std::mutex s_initMutex;
    std::lock_guard<std::mutex> lock(s_initMutex);
    if (!s_initialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        s_initialized = true;
    }
#endif
}

NetworkSocket::NetworkSocket() {
    ensurePlatformNetInit();
}

NetworkSocket::~NetworkSocket() {
    close();
}

void NetworkSocket::close() {
    // Wake blocked operations before waiting for them; close the descriptor only afterwards.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected.store(false);
#if defined(_WIN32) || defined(_WIN64)
        if (m_sock != static_cast<uintptr_t>(~0)) ::shutdown(static_cast<SOCKET>(m_sock), SD_BOTH);
#else
        if (m_sock >= 0) ::shutdown(m_sock, SHUT_RDWR);
#endif
    }
    std::scoped_lock operations(m_readMutex, m_writeMutex);
    std::lock_guard<std::mutex> lock(m_mutex);
#if defined(_WIN32) || defined(_WIN64)
    if (m_sock != static_cast<uintptr_t>(~0)) {
        closesocket(static_cast<SOCKET>(m_sock));
        m_sock = static_cast<uintptr_t>(~0);
    }
#else
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
#endif
    m_connected.store(false);
}

bool NetworkSocket::connect(const std::string& host, int port, int timeoutMs) {
    close();
    std::lock_guard<std::mutex> lock(m_mutex);

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        return false;
    }

#if defined(_WIN32) || defined(_WIN64)
    SOCKET sock = INVALID_SOCKET;
#else
    int sock = -1;
#endif

    for (auto* ai = res; ai != nullptr; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
#if defined(_WIN32) || defined(_WIN64)
        if (sock == INVALID_SOCKET) continue;
        u_long mode = 1; // Non-blocking
        ioctlsocket(sock, FIONBIO, &mode);
        int rc = ::connect(sock, ai->ai_addr, (int)ai->ai_addrlen);
        if (rc == 0) break;
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set ws;
            FD_ZERO(&ws);
            FD_SET(sock, &ws);
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            if (select(0, nullptr, &ws, nullptr, &tv) > 0) {
                int err = 0;
                int el = sizeof(err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
                if (err == 0) break;
            }
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
#else
        if (sock < 0) continue;
        if (sock >= FD_SETSIZE) { ::close(sock); sock = -1; continue; }
        int fl = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, fl | O_NONBLOCK);
        int rc = ::connect(sock, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) break;
        if (errno == EINPROGRESS) {
            fd_set ws;
            FD_ZERO(&ws);
            FD_SET(sock, &ws);
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            if (select(sock + 1, nullptr, &ws, nullptr, &tv) > 0) {
                int err = 0;
                socklen_t el = sizeof(err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &el);
                if (err == 0) break;
            }
        }
        ::close(sock);
        sock = -1;
#endif
    }
    freeaddrinfo(res);

#if defined(_WIN32) || defined(_WIN64)
    if (sock == INVALID_SOCKET) return false;
    m_sock = static_cast<uintptr_t>(sock);

    // Kích hoạt TCP_NODELAY & SO_KEEPALIVE để triệt tiêu độ trễ gói tin game
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&one, sizeof(one));
    tcp_keepalive keepalive{1, 60000, 10000};
    DWORD returned = 0;
    WSAIoctl(sock, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), nullptr, 0, &returned, nullptr, nullptr);
#else
    if (sock < 0) return false;
    m_sock = sock;

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    int idle = 60;
    int interval = 10;
    int probes = 3;
#ifdef TCP_KEEPIDLE
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#elif defined(TCP_KEEPALIVE)
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
#endif
#ifdef TCP_KEEPCNT
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
#endif
#endif

    m_connected.store(true);
    return true;
}

// send/recv only hold m_mutex to read the handle: a reader blocked in select() must not starve writers
int NetworkSocket::send(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> writer(m_writeMutex);
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_connected.load() || !data || length == 0) return -1;
    const auto sock = m_sock;
    lock.unlock();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const int count = static_cast<int>((std::min)(length, static_cast<size_t>((std::numeric_limits<int>::max)())));
    for (;;) {
        if (!m_connected.load()) return -1;
#if defined(_WIN32) || defined(_WIN64)
        const SOCKET s = static_cast<SOCKET>(sock);
        int result = ::send(s, reinterpret_cast<const char*>(data), count, 0);
        const int error = result < 0 ? WSAGetLastError() : 0;
        const bool retry = error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
        const int s = sock;
#ifdef MSG_NOSIGNAL
        int result = ::send(s, data, count, MSG_NOSIGNAL);
#else
        int result = ::send(s, data, count, 0);
#endif
        const bool retry = result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
        if (result > 0) return result;
        if (!retry || std::chrono::steady_clock::now() >= deadline) {
            m_connected.store(false);
            return -1;
        }
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(s, &writable);
        timeval timeout{0, 50000};
#if defined(_WIN32) || defined(_WIN64)
        select(0, nullptr, &writable, nullptr, &timeout);
#else
        select(s + 1, nullptr, &writable, nullptr, &timeout);
#endif
    }
}

int NetworkSocket::recv(uint8_t* buffer, size_t maxLength, int timeoutMs) {
    std::lock_guard<std::mutex> reader(m_readMutex);
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_connected.load() || !buffer || maxLength == 0) return -1;
    const auto sock = m_sock;
    lock.unlock();

#if defined(_WIN32) || defined(_WIN64)
    SOCKET s = static_cast<SOCKET>(sock);
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(s, &rs);
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(0, &rs, nullptr, nullptr, &tv);
    if (sel == 0 || (sel < 0 && WSAGetLastError() == WSAEINTR)) return 0;
    if (sel < 0) { m_connected.store(false); return -1; }

    int bytes = ::recv(s, reinterpret_cast<char*>(buffer), static_cast<int>((std::min)(maxLength, static_cast<size_t>((std::numeric_limits<int>::max)()))), 0);
    if (bytes < 0 && (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR)) return 0;
    if (bytes <= 0) {
        m_connected.store(false);
        return -1;
    }
    return bytes;
#else
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(sock, &rs);
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(sock + 1, &rs, nullptr, nullptr, &tv);
    if (sel == 0 || (sel < 0 && errno == EINTR)) return 0;
    if (sel < 0) { m_connected.store(false); return -1; }

    int bytes = ::recv(sock, buffer, maxLength, 0);
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 0;
    if (bytes <= 0) {
        m_connected.store(false);
        return -1;
    }
    return bytes;
#endif
}

int NetworkSocket::available() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected.load()) return 0;

#if defined(_WIN32) || defined(_WIN64)
    u_long bytes = 0;
    if (ioctlsocket(static_cast<SOCKET>(m_sock), FIONREAD, &bytes) == 0) {
        return (int)bytes;
    }
#else
    int bytes = 0;
    if (ioctl(m_sock, FIONREAD, &bytes) == 0) {
        return bytes;
    }
#endif
    return 0;
}

bool NetworkSocket::readFully(uint8_t* buffer, size_t length, int timeoutMs) {
    size_t total = 0;
    auto start = std::chrono::steady_clock::now();

    while (total < length) {
        int r = recv(buffer + total, length - total, 20);
        if (r < 0) return false;
        total += r;

        auto now = std::chrono::steady_clock::now();
        auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (el > timeoutMs) return false;

        if (r == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    return true;
}

// ============================================================================
// Socket Streams
// ============================================================================

SocketInputStream::SocketInputStream(std::shared_ptr<NetworkSocket> socket)
    : m_socket(socket) {}

int SocketInputStream::read() {
    uint8_t b = 0;
    if (m_socket && m_socket->recv(&b, 1, 50) == 1) {
        return b;
    }
    return -1;
}

int SocketInputStream::read(uint8_t* b, size_t offset, size_t length) {
    if (!m_socket || !b || length == 0) return 0;
    return m_socket->recv(b + offset, length, 50);
}

bool SocketInputStream::readFully(uint8_t* b, size_t length) {
    if (!m_socket) return false;
    return m_socket->readFully(b, length);
}

int SocketInputStream::available() {
    return m_socket ? m_socket->available() : 0;
}

void SocketInputStream::close() {
    if (m_socket) m_socket->close();
}

SocketOutputStream::SocketOutputStream(std::shared_ptr<NetworkSocket> socket)
    : m_socket(socket) {}

void SocketOutputStream::write(uint8_t b) {
    write(&b, 0, 1);
}

void SocketOutputStream::write(const uint8_t* b, size_t offset, size_t length) {
    if (m_socket && b && length > 0) {
        size_t sent = 0;
        while (sent < length) {
            const int count = m_socket->send(b + offset + sent, length - sent);
            if (count <= 0) throw std::runtime_error("Socket write failed");
            sent += static_cast<size_t>(count);
        }
    }
}

void SocketOutputStream::flush() {}
void SocketOutputStream::close() {
    if (m_socket) m_socket->close();
}

// ============================================================================
// SocketConnection
// ============================================================================

SocketConnection::SocketConnection(const std::string& host, int port)
    : m_host(host), m_port(port) {
    m_socket = std::make_shared<NetworkSocket>();
}

SocketConnection::~SocketConnection() {
    close();
}

bool SocketConnection::open(int timeoutMs) {
    return m_socket->connect(m_host, m_port, timeoutMs);
}

void SocketConnection::close() {
    if (m_socket) m_socket->close();
}

std::shared_ptr<SocketInputStream> SocketConnection::openInputStream() {
    if (!m_inStream) {
        m_inStream = std::make_shared<SocketInputStream>(m_socket);
    }
    return m_inStream;
}

std::shared_ptr<SocketOutputStream> SocketConnection::openOutputStream() {
    if (!m_outStream) {
        m_outStream = std::make_shared<SocketOutputStream>(m_socket);
    }
    return m_outStream;
}

// ============================================================================
// HttpConnection
// ============================================================================

HttpConnection::HttpConnection(const std::string& url)
    : m_url(url) {}

HttpConnection::~HttpConnection() {}

void HttpConnection::setRequestProperty(const std::string& key, const std::string& value) {
    m_headers[key] = value;
}

bool HttpConnection::executeRequest() {
    if (m_executed) return true;
    m_executed = true;

    // Phân tích cú pháp URL http://host:port/path
    std::string proto = "http://";
    std::string remain = m_url;
    if (remain.rfind(proto, 0) == 0) {
        remain = remain.substr(proto.size());
    }

    size_t slashPos = remain.find('/');
    std::string hostPort = (slashPos != std::string::npos) ? remain.substr(0, slashPos) : remain;
    std::string path = (slashPos != std::string::npos) ? remain.substr(slashPos) : "/";

    std::string host = hostPort;
    int port = 80;
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = hostPort.substr(0, colonPos);
        port = std::stoi(hostPort.substr(colonPos + 1));
    }

    NetworkSocket sock;
    if (!sock.connect(host, port, 5000)) {
        m_responseCode = 504; // Gateway Timeout
        return false;
    }

    // Soạn HTTP Request
    std::ostringstream req;
    req << m_method << " " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "User-Agent: J2ME-Loader/2.0 (Universal)\r\n";
    req << "Connection: close\r\n";

    for (const auto& h : m_headers) {
        req << h.first << ": " << h.second << "\r\n";
    }
    req << "\r\n";

    std::string reqStr = req.str();
    sock.send(reinterpret_cast<const uint8_t*>(reqStr.data()), reqStr.size());

    // Nhận HTTP Response
    std::string responseData;
    uint8_t chunk[2048];
    while (true) {
        int r = sock.recv(chunk, sizeof(chunk), 2000);
        if (r <= 0) break;
        responseData.append(reinterpret_cast<char*>(chunk), r);
    }

    // Phân tích mã phản hồi HTTP: HTTP/1.1 200 OK
    size_t lineEnd = responseData.find("\r\n");
    if (lineEnd != std::string::npos) {
        std::string statusLine = responseData.substr(0, lineEnd);
        std::istringstream ss(statusLine);
        std::string httpVer;
        ss >> httpVer >> m_responseCode;
    } else {
        m_responseCode = 500;
    }

    size_t bodyPos = responseData.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        m_responseBody = responseData.substr(bodyPos + 4);
    }

    return true;
}

int HttpConnection::getResponseCode() {
    executeRequest();
    return m_responseCode;
}

std::string HttpConnection::getHeaderField(const std::string& name) {
    executeRequest();
    auto it = m_responseHeaders.find(name);
    return (it != m_responseHeaders.end()) ? it->second : "";
}

std::string HttpConnection::getResponseBody() {
    executeRequest();
    return m_responseBody;
}

// ============================================================================
// GcfConnector Triển Khai
// ============================================================================

std::shared_ptr<SocketConnection> GcfConnector::openSocket(const std::string& url) {
    // socket://host:port
    std::string prefix = "socket://";
    std::string hp = (url.rfind(prefix, 0) == 0) ? url.substr(prefix.size()) : url;

    size_t colon = hp.rfind(':');
    if (colon == std::string::npos) return nullptr;

    std::string host = hp.substr(0, colon);
    int port = 0;
    try {
        port = std::stoi(hp.substr(colon + 1));
    } catch (...) {
        return nullptr;
    }

    return std::make_shared<SocketConnection>(host, port);
}

std::shared_ptr<HttpConnection> GcfConnector::openHttp(const std::string& url) {
    return std::make_shared<HttpConnection>(url);
}

std::shared_ptr<DatagramConnection> GcfConnector::openDatagram(const std::string& url) {
    auto conn = std::make_shared<DatagramConnection>();
    if (conn->open(url)) {
        return conn;
    }
    return nullptr;
}

} // namespace j2me
