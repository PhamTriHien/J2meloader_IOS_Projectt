#include "sensor_info.h"

namespace universal_loader {
namespace sensor {

SensorInfo::SensorInfo(std::vector<ChannelInfo> channelInfos,
                       int32_t connectionType,
                       std::string contextType,
                       std::string description,
                       std::string model,
                       std::string quantity,
                       int32_t maxBufferSize)
    : m_channelInfos(std::move(channelInfos)),
      m_connectionType(connectionType),
      m_contextType(std::move(contextType)),
      m_description(std::move(description)),
      m_model(std::move(model)),
      m_quantity(std::move(quantity)),
      m_maxBufferSize(maxBufferSize) {
    if (m_quantity.empty()) {
        throw std::invalid_argument("Sensor quantity cannot be empty");
    }
}

std::string SensorInfo::getUrl() const {
    std::string url = "sensor:" + m_quantity;
    if (!m_contextType.empty()) {
        url += ";contextType=" + m_contextType;
    }
    if (!m_model.empty() && m_model != "default") {
        url += ";model=" + m_model;
    }
    return url;
}

std::string SensorInfo::getProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return it->second;
    }
    throw std::invalid_argument("Property not found: " + name);
}

std::vector<std::string> SensorInfo::getPropertyNames() const {
    std::vector<std::string> names;
    names.reserve(m_properties.size());
    for (const auto& pair : m_properties) {
        names.push_back(pair.first);
    }
    return names;
}

void SensorInfo::setProperty(const std::string& name, const std::string& value) {
    m_properties[name] = value;
}

} // namespace sensor
} // namespace universal_loader
