#ifndef UNIVERSAL_LOADER_CHANNEL_INFO_H
#define UNIVERSAL_LOADER_CHANNEL_INFO_H

#include "sensor_types.h"
#include <string>
#include <vector>

namespace universal_loader {
namespace sensor {

// ChannelInfo Interface & Class (JSR-256 ChannelInfo.java)
class J2ME_API ChannelInfo {
public:
    ChannelInfo(std::string name,
                int32_t dataType,
                Unit unit,
                int32_t scale = 1,
                float accuracy = -1.0f,
                std::vector<MeasurementRange> ranges = {});
    ~ChannelInfo() = default;

    const std::string& getName() const { return m_name; }
    int32_t getDataType() const { return m_dataType; }
    const Unit& getUnit() const { return m_unit; }
    int32_t getScale() const { return m_scale; }
    float getAccuracy() const { return m_accuracy; }
    const std::vector<MeasurementRange>& getMeasurementRanges() const { return m_ranges; }

private:
    std::string m_name;
    int32_t m_dataType;
    Unit m_unit;
    int32_t m_scale;
    float m_accuracy;
    std::vector<MeasurementRange> m_ranges;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CHANNEL_INFO_H
