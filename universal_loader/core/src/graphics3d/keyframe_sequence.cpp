#include "keyframe_sequence.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace universal_loader {
namespace m3g {

KeyframeSequence::KeyframeSequence(int numKeyframes, int numComponents, int interpolation)
    : m_numKeyframes(std::max(1, numKeyframes)),
      m_numComponents(std::clamp(numComponents, 1, 4)),
      m_interpolation(interpolation),
      m_repeatMode(REPEAT_CONSTANT),
      m_duration(0),
      m_firstValid(0),
      m_lastValid(std::max(0, numKeyframes - 1)) {
    m_times.resize(m_numKeyframes, 0);
    m_values.resize(static_cast<size_t>(m_numKeyframes) * m_numComponents, 0.0f);
}

void KeyframeSequence::setDuration(int duration) {
    m_duration = std::max(0, duration);
}

int KeyframeSequence::getDuration() const {
    return m_duration;
}

void KeyframeSequence::setRepeatMode(int mode) {
    if (mode == REPEAT_CONSTANT || mode == REPEAT_LOOP) {
        m_repeatMode = mode;
    }
}

int KeyframeSequence::getRepeatMode() const {
    return m_repeatMode;
}

int KeyframeSequence::getInterpolationType() const {
    return m_interpolation;
}

int KeyframeSequence::getComponentCount() const {
    return m_numComponents;
}

int KeyframeSequence::getKeyframeCount() const {
    return m_numKeyframes;
}

void KeyframeSequence::setValidRange(int first, int last) {
    if (first >= 0 && first <= last && last < m_numKeyframes) {
        m_firstValid = first;
        m_lastValid = last;
    }
}

void KeyframeSequence::setKeyframe(int index, int time, const float* value) {
    if (index >= 0 && index < m_numKeyframes) {
        m_times[index] = time;
        if (value != nullptr) {
            size_t offset = static_cast<size_t>(index) * m_numComponents;
            for (int c = 0; c < m_numComponents; ++c) {
                m_values[offset + c] = value[c];
            }
        }
    }
}

bool KeyframeSequence::getKeyframe(int index, int* outTime, float* outValue) const {
    if (index < 0 || index >= m_numKeyframes) {
        return false;
    }
    if (outTime != nullptr) {
        *outTime = m_times[index];
    }
    if (outValue != nullptr) {
        size_t offset = static_cast<size_t>(index) * m_numComponents;
        for (int c = 0; c < m_numComponents; ++c) {
            outValue[c] = m_values[offset + c];
        }
    }
    return true;
}

bool KeyframeSequence::sample(int sequenceTime, float* outVal) const {
    if (outVal == nullptr || m_numKeyframes <= 0) {
        return false;
    }

    if (m_firstValid == m_lastValid) {
        size_t offset = static_cast<size_t>(m_firstValid) * m_numComponents;
        for (int c = 0; c < m_numComponents; ++c) {
            outVal[c] = m_values[offset + c];
        }
        return true;
    }

    int t = sequenceTime;
    if (m_repeatMode == REPEAT_LOOP) {
        if (m_duration <= 0) {
            return false;
        }
        t = t % m_duration;
        if (t < 0) {
            t += m_duration;
        }
        if (t < m_times[m_firstValid]) {
            t += m_duration;
        }
    } else { // REPEAT_CONSTANT
        if (t <= m_times[m_firstValid]) {
            size_t offset = static_cast<size_t>(m_firstValid) * m_numComponents;
            for (int c = 0; c < m_numComponents; ++c) {
                outVal[c] = m_values[offset + c];
            }
            return true;
        }
        if (t >= m_times[m_lastValid]) {
            size_t offset = static_cast<size_t>(m_lastValid) * m_numComponents;
            for (int c = 0; c < m_numComponents; ++c) {
                outVal[c] = m_values[offset + c];
            }
            return true;
        }
    }

    int idx0 = m_firstValid;
    int idx1 = m_firstValid;
    float s = 0.0f;

    if (m_repeatMode == REPEAT_LOOP && t >= m_times[m_lastValid]) {
        idx0 = m_lastValid;
        idx1 = m_firstValid;
        int dt = (m_duration - m_times[m_lastValid]) + m_times[m_firstValid];
        int elapsed = t - m_times[m_lastValid];
        s = (dt > 0) ? (static_cast<float>(elapsed) / static_cast<float>(dt)) : 0.0f;
    } else {
        while (idx0 + 1 <= m_lastValid && m_times[idx0 + 1] <= t) {
            idx0++;
        }
        idx1 = std::min(m_lastValid, idx0 + 1);
        int dt = m_times[idx1] - m_times[idx0];
        int elapsed = t - m_times[idx0];
        s = (dt > 0) ? (static_cast<float>(elapsed) / static_cast<float>(dt)) : 0.0f;
    }

    s = std::clamp(s, 0.0f, 1.0f);

    size_t off0 = static_cast<size_t>(idx0) * m_numComponents;
    size_t off1 = static_cast<size_t>(idx1) * m_numComponents;

    if (m_interpolation == INTERP_STEP || idx0 == idx1 || s <= 0.0f) {
        for (int c = 0; c < m_numComponents; ++c) {
            outVal[c] = m_values[off0 + c];
        }
        return true;
    }

    if (s >= 1.0f) {
        for (int c = 0; c < m_numComponents; ++c) {
            outVal[c] = m_values[off1 + c];
        }
        return true;
    }

    if (m_interpolation == INTERP_SLERP && m_numComponents == 4) {
        graphics3d::Quaternion q0(m_values[off0], m_values[off0 + 1], m_values[off0 + 2], m_values[off0 + 3]);
        graphics3d::Quaternion q1(m_values[off1], m_values[off1 + 1], m_values[off1 + 2], m_values[off1 + 3]);
        graphics3d::Quaternion qr = graphics3d::Quaternion::slerp(q0, q1, s);
        outVal[0] = qr.x;
        outVal[1] = qr.y;
        outVal[2] = qr.z;
        outVal[3] = qr.w;
        return true;
    }

    // Default: LINEAR or Hermite SPLINE linear component
    for (int c = 0; c < m_numComponents; ++c) {
        float v0 = m_values[off0 + c];
        float v1 = m_values[off1 + c];
        outVal[c] = v0 + s * (v1 - v0);
    }

    return true;
}

} // namespace m3g
} // namespace universal_loader
