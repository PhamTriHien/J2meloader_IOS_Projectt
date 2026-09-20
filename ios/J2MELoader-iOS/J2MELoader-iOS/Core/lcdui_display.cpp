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
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // a
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // c
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, // d
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // e
    {0x3C,0x66,0x60,0xF8,0x60,0x60,0xF0,0x00}, // f
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // g
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    {0x06,0x00,0x06,0x06,0x06,0x66,0x3C,0x00}, // j
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, // m
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // o
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // p
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // q
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, // r
    {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, // s
    {0x30,0x30,0xFC,0x30,0x30,0x34,0x18,0x00}, // t
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // u
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // v
    {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, // y
    {0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00}, // z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  // ~
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
    m_transX = 0;
    m_transY = 0;
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
    int ax = x + m_transX;
    int ay = y + m_transY;
    int x1 = std::max(0, std::min(m_width, ax));
    int y1 = std::max(0, std::min(m_height, ay));
    int x2 = std::max(0, std::min(m_width, ax + w));
    int y2 = std::max(0, std::min(m_height, ay + h));
    m_clip.x = x1;
    m_clip.y = y1;
    m_clip.width = std::max(0, x2 - x1);
    m_clip.height = std::max(0, y2 - y1);
}

void LcduiDisplay::clipRect(int x, int y, int w, int h) {
    int ax = x + m_transX;
    int ay = y + m_transY;
    int x1 = std::max(m_clip.x, ax);
    int y1 = std::max(m_clip.y, ay);
    int x2 = std::min(m_clip.x + m_clip.width, ax + w);
    int y2 = std::min(m_clip.y + m_clip.height, ay + h);
    m_clip.x = x1;
    m_clip.y = y1;
    m_clip.width = std::max(0, x2 - x1);
    m_clip.height = std::max(0, y2 - y1);
}

void LcduiDisplay::drawLine(int x1, int y1, int x2, int y2, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax1 = x1 + m_transX, ay1 = y1 + m_transY;
    int ax2 = x2 + m_transX, ay2 = y2 + m_transY;
    int dx = std::abs(ax2 - ax1), sx = ax1 < ax2 ? 1 : -1;
    int dy = -std::abs(ay2 - ay1), sy = ay1 < ay2 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        setPixelUnsafe(ax1, ay1, color);
        if (ax1 == ax2 && ay1 == ay2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; ax1 += sx; }
        if (e2 <= dx) { err += dx; ay1 += sy; }
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
    int ax = x + m_transX, ay = y + m_transY;
    int x1 = std::max(m_clip.x, ax);
    int y1 = std::max(m_clip.y, ay);
    int x2 = std::min(m_clip.x + m_clip.width, ax + w);
    int y2 = std::min(m_clip.y + m_clip.height, ay + h);

    for (int cy = y1; cy < y2; ++cy) {
        for (int cx = x1; cx < x2; ++cx) {
            setPixelUnsafe(cx, cy, color);
        }
    }
}

void LcduiDisplay::drawRGB(const int32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax = x + m_transX, ay = y + m_transY;
    for (int r = 0; r < height; ++r) {
        int cy = ay + r;
        if (cy < m_clip.y || cy >= m_clip.y + m_clip.height) continue;
        for (int c = 0; c < width; ++c) {
            int cx = ax + c;
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
    int ax = x + m_transX, ay = y + m_transY;
    const uint8_t* glyph = font8x8_basic[c - 32];
    for (int r = 0; r < 8; ++r) {
        uint8_t row = glyph[r];
        for (int b = 0; b < 8; ++b) {
            if (row & (1 << (7 - b))) {
                setPixelUnsafe(ax + b, ay + r, color);
            }
        }
    }
}

void LcduiDisplay::drawRegion(const uint32_t* srcPixels, int srcW, int srcH, int x_src, int y_src, int width, int height, int transform, int x_dest, int y_dest, int anchor) {
    if (!srcPixels || srcW <= 0 || srcH <= 0 || width <= 0 || height <= 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    // MIDP transform constants (same as Sprite): 90/270 family {4,5,6,7} swaps dims.
    int destW = (transform == 4 || transform == 5 || transform == 6 || transform == 7) ? height : width;
    int destH = (transform == 4 || transform == 5 || transform == 6 || transform == 7) ? width : height;

    int dx = x_dest + m_transX;
    int dy = y_dest + m_transY;

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

            // MIDP numbering (matches Sprite): 0=none, 1=mirror-rot180 (v-flip),
            // 2=mirror (h-flip), 3=rot180, 4=mirror-rot270, 5=rot90, 6=rot270, 7=mirror-rot90.
            int targetX = dx + c;
            int targetY = dy + r;

            switch (transform) {
            case 1: targetX = dx + c; targetY = dy + (height - 1 - r); break;
            case 2: targetX = dx + (width - 1 - c); targetY = dy + r; break;
            case 3: targetX = dx + (width - 1 - c); targetY = dy + (height - 1 - r); break;
            case 4: targetX = dx + (height - 1 - r); targetY = dy + (width - 1 - c); break;
            case 5: targetX = dx + (height - 1 - r); targetY = dy + c; break;
            case 6: targetX = dx + r; targetY = dy + (width - 1 - c); break;
            case 7: targetX = dx + r; targetY = dy + c; break;
            default: break;
            }

            setPixelUnsafe(targetX, targetY, pixel);
        }
    }
}

void LcduiDisplay::drawRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int rx = std::max(0, std::min(w / 2, arcWidth / 2));
    int ry = std::max(0, std::min(h / 2, arcHeight / 2));
    if (rx == 0 || ry == 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax = x + m_transX, ay = y + m_transY;
    // Straight lines
    drawLine(ax + rx, ay, ax + w - rx, ay, color);
    drawLine(ax + rx, ay + h, ax + w - rx, ay + h, color);
    drawLine(ax, ay + ry, ax, ay + h - ry, color);
    drawLine(ax + w, ay + ry, ax + w, ay + h - ry, color);

    // 4 corner arcs
    float step = 1.0f / (float)std::max(rx, ry);
    for (float t = 0.0f; t <= (float)M_PI_2 + step; t += step) {
        int dx = (int)std::round(rx * std::cos(t));
        int dy = (int)std::round(ry * std::sin(t));
        // Top-right
        setPixelUnsafe(ax + w - rx + dx, ay + ry - dy, color);
        // Top-left
        setPixelUnsafe(ax + rx - dx, ay + ry - dy, color);
        // Bottom-right
        setPixelUnsafe(ax + w - rx + dx, ay + h - ry + dy, color);
        // Bottom-left
        setPixelUnsafe(ax + rx - dx, ay + h - ry + dy, color);
    }
}

void LcduiDisplay::fillRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int rx = std::max(0, std::min(w / 2, arcWidth / 2));
    int ry = std::max(0, std::min(h / 2, arcHeight / 2));
    if (rx == 0 || ry == 0) {
        fillRect(x, y, w, h, color);
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax = x + m_transX, ay = y + m_transY;
    // Middle vertical block
    int x1 = std::max(m_clip.x, ax + rx);
    int x2 = std::min(m_clip.x + m_clip.width, ax + w - rx);
    int y1 = std::max(m_clip.y, ay);
    int y2 = std::min(m_clip.y + m_clip.height, ay + h);
    for (int cy = y1; cy <= y2; ++cy) {
        for (int cx = x1; cx <= x2; ++cx) setPixelUnsafe(cx, cy, color);
    }
    // Left & right middle blocks
    int my1 = std::max(m_clip.y, ay + ry);
    int my2 = std::min(m_clip.y + m_clip.height, ay + h - ry);
    for (int cy = my1; cy <= my2; ++cy) {
        for (int cx = std::max(m_clip.x, ax); cx < ax + rx && cx < m_clip.x + m_clip.width; ++cx) setPixelUnsafe(cx, cy, color);
        for (int cx = std::max(m_clip.x, ax + w - rx); cx <= ax + w && cx < m_clip.x + m_clip.width; ++cx) setPixelUnsafe(cx, cy, color);
    }
    // Corner rounded quadrants
    for (int dy = 0; dy <= ry; ++dy) {
        float normY = (float)(ry - dy) / (float)ry;
        int maxDx = (int)std::round((float)rx * std::sqrt(std::max(0.0f, 1.0f - normY * normY)));
        // Top corners
        int cyTop = ay + ry - dy;
        for (int cx = ax + rx - maxDx; cx <= ax + rx; ++cx) setPixelUnsafe(cx, cyTop, color);
        for (int cx = ax + w - rx; cx <= ax + w - rx + maxDx; ++cx) setPixelUnsafe(cx, cyTop, color);
        // Bottom corners
        int cyBot = ay + h - ry + dy;
        for (int cx = ax + rx - maxDx; cx <= ax + rx; ++cx) setPixelUnsafe(cx, cyBot, color);
        for (int cx = ax + w - rx; cx <= ax + w - rx + maxDx; ++cx) setPixelUnsafe(cx, cyBot, color);
    }
}

void LcduiDisplay::drawArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color) {
    if (w <= 0 || h <= 0 || arcAngle == 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    float cx = (x + m_transX) + w / 2.0f;
    float cy = (y + m_transY) + h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    float totalDeg = (float)std::abs(arcAngle);
    float dir = arcAngle >= 0 ? 1.0f : -1.0f;
    float step = 180.0f / ((float)M_PI * std::max(rx, ry));
    if (step <= 0.1f) step = 0.5f;

    for (float a = 0.0f; a <= totalDeg + step; a += step) {
        float deg = (float)startAngle + dir * std::min(a, totalDeg);
        float rad = deg * (float)M_PI / 180.0f;
        int px = (int)std::round(cx + rx * std::cos(rad));
        int py = (int)std::round(cy - ry * std::sin(rad)); // Inverted Y
        setPixelUnsafe(px, py, color);
    }
}

void LcduiDisplay::fillArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color) {
    if (w <= 0 || h <= 0 || arcAngle == 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax = x + m_transX, ay = y + m_transY;
    float cx = ax + w / 2.0f;
    float cy = ay + h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    if (rx <= 0 || ry <= 0) return;

    bool isFull = std::abs(arcAngle) >= 360;
    float a1 = (float)startAngle;
    float a2 = a1 + (float)arcAngle;
    if (arcAngle < 0) std::swap(a1, a2);

    auto angleBetween = [](float ang, float s, float e) -> bool {
        while (s < 0.0f) s += 360.0f;
        while (s >= 360.0f) s -= 360.0f;
        while (e < s) e += 360.0f;
        while (ang < s) ang += 360.0f;
        return ang <= e;
    };

    int x1 = std::max(m_clip.x, ax);
    int x2 = std::min(m_clip.x + m_clip.width - 1, ax + w);
    int y1 = std::max(m_clip.y, ay);
    int y2 = std::min(m_clip.y + m_clip.height - 1, ay + h);

    for (int py = y1; py <= y2; ++py) {
        float ny = (py - cy) / ry;
        if (std::abs(ny) > 1.0f) continue;
        for (int px = x1; px <= x2; ++px) {
            float nx = (px - cx) / rx;
            if (nx * nx + ny * ny <= 1.0f) {
                if (isFull) {
                    setPixelUnsafe(px, py, color);
                } else {
                    float rad = std::atan2(- (py - cy), px - cx);
                    float deg = rad * 180.0f / (float)M_PI;
                    if (deg < 0.0f) deg += 360.0f;
                    if (angleBetween(deg, a1, a2)) {
                        setPixelUnsafe(px, py, color);
                    }
                }
            }
        }
    }
}

void LcduiDisplay::drawString(const std::string& text, int x, int y, int anchor, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ax = x + m_transX, ay = y + m_transY;
    // Unicode path (Vietnamese/CJK): CoreText alpha bitmap blended with LCDUI color
    if (!text.empty() && needsUnicode(text) && native_text_render && native_free) {
        uint8_t *alpha = nullptr; int w = 0, h = 0;
        if (native_text_render(text.c_str(), 12, &alpha, &w, &h) && alpha && w > 0 && h > 0) {
            int drawX = ax, drawY = ay;
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

    int drawX = ax;
    int drawY = ay;

    // HCENTER = 1, LEFT = 4, RIGHT = 8
    if (anchor & 1) drawX -= textW / 2;
    else if (anchor & 8) drawX -= textW;

    // VCENTER = 2, TOP = 16, BOTTOM = 32, BASELINE = 64
    if (anchor & 2) drawY -= textH / 2;
    else if (anchor & (32 | 64)) drawY -= textH;

    for (size_t i = 0; i < text.length(); ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126) continue;
        const uint8_t* glyph = font8x8_basic[ch - 32];
        for (int r = 0; r < 8; ++r) {
            uint8_t row = glyph[r];
            for (int b = 0; b < 8; ++b) {
                if (row & (1 << (7 - b))) setPixelUnsafe(drawX + (int)i * 8 + b, drawY + r, color);
            }
        }
    }
}