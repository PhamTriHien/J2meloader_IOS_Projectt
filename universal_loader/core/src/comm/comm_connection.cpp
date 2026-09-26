#include "comm_connection.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace universal_loader {
namespace comm {

static std::string toLower(const std::string& str) {
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return res;
}

bool CommUrlParser::parse(const std::string& url, CommConfig& outConfig) {
    outConfig = CommConfig{};
    // Expected format: comm:<port>[;key=val;key=val...]
    const std::string prefix = "comm:";
    if (url.rfind(prefix, 0) != 0) {
        return false;
    }

    std::string remainder = url.substr(prefix.size());
    if (remainder.empty()) {
        return false;
    }

    size_t semiPos = remainder.find(';');
    if (semiPos == std::string::npos) {
        outConfig.port = remainder;
        return true;
    }

    outConfig.port = remainder.substr(0, semiPos);
    std::string params = remainder.substr(semiPos + 1);

    std::stringstream ss(params);
    std::string item;
    while (std::getline(ss, item, ';')) {
        size_t eqPos = item.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = toLower(item.substr(0, eqPos));
        std::string val = toLower(item.substr(eqPos + 1));

        if (key == "baudrate") {
            try {
                outConfig.baudRate = std::stoi(val);
            } catch (...) {}
        } else if (key == "bitsperchar") {
            try {
                int b = std::stoi(val);
                if (b == 7 || b == 8) outConfig.bitsPerChar = b;
            } catch (...) {}
        } else if (key == "stopbits") {
            try {
                int s = std::stoi(val);
                if (s == 1 || s == 2) outConfig.stopBits = s;
            } catch (...) {}
        } else if (key == "parity") {
            if (val == "even") {
                outConfig.parity = CommParity::Even;
            } else if (val == "odd") {
                outConfig.parity = CommParity::Odd;
            } else {
                outConfig.parity = CommParity::None;
            }
        } else if (key == "autocts") {
            outConfig.autoCts = (val == "on" || val == "1" || val == "true");
        } else if (key == "autorts") {
            outConfig.autoRts = (val == "on" || val == "1" || val == "true");
        }
    }

    return true;
}

CommConnection::CommConnection(const CommConfig& config)
    : m_config(config)
    , m_isOpen(true)
{
}

CommConnection::~CommConnection() {
    close();
}

std::shared_ptr<CommConnection> CommConnection::open(const std::string& url) {
    CommConfig cfg;
    if (!CommUrlParser::parse(url, cfg)) {
        return nullptr;
    }
    return std::make_shared<CommConnection>(cfg);
}

int CommConnection::getBaudRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.baudRate;
}

int CommConnection::setBaudRate(int baudrate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const int standardRates[] = {
        1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400
    };

    bool valid = false;
    for (int rate : standardRates) {
        if (baudrate == rate) {
            valid = true;
            break;
        }
    }

    if (valid) {
        m_config.baudRate = baudrate;
    }
    return m_config.baudRate;
}

void CommConnection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isOpen = false;
    m_rxBuffer.clear();
    m_txBuffer.clear();
}

size_t CommConnection::write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !data || len == 0) {
        return 0;
    }
    for (size_t i = 0; i < len; ++i) {
        m_txBuffer.push_back(data[i]);
    }
    return len;
}

size_t CommConnection::read(uint8_t* outBuf, size_t maxLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !outBuf || maxLen == 0) {
        return 0;
    }
    size_t count = std::min(maxLen, m_rxBuffer.size());
    for (size_t i = 0; i < count; ++i) {
        outBuf[i] = m_rxBuffer.front();
        m_rxBuffer.pop_front();
    }
    return count;
}

size_t CommConnection::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rxBuffer.size();
}

void CommConnection::feedInput(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isOpen || !data || len == 0) {
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        m_rxBuffer.push_back(data[i]);
    }
}

size_t CommConnection::extractOutput(uint8_t* outBuf, size_t maxLen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!outBuf || maxLen == 0) {
        return 0;
    }
    size_t count = std::min(maxLen, m_txBuffer.size());
    for (size_t i = 0; i < count; ++i) {
        outBuf[i] = m_txBuffer.front();
        m_txBuffer.pop_front();
    }
    return count;
}

} // namespace comm
} // namespace universal_loader
