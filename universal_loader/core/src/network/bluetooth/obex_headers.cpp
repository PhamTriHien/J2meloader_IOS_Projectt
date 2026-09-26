#include "obex_headers.h"
#include <cstring>

namespace j2me {
namespace bluetooth {

ObexHeaderSet::ObexHeaderSet() = default;

void ObexHeaderSet::setHeader(int headerId, int64_t val) {
    headers_[headerId] = val;
}

void ObexHeaderSet::setHeader(int headerId, const std::string& str) {
    headers_[headerId] = str;
}

void ObexHeaderSet::setHeader(int headerId, const std::vector<uint8_t>& bytes) {
    headers_[headerId] = bytes;
}

void ObexHeaderSet::removeHeader(int headerId) {
    headers_.erase(headerId);
}

bool ObexHeaderSet::hasHeader(int headerId) const {
    return headers_.find(headerId) != headers_.end();
}

std::vector<int> ObexHeaderSet::getHeaderList() const {
    std::vector<int> list;
    list.reserve(headers_.size());
    for (const auto& kv : headers_) {
        list.push_back(kv.first);
    }
    return list;
}

bool ObexHeaderSet::getHeaderInt(int headerId, int64_t& outVal) const {
    auto it = headers_.find(headerId);
    if (it != headers_.end() && std::holds_alternative<int64_t>(it->second)) {
        outVal = std::get<int64_t>(it->second);
        return true;
    }
    return false;
}

bool ObexHeaderSet::getHeaderString(int headerId, std::string& outVal) const {
    auto it = headers_.find(headerId);
    if (it != headers_.end()) {
        if (std::holds_alternative<std::string>(it->second)) {
            outVal = std::get<std::string>(it->second);
            return true;
        } else if (std::holds_alternative<std::vector<uint8_t>>(it->second)) {
            const auto& bytes = std::get<std::vector<uint8_t>>(it->second);
            size_t len = bytes.size();
            if (len > 0 && bytes[len - 1] == 0) {
                len--;
            }
            outVal = std::string(reinterpret_cast<const char*>(bytes.data()), len);
            return true;
        }
    }
    return false;
}

bool ObexHeaderSet::getHeaderBytes(int headerId, std::vector<uint8_t>& outVal) const {
    auto it = headers_.find(headerId);
    if (it != headers_.end()) {
        if (std::holds_alternative<std::vector<uint8_t>>(it->second)) {
            outVal = std::get<std::vector<uint8_t>>(it->second);
            return true;
        } else if (std::holds_alternative<std::string>(it->second)) {
            const auto& str = std::get<std::string>(it->second);
            outVal.assign(str.begin(), str.end());
            return true;
        }
    }
    return false;
}

void ObexHeaderSet::createAuthenticationChallenge(const std::string& realm, bool userId, bool access) {
    // Challenge header format: Tag-Length-Value
    // Tag 0x00: Nonce (16 bytes)
    // Tag 0x01: Options (1 byte, bit 0: userId required, bit 1: access)
    // Tag 0x02: Realm
    std::vector<uint8_t> challenge;

    // Nonce
    challenge.push_back(0x00);
    challenge.push_back(16);
    for (int i = 0; i < 16; ++i) challenge.push_back(static_cast<uint8_t>(i + 1));

    // Options
    challenge.push_back(0x01);
    challenge.push_back(1);
    uint8_t opt = (userId ? 0x01 : 0x00) | (access ? 0x02 : 0x00);
    challenge.push_back(opt);

    // Realm
    if (!realm.empty()) {
        challenge.push_back(0x02);
        challenge.push_back(static_cast<uint8_t>(realm.length() + 1));
        challenge.push_back(0x00); // Charset ASCII/UTF-8
        challenge.insert(challenge.end(), realm.begin(), realm.end());
    }

    setHeader(OBEX_HDR_APPLICATION_PARAMETER, challenge);
}

std::vector<uint8_t> ObexHeaderSet::serialize(uint8_t opOrResponseCode) const {
    std::vector<uint8_t> packet;
    packet.push_back(opOrResponseCode);
    packet.push_back(0); // placeholder length high
    packet.push_back(0); // placeholder length low

    for (const auto& kv : headers_) {
        uint8_t hid = static_cast<uint8_t>(kv.first);
        uint8_t encType = hid & 0xC0;

        if (encType == 0xC0) {
            // 4-byte integer (5 bytes total)
            packet.push_back(hid);
            int64_t v = 0;
            if (std::holds_alternative<int64_t>(kv.second)) {
                v = std::get<int64_t>(kv.second);
            }
            packet.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
            packet.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            packet.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            packet.push_back(static_cast<uint8_t>(v & 0xFF));
        } else if (encType == 0x80) {
            // 1-byte quantity (2 bytes total)
            packet.push_back(hid);
            int64_t v = 0;
            if (std::holds_alternative<int64_t>(kv.second)) {
                v = std::get<int64_t>(kv.second);
            }
            packet.push_back(static_cast<uint8_t>(v & 0xFF));
        } else if (encType == 0x00) {
            // Null-terminated string
            std::string s;
            if (std::holds_alternative<std::string>(kv.second)) {
                s = std::get<std::string>(kv.second);
            }
            uint16_t hdrLen = static_cast<uint16_t>(3 + s.length() + 1);
            packet.push_back(hid);
            packet.push_back(static_cast<uint8_t>((hdrLen >> 8) & 0xFF));
            packet.push_back(static_cast<uint8_t>(hdrLen & 0xFF));
            packet.insert(packet.end(), s.begin(), s.end());
            packet.push_back(0); // Null terminator
        } else if (encType == 0x40) {
            // Byte sequence
            std::vector<uint8_t> b;
            if (std::holds_alternative<std::vector<uint8_t>>(kv.second)) {
                b = std::get<std::vector<uint8_t>>(kv.second);
            } else if (std::holds_alternative<std::string>(kv.second)) {
                const auto& str = std::get<std::string>(kv.second);
                b.assign(str.begin(), str.end());
            }
            uint16_t hdrLen = static_cast<uint16_t>(3 + b.size());
            packet.push_back(hid);
            packet.push_back(static_cast<uint8_t>((hdrLen >> 8) & 0xFF));
            packet.push_back(static_cast<uint8_t>(hdrLen & 0xFF));
            packet.insert(packet.end(), b.begin(), b.end());
        }
    }

    uint16_t totalLen = static_cast<uint16_t>(packet.size());
    packet[1] = static_cast<uint8_t>((totalLen >> 8) & 0xFF);
    packet[2] = static_cast<uint8_t>(totalLen & 0xFF);

    return packet;
}

bool ObexHeaderSet::deserialize(const uint8_t* data, size_t len, uint8_t& outOpOrResponseCode) {
    if (!data || len < 3) return false;

    outOpOrResponseCode = data[0];
    uint16_t totalLen = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
    if (totalLen > len) totalLen = static_cast<uint16_t>(len);

    size_t offset = 3;
    headers_.clear();

    while (offset < totalLen) {
        uint8_t hid = data[offset++];
        uint8_t encType = hid & 0xC0;

        if (encType == 0xC0) {
            if (offset + 4 > totalLen) break;
            int64_t v = (static_cast<int64_t>(data[offset]) << 24) |
                        (static_cast<int64_t>(data[offset + 1]) << 16) |
                        (static_cast<int64_t>(data[offset + 2]) << 8) |
                        static_cast<int64_t>(data[offset + 3]);
            offset += 4;
            headers_[hid] = v;
        } else if (encType == 0x80) {
            if (offset + 1 > totalLen) break;
            headers_[hid] = static_cast<int64_t>(data[offset++]);
        } else {
            // Length-prefixed
            if (offset + 2 > totalLen) break;
            uint16_t hdrLen = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
            offset += 2;
            if (hdrLen < 3 || (offset + hdrLen - 3) > totalLen) break;

            size_t payloadLen = hdrLen - 3;
            if (encType == 0x00) {
                // String (minus null terminator if present)
                size_t strLen = payloadLen;
                if (strLen > 0 && data[offset + strLen - 1] == 0) {
                    strLen--;
                }
                std::string s(reinterpret_cast<const char*>(data + offset), strLen);
                headers_[hid] = s;
            } else {
                std::vector<uint8_t> b(data + offset, data + offset + payloadLen);
                headers_[hid] = b;
            }
            offset += payloadLen;
        }
    }

    return true;
}

} // namespace bluetooth
} // namespace j2me
