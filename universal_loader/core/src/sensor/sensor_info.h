#ifndef UNIVERSAL_LOADER_SENSOR_INFO_H
#define UNIVERSAL_LOADER_SENSOR_INFO_H

#include "sensor_types.h"
#include "channel_info.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace universal_loader {
namespace sensor {

// SensorInfo Interface & Class (JSR-256 SensorInfo.java)
class J2ME_API SensorInfo {
public:
    SensorInfo(std::vector<ChannelInfo> channelInfos,
               int32_t connectionType,
               std::string contextType,
               std::string description,
               std::string model,
               std::string quantity,
               int32_t maxBufferSize = 1024);
    ~SensorInfo() = default;

    const std::vector<ChannelInfo>& getChannelInfos() const { return m_channelInfos; }
    int32_t getConnectionType() const { return m_connectionType; }
    const std::string& getContextType() const { return m_contextType; }
    const std::string& getDescription() const { return m_description; }
    const std::string& getModel() const { return m_model; }
    const std::string& getQuantity() const { return m_quantity; }
    int32_t getMaxBufferSize() const { return m_maxBufferSize; }
    std::string getUrl() const;

    bool isAvailable() const { return m_available; }
    void setAvailable(bool available) { m_available = available; }

    bool isAvailabilityPushSupported() const { return false; }
    bool isConditionPushSupported() const { return false; }

    std::string getProperty(const std::string& name) const;
    std::vector<std::string> getPropertyNames() const;
    void setProperty(const std::string& name, const std::string& value);

private:
    std::vector<ChannelInfo> m_channelInfos;
    int32_t m_connectionType;
    std::string m_contextType;
    std::string m_description;
    std::string m_model;
    std::string m_quantity;
    int32_t m_maxBufferSize;
    bool m_available{true};
    std::unordered_map<std::string, std::string> m_properties;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SENSOR_INFO_H
