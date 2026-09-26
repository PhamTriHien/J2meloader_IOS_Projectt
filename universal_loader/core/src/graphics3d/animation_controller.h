#ifndef UNIVERSAL_LOADER_ANIMATION_CONTROLLER_H
#define UNIVERSAL_LOADER_ANIMATION_CONTROLLER_H

#include <cstdint>
#include <algorithm>
#include "../../include/j2me_core.h"

namespace universal_loader {
namespace m3g {

class J2ME_API AnimationController {
public:
    AnimationController() = default;
    ~AnimationController() = default;

    void setActiveInterval(int start, int end);
    int getActiveIntervalStart() const;
    int getActiveIntervalEnd() const;
    bool isActive(int worldTime) const;

    void setSpeed(float speed, int worldTime);
    float getSpeed() const;

    void setPosition(float sequenceTime, int worldTime);
    float getPosition(int worldTime) const;

    void setWeight(float weight);
    float getWeight() const;

    int getRefWorldTime() const;

private:
    int m_activationTime{0};
    int m_deactivationTime{0};
    float m_weight{1.0f};
    float m_speed{1.0f};
    int m_refWorldTime{0};
    float m_refSequenceTime{0.0f};
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_ANIMATION_CONTROLLER_H
