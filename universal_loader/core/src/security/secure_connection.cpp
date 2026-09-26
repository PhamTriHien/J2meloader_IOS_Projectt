#include "secure_connection.h"
#include <sstream>
#include <chrono>

namespace universal_loader {
namespace security {

SecureConnection::SecureConnection(const std::string& host, int port)
    : m_host(host)
    , m_port(port)
    , m_isOpen(true)
{
    // Generate default trusted certificate for this host
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    Certificate cert(
        "CN=" + host + ", O=J2ME Secure Host, C=US",
        "CN=DigiCert Global Root CA, O=DigiCert Inc, C=US",
        "X.509",
        "3",
        "SHA256withRSA",
        nowMs - 86400000LL,         // valid from yesterday
        nowMs + 365LL * 86400000LL, // valid for 1 year
        "04:00:00:00:00:01:15:4b:5a:c3:94"
    );

    m_securityInfo = SecurityInfo(
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
        "TLS",
        "1.2",
        cert
    );
}

SecureConnection::~SecureConnection() {
    close();
}

std::shared_ptr<SecureConnection> SecureConnection::open(const std::string& url, bool verifyHost) {
    const std::string prefix = "ssl://";
    if (url.rfind(prefix, 0) != 0) {
        return nullptr;
    }

    std::string remainder = url.substr(prefix.size());
    size_t colonPos = remainder.find(':');
    std::string host;
    int port = 443;

    if (colonPos != std::string::npos) {
        host = remainder.substr(0, colonPos);
        try {
            port = std::stoi(remainder.substr(colonPos + 1));
        } catch (...) {
            port = 443;
        }
    } else {
        host = remainder;
    }

    if (host.empty()) return nullptr;

    auto conn = std::make_shared<SecureConnection>(host, port);
    if (conn->performHandshake(verifyHost) != 0) {
        return nullptr;
    }
    return conn;
}

int SecureConnection::performHandshake(bool verifyHost) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!verifyHost) {
        return 0;
    }

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    return m_securityInfo.getServerCertificate().validate(m_host, nowMs);
}

void SecureConnection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isOpen = false;
    m_rxBuffer.clear();
    m_txBuffer.clear();
}

size_t SecureConnection::write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !data || len == 0) return 0;
    for (size_t i = 0; i < len; ++i) {
        m_txBuffer.push_back(data[i]);
    }
    return len;
}

size_t SecureConnection::read(uint8_t* outBuf, size_t maxLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !outBuf || maxLen == 0) return 0;
    size_t count = std::min(maxLen, m_rxBuffer.size());
    for (size_t i = 0; i < count; ++i) {
        outBuf[i] = m_rxBuffer.front();
        m_rxBuffer.pop_front();
    }
    return count;
}

size_t SecureConnection::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rxBuffer.size();
}

void SecureConnection::feedInput(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !data || len == 0) return;
    for (size_t i = 0; i < len; ++i) {
        m_rxBuffer.push_back(data[i]);
    }
}

size_t SecureConnection::extractOutput(uint8_t* outBuf, size_t maxLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!outBuf || maxLen == 0) return 0;
    size_t count = std::min(maxLen, m_txBuffer.size());
    for (size_t i = 0; i < count; ++i) {
        outBuf[i] = m_txBuffer.front();
        m_txBuffer.pop_front();
    }
    return count;
}

// -----------------------------------------------------------------------------
// HttpsConnection
// -----------------------------------------------------------------------------

HttpsConnection::HttpsConnection(const std::string& host, int port, const std::string& path)
    : m_host(host)
    , m_port(port)
    , m_path(path.empty() ? "/" : path)
    , m_isOpen(true)
{
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    Certificate cert(
        "CN=" + host + ", O=J2ME HTTPS Host, C=US",
        "CN=DigiCert Global Root CA, O=DigiCert Inc, C=US",
        "X.509",
        "3",
        "SHA256withRSA",
        nowMs - 86400000LL,
        nowMs + 365LL * 86400000LL,
        "04:00:00:00:00:02:18:4c:7a:d4:12"
    );

    m_securityInfo = SecurityInfo(
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
        "TLS",
        "1.3",
        cert
    );
}

HttpsConnection::~HttpsConnection() {
    close();
}

std::shared_ptr<HttpsConnection> HttpsConnection::open(const std::string& url) {
    const std::string prefix = "https://";
    if (url.rfind(prefix, 0) != 0) {
        return nullptr;
    }

    std::string remainder = url.substr(prefix.size());
    size_t slashPos = remainder.find('/');
    std::string hostPart = (slashPos != std::string::npos) ? remainder.substr(0, slashPos) : remainder;
    std::string pathPart = (slashPos != std::string::npos) ? remainder.substr(slashPos) : "/";

    std::string host;
    int port = 443;
    size_t colonPos = hostPart.find(':');
    if (colonPos != std::string::npos) {
        host = hostPart.substr(0, colonPos);
        try {
            port = std::stoi(hostPart.substr(colonPos + 1));
        } catch (...) {
            port = 443;
        }
    } else {
        host = hostPart;
    }

    if (host.empty()) return nullptr;
    return std::make_shared<HttpsConnection>(host, port, pathPart);
}

void HttpsConnection::setRequestProperty(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestHeaders[key] = value;
}

std::string HttpsConnection::getRequestProperty(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_requestHeaders.find(key);
    return (it != m_requestHeaders.end()) ? it->second : "";
}

void HttpsConnection::setResponseHeader(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responseHeaders[key] = value;
}

std::string HttpsConnection::getHeaderField(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_responseHeaders.find(key);
    return (it != m_responseHeaders.end()) ? it->second : "";
}

void HttpsConnection::feedResponse(int responseCode, const std::string& body) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responseCode = responseCode;
    m_responseBody.clear();
    for (char c : body) {
        m_responseBody.push_back(static_cast<uint8_t>(c));
    }
}

void HttpsConnection::feedResponse(int responseCode, const std::string& /*message*/, const std::map<std::string, std::string>& headers, const std::vector<uint8_t>& body) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responseCode = responseCode;
    for (const auto& kv : headers) {
        m_responseHeaders[kv.first] = kv.second;
    }
    m_responseBody.clear();
    m_responseBody.insert(m_responseBody.end(), body.begin(), body.end());
}

size_t HttpsConnection::write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !data || len == 0) return 0;
    for (size_t i = 0; i < len; ++i) {
        m_requestBody.push_back(data[i]);
    }
    return len;
}

size_t HttpsConnection::read(uint8_t* outBuf, size_t maxLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !outBuf || maxLen == 0) return 0;
    size_t count = std::min(maxLen, m_responseBody.size());
    for (size_t i = 0; i < count; ++i) {
        outBuf[i] = m_responseBody.front();
        m_responseBody.pop_front();
    }
    return count;
}

size_t HttpsConnection::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_responseBody.size();
}

void HttpsConnection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isOpen = false;
    m_requestBody.clear();
    m_responseBody.clear();
}

} // namespace security
} // namespace universal_loader
