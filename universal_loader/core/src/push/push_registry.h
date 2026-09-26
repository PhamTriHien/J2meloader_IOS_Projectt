#ifndef UNIVERSAL_LOADER_PUSH_REGISTRY_H
#define UNIVERSAL_LOADER_PUSH_REGISTRY_H

#include "push_types.h"
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace universal_loader {
namespace push {

class J2ME_API PushRegistry {
public:
    static PushRegistry& getInstance();

    // Standard MIDP 2.0 API methods
    void registerConnection(const std::string& connection, const std::string& midlet, const std::string& filter);
    bool unregisterConnection(const std::string& connection);
    std::vector<std::string> listConnections(bool availableOnly) const;
    std::string getMIDlet(const std::string& connection) const;
    std::string getFilter(const std::string& connection) const;
    int64_t registerAlarm(const std::string& midlet, int64_t timeMs);

    // Runtime and event processing
    bool notifyInboundConnection(const std::string& connection, const std::string& senderAddress);
    std::vector<std::string> checkAlarms(int64_t currentTimeMs);
    void setPendingConnectionState(const std::string& connection, bool available);
    bool isConnectionAvailable(const std::string& connection) const;

    // Optional listener callback when a MIDlet needs to be launched or woken
    using PushWakeCallback = std::function<void(const std::string& midlet, const std::string& reason)>;
    void setWakeCallback(PushWakeCallback cb);

    // Persistence to JSON file
    bool saveToFile(const std::string& filePath) const;
    bool loadFromFile(const std::string& filePath);
    void clear();

private:
    PushRegistry() = default;
    ~PushRegistry() = default;

    mutable std::mutex m_mutex;
    std::map<std::string, PushConnectionRegistration> m_connections;
    std::map<std::string, PushAlarmRegistration> m_alarms;
    PushWakeCallback m_wakeCallback;
};

} // namespace push
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PUSH_REGISTRY_H
