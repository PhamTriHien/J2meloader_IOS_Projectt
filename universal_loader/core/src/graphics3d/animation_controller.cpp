#include "animation_controller.h"

namespace universal_loader {
namespace m3g {

void AnimationController::setActiveInterval(int start, int end) {
    m_activationTime = start;
    m_deactivationTime = end;
}

int AnimationController::getActiveIntervalStart() const {
    return m_activationTime;
}

int AnimationController::getActiveIntervalEnd() const {
    return m_deactivationTime;
}

bool AnimationController::isActive(int worldTime) const {
    if (m_activationTime == m_deactivationTime) {
        return true;
    }
    return (worldTime >= m_activationTime && worldTime < m_deactivationTime);
}

void AnimationController::setSpeed(float speed, int worldTime) {
    m_refSequenceTime = getPosition(worldTime);
    m_refWorldTime = worldTime;
    m_speed = speed;
}

float AnimationController::getSpeed() const {
    return m_speed;
}

void AnimationController::setPosition(float sequenceTime, int worldTime) {
    m_refSequenceTime = sequenceTime;
    m_refWorldTime = worldTime;
}

float AnimationController::getPosition(int worldTime) const {
    return m_refSequenceTime + m_speed * static_cast<float>(worldTime - m_refWorldTime);
}

void AnimationController::setWeight(float weight) {
    m_weight = std::max(0.0f, weight);
}

float AnimationController::getWeight() const {
    return m_weight;
}

int AnimationController::getRefWorldTime() const {
    return m_refWorldTime;
}

} // namespace m3g
} // namespace universal_loader
