#include "channel_info.h"

namespace universal_loader {
namespace sensor {

ChannelInfo::ChannelInfo(std::string name,
                         int32_t dataType,
                         Unit unit,
                         int32_t scale,
                         float accuracy,
                         std::vector<MeasurementRange> ranges)
    : m_name(std::move(name)),
      m_dataType(dataType),
      m_unit(std::move(unit)),
      m_scale(scale),
      m_accuracy(accuracy),
      m_ranges(std::move(ranges)) {
    if (m_name.empty()) {
        throw std::invalid_argument("Channel name cannot be empty");
    }
}

} // namespace sensor
} // namespace universal_loader
