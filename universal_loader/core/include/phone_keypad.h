#pragma once

#include "j2me_core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace universal_loader {
namespace input {

// =========================================================================
// Standard J2ME Canvas Key Constants (matching javax.microedition.lcdui.Canvas)
// =========================================================================
constexpr int KEY_POUND      = 35;  // '#'
constexpr int KEY_STAR       = 42;  // '*'
constexpr int KEY_NUM0       = 48;  // '0'
constexpr int KEY_NUM1       = 49;  // '1'
constexpr int KEY_NUM2       = 50;  // '2'
constexpr int KEY_NUM3       = 51;  // '3'
constexpr int KEY_NUM4       = 52;  // '4'
constexpr int KEY_NUM5       = 53;  // '5'
constexpr int KEY_NUM6       = 54;  // '6'
constexpr int KEY_NUM7       = 55;  // '7'
constexpr int KEY_NUM8       = 56;  // '8'
constexpr int KEY_NUM9       = 57;  // '9'

constexpr int KEY_UP         = -1;
constexpr int KEY_DOWN       = -2;
constexpr int KEY_LEFT       = -3;
constexpr int KEY_RIGHT      = -4;
constexpr int KEY_FIRE       = -5;
constexpr int KEY_SOFT_LEFT  = -6;
constexpr int KEY_SOFT_RIGHT = -7;
constexpr int KEY_CLEAR      = -8;
constexpr int KEY_SEND       = -10;
constexpr int KEY_END        = -11;

// Game Actions
constexpr int GAME_ACTION_UP    = 1;
constexpr int GAME_ACTION_LEFT  = 2;
constexpr int GAME_ACTION_RIGHT = 5;
constexpr int GAME_ACTION_DOWN  = 6;
constexpr int GAME_ACTION_FIRE  = 8;
constexpr int GAME_ACTION_GAME_A= 9;
constexpr int GAME_ACTION_GAME_B= 10;
constexpr int GAME_ACTION_GAME_C= 11;
constexpr int GAME_ACTION_GAME_D= 12;

// Layout Types
enum KeyLayoutType {
    LAYOUT_DEFAULT  = 0, // Standard MIDP
    LAYOUT_SIEMENS  = 1, // Siemens SX1 / C65 / M55
    LAYOUT_MOTOROLA = 2, // Motorola E398 / V3 / V300
    LAYOUT_CUSTOM   = 3  // Custom Remapped
};

// Siemens Key Codes
constexpr int SIEMENS_KEY_UP         = -59;
constexpr int SIEMENS_KEY_DOWN       = -60;
constexpr int SIEMENS_KEY_LEFT       = -61;
constexpr int SIEMENS_KEY_RIGHT      = -62;
constexpr int SIEMENS_KEY_SOFT_LEFT  = -1;
constexpr int SIEMENS_KEY_SOFT_RIGHT = -4;

// Motorola Key Codes
constexpr int MOTOROLA_KEY_UP         = -1;
constexpr int MOTOROLA_KEY_DOWN       = -6;
constexpr int MOTOROLA_KEY_LEFT       = -2;
constexpr int MOTOROLA_KEY_RIGHT      = -5;
constexpr int MOTOROLA_KEY_FIRE       = -20;
constexpr int MOTOROLA_KEY_SOFT_LEFT  = -21;
constexpr int MOTOROLA_KEY_SOFT_RIGHT = -22;

/**
 * @brief Key mapping and conversion engine matching KeyMapper.java.
 */
class J2ME_API KeyMapper {
private:
    static KeyLayoutType s_layoutType;
    static std::unordered_map<int, int> s_customMapping;

public:
    static void setLayout(KeyLayoutType layout);
    static KeyLayoutType getLayout();

    static int convertKeyCode(int midpKeyCode);
    static int getGameAction(int keyCode);
    static int getKeyCode(int gameAction);
    static std::string getKeyName(int keyCode);

    static void setCustomMapping(int fromKeyCode, int toKeyCode);
    static void clearCustomMappings();
    static void reset();
};

/**
 * @brief Represents a single touch button on the virtual keypad.
 */
struct J2ME_API VirtualKey {
    int keyCode{0};
    float x{0.0f};      // Pixel coordinate X
    float y{0.0f};      // Pixel coordinate Y
    float width{0.0f};  // Pixel width
    float height{0.0f}; // Pixel height
    std::string label;
    bool isPressed{false};
};

/**
 * @brief Virtual phone keypad layout generator and hit tester.
 * Matching VirtualKeyboard.java from upstream J2ME-Loader.
 */
class J2ME_API VirtualKeypadEngine {
public:
    enum KeyboardType {
        TYPE_CUSTOM       = 0,
        TYPE_PHONE        = 1, // Standard 12-key ITU-T + Softkeys
        TYPE_PHONE_ARROWS = 2, // Phone grid + D-Pad
        TYPE_NUM_ARR      = 3, // Numbers + Arrows
        TYPE_ARR_NUM      = 4, // Arrows + Numbers
        TYPE_NUMBERS      = 5, // 12-key numbers only
        TYPE_ARROWS       = 6  // D-Pad + Fire + Softkeys only
    };

private:
    int m_screenWidth{240};
    int m_screenHeight{320};
    KeyboardType m_type{TYPE_PHONE_ARROWS};
    std::vector<VirtualKey> m_keys;
    int m_activeTouchKeyCode{0};

    void buildLayout();

public:
    VirtualKeypadEngine();
    explicit VirtualKeypadEngine(int screenWidth, int screenHeight, KeyboardType type = TYPE_PHONE_ARROWS);

    void setDimensions(int width, int height);
    void setType(KeyboardType type);
    KeyboardType getType() const { return m_type; }

    const std::vector<VirtualKey>& getKeys() const { return m_keys; }
    int hitTest(float touchX, float touchY) const;

    // Handles touch event (0 = ACTION_DOWN, 1 = ACTION_UP, 2 = ACTION_MOVE)
    // Returns midpKeyCode that changed, or 0 if none
    bool onTouchEvent(int action, float touchX, float touchY, int& outKeyCode, bool& outIsPressed);
};

/**
 * @brief Multi-Tap T9 predictive character cycling engine for phone keypad text input.
 */
class J2ME_API MultiTapInputEngine {
public:
    enum InputMode {
        MODE_LOWERCASE = 0,
        MODE_UPPERCASE = 1,
        MODE_NUMERIC   = 2
    };

private:
    InputMode m_mode{MODE_LOWERCASE};
    int m_currentKey{-1};
    size_t m_cycleIndex{0};
    int64_t m_lastPressTimeMs{0};
    int64_t m_commitTimeoutMs{1000};
    char m_pendingChar{'\0'};

    std::string getCandidatesForKey(int keyCode) const;

public:
    MultiTapInputEngine() = default;

    void setMode(InputMode mode);
    InputMode getMode() const { return m_mode; }
    void cycleMode();

    void setCommitTimeoutMs(int64_t ms) { m_commitTimeoutMs = ms; }
    int64_t getCommitTimeoutMs() const { return m_commitTimeoutMs; }

    /**
     * @brief Process key press event.
     * @param midpKeyCode Pressed key code.
     * @param currentTimeMs Current timestamp in milliseconds.
     * @param outCommittedChar Output character committed from previous cycle (if any).
     * @param outHasCommitted True if previous char was committed.
     * @param outPendingChar Output pending active character currently cycling.
     * @return True if key was recognized as multi-tap input.
     */
    bool handleKey(int midpKeyCode,
                   int64_t currentTimeMs,
                   char& outCommittedChar,
                   bool& outHasCommitted,
                   char& outPendingChar);

    /**
     * @brief Checks if pending character has timed out and should be committed.
     */
    bool checkTimeout(int64_t currentTimeMs, char& outCommittedChar);

    /**
     * @brief Manually commits pending character.
     */
    bool commitPending(char& outCommittedChar);

    void reset();
};

} // namespace input
} // namespace universal_loader
