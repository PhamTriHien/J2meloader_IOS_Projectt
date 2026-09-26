#pragma once

#include "j2me_core.h"
#include "wma_message.h"
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace universal_loader {
namespace messaging {

/**
 * @brief MessageConnection interface matching javax.wireless.messaging.MessageConnection.
 */
class J2ME_API MessageConnection {
public:
    static constexpr const char* BINARY_MESSAGE = "binary";
    static constexpr const char* MULTIPART_MESSAGE = "multipart";
    static constexpr const char* TEXT_MESSAGE = "text";

    virtual ~MessageConnection() = default;

    virtual std::shared_ptr<Message> newMessage(const std::string& type) = 0;
    virtual std::shared_ptr<Message> newMessage(const std::string& type, const std::string& address) = 0;
    virtual int numberOfSegments(const Message& message) = 0;
    virtual std::shared_ptr<Message> receive() = 0;
    virtual void send(const std::shared_ptr<Message>& message) = 0;
    virtual void setMessageListener(MessageListener* listener) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
};

/**
 * @brief Callback function type for intercepted SMS messages.
 */
using SmsInterceptCallback = void (*)(const char* address,
                                      const char* textPayload,
                                      const uint8_t* binaryPayload,
                                      size_t binaryLen,
                                      void* userData);

/**
 * @brief Simulated SMS Router managing inboxes, outboxes, listeners, and native callbacks.
 */
class J2ME_API SmsMessageRouter {
private:
    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<Message>> m_outbox;
    std::deque<std::shared_ptr<Message>> m_inbox;
    std::unordered_map<std::string, MessageListener*> m_listeners;
    SmsInterceptCallback m_interceptCallback{nullptr};
    void* m_callbackUserData{nullptr};
    bool m_autoReplyEnabled{true};
    std::string m_autoReplyTemplate{"OK"};

    SmsMessageRouter() = default;

public:
    static SmsMessageRouter& instance();

    void setInterceptCallback(SmsInterceptCallback callback, void* userData);
    void setAutoReply(bool enabled, const std::string& replyTemplate = "OK");

    void routeOutgoing(const std::shared_ptr<Message>& message, class SmsConnection* sourceConn);
    void injectIncoming(const std::shared_ptr<Message>& message);

    std::shared_ptr<Message> popIncoming(const std::string& addressFilter = "");
    size_t getIncomingCount() const;

    size_t getSentCount() const;
    std::vector<std::shared_ptr<Message>> getOutboxMessages() const;
    std::shared_ptr<Message> getLastSentMessage() const;
    void clearAll();

    void registerListener(const std::string& address, MessageListener* listener);
    void unregisterListener(const std::string& address);
};

/**
 * @brief SMS Connection implementation matching org.microemu.cldc.sms.Connection.
 */
class J2ME_API SmsConnection : public MessageConnection {
private:
    std::string m_url;
    std::string m_address;
    std::string m_host;
    int m_port{-1};
    bool m_closed{false};
    MessageListener* m_listener{nullptr};

    void parseAndValidateUrl(const std::string& url);
    static void validateHost(const std::string& host);
    static void validatePort(const std::string& port);

public:
    SmsConnection() = default;
    explicit SmsConnection(const std::string& url);
    ~SmsConnection() override;

    bool open(const std::string& url, int mode = 0, bool timeouts = false);

    // MessageConnection interface
    std::shared_ptr<Message> newMessage(const std::string& type) override;
    std::shared_ptr<Message> newMessage(const std::string& type, const std::string& address) override;
    int numberOfSegments(const Message& message) override;
    std::shared_ptr<Message> receive() override;
    void send(const std::shared_ptr<Message>& message) override;
    void setMessageListener(MessageListener* listener) override;

    // GcfConnection interface
    void close() override;
    bool isOpen() const override;

    // Properties
    std::string getUrl() const { return m_url; }
    std::string getAddress() const { return m_address; }
    std::string getHost() const { return m_host; }
    int getPort() const { return m_port; }
};

} // namespace messaging
} // namespace universal_loader
