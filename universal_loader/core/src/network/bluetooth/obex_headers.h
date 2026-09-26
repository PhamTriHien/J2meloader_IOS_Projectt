#pragma once

#include "j2me_core.h"
#include "bluetooth_types.h"
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>

namespace j2me {
namespace bluetooth {

class J2ME_API ObexHeaderSet {
public:
    using HeaderValue = std::variant<std::monostate, int64_t, std::string, std::vector<uint8_t>>;

    ObexHeaderSet();

    void setHeader(int headerId, int64_t val);
    void setHeader(int headerId, const std::string& str);
    void setHeader(int headerId, const std::vector<uint8_t>& bytes);
    void removeHeader(int headerId);

    bool hasHeader(int headerId) const;
    std::vector<int> getHeaderList() const;

    bool getHeaderInt(int headerId, int64_t& outVal) const;
    bool getHeaderString(int headerId, std::string& outVal) const;
    bool getHeaderBytes(int headerId, std::vector<uint8_t>& outVal) const;

    int getResponseCode() const { return responseCode_; }
    void setResponseCode(int code) { responseCode_ = code; }

    void createAuthenticationChallenge(const std::string& realm, bool userId, bool access);

    // Binary OBEX serialization & deserialization
    std::vector<uint8_t> serialize(uint8_t opOrResponseCode) const;
    bool deserialize(const uint8_t* data, size_t len, uint8_t& outOpOrResponseCode);

private:
    int responseCode_ = OBEX_HTTP_OK;
    std::map<int, HeaderValue> headers_;
};

} // namespace bluetooth
} // namespace j2me
