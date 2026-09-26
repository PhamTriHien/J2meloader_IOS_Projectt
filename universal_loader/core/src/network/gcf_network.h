#ifndef J2ME_GCF_NETWORK_H
#define J2ME_GCF_NETWORK_H

#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <mutex>
#include <atomic>

namespace j2me {

// --- 1. Cross-platform Raw TCP Socket Abstraction ---
class J2ME_API NetworkSocket {
public:
    NetworkSocket();
    ~NetworkSocket();

    bool connect(const std::string& host, int port, int timeoutMs = 5000);
    void close();

    bool isConnected() const { return m_connected; }
    int send(const uint8_t* data, size_t length);
    int recv(uint8_t* buffer, size_t maxLength, int timeoutMs = 20);
    int available();

    // Đọc trọn vẹn số byte yêu cầu (hỗ trợ phân mảnh gói tin mạng)
    bool readFully(uint8_t* buffer, size_t length, int timeoutMs = 3000);

private:
#if defined(_WIN32) || defined(_WIN64)
    uintptr_t m_sock{static_cast<uintptr_t>(~0)}; // INVALID_SOCKET
#else
    int m_sock{-1};
#endif
    std::atomic<bool> m_connected{false};
    mutable std::mutex m_mutex;

    static void ensurePlatformNetInit();
};

// --- 2. J2ME Stream Wrapper ---
class J2ME_API SocketInputStream {
public:
    SocketInputStream(std::shared_ptr<NetworkSocket> socket);
    int read();
    int read(uint8_t* b, size_t offset, size_t length);
    bool readFully(uint8_t* b, size_t length);
    int available();
    void close();

private:
    std::shared_ptr<NetworkSocket> m_socket;
};

class J2ME_API SocketOutputStream {
public:
    SocketOutputStream(std::shared_ptr<NetworkSocket> socket);
    void write(uint8_t b);
    void write(const uint8_t* b, size_t offset, size_t length);
    void flush();
    void close();

private:
    std::shared_ptr<NetworkSocket> m_socket;
};

// --- 3. J2ME SocketConnection Interface ---
class J2ME_API SocketConnection {
public:
    SocketConnection(const std::string& host, int port);
    ~SocketConnection();

    bool open(int timeoutMs = 5000);
    void close();

    std::shared_ptr<SocketInputStream> openInputStream();
    std::shared_ptr<SocketOutputStream> openOutputStream();

    std::string getAddress() const { return m_host; }
    int getPort() const { return m_port; }

private:
    std::string m_host;
    int m_port;
    std::shared_ptr<NetworkSocket> m_socket;
    std::shared_ptr<SocketInputStream> m_inStream;
    std::shared_ptr<SocketOutputStream> m_outStream;
};

// --- 4. J2ME HttpConnection Interface ---
class J2ME_API HttpConnection {
public:
    HttpConnection(const std::string& url);
    ~HttpConnection();

    void setRequestMethod(const std::string& method) { m_method = method; }
    void setRequestProperty(const std::string& key, const std::string& value);

    int getResponseCode();
    std::string getHeaderField(const std::string& name);
    std::string getResponseBody();

private:
    std::string m_url;
    std::string m_method{"GET"};
    std::map<std::string, std::string> m_headers;

    int m_responseCode{0};
    std::map<std::string, std::string> m_responseHeaders;
    std::string m_responseBody;
    bool m_executed{false};

    bool executeRequest();
};

class DatagramConnection;

// --- 5. Bộ điều phối Generic Connection Framework (GCF Connector) ---
class J2ME_API GcfConnector {
public:
    static std::shared_ptr<SocketConnection> openSocket(const std::string& url);
    static std::shared_ptr<HttpConnection> openHttp(const std::string& url);
    static std::shared_ptr<DatagramConnection> openDatagram(const std::string& url);
};

} // namespace j2me

#endif // J2ME_GCF_NETWORK_H
