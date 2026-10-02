#ifndef J2ME_FRAME_BUFFER_H
#define J2ME_FRAME_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <vector>
#include <atomic>
#include <mutex>
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
    // Copies the display frame as RGBA8888 bytes with every pixel repeated scale x scale.
    // Returns 1 when a frame was written, 0 when nothing changed since the last copy, -1 when dst is too small.
    int copyDisplayRgba(uint8_t* dst, size_t cap, int scale, bool force, int* outW, int* outH);

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
    std::mutex m_displayMutex; // held by the UI while it reads the display buffer

    bool isPixelInClip(int x, int y) const {
        return (x >= m_clip.x && x < (m_clip.x + m_clip.w) &&
                y >= m_clip.y && y < (m_clip.y + m_clip.h) &&
                x >= 0 && x < m_width && y >= 0 && y < m_height);
    }
};

} // namespace j2me

#endif // J2ME_FRAME_BUFFER_H
