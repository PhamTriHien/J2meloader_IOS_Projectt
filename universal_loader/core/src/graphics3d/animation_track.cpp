#include "animation_track.h"
#include <cmath>

namespace universal_loader {
namespace m3g {

AnimationTrack::AnimationTrack(std::shared_ptr<KeyframeSequence> sequence, int propertyId)
    : m_sequence(std::move(sequence)),
      m_controller(nullptr),
      m_propertyId(propertyId) {}

void AnimationTrack::setController(std::shared_ptr<AnimationController> controller) {
    m_controller = std::move(controller);
}

std::shared_ptr<AnimationController> AnimationTrack::getController() const {
    return m_controller;
}

std::shared_ptr<KeyframeSequence> AnimationTrack::getKeyframeSequence() const {
    return m_sequence;
}

int AnimationTrack::getTargetProperty() const {
    return m_propertyId;
}

bool AnimationTrack::sample(int worldTime, std::vector<float>& outSample, float& outWeight) const {
    if (!m_sequence || !m_controller) {
        return false;
    }

    if (!m_controller->isActive(worldTime)) {
        return false;
    }

    float weight = m_controller->getWeight();
    if (weight <= 0.0f) {
        return false;
    }

    float seqTime = m_controller->getPosition(worldTime);
    int sampleTime = static_cast<int>(std::round(seqTime));

    int numComp = m_sequence->getComponentCount();
    outSample.resize(numComp, 0.0f);

    if (!m_sequence->sample(sampleTime, outSample.data())) {
        return false;
    }

    outWeight = weight;
    return true;
}

} // namespace m3g
} // namespace universal_loader
