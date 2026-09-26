#ifndef UNIVERSAL_LOADER_SENSOR_DATA_H
#define UNIVERSAL_LOADER_SENSOR_DATA_H

#include "channel_info.h"
#include <vector>
#include <cstdint>

namespace universal_loader {
namespace sensor {

// Data Interface & Class (JSR-256 Data.java)
class J2ME_API Data {
public:
    explicit Data(ChannelInfo info);
    Data(ChannelInfo info, std::vector<double> doubleValues, std::vector<int64_t> timestamps);
    Data(ChannelInfo info, std::vector<int32_t> intValues, std::vector<int64_t> timestamps);
    Data(ChannelInfo info, std::vector<std::string> objectValues, std::vector<int64_t> timestamps);
    ~Data() = default;

    const ChannelInfo& getChannelInfo() const { return m_info; }
    const std::vector<double>& getDoubleValues() const;
    const std::vector<int32_t>& getIntValues() const;
    const std::vector<std::string>& getObjectValues() const;

    int64_t getTimestamp(size_t index) const;
    float getUncertainty(size_t index) const;
    bool isValid(size_t index) const;

    size_t size() const;

    void addDoubleSample(double value, int64_t timestamp = 0, float uncertainty = 0.0f, bool valid = true);
    void addIntSample(int32_t value, int64_t timestamp = 0, float uncertainty = 0.0f, bool valid = true);

private:
    ChannelInfo m_info;
    std::vector<double> m_doubleValues;
    std::vector<int32_t> m_intValues;
    std::vector<std::string> m_objectValues;
    std::vector<int64_t> m_timestamps;
    std::vector<float> m_uncertainties;
    std::vector<bool> m_validity;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SENSOR_DATA_H
