#ifndef J2ME_LCDUI_GRAPHICS_H
#define J2ME_LCDUI_GRAPHICS_H

#include "frame_buffer.h"
#include "../../include/j2me_core.h"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <atomic>

namespace j2me {

// --- MIDP 2.0 Anchor Constants ---
enum LcduiAnchor {
    ANCHOR_HCENTER  = 1,
    ANCHOR_VCENTER  = 2,
    ANCHOR_LEFT     = 4,
    ANCHOR_RIGHT    = 8,
    ANCHOR_TOP      = 16,
    ANCHOR_BOTTOM   = 32,
    ANCHOR_BASELINE = 64
};

// --- MIDP 2.0 Sprite / Graphics Transforms ---
enum LcduiTransform {
    TRANS_NONE           = 0,
    TRANS_MIRROR_ROT180  = 1,
    TRANS_MIRROR         = 2,
    TRANS_ROT180         = 3,
    TRANS_MIRROR_ROT270  = 4,
    TRANS_ROT90          = 5,
    TRANS_ROT270         = 6,
    TRANS_MIRROR_ROT90   = 7
};

class LcduiGraphics;

// --- LcduiImage (Mutable / Immutable Bitmap Image) ---
class J2ME_API LcduiImage {
public:
    static std::shared_ptr<LcduiImage> createImage(int width, int height);
    static std::shared_ptr<LcduiImage> createImage(int width, int height, uint32_t argb);
    static std::shared_ptr<LcduiImage> createImage(const LcduiImage& source);
    static std::shared_ptr<LcduiImage> createImage(const LcduiImage& image, int x, int y, int width, int height, int transform);
    static std::shared_ptr<LcduiImage> createImage(const uint8_t* rawData, size_t size);
    static std::shared_ptr<LcduiImage> createRGBImage(const uint32_t* rgb, int width, int height, bool processAlpha);

    LcduiImage(int width, int height, bool isMutable);
    ~LcduiImage();

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isMutable() const { return m_isMutable; }

    std::shared_ptr<LcduiGraphics> getGraphics();
    const uint32_t* getPixels() const { return m_pixels.data(); }
    uint32_t* getPixelsMutable() { return m_pixels.data(); }

    void getRGB(int32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height) const;

private:
    int m_width;
    int m_height;
    bool m_isMutable;
    std::vector<uint32_t> m_pixels;
    std::shared_ptr<LcduiGraphics> m_graphicsContext;
};

// --- LcduiGraphics (Context vẽ 2D MIDP 2.0 đầy đủ) ---
class J2ME_API LcduiGraphics {
public:
    static constexpr int SOLID  = 0;
    static constexpr int DOTTED = 1;

    LcduiGraphics(uint32_t* targetBuffer, int bufferWidth, int bufferHeight);
    ~LcduiGraphics();

    void translate(int x, int y);
    int getTranslateX() const { return m_transX; }
    int getTranslateY() const { return m_transY; }

    void setClip(int x, int y, int width, int height);
    void clipRect(int x, int y, int width, int height);
    int getClipX() const { return m_clipX - m_transX; }
    int getClipY() const { return m_clipY - m_transY; }
    int getClipWidth() const { return m_clipW; }
    int getClipHeight() const { return m_clipH; }

    int getBufferWidth() const { return m_bufW; }
    int getBufferHeight() const { return m_bufH; }
    uint32_t* getRawBuffer() { return m_target; }
    const uint32_t* getRawBuffer() const { return m_target; }

    void setColor(uint32_t argbColor);
    void setColorRGB(int r, int g, int b);
    uint32_t getColor() const { return m_currentColor; }

    void setGrayScale(int value);
    int getGrayScale() const;
    int getRedComponent() const;
    int getGreenComponent() const;
    int getBlueComponent() const;

    void setStrokeStyle(int stroke);
    int getStrokeStyle() const;

    // Primitives
    void drawLine(int x1, int y1, int x2, int y2);
    void drawRect(int x, int y, int width, int height);
    void fillRect(int x, int y, int width, int height);
    void drawRoundRect(int x, int y, int width, int height, int arcW, int arcH);
    void fillRoundRect(int x, int y, int width, int height, int arcW, int arcH);
    void drawArc(int x, int y, int width, int height, int startAngle, int arcAngle);
    void fillArc(int x, int y, int width, int height, int startAngle, int arcAngle);
    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3);
    void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);
    void drawPolygon(const int* xPoints, const int* yPoints, int nPoints);
    void fillPolygon(const int* xPoints, const int* yPoints, int nPoints);

    // Image & Region Operations
    void copyArea(int x_src, int y_src, int width, int height, int x_dest, int y_dest, int anchor);
    void drawRGB(const uint32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha);
    void drawImage(LcduiImage* img, int x, int y, int anchor);
    void drawRegion(LcduiImage* src, int x_src, int y_src, int width, int height, int transform, int x_dest, int y_dest, int anchor);

    // Text Operations & Font
    void setFont(std::shared_ptr<class LcduiFont> font);
    std::shared_ptr<class LcduiFont> getFont() const;
    void drawString(const std::string& text, int x, int y, int anchor);
    void drawSubstring(const std::string& text, int offset, int len, int x, int y, int anchor);
    void drawChar(char c, int x, int y, int anchor);
    void drawChars(const char* data, int offset, int length, int x, int y, int anchor);

private:
    uint32_t* m_target;
    int m_bufW;
    int m_bufH;

    int m_transX{0};
    int m_transY{0};

    int m_clipX{0};
    int m_clipY{0};
    int m_clipW{0};
    int m_clipH{0};

    uint32_t m_currentColor{0xFFFFFFFF};
    int m_strokeStyle{SOLID};
    std::shared_ptr<class LcduiFont> m_font;

    void calculateAnchor(int x, int y, int w, int h, int anchor, int& outX, int& outY);
    void drawPixelRaw(int x, int y, uint32_t color);
    bool isClipped(int x, int y) const {
        return (x < m_clipX || x >= m_clipX + m_clipW || y < m_clipY || y >= m_clipY + m_clipH);
    }
};

// --- GameCanvas Engine (Polling Key States & Double Buffer) ---
class J2ME_API GameCanvasEngine {
public:
    enum KeyMask {
        UP_PRESSED     = (1 << 1),
        LEFT_PRESSED   = (1 << 2),
        RIGHT_PRESSED  = (1 << 5),
        DOWN_PRESSED   = (1 << 6),
        FIRE_PRESSED   = (1 << 8),
        GAME_A_PRESSED = (1 << 9),
        GAME_B_PRESSED = (1 << 10),
        GAME_C_PRESSED = (1 << 11),
        GAME_D_PRESSED = (1 << 12)
    };

    GameCanvasEngine(int width, int height);
    ~GameCanvasEngine();

    int getKeyStates();
    void setKeyState(int j2meKeyCode, bool isPressed);

    std::shared_ptr<LcduiGraphics> getGraphics();
    void flushGraphics();
    void flushGraphics(int x, int y, int width, int height);

    const uint32_t* getDisplayBuffer() const { return m_displayBuffer.data(); }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    int m_width;
    int m_height;
    std::atomic<int> m_keyStates{0};

    std::vector<uint32_t> m_drawBuffer;
    std::vector<uint32_t> m_displayBuffer;
    std::shared_ptr<LcduiGraphics> m_graphics;
};

// --- Sprite Engine (Animation Frames & Pixel-Level Collision) ---
class J2ME_API SpriteEngine {
public:
    SpriteEngine(std::shared_ptr<LcduiImage> image, int frameWidth, int frameHeight);
    ~SpriteEngine();

    void setPosition(int x, int y) { m_x = x; m_y = y; }
    int getX() const { return m_x; }
    int getY() const { return m_y; }

    void setFrame(int sequenceIndex);
    int getFrame() const { return m_currentFrame; }
    void setFrameSequence(const std::vector<int>& sequence);
    void nextFrame();
    void prevFrame();

    void setTransform(int transform) { m_transform = transform; }
    int getTransform() const { return m_transform; }

    void paint(LcduiGraphics* g);

    // Collision Detection
    bool collidesWith(const SpriteEngine& other, bool pixelLevel) const;
    bool collidesWithImage(const LcduiImage& image, int otherX, int otherY, bool pixelLevel) const;

private:
    std::shared_ptr<LcduiImage> m_image;
    int m_frameW;
    int m_frameH;
    int m_cols{1};
    int m_rows{1};
    int m_totalFrames{1};

    int m_x{0};
    int m_y{0};
    int m_transform{TRANS_NONE};

    int m_currentFrame{0};
    std::vector<int> m_sequence;
    int m_seqIndex{0};

    uint32_t getFramePixel(int frame, int u, int v) const;
};

} // namespace j2me

#endif // J2ME_LCDUI_GRAPHICS_H
