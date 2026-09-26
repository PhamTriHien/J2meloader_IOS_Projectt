#ifndef UNIVERSAL_LOADER_CHANNEL_H
#define UNIVERSAL_LOADER_CHANNEL_H

#include "channel_info.h"
#include "condition.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace universal_loader {
namespace sensor {

class SensorConnection;

// Channel Interface & Class (JSR-256 Channel.java)
class J2ME_API Channel {
public:
    Channel(ChannelInfo info, std::string sensorUrl = "");
    ~Channel() = default;

    const ChannelInfo& getChannelInfo() const { return m_info; }
    std::string getChannelUrl() const;

    void addCondition(ConditionListener* listener, std::shared_ptr<Condition> condition);
    void removeCondition(ConditionListener* listener, std::shared_ptr<Condition> condition);
    void removeConditionListener(ConditionListener* listener);
    void removeAllConditions();

    std::vector<std::shared_ptr<Condition>> getConditions(ConditionListener* listener) const;

    // Internal notification
    void notifySample(SensorConnection* connection, double value);

private:
    ChannelInfo m_info;
    std::string m_sensorUrl;
    mutable std::mutex m_mutex;
    std::unordered_map<ConditionListener*, std::vector<std::shared_ptr<Condition>>> m_conditions;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CHANNEL_H
