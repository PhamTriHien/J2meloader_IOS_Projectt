#ifndef UNIVERSAL_LOADER_ANIMATION_TRACK_H
#define UNIVERSAL_LOADER_ANIMATION_TRACK_H

#include <memory>
#include <vector>
#include "../../include/j2me_core.h"
#include "keyframe_sequence.h"
#include "animation_controller.h"

namespace universal_loader {
namespace m3g {

// JSR-184 AnimationTrack Target Property Constants
constexpr int ANIM_ALPHA          = 256;
constexpr int ANIM_AMBIENT_COLOR  = 257;
constexpr int ANIM_COLOR          = 258;
constexpr int ANIM_CROP           = 259;
constexpr int ANIM_DENSITY        = 260;
constexpr int ANIM_DIFFUSE_COLOR  = 261;
constexpr int ANIM_EMISSIVE_COLOR = 262;
constexpr int ANIM_FAR_DISTANCE   = 263;
constexpr int ANIM_FIELD_OF_VIEW  = 264;
constexpr int ANIM_INTENSITY      = 265;
constexpr int ANIM_MORPH_WEIGHTS  = 266;
constexpr int ANIM_NEAR_DISTANCE  = 267;
constexpr int ANIM_ORIENTATION    = 268;
constexpr int ANIM_PICKABILITY    = 269;
constexpr int ANIM_SHININESS      = 270;
constexpr int ANIM_SPECULAR_COLOR = 271;
constexpr int ANIM_SPOT_ANGLE     = 272;
constexpr int ANIM_SPOT_EXPONENT  = 273;
constexpr int ANIM_TRANSLATION    = 274;
constexpr int ANIM_VISIBILITY     = 275;
constexpr int ANIM_SCALE          = 276;

class J2ME_API AnimationTrack {
public:
    AnimationTrack(std::shared_ptr<KeyframeSequence> sequence, int propertyId);
    ~AnimationTrack() = default;

    void setController(std::shared_ptr<AnimationController> controller);
    std::shared_ptr<AnimationController> getController() const;
    std::shared_ptr<KeyframeSequence> getKeyframeSequence() const;
    int getTargetProperty() const;

    bool sample(int worldTime, std::vector<float>& outSample, float& outWeight) const;

private:
    std::shared_ptr<KeyframeSequence> m_sequence;
    std::shared_ptr<AnimationController> m_controller;
    int m_propertyId{0};
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_ANIMATION_TRACK_H
