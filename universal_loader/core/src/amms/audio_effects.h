#ifndef UNIVERSAL_LOADER_AMMS_AUDIO_EFFECTS_H
#define UNIVERSAL_LOADER_AMMS_AUDIO_EFFECTS_H

#include "amms_types.h"
#include <string>
#include <vector>
#include <map>

namespace universal_loader {
namespace amms {

class J2ME_API ReverbControl {
public:
    ReverbControl();

    int getReverbLevel() const { return m_level; }
    int setReverbLevel(int levelMb);

    int getReverbTime() const { return m_timeMs; }
    void setReverbTime(int timeMs);

    void setPreset(const std::string& preset);
    std::string getPreset() const { return m_preset; }
    std::vector<std::string> getPresetNames() const;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled{true};
    std::string m_preset{"mediumroom"};
    int m_level{-1000};  // -10.0 dB
    int m_timeMs{1500};  // 1.5 seconds
};

class J2ME_API EqualizerControl {
public:
    EqualizerControl();

    int getNumberOfBands() const { return static_cast<int>(m_bands.size()); }
    int getBand(int freqMilliHz) const;
    int getBandLevel(int band) const;
    void setBandLevel(int band, int levelMb);
    int getCenterFreq(int band) const;
    int getMinBandLevel() const { return -1000; } // -10.0 dB
    int getMaxBandLevel() const { return 1000; }  // +10.0 dB

    void setPreset(const std::string& preset);
    std::string getPreset() const { return m_preset; }
    std::vector<std::string> getPresetNames() const;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    struct BandInfo {
        int centerFreqMilliHz;
        int levelMb;
    };

    bool m_enabled{true};
    std::string m_preset{"flat"};
    std::vector<BandInfo> m_bands;
};

class J2ME_API PanControl {
public:
    PanControl() = default;

    int getPan() const { return m_pan; }
    int setPan(int pan);

private:
    int m_pan{0}; // -100 (left) .. 100 (right)
};

class J2ME_API EffectModule {
public:
    EffectModule();
    ~EffectModule() = default;

    ReverbControl& getReverb() { return m_reverb; }
    const ReverbControl& getReverb() const { return m_reverb; }

    EqualizerControl& getEqualizer() { return m_equalizer; }
    const EqualizerControl& getEqualizer() const { return m_equalizer; }

    PanControl& getPan() { return m_pan; }
    const PanControl& getPan() const { return m_pan; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    bool isEnforced() const { return m_enforced; }
    void setEnforced(bool enforced) { m_enforced = enforced; }

    int getScope() const { return m_scope; }
    void setScope(int scope) { m_scope = scope; }

private:
    bool m_enabled{true};
    bool m_enforced{false};
    int m_scope{SCOPE_LIVE_ONLY};

    ReverbControl m_reverb;
    EqualizerControl m_equalizer;
    PanControl m_pan;
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_AUDIO_EFFECTS_H
