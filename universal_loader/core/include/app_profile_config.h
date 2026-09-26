#ifndef UNIVERSAL_LOADER_APP_PROFILE_CONFIG_H
#define UNIVERSAL_LOADER_APP_PROFILE_CONFIG_H

#include "j2me_core.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <memory>

namespace universal_loader::config {

struct ResolutionPreset {
    const char* name;
    int width;
    int height;
};

inline const ResolutionPreset PRESET_RESOLUTIONS[] = {
    {"128 x 128 (Square / Siemens C65)", 128, 128},
    {"128 x 160 (Nokia Series 40 v1/v2)", 128, 160},
    {"176 x 208 (Nokia Series 60 v1/v2 / N-Gage)", 176, 208},
    {"176 x 220 (Sony Ericsson K700/K750 / Moto V3)", 176, 220},
    {"208 x 208 (Nokia 6230 / 8800)", 208, 208},
    {"240 x 320 (QVGA Standard MIDP 2.0)", 240, 320},
    {"320 x 240 (QVGA Landscape / Nokia E71)", 320, 240},
    {"352 x 416 (Nokia Series 60 v3 High-Res)", 352, 416},
    {"360 x 640 (nHD Touch / Nokia 5800 / N97)", 360, 640},
    {"480 x 800 (WVGA Touch)", 480, 800},
    {"640 x 360 (nHD Landscape)", 640, 360},
    {"800 x 480 (WVGA Landscape)", 800, 480}
};

inline constexpr size_t PRESET_RESOLUTION_COUNT = sizeof(PRESET_RESOLUTIONS) / sizeof(PRESET_RESOLUTIONS[0]);

class J2ME_API ProfileModel {
public:
    static constexpr int VERSION = 3;

    int version{VERSION};
    int screenWidth{240};
    int screenHeight{320};
    uint32_t screenBackgroundColor{0xD0D0D0};
    int screenScaleRatio{100};
    int orientation{0}; // 0: default, 1: auto, 2: port, 3: land
    bool screenScaleToFit{true};
    bool screenKeepAspectRatio{true};
    int screenScaleType{1}; // 0: none, 1: fit, 2: fill
    int screenGravity{1};   // 0: left, 1: center, 2: right, etc.
    bool screenFilter{false};
    bool immediateMode{false};
    bool hwAcceleration{false};
    int graphicsMode{1}; // 0: software, 1: hw_gles, 2: hw_view
    bool parallelRedrawScreen{false};
    bool showFps{false};
    int fpsLimit{0}; // 0 = unlimited
    bool forceFullscreen{false};
    int fontSizeSmall{18};
    int fontSizeMedium{22};
    int fontSizeLarge{26};
    bool fontApplyDimensions{false};
    bool fontAA{true};
    bool touchInput{true};
    bool showKeyboard{true};
    int vkType{0}; // 0: custom, 1: phone, 2: phone_arrows, 3: num_arr, 4: arr_num, 5: numbers, 6: arrows
    int vkButtonShape{2}; // 0: oval, 1: rect, 2: round_rect
    int vkAlpha{64};
    bool vkForceOpacity{false};
    bool vkFeedback{false};
    int vkHideDelay{0};
    uint32_t vkBgColor{0xD0D0D0};
    uint32_t vkBgColorSelected{0x000080};
    uint32_t vkFgColor{0x000080};
    uint32_t vkFgColorSelected{0xFFFFFF};
    uint32_t vkOutlineColor{0xFFFFFF};
    int keyCodesLayout{0}; // 0: Nokia/SE, 1: Siemens, 2: Motorola, 3: Custom
    std::map<int, int> keyCodeMap;
    std::map<int, int> keyMappings;
    std::string systemProperties;

    ProfileModel();

    std::string serializeJson() const;
    bool deserializeJson(const std::string& json);
    void migrateVersion();
};

class J2ME_API ConfigDirs {
public:
    std::string emulatorDir;
    std::string dataDir;
    std::string configsDir;
    std::string profilesDir;
    std::string shadersDir;
    std::string fsInternalDir;
    std::string fsExternalDir;

    void init(const std::string& rootDir);
};

class J2ME_API ProfilesManager {
public:
    static std::string getDefaultSystemProperties();
    static bool loadConfig(const std::string& filePath, ProfileModel& outModel);
    static bool saveConfig(const std::string& filePath, const ProfileModel& model);
    static std::vector<std::string> getProfilesList(const std::string& profilesDir);
};

} // namespace universal_loader::config

#endif // UNIVERSAL_LOADER_APP_PROFILE_CONFIG_H
