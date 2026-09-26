#include "phone_keypad.h"
#include <algorithm>
#include <cctype>

namespace universal_loader {
namespace input {

// =========================================================================
// KeyMapper Implementation
// =========================================================================
KeyLayoutType KeyMapper::s_layoutType = LAYOUT_DEFAULT;
std::unordered_map<int, int> KeyMapper::s_customMapping;

void KeyMapper::setLayout(KeyLayoutType layout) {
    s_layoutType = layout;
}

KeyLayoutType KeyMapper::getLayout() {
    return s_layoutType;
}

int KeyMapper::convertKeyCode(int midpKeyCode) {
    auto it = s_customMapping.find(midpKeyCode);
    if (it != s_customMapping.end()) {
        return it->second;
    }

    if (s_layoutType == LAYOUT_SIEMENS) {
        switch (midpKeyCode) {
            case KEY_UP:         return SIEMENS_KEY_UP;
            case KEY_DOWN:       return SIEMENS_KEY_DOWN;
            case KEY_LEFT:       return SIEMENS_KEY_LEFT;
            case KEY_RIGHT:      return SIEMENS_KEY_RIGHT;
            case KEY_SOFT_LEFT:  return SIEMENS_KEY_SOFT_LEFT;
            case KEY_SOFT_RIGHT: return SIEMENS_KEY_SOFT_RIGHT;
            default: break;
        }
    } else if (s_layoutType == LAYOUT_MOTOROLA) {
        switch (midpKeyCode) {
            case KEY_UP:         return MOTOROLA_KEY_UP;
            case KEY_DOWN:       return MOTOROLA_KEY_DOWN;
            case KEY_LEFT:       return MOTOROLA_KEY_LEFT;
            case KEY_RIGHT:      return MOTOROLA_KEY_RIGHT;
            case KEY_FIRE:       return MOTOROLA_KEY_FIRE;
            case KEY_SOFT_LEFT:  return MOTOROLA_KEY_SOFT_LEFT;
            case KEY_SOFT_RIGHT: return MOTOROLA_KEY_SOFT_RIGHT;
            default: break;
        }
    }

    return midpKeyCode;
}

int KeyMapper::getGameAction(int keyCode) {
    if (s_layoutType == LAYOUT_SIEMENS) {
        switch (keyCode) {
            case SIEMENS_KEY_UP:    return GAME_ACTION_UP;
            case SIEMENS_KEY_DOWN:  return GAME_ACTION_DOWN;
            case SIEMENS_KEY_LEFT:  return GAME_ACTION_LEFT;
            case SIEMENS_KEY_RIGHT: return GAME_ACTION_RIGHT;
            case KEY_FIRE:          return GAME_ACTION_FIRE;
            case KEY_NUM2: return GAME_ACTION_UP;
            case KEY_NUM8: return GAME_ACTION_DOWN;
            case KEY_NUM4: return GAME_ACTION_LEFT;
            case KEY_NUM6: return GAME_ACTION_RIGHT;
            case KEY_NUM5: return GAME_ACTION_FIRE;
            case KEY_NUM7: return GAME_ACTION_GAME_A;
            case KEY_NUM9: return GAME_ACTION_GAME_B;
            case KEY_STAR: return GAME_ACTION_GAME_C;
            case KEY_POUND: return GAME_ACTION_GAME_D;
            default: break;
        }
    } else if (s_layoutType == LAYOUT_MOTOROLA) {
        switch (keyCode) {
            case MOTOROLA_KEY_UP:    return GAME_ACTION_UP;
            case MOTOROLA_KEY_DOWN:  return GAME_ACTION_DOWN;
            case MOTOROLA_KEY_LEFT:  return GAME_ACTION_LEFT;
            case MOTOROLA_KEY_RIGHT: return GAME_ACTION_RIGHT;
            case MOTOROLA_KEY_FIRE:  return GAME_ACTION_FIRE;
            case KEY_NUM2: return GAME_ACTION_UP;
            case KEY_NUM8: return GAME_ACTION_DOWN;
            case KEY_NUM4: return GAME_ACTION_LEFT;
            case KEY_NUM6: return GAME_ACTION_RIGHT;
            case KEY_NUM5: return GAME_ACTION_FIRE;
            case KEY_NUM7: return GAME_ACTION_GAME_A;
            case KEY_NUM9: return GAME_ACTION_GAME_B;
            case KEY_STAR: return GAME_ACTION_GAME_C;
            case KEY_POUND: return GAME_ACTION_GAME_D;
            default: break;
        }
    }

    switch (keyCode) {
        case KEY_UP:    case KEY_NUM2: return GAME_ACTION_UP;
        case KEY_DOWN:  case KEY_NUM8: return GAME_ACTION_DOWN;
        case KEY_LEFT:  case KEY_NUM4: return GAME_ACTION_LEFT;
        case KEY_RIGHT: case KEY_NUM6: return GAME_ACTION_RIGHT;
        case KEY_FIRE:  case KEY_NUM5: return GAME_ACTION_FIRE;
        case KEY_NUM7: return GAME_ACTION_GAME_A;
        case KEY_NUM9: return GAME_ACTION_GAME_B;
        case KEY_STAR: return GAME_ACTION_GAME_C;
        case KEY_POUND: return GAME_ACTION_GAME_D;
        default: return 0;
    }
}

int KeyMapper::getKeyCode(int gameAction) {
    if (s_layoutType == LAYOUT_SIEMENS) {
        switch (gameAction) {
            case GAME_ACTION_UP:    return SIEMENS_KEY_UP;
            case GAME_ACTION_DOWN:  return SIEMENS_KEY_DOWN;
            case GAME_ACTION_LEFT:  return SIEMENS_KEY_LEFT;
            case GAME_ACTION_RIGHT: return SIEMENS_KEY_RIGHT;
            default: break;
        }
    } else if (s_layoutType == LAYOUT_MOTOROLA) {
        switch (gameAction) {
            case GAME_ACTION_UP:    return MOTOROLA_KEY_UP;
            case GAME_ACTION_DOWN:  return MOTOROLA_KEY_DOWN;
            case GAME_ACTION_LEFT:  return MOTOROLA_KEY_LEFT;
            case GAME_ACTION_RIGHT: return MOTOROLA_KEY_RIGHT;
            case GAME_ACTION_FIRE:  return MOTOROLA_KEY_FIRE;
            default: break;
        }
    }

    switch (gameAction) {
        case GAME_ACTION_UP:    return KEY_UP;
        case GAME_ACTION_DOWN:  return KEY_DOWN;
        case GAME_ACTION_LEFT:  return KEY_LEFT;
        case GAME_ACTION_RIGHT: return KEY_RIGHT;
        case GAME_ACTION_FIRE:  return KEY_FIRE;
        case GAME_ACTION_GAME_A: return KEY_NUM7;
        case GAME_ACTION_GAME_B: return KEY_NUM9;
        case GAME_ACTION_GAME_C: return KEY_STAR;
        case GAME_ACTION_GAME_D: return KEY_POUND;
        default: return 0;
    }
}

std::string KeyMapper::getKeyName(int keyCode) {
    if (s_layoutType == LAYOUT_SIEMENS) {
        switch (keyCode) {
            case SIEMENS_KEY_UP:         return "UP";
            case SIEMENS_KEY_DOWN:       return "DOWN";
            case SIEMENS_KEY_LEFT:       return "LEFT";
            case SIEMENS_KEY_RIGHT:      return "RIGHT";
            case SIEMENS_KEY_SOFT_LEFT:  return "SOFT1";
            case SIEMENS_KEY_SOFT_RIGHT: return "SOFT2";
            default: break;
        }
    } else if (s_layoutType == LAYOUT_MOTOROLA) {
        switch (keyCode) {
            case MOTOROLA_KEY_UP:         return "UP";
            case MOTOROLA_KEY_DOWN:       return "DOWN";
            case MOTOROLA_KEY_LEFT:       return "LEFT";
            case MOTOROLA_KEY_RIGHT:      return "RIGHT";
            case MOTOROLA_KEY_FIRE:       return "SELECT";
            case MOTOROLA_KEY_SOFT_LEFT:  return "SOFT1";
            case MOTOROLA_KEY_SOFT_RIGHT: return "SOFT2";
            default: break;
        }
    }

    switch (keyCode) {
        case KEY_UP:    return "UP";
        case KEY_DOWN:  return "DOWN";
        case KEY_LEFT:  return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_FIRE:  return "SELECT";
        case KEY_SOFT_LEFT:  return "SOFT1";
        case KEY_SOFT_RIGHT: return "SOFT2";
        case KEY_CLEAR: return "CLEAR";
        case KEY_SEND:  return "SEND";
        case KEY_END:   return "END";
        case KEY_NUM0:  return "0";
        case KEY_NUM1:  return "1";
        case KEY_NUM2:  return "2";
        case KEY_NUM3:  return "3";
        case KEY_NUM4:  return "4";
        case KEY_NUM5:  return "5";
        case KEY_NUM6:  return "6";
        case KEY_NUM7:  return "7";
        case KEY_NUM8:  return "8";
        case KEY_NUM9:  return "9";
        case KEY_STAR:  return "*";
        case KEY_POUND: return "#";
        default:
            if (keyCode >= 32 && keyCode <= 126) {
                return std::string(1, static_cast<char>(keyCode));
            }
            return "UNKNOWN";
    }
}

void KeyMapper::setCustomMapping(int fromKeyCode, int toKeyCode) {
    s_customMapping[fromKeyCode] = toKeyCode;
}

void KeyMapper::clearCustomMappings() {
    s_customMapping.clear();
}

void KeyMapper::reset() {
    s_layoutType = LAYOUT_DEFAULT;
    s_customMapping.clear();
}

// =========================================================================
// VirtualKeypadEngine Implementation
// =========================================================================
VirtualKeypadEngine::VirtualKeypadEngine() {
    buildLayout();
}

VirtualKeypadEngine::VirtualKeypadEngine(int screenWidth, int screenHeight, KeyboardType type)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight), m_type(type) {
    buildLayout();
}

void VirtualKeypadEngine::setDimensions(int width, int height) {
    if (width > 0 && height > 0) {
        m_screenWidth = width;
        m_screenHeight = height;
        buildLayout();
    }
}

void VirtualKeypadEngine::setType(KeyboardType type) {
    m_type = type;
    buildLayout();
}

void VirtualKeypadEngine::buildLayout() {
    m_keys.clear();

    float w = static_cast<float>(m_screenWidth);
    float h = static_cast<float>(m_screenHeight);

    // Softkeys at top of keypad area
    float softW = w * 0.35f;
    float softH = 36.0f;
    float softY = h - 180.0f;

    VirtualKey softLeft{KEY_SOFT_LEFT, 5.0f, softY, softW, softH, "Soft1", false};
    VirtualKey softRight{KEY_SOFT_RIGHT, w - softW - 5.0f, softY, softW, softH, "Soft2", false};
    m_keys.push_back(softLeft);
    m_keys.push_back(softRight);

    // D-Pad and Fire
    float padSize = 40.0f;
    float padCenterX = w * 0.25f;
    float padCenterY = h - 90.0f;

    VirtualKey dUp{KEY_UP, padCenterX - padSize * 0.5f, padCenterY - padSize * 1.2f, padSize, padSize, "Up", false};
    VirtualKey dDown{KEY_DOWN, padCenterX - padSize * 0.5f, padCenterY + padSize * 0.2f, padSize, padSize, "Down", false};
    VirtualKey dLeft{KEY_LEFT, padCenterX - padSize * 1.2f, padCenterY - padSize * 0.5f, padSize, padSize, "Left", false};
    VirtualKey dRight{KEY_RIGHT, padCenterX + padSize * 0.2f, padCenterY - padSize * 0.5f, padSize, padSize, "Right", false};
    VirtualKey dFire{KEY_FIRE, padCenterX - padSize * 0.5f, padCenterY - padSize * 0.5f, padSize, padSize, "OK", false};

    m_keys.push_back(dUp);
    m_keys.push_back(dDown);
    m_keys.push_back(dLeft);
    m_keys.push_back(dRight);
    m_keys.push_back(dFire);

    // 12-key ITU-T phone grid (3 columns x 4 rows)
    float gridStartX = w * 0.55f;
    float gridStartY = h - 140.0f;
    float colW = (w - gridStartX - 10.0f) / 3.0f;
    float rowH = 30.0f;

    struct KeyDef {
        int code;
        const char* label;
    };

    KeyDef grid[4][3] = {
        {{KEY_NUM1, "1"}, {KEY_NUM2, "2"}, {KEY_NUM3, "3"}},
        {{KEY_NUM4, "4"}, {KEY_NUM5, "5"}, {KEY_NUM6, "6"}},
        {{KEY_NUM7, "7"}, {KEY_NUM8, "8"}, {KEY_NUM9, "9"}},
        {{KEY_STAR, "*"}, {KEY_NUM0, "0"}, {KEY_POUND, "#"}}
    };

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            VirtualKey vk;
            vk.keyCode = grid[r][c].code;
            vk.label = grid[r][c].label;
            vk.x = gridStartX + c * colW;
            vk.y = gridStartY + r * rowH;
            vk.width = colW - 2.0f;
            vk.height = rowH - 2.0f;
            vk.isPressed = false;
            m_keys.push_back(vk);
        }
    }
}

int VirtualKeypadEngine::hitTest(float touchX, float touchY) const {
    for (const auto& key : m_keys) {
        if (touchX >= key.x && touchX <= key.x + key.width &&
            touchY >= key.y && touchY <= key.y + key.height) {
            return key.keyCode;
        }
    }
    return 0;
}

bool VirtualKeypadEngine::onTouchEvent(int action, float touchX, float touchY, int& outKeyCode, bool& outIsPressed) {
    if (action == 0) { // ACTION_DOWN
        int code = hitTest(touchX, touchY);
        if (code != 0) {
            m_activeTouchKeyCode = code;
            outKeyCode = code;
            outIsPressed = true;
            for (auto& key : m_keys) {
                if (key.keyCode == code) key.isPressed = true;
            }
            return true;
        }
    } else if (action == 1) { // ACTION_UP
        if (m_activeTouchKeyCode != 0) {
            outKeyCode = m_activeTouchKeyCode;
            outIsPressed = false;
            for (auto& key : m_keys) {
                if (key.keyCode == m_activeTouchKeyCode) key.isPressed = false;
            }
            m_activeTouchKeyCode = 0;
            return true;
        }
    } else if (action == 2) { // ACTION_MOVE
        int code = hitTest(touchX, touchY);
        if (code != m_activeTouchKeyCode) {
            if (m_activeTouchKeyCode != 0) {
                // Release old
                outKeyCode = m_activeTouchKeyCode;
                outIsPressed = false;
                for (auto& key : m_keys) {
                    if (key.keyCode == m_activeTouchKeyCode) key.isPressed = false;
                }
                m_activeTouchKeyCode = 0;
                return true;
            }
            if (code != 0) {
                // Press new
                m_activeTouchKeyCode = code;
                outKeyCode = code;
                outIsPressed = true;
                for (auto& key : m_keys) {
                    if (key.keyCode == code) key.isPressed = true;
                }
                return true;
            }
        }
    }
    return false;
}

// =========================================================================
// MultiTapInputEngine Implementation
// =========================================================================
void MultiTapInputEngine::setMode(InputMode mode) {
    m_mode = mode;
    reset();
}

void MultiTapInputEngine::cycleMode() {
    if (m_mode == MODE_LOWERCASE) {
        m_mode = MODE_UPPERCASE;
    } else if (m_mode == MODE_UPPERCASE) {
        m_mode = MODE_NUMERIC;
    } else {
        m_mode = MODE_LOWERCASE;
    }
    reset();
}

std::string MultiTapInputEngine::getCandidatesForKey(int keyCode) const {
    if (m_mode == MODE_NUMERIC) {
        switch (keyCode) {
            case KEY_NUM0: return "0";
            case KEY_NUM1: return "1";
            case KEY_NUM2: return "2";
            case KEY_NUM3: return "3";
            case KEY_NUM4: return "4";
            case KEY_NUM5: return "5";
            case KEY_NUM6: return "6";
            case KEY_NUM7: return "7";
            case KEY_NUM8: return "8";
            case KEY_NUM9: return "9";
            case KEY_POUND: return "#";
            default: return "";
        }
    }

    bool upper = (m_mode == MODE_UPPERCASE);
    switch (keyCode) {
        case KEY_NUM1: return ".,-?!'1@_:";
        case KEY_NUM2: return upper ? "ABC2" : "abc2";
        case KEY_NUM3: return upper ? "DEF3" : "def3";
        case KEY_NUM4: return upper ? "GHI4" : "ghi4";
        case KEY_NUM5: return upper ? "JKL5" : "jkl5";
        case KEY_NUM6: return upper ? "MNO6" : "mno6";
        case KEY_NUM7: return upper ? "PQRS7" : "pqrs7";
        case KEY_NUM8: return upper ? "TUV8" : "tuv8";
        case KEY_NUM9: return upper ? "WXYZ9" : "wxyz9";
        case KEY_NUM0: return " 0";
        case KEY_POUND: return "#";
        default: return "";
    }
}

bool MultiTapInputEngine::handleKey(int midpKeyCode,
                                   int64_t currentTimeMs,
                                   char& outCommittedChar,
                                   bool& outHasCommitted,
                                   char& outPendingChar) {
    outHasCommitted = false;
    outCommittedChar = '\0';
    outPendingChar = '\0';

    if (midpKeyCode == KEY_STAR) {
        if (m_pendingChar != '\0') {
            outCommittedChar = m_pendingChar;
            outHasCommitted = true;
            m_pendingChar = '\0';
        }
        cycleMode();
        return true;
    }

    std::string candidates = getCandidatesForKey(midpKeyCode);
    if (candidates.empty()) {
        if (m_pendingChar != '\0') {
            outCommittedChar = m_pendingChar;
            outHasCommitted = true;
            m_pendingChar = '\0';
            m_currentKey = -1;
        }
        return false;
    }

    // Check if cycling on the same key within timeout
    if (midpKeyCode == m_currentKey &&
        (currentTimeMs - m_lastPressTimeMs) < m_commitTimeoutMs) {
        m_cycleIndex = (m_cycleIndex + 1) % candidates.size();
        m_pendingChar = candidates[m_cycleIndex];
        outPendingChar = m_pendingChar;
    } else {
        // Different key or timed out -> commit previous character
        if (m_pendingChar != '\0') {
            outCommittedChar = m_pendingChar;
            outHasCommitted = true;
        }
        m_currentKey = midpKeyCode;
        m_cycleIndex = 0;
        m_pendingChar = candidates[0];
        outPendingChar = m_pendingChar;
    }

    m_lastPressTimeMs = currentTimeMs;
    return true;
}

bool MultiTapInputEngine::checkTimeout(int64_t currentTimeMs, char& outCommittedChar) {
    outCommittedChar = '\0';
    if (m_pendingChar != '\0' && (currentTimeMs - m_lastPressTimeMs) >= m_commitTimeoutMs) {
        outCommittedChar = m_pendingChar;
        m_pendingChar = '\0';
        m_currentKey = -1;
        return true;
    }
    return false;
}

bool MultiTapInputEngine::commitPending(char& outCommittedChar) {
    outCommittedChar = '\0';
    if (m_pendingChar != '\0') {
        outCommittedChar = m_pendingChar;
        m_pendingChar = '\0';
        m_currentKey = -1;
        return true;
    }
    return false;
}

void MultiTapInputEngine::reset() {
    m_currentKey = -1;
    m_cycleIndex = 0;
    m_lastPressTimeMs = 0;
    m_pendingChar = '\0';
}

} // namespace input
} // namespace universal_loader
