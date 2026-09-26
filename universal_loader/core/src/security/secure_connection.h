#ifndef UNIVERSAL_LOADER_SECURE_CONNECTION_H
#define UNIVERSAL_LOADER_SECURE_CONNECTION_H

#include "pki_types.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <memory>

namespace universal_loader {
namespace security {

class J2ME_API SecureConnection {
public:
    SecureConnection(const std::string& host, int port);
    ~SecureConnection();

    static std::shared_ptr<SecureConnection> open(const std::string& url, bool verifyHost = true);

    const std::string& getHost() const { return m_host; }
    int getPort() const { return m_port; }
    bool isOpen() const { return m_isOpen; }
    void close();

    const SecurityInfo& getSecurityInfo() const { return m_securityInfo; }
    void setSecurityInfo(const SecurityInfo& info) { m_securityInfo = info; }

    int performHandshake(bool verifyHost = true);

    size_t write(const uint8_t* data, size_t len);
    size_t read(uint8_t* outBuf, size_t maxLen);
    size_t available() const;

    void feedInput(const uint8_t* data, size_t len);
    size_t extractOutput(uint8_t* outBuf, size_t maxLen);

private:
    mutable std::mutex m_mutex;
    std::string m_host;
    int m_port{443};
    bool m_isOpen{true};
    SecurityInfo m_securityInfo;

    std::deque<uint8_t> m_rxBuffer;
    std::deque<uint8_t> m_txBuffer;
};

class J2ME_API HttpsConnection {
public:
    HttpsConnection(const std::string& host, int port, const std::string& path);
    ~HttpsConnection();

    static std::shared_ptr<HttpsConnection> open(const std::string& url);

    const std::string& getHost() const { return m_host; }
    int getPort() const { return m_port; }
    const std::string& getPath() const { return m_path; }
    std::string getFile() const { return m_path; }
    std::string getProtocol() const { return "https"; }

    void setRequestMethod(const std::string& method) { m_method = method; }
    const std::string& getRequestMethod() const { return m_method; }

    void setRequestProperty(const std::string& key, const std::string& value);
    std::string getRequestProperty(const std::string& key) const;

    void setResponseHeader(const std::string& key, const std::string& value);
    std::string getHeaderField(const std::string& key) const;

    int getResponseCode() const { return m_responseCode; }
    void setResponseCode(int code) { m_responseCode = code; }
    std::string getResponseMessage() const { return m_responseCode == 200 ? "OK" : "Status " + std::to_string(m_responseCode); }
    int64_t getLength() const { std::lock_guard<std::mutex> lk(m_mutex); return static_cast<int64_t>(m_responseBody.size()); }

    const SecurityInfo& getSecurityInfo() const { return m_securityInfo; }
    void setSecurityInfo(const SecurityInfo& info) { m_securityInfo = info; }

    size_t write(const uint8_t* data, size_t len);
    size_t read(uint8_t* outBuf, size_t maxLen);
    size_t available() const;

    void feedResponse(int responseCode, const std::string& body);
    void feedResponse(int responseCode, const std::string& message, const std::map<std::string, std::string>& headers, const std::vector<uint8_t>& body);
    void close();
    bool isOpen() const { return m_isOpen; }

private:
    mutable std::mutex m_mutex;
    std::string m_host;
    int m_port{443};
    std::string m_path{"/"};
    std::string m_method{"GET"};
    int m_responseCode{200};
    bool m_isOpen{true};

    std::map<std::string, std::string> m_requestHeaders;
    std::map<std::string, std::string> m_responseHeaders;
    SecurityInfo m_securityInfo;

    std::deque<uint8_t> m_requestBody;
    std::deque<uint8_t> m_responseBody;
};

} // namespace security
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SECURE_CONNECTION_H
