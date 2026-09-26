#include "audio_effects.h"
#include <algorithm>
#include <cmath>

namespace universal_loader {
namespace amms {

// -------------------------------------------------------------
// ReverbControl
// -------------------------------------------------------------
ReverbControl::ReverbControl()
    : m_enabled(true)
    , m_preset("mediumroom")
    , m_level(-1000)
    , m_timeMs(1500)
{
}

int ReverbControl::setReverbLevel(int levelMb) {
    m_level = std::clamp(levelMb, -10000, 0);
    return m_level;
}

void ReverbControl::setReverbTime(int timeMs) {
    m_timeMs = std::clamp(timeMs, 100, 20000);
}

void ReverbControl::setPreset(const std::string& preset) {
    m_preset = preset;
    if (preset == "smallroom") {
        m_level = -1200;
        m_timeMs = 800;
    } else if (preset == "mediumroom") {
        m_level = -1000;
        m_timeMs = 1500;
    } else if (preset == "largeroom") {
        m_level = -800;
        m_timeMs = 2500;
    } else if (preset == "hall") {
        m_level = -600;
        m_timeMs = 4000;
    } else if (preset == "plate") {
        m_level = -900;
        m_timeMs = 2000;
    }
}

std::vector<std::string> ReverbControl::getPresetNames() const {
    return {"smallroom", "mediumroom", "largeroom", "hall", "plate"};
}

// -------------------------------------------------------------
// EqualizerControl
// -------------------------------------------------------------
EqualizerControl::EqualizerControl()
    : m_enabled(true)
    , m_preset("flat")
{
    m_bands.push_back({60000, 0});      // 60 Hz
    m_bands.push_back({230000, 0});     // 230 Hz
    m_bands.push_back({910000, 0});     // 910 Hz
    m_bands.push_back({3600000, 0});    // 3.6 kHz
    m_bands.push_back({14000000, 0});   // 14 kHz
}

int EqualizerControl::getBand(int freqMilliHz) const {
    int bestBand = 0;
    int minDiff = std::abs(m_bands[0].centerFreqMilliHz - freqMilliHz);
    for (size_t i = 1; i < m_bands.size(); ++i) {
        int diff = std::abs(m_bands[i].centerFreqMilliHz - freqMilliHz);
        if (diff < minDiff) {
            minDiff = diff;
            bestBand = static_cast<int>(i);
        }
    }
    return bestBand;
}

int EqualizerControl::getBandLevel(int band) const {
    if (band >= 0 && band < static_cast<int>(m_bands.size())) {
        return m_bands[band].levelMb;
    }
    return 0;
}

void EqualizerControl::setBandLevel(int band, int levelMb) {
    if (band >= 0 && band < static_cast<int>(m_bands.size())) {
        m_bands[band].levelMb = std::clamp(levelMb, getMinBandLevel(), getMaxBandLevel());
    }
}

int EqualizerControl::getCenterFreq(int band) const {
    if (band >= 0 && band < static_cast<int>(m_bands.size())) {
        return m_bands[band].centerFreqMilliHz;
    }
    return 0;
}

void EqualizerControl::setPreset(const std::string& preset) {
    m_preset = preset;
    if (preset == "flat") {
        for (auto& b : m_bands) b.levelMb = 0;
    } else if (preset == "bass_boost") {
        if (m_bands.size() >= 5) {
            m_bands[0].levelMb = 600;
            m_bands[1].levelMb = 400;
            m_bands[2].levelMb = 0;
            m_bands[3].levelMb = 0;
            m_bands[4].levelMb = 0;
        }
    } else if (preset == "rock") {
        if (m_bands.size() >= 5) {
            m_bands[0].levelMb = 500;
            m_bands[1].levelMb = 300;
            m_bands[2].levelMb = -200;
            m_bands[3].levelMb = 400;
            m_bands[4].levelMb = 600;
        }
    } else if (preset == "vocal") {
        if (m_bands.size() >= 5) {
            m_bands[0].levelMb = -200;
            m_bands[1].levelMb = 0;
            m_bands[2].levelMb = 600;
            m_bands[3].levelMb = 300;
            m_bands[4].levelMb = -100;
        }
    }
}

std::vector<std::string> EqualizerControl::getPresetNames() const {
    return {"flat", "bass_boost", "rock", "vocal"};
}

// -------------------------------------------------------------
// PanControl
// -------------------------------------------------------------
int PanControl::setPan(int pan) {
    m_pan = std::clamp(pan, -100, 100);
    return m_pan;
}

// -------------------------------------------------------------
// EffectModule
// -------------------------------------------------------------
EffectModule::EffectModule()
    : m_enabled(true)
    , m_enforced(false)
    , m_scope(SCOPE_LIVE_ONLY)
{
}

void EffectModule::setEnabled(bool enabled) {
    m_enabled = enabled;
    m_reverb.setEnabled(enabled);
    m_equalizer.setEnabled(enabled);
}

} // namespace amms
} // namespace universal_loader
