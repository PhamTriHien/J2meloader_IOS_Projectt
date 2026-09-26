#ifndef UNIVERSAL_LOADER_PKI_TYPES_H
#define UNIVERSAL_LOADER_PKI_TYPES_H

#include "j2me_core.h"
#include <string>
#include <cstdint>
#include <algorithm>

namespace universal_loader {
namespace security {

// Standard CertificateException reason codes (MIDP 2.0 / JSR-118)
constexpr uint8_t CERT_BAD_EXTENSIONS               = 1;
constexpr uint8_t CERT_CHAIN_TOO_LONG               = 2;
constexpr uint8_t CERT_EXPIRED                      = 3;
constexpr uint8_t CERT_UNAUTHORIZED_INTERMEDIATE_CA = 4;
constexpr uint8_t CERT_MISSING_SIGNATURE            = 5;
constexpr uint8_t CERT_NOT_YET_VALID                = 6;
constexpr uint8_t CERT_SITENAME_MISMATCH            = 7;
constexpr uint8_t CERT_UNRECOGNIZED_ISSUER          = 8;
constexpr uint8_t CERT_UNSUPPORTED_SIGALG           = 9;
constexpr uint8_t CERT_INAPPROPRIATE_KEY_USAGE      = 10;
constexpr uint8_t CERT_BROKEN_CHAIN                 = 11;
constexpr uint8_t CERT_ROOT_CA_EXPIRED              = 12;
constexpr uint8_t CERT_UNSUPPORTED_PUBLIC_KEY_TYPE  = 13;
constexpr uint8_t CERT_VERIFICATION_FAILED          = 14;

// Aliases with CERT_ERR_ prefix
constexpr uint8_t CERT_ERR_BAD_EXTENSIONS               = CERT_BAD_EXTENSIONS;
constexpr uint8_t CERT_ERR_CHAIN_TOO_LONG               = CERT_CHAIN_TOO_LONG;
constexpr uint8_t CERT_ERR_EXPIRED                      = CERT_EXPIRED;
constexpr uint8_t CERT_ERR_UNAUTHORIZED_INTERMEDIATE_CA = CERT_UNAUTHORIZED_INTERMEDIATE_CA;
constexpr uint8_t CERT_ERR_MISSING_SIGNATURE            = CERT_MISSING_SIGNATURE;
constexpr uint8_t CERT_ERR_NOT_YET_VALID                = CERT_NOT_YET_VALID;
constexpr uint8_t CERT_ERR_SITENAME_MISMATCH            = CERT_SITENAME_MISMATCH;
constexpr uint8_t CERT_ERR_UNRECOGNIZED_ISSUER          = CERT_UNRECOGNIZED_ISSUER;
constexpr uint8_t CERT_ERR_UNSUPPORTED_SIGALG           = CERT_UNSUPPORTED_SIGALG;
constexpr uint8_t CERT_ERR_INAPPROPRIATE_KEY_USAGE      = CERT_INAPPROPRIATE_KEY_USAGE;
constexpr uint8_t CERT_ERR_BROKEN_CHAIN                 = CERT_BROKEN_CHAIN;
constexpr uint8_t CERT_ERR_ROOT_CA_EXPIRED              = CERT_ROOT_CA_EXPIRED;
constexpr uint8_t CERT_ERR_UNSUPPORTED_PUBLIC_KEY_TYPE  = CERT_UNSUPPORTED_PUBLIC_KEY_TYPE;
constexpr uint8_t CERT_ERR_VERIFICATION_FAILED          = CERT_VERIFICATION_FAILED;

class J2ME_API Certificate {
public:
    Certificate() = default;
    Certificate(const std::string& subject,
                const std::string& issuer,
                const std::string& type,
                const std::string& version,
                const std::string& sigAlgName,
                int64_t notBefore,
                int64_t notAfter,
                const std::string& serialNumber)
        : m_subject(subject)
        , m_issuer(issuer)
        , m_type(type.empty() ? "X.509" : type)
        , m_version(version.empty() ? "3" : version)
        , m_sigAlgName(sigAlgName.empty() ? "SHA256withRSA" : sigAlgName)
        , m_notBefore(notBefore)
        , m_notAfter(notAfter)
        , m_serialNumber(serialNumber)
    {}

    const std::string& getSubject() const { return m_subject; }
    const std::string& getIssuer() const { return m_issuer; }
    const std::string& getType() const { return m_type; }
    const std::string& getVersion() const { return m_version; }
    const std::string& getSigAlgName() const { return m_sigAlgName; }
    int64_t getNotBefore() const { return m_notBefore; }
    int64_t getNotAfter() const { return m_notAfter; }
    const std::string& getSerialNumber() const { return m_serialNumber; }

    void setSubject(const std::string& v) { m_subject = v; }
    void setIssuer(const std::string& v) { m_issuer = v; }
    void setType(const std::string& v) { m_type = v; }
    void setVersion(const std::string& v) { m_version = v; }
    void setSigAlgName(const std::string& v) { m_sigAlgName = v; }
    void setNotBefore(int64_t v) { m_notBefore = v; }
    void setNotAfter(int64_t v) { m_notAfter = v; }
    void setSerialNumber(const std::string& v) { m_serialNumber = v; }

    std::string extractCN() const { return extractCommonName(); }
    bool matchesHost(const std::string& host) const { return matchHost(host, extractCommonName()); }
    bool isExpired(int64_t currentTimeMs) const { return m_notAfter > 0 && currentTimeMs > m_notAfter; }
    bool isNotYetValid(int64_t currentTimeMs) const { return m_notBefore > 0 && currentTimeMs < m_notBefore; }

    // Validates validity period and hostname matching. Returns 0 on success or CERT_* error code.
    int validate(const std::string& expectedHost, int64_t currentTimeMs) const {
        if (m_notBefore > 0 && currentTimeMs < m_notBefore) {
            return CERT_NOT_YET_VALID;
        }
        if (m_notAfter > 0 && currentTimeMs > m_notAfter) {
            return CERT_EXPIRED;
        }

        if (!expectedHost.empty()) {
            std::string cn = extractCommonName();
            if (!matchHost(expectedHost, cn)) {
                return CERT_SITENAME_MISMATCH;
            }
        }
        return 0; // Success
    }

private:
    std::string m_subject;
    std::string m_issuer;
    std::string m_type{"X.509"};
    std::string m_version{"3"};
    std::string m_sigAlgName{"SHA256withRSA"};
    int64_t m_notBefore{0};
    int64_t m_notAfter{0};
    std::string m_serialNumber;

    std::string extractCommonName() const {
        size_t cnPos = m_subject.find("CN=");
        if (cnPos == std::string::npos) return "";
        size_t start = cnPos + 3;
        size_t comma = m_subject.find(',', start);
        if (comma != std::string::npos) {
            return m_subject.substr(start, comma - start);
        }
        return m_subject.substr(start);
    }

    static bool matchHost(const std::string& host, const std::string& pattern) {
        if (pattern.empty()) return false;
        if (equalIgnoreCase(host, pattern)) return true;

        // Wildcard match: e.g. *.example.com matches api.example.com
        if (pattern.rfind("*.", 0) == 0) {
            std::string suffix = pattern.substr(1); // ".example.com"
            if (host.size() > suffix.size()) {
                std::string hostSuffix = host.substr(host.size() - suffix.size());
                if (equalIgnoreCase(hostSuffix, suffix)) {
                    // Ensure only one domain label is matched
                    std::string prefix = host.substr(0, host.size() - suffix.size());
                    return prefix.find('.') == std::string::npos;
                }
            }
        }
        return false;
    }

    static bool equalIgnoreCase(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }
};

class J2ME_API CertificateException {
public:
    enum Reason : uint8_t {
        BAD_EXTENSIONS               = CERT_BAD_EXTENSIONS,
        CERT_CHAIN_TOO_LONG          = CERT_CHAIN_TOO_LONG,
        EXPIRED                      = CERT_EXPIRED,
        UNAUTHORIZED_INTERMEDIATE_CA = CERT_UNAUTHORIZED_INTERMEDIATE_CA,
        MISSING_SIGNATURE            = CERT_MISSING_SIGNATURE,
        NOT_YET_VALID                = CERT_NOT_YET_VALID,
        SITENAME_MISMATCH            = CERT_SITENAME_MISMATCH,
        UNRECOGNIZED_ISSUER          = CERT_UNRECOGNIZED_ISSUER,
        UNSUPPORTED_SIGALG           = CERT_UNSUPPORTED_SIGALG,
        INAPPROPRIATE_KEY_USAGE      = CERT_INAPPROPRIATE_KEY_USAGE,
        BROKEN_CHAIN                 = CERT_BROKEN_CHAIN,
        ROOT_CA_EXPIRED              = CERT_ROOT_CA_EXPIRED,
        UNSUPPORTED_PUBLIC_KEY_TYPE  = CERT_UNSUPPORTED_PUBLIC_KEY_TYPE,
        VERIFICATION_FAILED          = CERT_VERIFICATION_FAILED,
    };

    CertificateException(uint8_t reason, const std::string& message = "", const Certificate& cert = Certificate())
        : m_reason(reason)
        , m_message(message)
        , m_certificate(cert)
    {}

    uint8_t getReason() const { return m_reason; }
    const std::string& getMessage() const { return m_message; }
    const Certificate& getCertificate() const { return m_certificate; }

    const char* getReasonName() const { return getReasonName(m_reason); }

    static const char* getReasonName(uint8_t reason) {
        switch (reason) {
            case BAD_EXTENSIONS: return "BAD_EXTENSIONS";
            case CERT_CHAIN_TOO_LONG: return "CERT_CHAIN_TOO_LONG";
            case EXPIRED: return "EXPIRED";
            case UNAUTHORIZED_INTERMEDIATE_CA: return "UNAUTHORIZED_INTERMEDIATE_CA";
            case MISSING_SIGNATURE: return "MISSING_SIGNATURE";
            case NOT_YET_VALID: return "NOT_YET_VALID";
            case SITENAME_MISMATCH: return "SITENAME_MISMATCH";
            case UNRECOGNIZED_ISSUER: return "UNRECOGNIZED_ISSUER";
            case UNSUPPORTED_SIGALG: return "UNSUPPORTED_SIGALG";
            case INAPPROPRIATE_KEY_USAGE: return "INAPPROPRIATE_KEY_USAGE";
            case BROKEN_CHAIN: return "BROKEN_CHAIN";
            case ROOT_CA_EXPIRED: return "ROOT_CA_EXPIRED";
            case UNSUPPORTED_PUBLIC_KEY_TYPE: return "UNSUPPORTED_PUBLIC_KEY_TYPE";
            case VERIFICATION_FAILED: return "VERIFICATION_FAILED";
            default: return "UNKNOWN_REASON";
        }
    }

private:
    uint8_t m_reason{CERT_VERIFICATION_FAILED};
    std::string m_message;
    Certificate m_certificate;
};

class J2ME_API SecurityInfo {
public:
    SecurityInfo() = default;
    SecurityInfo(const std::string& cipherSuite,
                 const std::string& protocolName,
                 const std::string& protocolVersion,
                 const Certificate& cert)
        : m_cipherSuite(cipherSuite)
        , m_protocolName(protocolName)
        , m_protocolVersion(protocolVersion)
        , m_serverCertificate(cert)
    {}

    const std::string& getCipherSuite() const { return m_cipherSuite; }
    const std::string& getProtocolName() const { return m_protocolName; }
    const std::string& getProtocolVersion() const { return m_protocolVersion; }
    const Certificate& getServerCertificate() const { return m_serverCertificate; }

    void setCipherSuite(const std::string& v) { m_cipherSuite = v; }
    void setProtocolName(const std::string& v) { m_protocolName = v; }
    void setProtocolVersion(const std::string& v) { m_protocolVersion = v; }
    void setServerCertificate(const Certificate& cert) { m_serverCertificate = cert; }

private:
    std::string m_cipherSuite{"TLS_RSA_WITH_AES_128_CBC_SHA"};
    std::string m_protocolName{"TLS"};
    std::string m_protocolVersion{"1.2"};
    Certificate m_serverCertificate;
};

} // namespace security
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PKI_TYPES_H
