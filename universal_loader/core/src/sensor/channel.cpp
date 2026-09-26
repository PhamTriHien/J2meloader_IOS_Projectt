#include "channel.h"
#include <algorithm>

namespace universal_loader {
namespace sensor {

Channel::Channel(ChannelInfo info, std::string sensorUrl)
    : m_info(std::move(info)), m_sensorUrl(std::move(sensorUrl)) {
}

std::string Channel::getChannelUrl() const {
    if (m_sensorUrl.empty()) {
        return "";
    }
    return m_sensorUrl + "?channel=" + m_info.getName();
}

void Channel::addCondition(ConditionListener* listener, std::shared_ptr<Condition> condition) {
    if (!listener || !condition) {
        throw std::invalid_argument("Listener and condition cannot be null");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& vec = m_conditions[listener];
    if (std::find(vec.begin(), vec.end(), condition) == vec.end()) {
        vec.push_back(std::move(condition));
    }
}

void Channel::removeCondition(ConditionListener* listener, std::shared_ptr<Condition> condition) {
    if (!listener || !condition) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_conditions.find(listener);
    if (it != m_conditions.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), condition), vec.end());
        if (vec.empty()) {
            m_conditions.erase(it);
        }
    }
}

void Channel::removeConditionListener(ConditionListener* listener) {
    if (!listener) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_conditions.erase(listener);
}

void Channel::removeAllConditions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_conditions.clear();
}

std::vector<std::shared_ptr<Condition>> Channel::getConditions(ConditionListener* listener) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!listener) return {};
    auto it = m_conditions.find(listener);
    if (it != m_conditions.end()) {
        return it->second;
    }
    return {};
}

void Channel::notifySample(SensorConnection* connection, double value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_conditions) {
        ConditionListener* listener = pair.first;
        if (!listener) continue;
        for (const auto& cond : pair.second) {
            if (cond && cond->isMet(value)) {
                listener->conditionMet(connection, this, cond.get(), value);
            }
        }
    }
}

} // namespace sensor
} // namespace universal_loader
