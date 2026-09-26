#include "wma_message.h"
#include <utility>

namespace universal_loader {
namespace messaging {

// =========================================================================
// MessageImpl
// =========================================================================
MessageImpl::MessageImpl(std::string address, int64_t timestamp)
    : m_address(std::move(address)), m_timestamp(timestamp) {
}

std::string MessageImpl::getAddress() const {
    return m_address;
}

void MessageImpl::setAddress(const std::string& address) {
    m_address = address;
}

int64_t MessageImpl::getTimestamp() const {
    return m_timestamp;
}

// =========================================================================
// TextMessageImpl
// =========================================================================
TextMessageImpl::TextMessageImpl(std::string address, int64_t timestamp)
    : MessageImpl(std::move(address), timestamp) {
}

std::string TextMessageImpl::getPayloadText() const {
    return m_textPayload;
}

void TextMessageImpl::setPayloadText(const std::string& text) {
    m_textPayload = text;
}

// =========================================================================
// BinaryMessageImpl
// =========================================================================
BinaryMessageImpl::BinaryMessageImpl(std::string address, int64_t timestamp)
    : MessageImpl(std::move(address), timestamp) {
}

std::vector<uint8_t> BinaryMessageImpl::getPayloadData() const {
    return m_binaryPayload;
}

void BinaryMessageImpl::setPayloadData(const std::vector<uint8_t>& data) {
    m_binaryPayload = data;
}

void BinaryMessageImpl::setPayloadData(const uint8_t* data, size_t length) {
    if (data && length > 0) {
        m_binaryPayload.assign(data, data + length);
    } else {
        m_binaryPayload.clear();
    }
}

} // namespace messaging
} // namespace universal_loader
