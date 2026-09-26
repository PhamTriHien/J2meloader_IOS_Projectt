#include "sensor_connection.h"
#include <chrono>

namespace universal_loader {
namespace sensor {

static int64_t currentEpochMillis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

SensorConnection::SensorConnection(SensorInfo info)
    : m_info(std::move(info)) {
    std::string sensorUrl = m_info.getUrl();
    for (const auto& chInfo : m_info.getChannelInfos()) {
        m_channels.push_back(std::make_unique<Channel>(chInfo, sensorUrl));
    }
}

SensorConnection::~SensorConnection() {
    close();
}

void SensorConnection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = SENSOR_STATE_CLOSED;
    m_dataListener = nullptr;
    m_sampleHistory.clear();
}

Channel* SensorConnection::getChannel(const ChannelInfo& channelInfo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& ch : m_channels) {
        if (ch->getChannelInfo().getName() == channelInfo.getName()) {
            return ch.get();
        }
    }
    return nullptr;
}

Channel* SensorConnection::getChannel(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_channels.size()) {
        return m_channels[index].get();
    }
    return nullptr;
}

Channel* SensorConnection::getChannel(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& ch : m_channels) {
        if (ch->getChannelInfo().getName() == name) {
            return ch.get();
        }
    }
    return nullptr;
}

void SensorConnection::setDataListener(DataListener* listener,
                                       int32_t bufferSize,
                                       int64_t bufferingPeriod,
                                       bool isTimestampIncluded,
                                       bool isUncertaintyIncluded,
                                       bool isValidityIncluded) {
    if (!listener) {
        throw std::invalid_argument("DataListener cannot be null");
    }
    if (bufferSize < 1 || bufferSize > m_info.getMaxBufferSize()) {
        throw std::invalid_argument("Invalid bufferSize for DataListener");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataListener = listener;
    m_bufferSize = bufferSize;
    m_bufferingPeriod = bufferingPeriod;
    m_includeTimestamp = isTimestampIncluded;
    m_includeUncertainty = isUncertaintyIncluded;
    m_includeValidity = isValidityIncluded;
    m_state = SENSOR_STATE_LISTENING;
}

void SensorConnection::removeDataListener() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataListener = nullptr;
    if (m_state == SENSOR_STATE_LISTENING) {
        m_state = SENSOR_STATE_OPENED;
    }
}

std::vector<Data> SensorConnection::getData(int32_t bufferSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == SENSOR_STATE_CLOSED) {
        throw std::logic_error("SensorConnection is closed");
    }
    if (bufferSize < 1) {
        throw std::invalid_argument("bufferSize must be >= 1");
    }

    std::vector<Data> result;
    result.reserve(m_channels.size());
    for (const auto& ch : m_channels) {
        result.emplace_back(ch->getChannelInfo());
    }

    if (m_sampleHistory.empty()) {
        // Generate a default 0.0 sample if no data pushed yet
        int64_t now = currentEpochMillis();
        for (size_t c = 0; c < result.size(); ++c) {
            const auto& chInfo = result[c].getChannelInfo();
            if (chInfo.getDataType() == CHANNEL_TYPE_DOUBLE) {
                result[c].addDoubleSample(0.0, now, 0.0f, true);
            } else if (chInfo.getDataType() == CHANNEL_TYPE_INT) {
                result[c].addIntSample(0, now, 0.0f, true);
            }
        }
        return result;
    }

    int32_t count = std::min(bufferSize, static_cast<int32_t>(m_sampleHistory.size()));
    size_t startIdx = m_sampleHistory.size() - count;

    for (size_t i = startIdx; i < m_sampleHistory.size(); ++i) {
        const auto& sample = m_sampleHistory[i];
        for (size_t c = 0; c < result.size() && c < sample.values.size(); ++c) {
            const auto& chInfo = result[c].getChannelInfo();
            double val = sample.values[c];
            if (chInfo.getDataType() == CHANNEL_TYPE_DOUBLE) {
                result[c].addDoubleSample(val, sample.timestamp, 0.0f, true);
            } else if (chInfo.getDataType() == CHANNEL_TYPE_INT) {
                result[c].addIntSample(static_cast<int32_t>(val * chInfo.getScale()), sample.timestamp, 0.0f, true);
            }
        }
    }

    return result;
}

void SensorConnection::pushSamples(const std::vector<double>& channelValues, int64_t timestamp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == SENSOR_STATE_CLOSED) return;

    if (timestamp == 0) {
        timestamp = currentEpochMillis();
    }

    Sample sample;
    sample.values = channelValues;
    sample.timestamp = timestamp;

    m_sampleHistory.push_back(sample);
    while (m_sampleHistory.size() > static_cast<size_t>(m_info.getMaxBufferSize())) {
        m_sampleHistory.pop_front();
    }

    // Trigger channel conditions
    for (size_t c = 0; c < m_channels.size() && c < channelValues.size(); ++c) {
        m_channels[c]->notifySample(this, channelValues[c]);
    }

    // Dispatch to DataListener if ready
    if (m_dataListener && m_state == SENSOR_STATE_LISTENING) {
        if (static_cast<int32_t>(m_sampleHistory.size()) >= m_bufferSize) {
            std::vector<Data> batch;
            batch.reserve(m_channels.size());
            for (const auto& ch : m_channels) {
                batch.emplace_back(ch->getChannelInfo());
            }

            int32_t take = m_bufferSize;
            size_t startIdx = m_sampleHistory.size() - take;
            for (size_t i = startIdx; i < m_sampleHistory.size(); ++i) {
                const auto& s = m_sampleHistory[i];
                for (size_t c = 0; c < batch.size() && c < s.values.size(); ++c) {
                    const auto& chInfo = batch[c].getChannelInfo();
                    double val = s.values[c];
                    int64_t ts = m_includeTimestamp ? s.timestamp : 0;
                    if (chInfo.getDataType() == CHANNEL_TYPE_DOUBLE) {
                        batch[c].addDoubleSample(val, ts, 0.0f, true);
                    } else if (chInfo.getDataType() == CHANNEL_TYPE_INT) {
                        batch[c].addIntSample(static_cast<int32_t>(val * chInfo.getScale()), ts, 0.0f, true);
                    }
                }
            }

            m_dataListener->dataReceived(this, batch, m_bufferSize > 1);
        }
    }
}

} // namespace sensor
} // namespace universal_loader
