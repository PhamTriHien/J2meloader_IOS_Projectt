#include "datagram_connection.h"
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

namespace j2me {

// ============================================================================
// Datagram Implementation
// ============================================================================

Datagram::Datagram(int size) {
    if (size < 0) {
        throw std::invalid_argument("Size must not be negative");
    }
    m_data.resize(size, 0);
    m_length = size;
    m_offset = 0;
    m_readPos = 0;
    m_writePos = 0;
}

Datagram::Datagram(int size, const std::string& address)
    : Datagram(size) {
    m_address = address;
}

Datagram::Datagram(const uint8_t* buf, int size) {
    if (size < 0) {
        throw std::invalid_argument("Size must not be negative");
    }
    if (buf && size > 0) {
        m_data.assign(buf, buf + size);
    } else {
        m_data.resize(size, 0);
    }
    m_length = size;
    m_offset = 0;
    m_readPos = 0;
    m_writePos = 0;
}

Datagram::Datagram(const uint8_t* buf, int size, const std::string& address)
    : Datagram(buf, size) {
    m_address = address;
}

void Datagram::setLength(int len) {
    if (len < 0 || m_offset + len > static_cast<int>(m_data.size())) {
        throw std::invalid_argument("Length exceeds buffer bounds");
    }
    m_length = len;
}

void Datagram::setData(const uint8_t* buf, int offset, int length) {
    if (offset < 0 || length < 0) {
        throw std::invalid_argument("Invalid offset or length");
    }
    if (buf && length > 0) {
        m_data.resize(offset + length);
        std::memcpy(m_data.data() + offset, buf, length);
    } else {
        m_data.resize(offset + length, 0);
    }
    m_offset = offset;
    m_length = length;
    m_readPos = offset;
    m_writePos = offset;
}

void Datagram::reset() {
    m_readPos = m_offset;
    m_writePos = m_offset;
}

void Datagram::write(const uint8_t* buf, size_t length) {
    if (!buf || length == 0) return;
    if (m_writePos + length > m_data.size()) {
        m_data.resize(m_writePos + length);
    }
    std::memcpy(m_data.data() + m_writePos, buf, length);
    m_writePos += static_cast<int>(length);
    if (m_writePos - m_offset > m_length) {
        m_length = m_writePos - m_offset;
    }
}

void Datagram::writeByte(int v) {
    uint8_t b = static_cast<uint8_t>(v & 0xFF);
    write(&b, 1);
}

void Datagram::writeShort(int v) {
    uint8_t b[2] = {
        static_cast<uint8_t>((v >> 8) & 0xFF),
        static_cast<uint8_t>(v & 0xFF)
    };
    write(b, 2);
}

void Datagram::writeInt(int v) {
    uint8_t b[4] = {
        static_cast<uint8_t>((v >> 24) & 0xFF),
        static_cast<uint8_t>((v >> 16) & 0xFF),
        static_cast<uint8_t>((v >> 8) & 0xFF),
        static_cast<uint8_t>(v & 0xFF)
    };
    write(b, 4);
}

void Datagram::writeLong(int64_t v) {
    uint8_t b[8] = {
        static_cast<uint8_t>((v >> 56) & 0xFF),
        static_cast<uint8_t>((v >> 48) & 0xFF),
        static_cast<uint8_t>((v >> 40) & 0xFF),
        static_cast<uint8_t>((v >> 32) & 0xFF),
        static_cast<uint8_t>((v >> 24) & 0xFF),
        static_cast<uint8_t>((v >> 16) & 0xFF),
        static_cast<uint8_t>((v >> 8) & 0xFF),
        static_cast<uint8_t>(v & 0xFF)
    };
    write(b, 8);
}

void Datagram::writeUTF(const std::string& str) {
    writeShort(static_cast<int>(str.size()));
    if (!str.empty()) {
        write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }
}

size_t Datagram::read(uint8_t* out, size_t length) {
    if (!out || length == 0) return 0;
    int available = (m_offset + m_length) - m_readPos;
    if (available <= 0) return 0;
    size_t toRead = std::min(length, static_cast<size_t>(available));
    std::memcpy(out, m_data.data() + m_readPos, toRead);
    m_readPos += static_cast<int>(toRead);
    return toRead;
}

uint8_t Datagram::readByte() {
    uint8_t b = 0;
    if (read(&b, 1) != 1) {
        throw std::runtime_error("End of datagram stream");
    }
    return b;
}

int16_t Datagram::readShort() {
    uint8_t b[2];
    if (read(b, 2) != 2) {
        throw std::runtime_error("End of datagram stream");
    }
    return static_cast<int16_t>((b[0] << 8) | b[1]);
}

int32_t Datagram::readInt() {
    uint8_t b[4];
    if (read(b, 4) != 4) {
        throw std::runtime_error("End of datagram stream");
    }
    return static_cast<int32_t>((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}

int64_t Datagram::readLong() {
    uint8_t b[8];
    if (read(b, 8) != 8) {
        throw std::runtime_error("End of datagram stream");
    }
    int64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | b[i];
    }
    return v;
}

std::string Datagram::readUTF() {
    int16_t len = readShort();
    if (len < 0) return "";
    std::string str(len, '\0');
    if (len > 0) {
        if (read(reinterpret_cast<uint8_t*>(&str[0]), len) != static_cast<size_t>(len)) {
            throw std::runtime_error("Premature end of datagram UTF string");
        }
    }
    return str;
}

// ============================================================================
// DatagramConnection Implementation
// ============================================================================

void DatagramConnection::ensurePlatformNetInit() {
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

DatagramConnection::DatagramConnection() {
    ensurePlatformNetInit();
}

DatagramConnection::~DatagramConnection() {
    close();
}

bool DatagramConnection::open(const std::string& url, int timeoutMs) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    close();

    std::string s = url;
    const std::string proto = "datagram://";
    if (s.rfind(proto, 0) == 0) {
        s = s.substr(proto.length());
    }

    std::string host;
    int port = 0;
    auto colonPos = s.find(':');
    if (colonPos != std::string::npos) {
        host = s.substr(0, colonPos);
        std::string portStr = s.substr(colonPos + 1);
        if (!portStr.empty()) {
            port = std::stoi(portStr);
        }
    } else {
        host = s;
    }

#if defined(_WIN32) || defined(_WIN64)
    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return false;
#else
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return false;
#endif

    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in bindAddr;
    std::memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;

    if (host.empty()) {
        // Server mode: datagram://:port
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddr.sin_port = htons(static_cast<uint16_t>(port));
        if (::bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
#if defined(_WIN32) || defined(_WIN64)
            closesocket(sock);
#else
            ::close(sock);
#endif
            return false;
        }
    } else {
        // Client mode: datagram://host:port
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddr.sin_port = 0; // Bind to ephemeral port
        if (::bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
#if defined(_WIN32) || defined(_WIN64)
            closesocket(sock);
#else
            ::close(sock);
#endif
            return false;
        }
        m_defaultAddress = "datagram://" + host + ":" + std::to_string(port);
    }

    sockaddr_in localAddr;
#if defined(_WIN32) || defined(_WIN64)
    int len = sizeof(localAddr);
#else
    socklen_t len = sizeof(localAddr);
#endif
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&localAddr), &len) == 0) {
        m_localPort = ntohs(localAddr.sin_port);
        char ipBuf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &localAddr.sin_addr, ipBuf, sizeof(ipBuf))) {
            m_localAddress = ipBuf;
        } else {
            m_localAddress = "127.0.0.1";
        }
    }

    m_sock = static_cast<uintptr_t>(sock);
    m_open.store(true);
    return true;
}

void DatagramConnection::close() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    m_open.store(false);
}

std::shared_ptr<Datagram> DatagramConnection::newDatagram(int size) {
    return std::make_shared<Datagram>(size, m_defaultAddress);
}

std::shared_ptr<Datagram> DatagramConnection::newDatagram(int size, const std::string& addr) {
    return std::make_shared<Datagram>(size, addr);
}

std::shared_ptr<Datagram> DatagramConnection::newDatagram(const uint8_t* buf, int size) {
    return std::make_shared<Datagram>(buf, size, m_defaultAddress);
}

std::shared_ptr<Datagram> DatagramConnection::newDatagram(const uint8_t* buf, int size, const std::string& addr) {
    return std::make_shared<Datagram>(buf, size, addr);
}

bool DatagramConnection::send(Datagram* dgram) {
    if (!m_open.load() || !dgram) return false;

    std::string target = dgram->getAddress();
    if (target.empty()) {
        target = m_defaultAddress;
    }
    if (target.empty()) return false;

    const std::string proto = "datagram://";
    if (target.rfind(proto, 0) == 0) {
        target = target.substr(proto.length());
    }

    auto colonPos = target.find(':');
    if (colonPos == std::string::npos) return false;

    std::string host = target.substr(0, colonPos);
    std::string portStr = target.substr(colonPos + 1);
    if (portStr.empty()) return false;
    int port = std::stoi(portStr);

    sockaddr_in destAddr;
    std::memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(static_cast<uint16_t>(port));

    addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        return false;
    }
    std::memcpy(&destAddr, res->ai_addr, sizeof(destAddr));
    freeaddrinfo(res);

    int toSend = dgram->getLength();
    const char* sendBuf = reinterpret_cast<const char*>(dgram->getData()) + dgram->getOffset();

#if defined(_WIN32) || defined(_WIN64)
    int sent = ::sendto(static_cast<SOCKET>(m_sock), sendBuf, toSend, 0,
                        reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
#else
    int sent = ::sendto(m_sock, sendBuf, toSend, 0,
                        reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
#endif

    return sent >= 0;
}

bool DatagramConnection::receive(Datagram* dgram, int timeoutMs) {
    if (!m_open.load() || !dgram) return false;

    fd_set readFds;
    FD_ZERO(&readFds);
#if defined(_WIN32) || defined(_WIN64)
    FD_SET(static_cast<SOCKET>(m_sock), &readFds);
#else
    FD_SET(m_sock, &readFds);
#endif

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

#if defined(_WIN32) || defined(_WIN64)
    int selRet = ::select(0, &readFds, nullptr, nullptr, timeoutMs > 0 ? &tv : nullptr);
#else
    int selRet = ::select(m_sock + 1, &readFds, nullptr, nullptr, timeoutMs > 0 ? &tv : nullptr);
#endif
    if (selRet <= 0) return false;

    sockaddr_in srcAddr;
#if defined(_WIN32) || defined(_WIN64)
    int srcLen = sizeof(srcAddr);
#else
    socklen_t srcLen = sizeof(srcAddr);
#endif
    std::memset(&srcAddr, 0, sizeof(srcAddr));

    char* recvBuf = reinterpret_cast<char*>(dgram->getDataMutable()) + dgram->getOffset();
    int maxLen = dgram->getLength();

#if defined(_WIN32) || defined(_WIN64)
    int recvd = ::recvfrom(static_cast<SOCKET>(m_sock), recvBuf, maxLen, 0,
                           reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
#else
    int recvd = ::recvfrom(m_sock, recvBuf, maxLen, 0,
                           reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
#endif
    if (recvd <= 0) return false;

    dgram->setLength(recvd);

    char ipBuf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &srcAddr.sin_addr, ipBuf, sizeof(ipBuf))) {
        dgram->setAddress("datagram://" + std::string(ipBuf) + ":" + std::to_string(ntohs(srcAddr.sin_port)));
    }
    dgram->reset();
    return true;
}

} // namespace j2me
