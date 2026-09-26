#ifndef J2ME_FRAME_BUFFER_H
#define J2ME_FRAME_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <vector>
#include <atomic>
#include <string>

namespace j2me {

struct Rect {
    int x;
    int y;
    int w;
    int h;
};

class FrameBuffer {
public:
    FrameBuffer(int width = 240, int height = 320);
    ~FrameBuffer();

    void resize(int width, int height);
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    // --- Double buffering & Thread-safe Publishing ---
    void publishFrame();
    const uint32_t* lockDisplayFrame(int* outW, int* outH, bool* outDirty);
    void unlockDisplayFrame();

    // --- 2D Rasterizer Primitives ---
    void clear(uint32_t argbColor);
    void setClip(int x, int y, int w, int h);
    void resetClip();
    Rect getClip() const { return m_clip; }

    void drawPixel(int x, int y, uint32_t argbColor);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t argbColor);
    void drawRect(int x, int y, int w, int h, uint32_t argbColor);
    void fillRect(int x, int y, int w, int h, uint32_t argbColor);
    void drawRoundRect(int x, int y, int w, int h, int arcW, int arcH, uint32_t argbColor);
    void fillRoundRect(int x, int y, int w, int h, int arcW, int arcH, uint32_t argbColor);
    void drawArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t argbColor);
    void fillArc(int x, int y, int w, int h, int startAngle, int arcAngle, uint32_t argbColor);

    void drawRGB(const uint32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha);
    void drawString(const std::string& text, int x, int y, uint32_t argbColor);
    void drawChar(char c, int x, int y, uint32_t argbColor);

    uint32_t* getRawDrawBuffer() { return m_drawBuffer.data(); }

private:
    int m_width;
    int m_height;
    Rect m_clip;

    std::vector<uint32_t> m_drawBuffer;
    std::vector<uint32_t> m_displayBuffer;
    std::vector<uint32_t> m_stagingBuffer;

    std::atomic<bool> m_isDirty{false};
    std::atomic<bool> m_isDisplayLocked{false};

    bool isPixelInClip(int x, int y) const {
        return (x >= m_clip.x && x < (m_clip.x + m_clip.w) &&
                y >= m_clip.y && y < (m_clip.y + m_clip.h) &&
                x >= 0 && x < m_width && y >= 0 && y < m_height);
    }
};

} // namespace j2me

#endif // J2ME_FRAME_BUFFER_H
