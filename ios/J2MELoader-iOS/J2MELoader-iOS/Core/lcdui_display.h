#ifndef LCDUI_DISPLAY_H
#define LCDUI_DISPLAY_H

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>

struct ClipRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class LcduiDisplay {
public:
    LcduiDisplay(int width = 240, int height = 320);
    ~LcduiDisplay();

    void resize(int width, int height);
    void clear(uint32_t color = 0xFF050814);

    void setColor(uint32_t color) { m_currentColor = color; }
    uint32_t getColor() const { return m_currentColor; }

    void setClip(int x, int y, int w, int h);
    void clipRect(int x, int y, int w, int h);
    ClipRect getClip() const { return m_clip; }
    void resetClip();

    void drawLine(int x1, int y1, int x2, int y2, uint32_t color);
    void drawRect(int x, int y, int w, int h, uint32_t color);
    void fillRect(int x, int y, int w, int h, uint32_t color);
    void drawRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color);
    void fillRoundRect(int x, int y, int w, int h, int arcWidth, int arcHeight, uint32_t color);
    void drawArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color);
    void fillArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t color);

    void drawRGB(const int32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha);
    void drawRegion(const uint32_t* srcPixels, int srcW, int srcH, int x_src, int y_src, int width, int height, int transform, int x_dest, int y_dest, int anchor);
    void drawChar(char c, int x, int y, uint32_t color);
    void drawString(const std::string& text, int x, int y, int anchor, uint32_t color);

    const uint32_t* getBuffer() const { return m_buffer.data(); }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    std::mutex& getMutex() { return m_mutex; }

private:
    int m_width;
    int m_height;
    uint32_t m_currentColor = 0xFF000000;
    std::vector<uint32_t> m_buffer;
    ClipRect m_clip;
    std::mutex m_mutex;

    inline void setPixelUnsafe(int x, int y, uint32_t color) {
        if (x >= m_clip.x && x < m_clip.x + m_clip.width &&
            y >= m_clip.y && y < m_clip.y + m_clip.height) {
            
            // Alpha blending (RGBA)
            uint32_t alpha = (color >> 24) & 0xFF;
            if (alpha == 255) {
                m_buffer[y * m_width + x] = color;
            } else if (alpha > 0) {
                uint32_t dst = m_buffer[y * m_width + x];
                uint32_t invAlpha = 255 - alpha;
                
                uint32_t r = (((color >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * invAlpha) >> 8;
                uint32_t g = (((color >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * invAlpha) >> 8;
                uint32_t b = ((color & 0xFF) * alpha + (dst & 0xFF) * invAlpha) >> 8;
                
                m_buffer[y * m_width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }
};

#endif // LCDUI_DISPLAY_H