#ifndef J2ME_LCDUI_FONT_H
#define J2ME_LCDUI_FONT_H

#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>

namespace j2me {

// --- MIDP 2.0 Font Constants (javax.microedition.lcdui.Font) ---
enum LcduiFontFace {
    FACE_SYSTEM       = 0,
    FACE_MONOSPACE    = 32,
    FACE_PROPORTIONAL = 64
};

enum LcduiFontStyle {
    STYLE_PLAIN      = 0,
    STYLE_BOLD       = 1,
    STYLE_ITALIC     = 2,
    STYLE_UNDERLINED = 4
};

enum LcduiFontSize {
    SIZE_MEDIUM = 0,
    SIZE_SMALL  = 8,
    SIZE_LARGE  = 16
};

enum LcduiFontSpecifier {
    FONT_STATIC_TEXT = 0,
    FONT_INPUT_TEXT  = 1
};

// --- LcduiFont Class ---
class J2ME_API LcduiFont {
public:
    LcduiFont(int face, int style, int size, float customHeight = 0.0f);
    ~LcduiFont() = default;

    static std::shared_ptr<LcduiFont> getFont(int face, int style, int size);
    static std::shared_ptr<LcduiFont> getDefaultFont();
    static std::shared_ptr<LcduiFont> getFont(int fontSpecifier);

    int getFace() const { return m_face; }
    int getStyle() const { return m_style; }
    int getSize() const { return m_size; }
    int getHeight() const { return m_height; }
    int getBaselinePosition() const { return m_ascent; }
    int getAscent() const { return m_ascent; }
    int getDescent() const { return m_descent; }

    bool isBold() const { return (m_style & STYLE_BOLD) != 0; }
    bool isItalic() const { return (m_style & STYLE_ITALIC) != 0; }
    bool isPlain() const { return m_style == STYLE_PLAIN; }
    bool isUnderlined() const { return (m_style & STYLE_UNDERLINED) != 0; }

    int charWidth(char c) const;
    int charsWidth(const char* ch, int offset, int length) const;
    int stringWidth(const std::string& text) const;
    int substringWidth(const std::string& str, int offset, int len) const;

    // Rasterization
    void renderChar(char c, uint32_t* target, int bufW, int bufH,
                    int clipX, int clipY, int clipW, int clipH,
                    int x, int y, uint32_t color) const;

    void renderString(const std::string& text, uint32_t* target, int bufW, int bufH,
                      int clipX, int clipY, int clipW, int clipH,
                      int x, int y, int anchor, uint32_t color) const;

    void renderSubstring(const std::string& text, int offset, int len,
                         uint32_t* target, int bufW, int bufH,
                         int clipX, int clipY, int clipW, int clipH,
                         int x, int y, int anchor, uint32_t color) const;

private:
    int m_face{FACE_SYSTEM};
    int m_style{STYLE_PLAIN};
    int m_size{SIZE_MEDIUM};
    int m_height{18};
    int m_ascent{14};
    int m_descent{4};
    int m_scale{1};

    int getCharBaseWidth(char c) const;
};

} // namespace j2me

#endif // J2ME_LCDUI_FONT_H
