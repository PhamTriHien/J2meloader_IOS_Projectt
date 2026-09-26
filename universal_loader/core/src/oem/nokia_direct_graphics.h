#ifndef UNIVERSAL_LOADER_NOKIA_DIRECT_GRAPHICS_H
#define UNIVERSAL_LOADER_NOKIA_DIRECT_GRAPHICS_H

#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>
#include "../../include/j2me_core.h"
#include "../lcdui/lcdui_graphics.h"

namespace universal_loader {
namespace oem {

// Nokia DirectGraphics Manipulation Constants matching com.nokia.mid.ui.DirectGraphics
constexpr int32_t FLIP_HORIZONTAL = 8192;  // 0x2000
constexpr int32_t FLIP_VERTICAL   = 16384; // 0x4000
constexpr int32_t ROTATE_90       = 90;
constexpr int32_t ROTATE_180      = 180;
constexpr int32_t ROTATE_270      = 270;

// Nokia Pixel Format Constants
constexpr int32_t TYPE_BYTE_1_GRAY            = 1;
constexpr int32_t TYPE_BYTE_1_GRAY_VERTICAL   = -1;
constexpr int32_t TYPE_BYTE_2_GRAY            = 2;
constexpr int32_t TYPE_BYTE_4_GRAY            = 4;
constexpr int32_t TYPE_BYTE_8_GRAY            = 8;
constexpr int32_t TYPE_BYTE_332_RGB           = 332;
constexpr int32_t TYPE_USHORT_4444_ARGB       = 4444;
constexpr int32_t TYPE_USHORT_444_RGB         = 444;
constexpr int32_t TYPE_USHORT_555_RGB         = 555;
constexpr int32_t TYPE_USHORT_1555_ARGB       = 1555;
constexpr int32_t TYPE_USHORT_565_RGB         = 565;
constexpr int32_t TYPE_INT_888_RGB            = 888;
constexpr int32_t TYPE_INT_8888_ARGB          = 8888;

class J2ME_API NokiaDirectGraphics {
public:
    explicit NokiaDirectGraphics(j2me::LcduiGraphics* targetGraphics);
    ~NokiaDirectGraphics() = default;

    // Convert Nokia manipulation flags into LCDUI 8 Transforms
    static int32_t getTransformation(int32_t manipulation);

    void setARGBColor(uint32_t argbColor);
    int32_t getAlphaComponent() const;
    int32_t getNativePixelFormat() const;

    // Drawing with manipulations
    void drawImage(std::shared_ptr<j2me::LcduiImage> image, int32_t x, int32_t y,
                   int32_t anchor, int32_t manipulation);

    // drawPixels: short formats (4444, 444, 565)
    void drawPixels(const uint16_t* pixels, bool transparency, int32_t offset,
                    int32_t scanlength, int32_t x, int32_t y, int32_t width,
                    int32_t height, int32_t manipulation, int32_t format);

    // drawPixels: int formats (888, 8888)
    void drawPixels(const uint32_t* pixels, bool transparency, int32_t offset,
                    int32_t scanlength, int32_t x, int32_t y, int32_t width,
                    int32_t height, int32_t manipulation, int32_t format);

    // drawPixels: byte formats (1_GRAY, 1_GRAY_VERTICAL)
    void drawPixels(const uint8_t* pixels, const uint8_t* transparencyMask,
                    int32_t offset, int32_t scanlength, int32_t x, int32_t y,
                    int32_t width, int32_t height, int32_t manipulation, int32_t format);

    // getPixels
    void getPixels(uint16_t* pixels, int32_t offset, int32_t scanlength,
                   int32_t x, int32_t y, int32_t width, int32_t height, int32_t format);
    void getPixels(uint32_t* pixels, int32_t offset, int32_t scanlength,
                   int32_t x, int32_t y, int32_t width, int32_t height, int32_t format);

    // Polygon & Triangle primitives
    void drawPolygon(const int32_t* xPoints, const int32_t* yPoints, int32_t nPoints, uint32_t argbColor);
    void fillPolygon(const int32_t* xPoints, const int32_t* yPoints, int32_t nPoints, uint32_t argbColor);
    void drawTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t argbColor);
    void fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, uint32_t argbColor);

private:
    j2me::LcduiGraphics* m_target{nullptr};
    uint32_t m_currentArgbColor{0xFF000000};
    uint8_t m_alphaComponent{0xFF};

    static uint32_t blendColors(uint32_t src, uint32_t dst);
    static uint32_t decodeShortPixel(uint16_t pixel, int32_t format);
};

} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_NOKIA_DIRECT_GRAPHICS_H
