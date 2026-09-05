#include "lcdui_display.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdlib>
// Full-Unicode font via iOS CoreText (weak-linked; fallback 8x8 on other builds)
extern "C" {
bool native_text_measure(const char *utf8, int px, int *outW, int *outH) __attribute__((weak));
bool native_text_render(const char *utf8, int px, uint8_t **outAlpha, int *outW, int *outH) __attribute__((weak));
void native_free(void *p) __attribute__((weak));
}
static bool needsUnicode(const std::string& s) {
    for (unsigned char c : s) if (c < 32 || c > 126) return true;
    return false;
}

// Standard 8x8 font bitmap for ASCII characters 32-126
static const uint8_t font8x8_basic[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // #
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, // $
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
    {0x30,0x30,0x10,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // /
    {0x3C,0x66,0xC3,0xC3,0xC3,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x0F,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // =
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // >
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // ?
    {0x3C,0x66,0x6E,0x6A,0x6E,0x60,0x3C,0x00}, // @
    {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x0E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // \
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}  // `
};

LcduiDisplay::LcduiDisplay(int width, int height)
    : m_width(width), m_height(height) {
    m_buffer.resize(width * height, 0xFF050814);
    resetClip();
}

LcduiDisplay::~LcduiDisplay() {}

void LcduiDisplay::resize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_width = width;
    m_height = height;
    m_buffer.resize(width * height, 0xFF050814);
    resetClip();
}

void LcduiDisplay::clear(uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_buffer.begin(), m_buffer.end(), color);
}

void LcduiDisplay::resetClip() {
    m_clip = { 0, 0, m_width, m_height };
}

void LcduiDisplay::setClip(int x, int y, int w, int h) {
    int x1 = std::max(0, std::min(m_width, x));
    int y1 = std::max(0, std::min(m_height, y));
    int x2 = std::max(0, std::min(m_width, x + w));
    int y2 = std::max(0, std::min(m_height, y + h));
    m_clip.x = x1;
    m_clip.y = y1;
    m_clip.width = std::max(0, x2 - x1);
    m_clip.height = std::max(0, y2 - y1);
}

void LcduiDisplay::clipRect(int x, int y, int w, int h) {
    int x1 = std::max(m_clip.x, x);
    int y1 = std::max(m_clip.y, y);
    int x2 = std::min(m_clip.x + m_clip.width, x + w);
    int y2 = std::min(m_clip.y + m_clip.height, y + h);
    m_clip.x = x1;
    m_clip.y = y1;
    m_clip.width = std::max(0, x2 - x1);
    m_clip.height = std::max(0, y2 - y1);
}

void LcduiDisplay::drawLine(int x1, int y1, int x2, int y2, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        setPixelUnsafe(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void LcduiDisplay::drawRect(int x, int y, int w, int h, uint32_t color) {
    drawLine(x, y, x + w, y, color);
    drawLine(x + w, y, x + w, y + h, color);
    drawLine(x + w, y + h, x, y + h, color);
    drawLine(x, y + h, x, y, color);
}

void LcduiDisplay::fillRect(int x, int y, int w, int h, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int x1 = std::max(m_clip.x, x);
    int y1 = std::max(m_clip.y, y);
    int x2 = std::min(m_clip.x + m_clip.width, x + w);
    int y2 = std::min(m_clip.y + m_clip.height, y + h);

    for (int cy = y1; cy < y2; ++cy) {
        for (int cx = x1; cx < x2; ++cx) {
            setPixelUnsafe(cx, cy, color);
        }
    }
}

void LcduiDisplay::drawRGB(const int32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int r = 0; r < height; ++r) {
        int cy = y + r;
        if (cy < m_clip.y || cy >= m_clip.y + m_clip.height) continue;
        for (int c = 0; c < width; ++c) {
            int cx = x + c;
            if (cx < m_clip.x || cx >= m_clip.x + m_clip.width) continue;
            uint32_t pixel = (uint32_t)rgbData[offset + r * scanlength + c];
            if (!processAlpha) pixel |= 0xFF000000;
            setPixelUnsafe(cx, cy, pixel);
        }
    }
}

void LcduiDisplay::drawChar(char c, int x, int y, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (c < 32 || c > 126) return;
    const uint8_t* glyph = font8x8_basic[c - 32];
    for (int r = 0; r < 8; ++r) {
        uint8_t row = glyph[r];
        for (int b = 0; b < 8; ++b) {
            if (row & (1 << (7 - b))) {
                setPixelUnsafe(x + b, y + r, color);
            }
        }
    }
}

void LcduiDisplay::drawRegion(const uint32_t* srcPixels, int srcW, int srcH, int x_src, int y_src, int width, int height, int transform, int x_dest, int y_dest, int anchor) {
    if (!srcPixels || srcW <= 0 || srcH <= 0 || width <= 0 || height <= 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    int destW = (transform == 5 || transform == 6 || transform == 1 || transform == 3) ? height : width;
    int destH = (transform == 5 || transform == 6 || transform == 1 || transform == 3) ? width : height;

    int dx = x_dest;
    int dy = y_dest;

    if (anchor & 1) dx -= destW / 2; // HCENTER
    else if (anchor & 8) dx -= destW; // RIGHT

    if (anchor & 2) dy -= destH / 2; // VCENTER
    else if (anchor & 32) dy -= destH; // BOTTOM

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            int sx = x_src + c;
            int sy = y_src + r;
            if (sx < 0 || sx >= srcW || sy < 0 || sy >= srcH) continue;

            uint32_t pixel = srcPixels[sy * srcW + sx];
            if ((pixel >> 24) == 0) continue;

            int targetX = dx + c;
            int targetY = dy + r;

            switch (transform) {
            case 1: targetX = dx + (height - 1 - r); targetY = dy + c; break;
            case 2: targetX = dx + (width - 1 - c); targetY = dy + (height - 1 - r); break;
            case 3: targetX = dx + r; targetY = dy + (width - 1 - c); break;
            case 4: targetX = dx + (width - 1 - c); targetY = dy + r; break;
            case 5: targetX = dx + (height - 1 - r); targetY = dy + (width - 1 - c); break;
            case 6: targetX = dx + c; targetY = dy + (height - 1 - r); break;
            case 7: targetX = dx + r; targetY = dy + c; break;
            default: break;
            }

            setPixelUnsafe(targetX, targetY, pixel);
        }
    }
}

void LcduiDisplay::drawRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color) {
    drawRect(x, y, w, h, color);
}

void LcduiDisplay::fillRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color) {
    fillRect(x, y, w, h, color);
}

void LcduiDisplay::drawArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color) {
    drawRect(x, y, w, h, color);
}

void LcduiDisplay::fillArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color) {
    fillRect(x, y, w, h, color);
}

void LcduiDisplay::drawString(const std::string& text, int x, int y, int anchor, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Unicode path (Vietnamese/CJK): CoreText alpha bitmap blended with LCDUI color
    if (!text.empty() && needsUnicode(text) && native_text_render && native_free) {
        uint8_t *alpha = nullptr; int w = 0, h = 0;
        if (native_text_render(text.c_str(), 12, &alpha, &w, &h) && alpha && w > 0 && h > 0) {
            int drawX = x, drawY = y;
            if (anchor & 1) drawX -= w / 2;
            else if (anchor & 8) drawX -= w;
            if (anchor & 2) drawY -= h / 2;
            else if (anchor & (32 | 64)) drawY -= h;
            uint32_t cr = (color >> 16) & 0xFF, cg = (color >> 8) & 0xFF, cb = color & 0xFF;
            for (int r = 0; r < h; ++r) {
                for (int c = 0; c < w; ++c) {
                    uint8_t a = alpha[r * w + c];
                    if (a < 8) continue;
                    uint32_t blended = 0xFF000000 | (cr << 16) | (cg << 8) | cb;
                    // simple coverage: skip faint, solid otherwise (keeps retro crisp)
                    if (a > 128) setPixelUnsafe(drawX + c, drawY + r, blended);
                    else setPixelUnsafe(drawX + c, drawY + r, (blended & 0x00FFFFFF) | ((uint32_t)a << 24));
                }
            }
            native_free(alpha);
            return;
        }
        if (alpha) native_free(alpha);
    }
    int textW = (int)text.length() * 8;
    int textH = 8;

    int drawX = x;
    int drawY = y;

    // HCENTER = 1, LEFT = 4, RIGHT = 8
    if (anchor & 1) drawX -= textW / 2;
    else if (anchor & 8) drawX -= textW;

    // VCENTER = 2, TOP = 16, BOTTOM = 32, BASELINE = 64
    if (anchor & 2) drawY -= textH / 2;
    else if (anchor & (32 | 64)) drawY -= textH;

    for (size_t i = 0; i < text.length(); ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126) continue; // non-ASCII without native bridge: skip (no tofu)
        const uint8_t* glyph = font8x8_basic[ch - 32];
        for (int r = 0; r < 8; ++r) {
            uint8_t row = glyph[r];
            for (int b = 0; b < 8; ++b) {
                if (row & (1 << (7 - b))) setPixelUnsafe(drawX + (int)i * 8 + b, drawY + r, color);
            }
        }
    }
}