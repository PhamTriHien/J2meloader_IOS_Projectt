#ifndef UNIVERSAL_LOADER_SENSOR_CONNECTION_H
#define UNIVERSAL_LOADER_SENSOR_CONNECTION_H

#include "sensor_types.h"
#include "sensor_info.h"
#include "channel.h"
#include "data.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <deque>

namespace universal_loader {
namespace sensor {

// Data Listener Interface (JSR-256 DataListener.java)
class J2ME_API DataListener {
public:
    virtual ~DataListener() = default;
    virtual void dataReceived(SensorConnection* sensor, const std::vector<Data>& data, bool isBuffer) = 0;
};

// SensorConnection Interface & Class (JSR-256 SensorConnection.java)
class J2ME_API SensorConnection {
public:
    explicit SensorConnection(SensorInfo info);
    ~SensorConnection();

    int32_t getState() const { return m_state.load(); }
    void close();

    const SensorInfo& getSensorInfo() const { return m_info; }

    Channel* getChannel(const ChannelInfo& channelInfo);
    Channel* getChannel(size_t index);
    Channel* getChannel(const std::string& name);
    size_t getChannelCount() const { return m_channels.size(); }

    void setDataListener(DataListener* listener,
                         int32_t bufferSize,
                         int64_t bufferingPeriod = 0,
                         bool isTimestampIncluded = true,
                         bool isUncertaintyIncluded = false,
                         bool isValidityIncluded = true);
    void removeDataListener();

    std::vector<Data> getData(int32_t bufferSize);

    // Host platform sample injection
    void pushSamples(const std::vector<double>& channelValues, int64_t timestamp = 0);

private:
    SensorInfo m_info;
    std::atomic<int32_t> m_state{SENSOR_STATE_OPENED};
    std::vector<std::unique_ptr<Channel>> m_channels;

    mutable std::mutex m_mutex;
    DataListener* m_dataListener{nullptr};
    int32_t m_bufferSize{1};
    int64_t m_bufferingPeriod{0};
    bool m_includeTimestamp{true};
    bool m_includeUncertainty{false};
    bool m_includeValidity{true};

    struct Sample {
        std::vector<double> values;
        int64_t timestamp;
    };
    std::deque<Sample> m_sampleHistory;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SENSOR_CONNECTION_H
