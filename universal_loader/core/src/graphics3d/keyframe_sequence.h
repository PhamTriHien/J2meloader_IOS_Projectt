#ifndef UNIVERSAL_LOADER_KEYFRAME_SEQUENCE_H
#define UNIVERSAL_LOADER_KEYFRAME_SEQUENCE_H

#include <vector>
#include <cstdint>
#include "../../include/j2me_core.h"
#include "math3d.h"

namespace universal_loader {
namespace m3g {

// JSR-184 KeyframeSequence Interpolation Constants
constexpr int INTERP_LINEAR = 176;
constexpr int INTERP_STEP   = 192;
constexpr int INTERP_SLERP  = 193;
constexpr int INTERP_SPLINE = 194;

// JSR-184 KeyframeSequence Repeat Mode Constants
constexpr int REPEAT_CONSTANT = 192;
constexpr int REPEAT_LOOP     = 193;

class J2ME_API KeyframeSequence {
public:
    KeyframeSequence(int numKeyframes, int numComponents, int interpolation);
    ~KeyframeSequence() = default;

    void setDuration(int duration);
    int getDuration() const;

    void setRepeatMode(int mode);
    int getRepeatMode() const;

    int getInterpolationType() const;
    int getComponentCount() const;
    int getKeyframeCount() const;

    void setValidRange(int first, int last);
    void setKeyframe(int index, int time, const float* value);
    bool getKeyframe(int index, int* outTime, float* outValue) const;

    bool sample(int sequenceTime, float* outVal) const;

private:
    int m_numKeyframes{0};
    int m_numComponents{0};
    int m_interpolation{INTERP_LINEAR};
    int m_repeatMode{REPEAT_CONSTANT};
    int m_duration{0};
    int m_firstValid{0};
    int m_lastValid{0};

    std::vector<int> m_times;
    std::vector<float> m_values;
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_KEYFRAME_SEQUENCE_H
