#include "sms_connection.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace universal_loader {
namespace messaging {

// =========================================================================
// SmsMessageRouter Implementation
// =========================================================================
SmsMessageRouter& SmsMessageRouter::instance() {
    static SmsMessageRouter s_instance;
    return s_instance;
}

void SmsMessageRouter::setInterceptCallback(SmsInterceptCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interceptCallback = callback;
    m_callbackUserData = userData;
}

void SmsMessageRouter::setAutoReply(bool enabled, const std::string& replyTemplate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoReplyEnabled = enabled;
    m_autoReplyTemplate = replyTemplate;
}

void SmsMessageRouter::routeOutgoing(const std::shared_ptr<Message>& message, SmsConnection* sourceConn) {
    if (!message) return;

    SmsInterceptCallback cb = nullptr;
    void* userData = nullptr;
    MessageListener* targetListener = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outbox.push_back(message);

        cb = m_interceptCallback;
        userData = m_callbackUserData;

        auto it = m_listeners.find(message->getAddress());
        if (it != m_listeners.end()) {
            targetListener = it->second;
        }

        // If auto-reply is enabled, generate a response into inbox
        if (m_autoReplyEnabled) {
            auto reply = std::make_shared<TextMessageImpl>(
                message->getAddress(),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()
            );
            reply->setPayloadText(m_autoReplyTemplate);
            m_inbox.push_back(reply);
        }
    }

    // Invoke external native callback outside mutex
    if (cb) {
        std::string addr = message->getAddress();
        const char* textPayload = nullptr;
        const uint8_t* binPayload = nullptr;
        size_t binLen = 0;

        auto textMsg = std::dynamic_pointer_cast<TextMessage>(message);
        std::string txt;
        if (textMsg) {
            txt = textMsg->getPayloadText();
            textPayload = txt.c_str();
        }

        auto binMsg = std::dynamic_pointer_cast<BinaryMessage>(message);
        std::vector<uint8_t> bin;
        if (binMsg) {
            bin = binMsg->getPayloadData();
            binPayload = bin.data();
            binLen = bin.size();
        }

        cb(addr.c_str(), textPayload, binPayload, binLen, userData);
    }

    // Notify registered listener
    if (targetListener && sourceConn) {
        targetListener->notifyIncomingMessage(sourceConn);
    }
}

void SmsMessageRouter::injectIncoming(const std::shared_ptr<Message>& message) {
    if (!message) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inbox.push_back(message);
}

std::shared_ptr<Message> SmsMessageRouter::popIncoming(const std::string& addressFilter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_inbox.empty()) {
        return nullptr;
    }

    if (addressFilter.empty()) {
        auto msg = m_inbox.front();
        m_inbox.pop_front();
        return msg;
    }

    for (auto it = m_inbox.begin(); it != m_inbox.end(); ++it) {
        if ((*it)->getAddress() == addressFilter) {
            auto msg = *it;
            m_inbox.erase(it);
            return msg;
        }
    }

    // If no matching filter, pop front
    auto msg = m_inbox.front();
    m_inbox.pop_front();
    return msg;
}

size_t SmsMessageRouter::getIncomingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inbox.size();
}

size_t SmsMessageRouter::getSentCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outbox.size();
}

std::vector<std::shared_ptr<Message>> SmsMessageRouter::getOutboxMessages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_outbox;
}

std::shared_ptr<Message> SmsMessageRouter::getLastSentMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_outbox.empty()) return nullptr;
    return m_outbox.back();
}

void SmsMessageRouter::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outbox.clear();
    m_inbox.clear();
    m_listeners.clear();
}

void SmsMessageRouter::registerListener(const std::string& address, MessageListener* listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (listener) {
        m_listeners[address] = listener;
    } else {
        m_listeners.erase(address);
    }
}

void SmsMessageRouter::unregisterListener(const std::string& address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(address);
}

// =========================================================================
// SmsConnection Implementation
// =========================================================================
SmsConnection::SmsConnection(const std::string& url) {
    open(url);
}

SmsConnection::~SmsConnection() {
    close();
}

void SmsConnection::validateHost(const std::string& host) {
    for (size_t i = 0; i < host.length(); ++i) {
        char ch = host[i];
        if (i == 0 && ch == '+') {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            throw std::invalid_argument("Invalid SMS number: " + host);
        }
    }
}

void SmsConnection::validatePort(const std::string& port) {
    for (char ch : port) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            throw std::invalid_argument("Invalid SMS port: " + port);
        }
    }
    try {
        int portValue = std::stoi(port);
        if (portValue < 0 || portValue > 65535) {
            throw std::invalid_argument("Invalid SMS port range: " + port);
        }
    } catch (...) {
        throw std::invalid_argument("Invalid SMS port: " + port);
    }
}

void SmsConnection::parseAndValidateUrl(const std::string& url) {
    m_url = url;
    std::string prefix = "sms://";
    std::string cbsPrefix = "cbs://";

    size_t prefixLen = 0;
    if (url.rfind(prefix, 0) == 0) {
        prefixLen = prefix.length();
    } else if (url.rfind(cbsPrefix, 0) == 0) {
        prefixLen = cbsPrefix.length();
    } else {
        throw std::invalid_argument("Invalid SMS/CBS URL protocol: " + url);
    }

    std::string rawAddr = url.substr(prefixLen);
    m_address = rawAddr;

    size_t portSepIndex = rawAddr.rfind(':');
    std::string portStr;
    if (portSepIndex != std::string::npos) {
        portStr = rawAddr.substr(portSepIndex + 1);
        m_host = rawAddr.substr(0, portSepIndex);
    } else {
        m_host = rawAddr;
    }

    if (!m_host.empty()) {
        validateHost(m_host);
    }
    if (!portStr.empty()) {
        validatePort(portStr);
        m_port = std::stoi(portStr);
    } else {
        m_port = -1;
    }
}

bool SmsConnection::open(const std::string& url, int /*mode*/, bool /*timeouts*/) {
    parseAndValidateUrl(url);
    m_closed = false;
    return true;
}

std::shared_ptr<Message> SmsConnection::newMessage(const std::string& type) {
    return newMessage(type, m_address);
}

std::shared_ptr<Message> SmsConnection::newMessage(const std::string& type, const std::string& address) {
    if (type == MessageConnection::TEXT_MESSAGE) {
        return std::make_shared<TextMessageImpl>(address, 0);
    } else if (type == MessageConnection::BINARY_MESSAGE) {
        return std::make_shared<BinaryMessageImpl>(address, 0);
    } else {
        throw std::invalid_argument("Message type is invalid: " + type);
    }
}

int SmsConnection::numberOfSegments(const Message& message) {
    auto textMsg = dynamic_cast<const TextMessage*>(&message);
    if (textMsg) {
        size_t len = textMsg->getPayloadText().length();
        if (len <= 160) return 1;
        return static_cast<int>(std::ceil(static_cast<double>(len) / 153.0));
    }

    auto binMsg = dynamic_cast<const BinaryMessage*>(&message);
    if (binMsg) {
        size_t len = binMsg->getPayloadData().size();
        if (len <= 140) return 1;
        return static_cast<int>(std::ceil(static_cast<double>(len) / 134.0));
    }

    return 1;
}

std::shared_ptr<Message> SmsConnection::receive() {
    if (m_closed) {
        throw std::runtime_error("MessageConnection is closed");
    }

    auto msg = SmsMessageRouter::instance().popIncoming(m_address);
    if (!msg) {
        // Return default simulated text message if inbox was empty
        auto fallback = std::make_shared<TextMessageImpl>(
            m_address,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        );
        fallback->setPayloadText("sms");
        return fallback;
    }
    return msg;
}

void SmsConnection::send(const std::shared_ptr<Message>& message) {
    if (m_closed) {
        throw std::runtime_error("MessageConnection is closed");
    }
    if (!message) {
        throw std::invalid_argument("Message cannot be null");
    }

    SmsMessageRouter::instance().routeOutgoing(message, this);
}

void SmsConnection::setMessageListener(MessageListener* listener) {
    m_listener = listener;
    SmsMessageRouter::instance().registerListener(m_address, listener);
}

void SmsConnection::close() {
    if (!m_closed) {
        m_closed = true;
        if (m_listener) {
            SmsMessageRouter::instance().unregisterListener(m_address);
            m_listener = nullptr;
        }
    }
}

bool SmsConnection::isOpen() const {
    return !m_closed;
}

} // namespace messaging
} // namespace universal_loader
