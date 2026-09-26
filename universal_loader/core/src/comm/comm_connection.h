#ifndef UNIVERSAL_LOADER_COMM_CONNECTION_H
#define UNIVERSAL_LOADER_COMM_CONNECTION_H

#include "comm_types.h"
#include <deque>
#include <mutex>
#include <memory>

namespace universal_loader {
namespace comm {

class J2ME_API CommConnection {
public:
    explicit CommConnection(const CommConfig& config);
    ~CommConnection();

    // Factory method parsing comm: URL
    static std::shared_ptr<CommConnection> open(const std::string& url);

    // Standard javax.microedition.io.CommConnection methods
    int getBaudRate() const;
    int setBaudRate(int baudrate);

    const CommConfig& getConfig() const { return m_config; }
    bool isOpen() const { return m_isOpen; }
    void close();

    // StreamConnection I/O
    size_t write(const uint8_t* data, size_t len);
    size_t read(uint8_t* outBuf, size_t maxLen);
    size_t available() const;

    // External / host serial communication bridging
    void feedInput(const uint8_t* data, size_t len);
    size_t extractOutput(uint8_t* outBuf, size_t maxLen);

private:
    mutable std::mutex m_mutex;
    CommConfig m_config;
    bool m_isOpen{true};
    std::deque<uint8_t> m_rxBuffer; // Data received from external, ready to be read by MIDlet
    std::deque<uint8_t> m_txBuffer; // Data transmitted by MIDlet, ready to be sent to external
};

} // namespace comm
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_COMM_CONNECTION_H
