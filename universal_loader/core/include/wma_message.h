#pragma once

#include "j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace universal_loader {
namespace messaging {

/**
 * @brief Base interface for wireless messages (JSR-120 WMA).
 * Directly matches javax.wireless.messaging.Message from upstream J2ME-Loader.
 */
class J2ME_API Message {
public:
    virtual ~Message() = default;

    virtual std::string getAddress() const = 0;
    virtual void setAddress(const std::string& address) = 0;
    virtual int64_t getTimestamp() const = 0; // Milliseconds since Unix epoch, 0 if unset
};

/**
 * @brief Text payload message interface (JSR-120 WMA).
 * Directly matches javax.wireless.messaging.TextMessage from upstream J2ME-Loader.
 */
class J2ME_API TextMessage : public virtual Message {
public:
    virtual ~TextMessage() = default;

    virtual std::string getPayloadText() const = 0;
    virtual void setPayloadText(const std::string& text) = 0;
};

/**
 * @brief Binary payload message interface (JSR-120 WMA).
 * Directly matches javax.wireless.messaging.BinaryMessage from upstream J2ME-Loader.
 */
class J2ME_API BinaryMessage : public virtual Message {
public:
    virtual ~BinaryMessage() = default;

    virtual std::vector<uint8_t> getPayloadData() const = 0;
    virtual void setPayloadData(const std::vector<uint8_t>& data) = 0;
    virtual void setPayloadData(const uint8_t* data, size_t length) = 0;
};

/**
 * @brief Concrete base implementation for WMA messages.
 * Matches org.microemu.cldc.sms.MessageImpl from upstream J2ME-Loader.
 */
class J2ME_API MessageImpl : public virtual Message {
protected:
    std::string m_address;
    int64_t m_timestamp{0};

public:
    explicit MessageImpl(std::string address = "", int64_t timestamp = 0);
    ~MessageImpl() override = default;

    std::string getAddress() const override;
    void setAddress(const std::string& address) override;
    int64_t getTimestamp() const override;
};

/**
 * @brief Concrete TextMessage implementation.
 * Matches org.microemu.cldc.sms.TextMessageImpl from upstream J2ME-Loader.
 */
class J2ME_API TextMessageImpl : public MessageImpl, public TextMessage {
private:
    std::string m_textPayload;

public:
    explicit TextMessageImpl(std::string address = "", int64_t timestamp = 0);
    ~TextMessageImpl() override = default;

    std::string getPayloadText() const override;
    void setPayloadText(const std::string& text) override;

    // Disambiguate Message methods from virtual inheritance
    std::string getAddress() const override { return MessageImpl::getAddress(); }
    void setAddress(const std::string& address) override { MessageImpl::setAddress(address); }
    int64_t getTimestamp() const override { return MessageImpl::getTimestamp(); }
};

/**
 * @brief Concrete BinaryMessage implementation.
 * Matches org.microemu.cldc.sms.BinaryMessageImpl from upstream J2ME-Loader.
 */
class J2ME_API BinaryMessageImpl : public MessageImpl, public BinaryMessage {
private:
    std::vector<uint8_t> m_binaryPayload;

public:
    explicit BinaryMessageImpl(std::string address = "", int64_t timestamp = 0);
    ~BinaryMessageImpl() override = default;

    std::vector<uint8_t> getPayloadData() const override;
    void setPayloadData(const std::vector<uint8_t>& data) override;
    void setPayloadData(const uint8_t* data, size_t length) override;

    // Disambiguate Message methods from virtual inheritance
    std::string getAddress() const override { return MessageImpl::getAddress(); }
    void setAddress(const std::string& address) override { MessageImpl::setAddress(address); }
    int64_t getTimestamp() const override { return MessageImpl::getTimestamp(); }
};

/**
 * @brief Listener for asynchronous incoming message notifications.
 * Matches javax.wireless.messaging.MessageListener from upstream J2ME-Loader.
 */
class MessageConnection;

class J2ME_API MessageListener {
public:
    virtual ~MessageListener() = default;
    virtual void notifyIncomingMessage(MessageConnection* conn) = 0;
};

} // namespace messaging
} // namespace universal_loader
