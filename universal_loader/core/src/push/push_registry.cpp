#include "push_registry.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace universal_loader {
namespace push {

PushRegistry& PushRegistry::getInstance() {
    static PushRegistry instance;
    return instance;
}

void PushRegistry::registerConnection(const std::string& connection, const std::string& midlet, const std::string& filter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PushConnectionRegistration reg;
    reg.connection = connection;
    reg.midlet = midlet;
    reg.filter = filter.empty() ? "*" : filter;
    reg.available = false;
    m_connections[connection] = reg;
}

bool PushRegistry::unregisterConnection(const std::string& connection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it != m_connections.end()) {
        m_connections.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> PushRegistry::listConnections(bool availableOnly) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& kv : m_connections) {
        if (!availableOnly || kv.second.available) {
            result.push_back(kv.first);
        }
    }
    return result;
}

std::string PushRegistry::getMIDlet(const std::string& connection) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it != m_connections.end()) {
        return it->second.midlet;
    }
    return "";
}

std::string PushRegistry::getFilter(const std::string& connection) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it != m_connections.end()) {
        return it->second.filter;
    }
    return "";
}

int64_t PushRegistry::registerAlarm(const std::string& midlet, int64_t timeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int64_t previousTime = 0;
    auto it = m_alarms.find(midlet);
    if (it != m_alarms.end()) {
        previousTime = it->second.alarmTimeMs;
    }

    if (timeMs <= 0) {
        // timeMs <= 0 cancels existing alarm
        m_alarms.erase(midlet);
    } else {
        PushAlarmRegistration reg;
        reg.midlet = midlet;
        reg.alarmTimeMs = timeMs;
        reg.fired = false;
        m_alarms[midlet] = reg;
    }
    return previousTime;
}

bool PushRegistry::notifyInboundConnection(const std::string& connection, const std::string& senderAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it == m_connections.end()) {
        return false;
    }

    if (it->second.matchesFilter(senderAddress)) {
        it->second.available = true;
        if (m_wakeCallback) {
            m_wakeCallback(it->second.midlet, connection);
        }
        return true;
    }
    return false;
}

std::vector<std::string> PushRegistry::checkAlarms(int64_t currentTimeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> woken;
    for (auto& kv : m_alarms) {
        if (!kv.second.fired && currentTimeMs >= kv.second.alarmTimeMs) {
            kv.second.fired = true;
            woken.push_back(kv.first);
            if (m_wakeCallback) {
                m_wakeCallback(kv.first, "alarm");
            }
        }
    }
    return woken;
}

void PushRegistry::setPendingConnectionState(const std::string& connection, bool available) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it != m_connections.end()) {
        it->second.available = available;
    }
}

bool PushRegistry::isConnectionAvailable(const std::string& connection) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connection);
    if (it != m_connections.end()) {
        return it->second.available;
    }
    return false;
}

void PushRegistry::setWakeCallback(PushWakeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_wakeCallback = cb;
}

void PushRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.clear();
    m_alarms.clear();
}

bool PushRegistry::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream ofs(filePath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "{\n";
    ofs << "  \"connections\": [\n";
    size_t cIdx = 0;
    for (const auto& kv : m_connections) {
        ofs << "    {\n";
        ofs << "      \"connection\": \"" << kv.second.connection << "\",\n";
        ofs << "      \"midlet\": \"" << kv.second.midlet << "\",\n";
        ofs << "      \"filter\": \"" << kv.second.filter << "\",\n";
        ofs << "      \"available\": " << (kv.second.available ? "true" : "false") << "\n";
        ofs << "    }" << (cIdx + 1 < m_connections.size() ? "," : "") << "\n";
        cIdx++;
    }
    ofs << "  ],\n";

    ofs << "  \"alarms\": [\n";
    size_t aIdx = 0;
    for (const auto& kv : m_alarms) {
        ofs << "    {\n";
        ofs << "      \"midlet\": \"" << kv.second.midlet << "\",\n";
        ofs << "      \"alarmTimeMs\": " << kv.second.alarmTimeMs << ",\n";
        ofs << "      \"fired\": " << (kv.second.fired ? "true" : "false") << "\n";
        ofs << "    }" << (aIdx + 1 < m_alarms.size() ? "," : "") << "\n";
        aIdx++;
    }
    ofs << "  ]\n";
    ofs << "}\n";

    return ofs.good();
}

static std::string extractField(const std::string& block, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = block.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < block.size() && (block[pos] == ' ' || block[pos] == '\t' || block[pos] == '\r' || block[pos] == '\n')) {
        pos++;
    }
    if (pos >= block.size()) return "";

    if (block[pos] == '"') {
        size_t endPos = block.find('"', pos + 1);
        if (endPos != std::string::npos) {
            return block.substr(pos + 1, endPos - pos - 1);
        }
    } else {
        size_t endPos = block.find_first_of(",}\n\r", pos);
        if (endPos != std::string::npos) {
            return block.substr(pos, endPos - pos);
        } else {
            return block.substr(pos);
        }
    }
    return "";
}

bool PushRegistry::loadFromFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.clear();
    m_alarms.clear();

    // Parse connections array
    size_t connStart = content.find("\"connections\":");
    if (connStart != std::string::npos) {
        size_t arrOpen = content.find('[', connStart);
        size_t arrClose = content.find(']', arrOpen);
        if (arrOpen != std::string::npos && arrClose != std::string::npos) {
            std::string arrBlock = content.substr(arrOpen, arrClose - arrOpen + 1);
            size_t cur = 0;
            while ((cur = arrBlock.find('{', cur)) != std::string::npos) {
                size_t objClose = arrBlock.find('}', cur);
                if (objClose == std::string::npos) break;
                std::string itemBlock = arrBlock.substr(cur, objClose - cur + 1);

                std::string conn = extractField(itemBlock, "connection");
                std::string midlet = extractField(itemBlock, "midlet");
                std::string filter = extractField(itemBlock, "filter");
                std::string availStr = extractField(itemBlock, "available");

                if (!conn.empty()) {
                    PushConnectionRegistration reg;
                    reg.connection = conn;
                    reg.midlet = midlet;
                    reg.filter = filter.empty() ? "*" : filter;
                    reg.available = (availStr == "true" || availStr == "1");
                    m_connections[conn] = reg;
                }
                cur = objClose + 1;
            }
        }
    }

    // Parse alarms array
    size_t alarmStart = content.find("\"alarms\":");
    if (alarmStart != std::string::npos) {
        size_t arrOpen = content.find('[', alarmStart);
        size_t arrClose = content.find(']', arrOpen);
        if (arrOpen != std::string::npos && arrClose != std::string::npos) {
            std::string arrBlock = content.substr(arrOpen, arrClose - arrOpen + 1);
            size_t cur = 0;
            while ((cur = arrBlock.find('{', cur)) != std::string::npos) {
                size_t objClose = arrBlock.find('}', cur);
                if (objClose == std::string::npos) break;
                std::string itemBlock = arrBlock.substr(cur, objClose - cur + 1);

                std::string midlet = extractField(itemBlock, "midlet");
                std::string timeStr = extractField(itemBlock, "alarmTimeMs");
                std::string firedStr = extractField(itemBlock, "fired");

                if (!midlet.empty() && !timeStr.empty()) {
                    PushAlarmRegistration reg;
                    reg.midlet = midlet;
                    reg.alarmTimeMs = std::stoll(timeStr);
                    reg.fired = (firedStr == "true" || firedStr == "1");
                    m_alarms[midlet] = reg;
                }
                cur = objClose + 1;
            }
        }
    }

    return true;
}

} // namespace push
} // namespace universal_loader
