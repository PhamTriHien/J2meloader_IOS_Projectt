#include "data.h"
#include <stdexcept>

namespace universal_loader {
namespace sensor {

Data::Data(ChannelInfo info)
    : m_info(std::move(info)) {
}

Data::Data(ChannelInfo info, std::vector<double> doubleValues, std::vector<int64_t> timestamps)
    : m_info(std::move(info)),
      m_doubleValues(std::move(doubleValues)),
      m_timestamps(std::move(timestamps)),
      m_uncertainties(m_doubleValues.size(), 0.0f),
      m_validity(m_doubleValues.size(), true) {
}

Data::Data(ChannelInfo info, std::vector<int32_t> intValues, std::vector<int64_t> timestamps)
    : m_info(std::move(info)),
      m_intValues(std::move(intValues)),
      m_timestamps(std::move(timestamps)),
      m_uncertainties(m_intValues.size(), 0.0f),
      m_validity(m_intValues.size(), true) {
}

Data::Data(ChannelInfo info, std::vector<std::string> objectValues, std::vector<int64_t> timestamps)
    : m_info(std::move(info)),
      m_objectValues(std::move(objectValues)),
      m_timestamps(std::move(timestamps)),
      m_uncertainties(m_objectValues.size(), 0.0f),
      m_validity(m_objectValues.size(), true) {
}

const std::vector<double>& Data::getDoubleValues() const {
    if (m_info.getDataType() != CHANNEL_TYPE_DOUBLE) {
        throw std::logic_error("Channel data type is not double");
    }
    return m_doubleValues;
}

const std::vector<int32_t>& Data::getIntValues() const {
    if (m_info.getDataType() != CHANNEL_TYPE_INT) {
        throw std::logic_error("Channel data type is not int");
    }
    return m_intValues;
}

const std::vector<std::string>& Data::getObjectValues() const {
    if (m_info.getDataType() != CHANNEL_TYPE_OBJECT) {
        throw std::logic_error("Channel data type is not object");
    }
    return m_objectValues;
}

int64_t Data::getTimestamp(size_t index) const {
    if (index >= m_timestamps.size()) {
        throw std::out_of_range("Index out of bounds for timestamp");
    }
    return m_timestamps[index];
}

float Data::getUncertainty(size_t index) const {
    if (index >= m_uncertainties.size()) {
        return 0.0f;
    }
    return m_uncertainties[index];
}

bool Data::isValid(size_t index) const {
    if (index >= m_validity.size()) {
        return true;
    }
    return m_validity[index];
}

size_t Data::size() const {
    if (m_info.getDataType() == CHANNEL_TYPE_DOUBLE) return m_doubleValues.size();
    if (m_info.getDataType() == CHANNEL_TYPE_INT) return m_intValues.size();
    if (m_info.getDataType() == CHANNEL_TYPE_OBJECT) return m_objectValues.size();
    return 0;
}

void Data::addDoubleSample(double value, int64_t timestamp, float uncertainty, bool valid) {
    m_doubleValues.push_back(value);
    m_timestamps.push_back(timestamp);
    m_uncertainties.push_back(uncertainty);
    m_validity.push_back(valid);
}

void Data::addIntSample(int32_t value, int64_t timestamp, float uncertainty, bool valid) {
    m_intValues.push_back(value);
    m_timestamps.push_back(timestamp);
    m_uncertainties.push_back(uncertainty);
    m_validity.push_back(valid);
}

} // namespace sensor
} // namespace universal_loader
