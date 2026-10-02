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
    // 1. Cổ điển J2ME Feature Phones
    {"96 x 65 (Nokia 3510i / 3310)", 96, 65},
    {"128 x 128 (Vuông / Siemens C65 / Nokia S40)", 128, 128},
    {"128 x 160 (Nokia 6030 / 2600 / S40 v1-v2)", 128, 160},
    {"130 x 130 (Motorola C350)", 130, 130},
    {"176 x 208 (Nokia 6600 / 7610 / N70 / N-Gage)", 176, 208},
    {"176 x 220 (Sony Ericsson K700/K750 / Moto V3)", 176, 220},
    {"208 x 208 (Nokia 6230 / 8800)", 208, 208},
    {"240 x 320 (QVGA Chuẩn MIDP 2.0 / Nokia N73/N95/6300/X2)", 240, 320},
    {"320 x 240 (QVGA Ngang / Nokia E71/E72/C3/E63)", 320, 240},
    {"240 x 400 (WQVGA / Samsung Star / LG Cookie)", 240, 400},
    {"400 x 240 (WQVGA Ngang / LG Arena / Samsung Omnia)", 400, 240},
    {"240 x 432 (Sony Ericsson Aino)", 240, 432},
    {"352 x 416 (Nokia N80 / N90 / S60 v3 High-Res)", 352, 416},
    {"320 x 480 (HVGA / iPhone 3GS / HTC Magic)", 320, 480},
    {"480 x 320 (HVGA Ngang)", 480, 320},

    // 2. Symbian^3, Meego & Touch Phones
    {"360 x 640 (nHD 16:9 Dọc / Nokia 5800 / N97 / 5230)", 360, 640},
    {"640 x 360 (nHD 16:9 Ngang / Nokia N8 / C7 / E7 / 808)", 640, 360},
    {"480 x 640 (VGA Dọc / Pocket PC / BlackBerry Bold)", 480, 640},
    {"640 x 480 (VGA Ngang / Nokia E6 / HTC Touch Pro)", 640, 480},
    {"480 x 800 (WVGA 5:3 Dọc / Nokia Lumia / Galaxy S1)", 480, 800},
    {"800 x 480 (WVGA 5:3 Ngang / Nokia N900 / Galaxy S)", 800, 480},
    {"480 x 854 (FWVGA 16:9 Dọc / Sony Xperia Arc)", 480, 854},
    {"854 x 480 (FWVGA 16:9 Ngang / Xperia Play)", 854, 480},

    // 3. Smartphone Hiện Đại (16:9, 18:9, 19.5:9, 20:9, 21:9)
    {"360 x 720 (HD+ 18:9 Dọc)", 360, 720},
    {"720 x 360 (HD+ 18:9 Ngang)", 720, 360},
    {"360 x 780 (FHD+ 19.5:9 Dọc / iPhone 12-16)", 360, 780},
    {"780 x 360 (FHD+ 19.5:9 Ngang / iPhone 12-16)", 780, 360},
    {"360 x 800 (FHD+ 20:9 Dọc / Galaxy S21-S24 / Pixel / Xiaomi)", 360, 800},
    {"800 x 360 (FHD+ 20:9 Ngang / Chuẩn 2400x1080 Mobile Widescreen)", 800, 360},
    {"360 x 840 (CinemaWide 21:9 Dọc / Sony Xperia 1-5)", 360, 840},
    {"840 x 360 (CinemaWide 21:9 Ngang / Sony Xperia)", 840, 360},
    {"540 x 960 (qHD Dọc)", 540, 960},
    {"960 x 540 (qHD Ngang)", 960, 540},
    {"720 x 1280 (HD 720p Dọc)", 720, 1280},
    {"1280 x 720 (HD 720p Ngang)", 1280, 720},
    {"1080 x 1920 (FHD 1080p Dọc)", 1080, 1920},
    {"1920 x 1080 (FHD 1080p Ngang)", 1920, 1080},
    {"1080 x 2400 (FHD+ 20:9 Gốc 2400x1080 Dọc)", 1080, 2400},
    {"2400 x 1080 (FHD+ 20:9 Gốc 2400x1080 Ngang)", 2400, 1080},
    {"1024 x 600 (Tablet 7 inch WSVGA)", 1024, 600},
    {"1024 x 768 (iPad 4:3 XGA)", 1024, 768},
    {"1280 x 800 (Tablet 16:10 WXGA)", 1280, 800}
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
