#include <cstdio>
#include <cstdlib>
#include "lcdui_graphics.h"
#include "font.h"
#include "../jvm/jar_reader.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace j2me {

// ============================================================================
// 1. LcduiImage Triển Khai
// ============================================================================

std::shared_ptr<LcduiImage> LcduiImage::createImage(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    return std::make_shared<LcduiImage>(width, height, true);
}

std::shared_ptr<LcduiImage> LcduiImage::createImage(int width, int height, uint32_t argb) {
    if (width <= 0 || height <= 0) return nullptr;
    auto img = std::make_shared<LcduiImage>(width, height, true);
    std::fill(img->m_pixels.begin(), img->m_pixels.end(), argb);
    return img;
}

std::shared_ptr<LcduiImage> LcduiImage::createImage(const LcduiImage& source) {
    auto img = std::make_shared<LcduiImage>(source.m_width, source.m_height, false);
    img->m_pixels = source.m_pixels;
    return img;
}

std::shared_ptr<LcduiImage> LcduiImage::createImage(const LcduiImage& image, int x, int y, int width, int height, int transform) {
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x + width > image.m_width || y + height > image.m_height) {
        return nullptr;
    }
    bool swapDims = (transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
                     transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270);
    int targetW = swapDims ? height : width;
    int targetH = swapDims ? width : height;

    auto result = std::make_shared<LcduiImage>(targetW, targetH, false);
    uint32_t* dst = result->getPixelsMutable();
    const uint32_t* src = image.getPixels();

    for (int dy = 0; dy < targetH; ++dy) {
        for (int dx = 0; dx < targetW; ++dx) {
            int sx = 0, sy = 0;
            switch (transform) {
                case TRANS_NONE:
                    sx = x + dx; sy = y + dy; break;
                case TRANS_MIRROR:
                    sx = x + (width - 1 - dx); sy = y + dy; break;
                case TRANS_ROT180:
                    sx = x + (width - 1 - dx); sy = y + (height - 1 - dy); break;
                case TRANS_MIRROR_ROT180:
                    sx = x + dx; sy = y + (height - 1 - dy); break;
                case TRANS_ROT90:
                    sx = x + dy; sy = y + (height - 1 - dx); break;
                case TRANS_ROT270:
                    sx = x + (width - 1 - dy); sy = y + dx; break;
                case TRANS_MIRROR_ROT90:
                    sx = x + (width - 1 - dy); sy = y + (height - 1 - dx); break;
                case TRANS_MIRROR_ROT270:
                    sx = x + dy; sy = y + dx; break;
                default:
                    sx = x + dx; sy = y + dy; break;
            }
            dst[dy * targetW + dx] = src[sy * image.m_width + sx];
        }
    }
    return result;
}

std::shared_ptr<LcduiImage> LcduiImage::createRGBImage(const uint32_t* rgb, int width, int height, bool processAlpha) {
    if (!rgb || width <= 0 || height <= 0) return nullptr;
    auto img = std::make_shared<LcduiImage>(width, height, false);
    uint32_t* dst = img->getPixelsMutable();
    size_t total = (size_t)width * height;
    if (processAlpha) {
        std::memcpy(dst, rgb, total * sizeof(uint32_t));
    } else {
        for (size_t i = 0; i < total; ++i) {
            dst[i] = 0xFF000000 | (rgb[i] & 0x00FFFFFF);
        }
    }
    return img;
}

static inline uint8_t pngPaethPredictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<uint8_t>(a);
    if (pb <= pc) return static_cast<uint8_t>(b);
    return static_cast<uint8_t>(c);
}

// Reverses the PNG scanline filters of one (sub)image in place. Returns false on a bad filter type.
static bool pngUnfilter(uint8_t* data, size_t rows, size_t rowBytes, size_t bytesPerPixel) {
    const uint8_t* prev = nullptr;
    for (size_t y = 0; y < rows; ++y) {
        uint8_t filter = data[0];
        uint8_t* row = data + 1;
        for (size_t x = 0; x < rowBytes; ++x) {
            int a = x >= bytesPerPixel ? row[x - bytesPerPixel] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= bytesPerPixel) ? prev[x - bytesPerPixel] : 0;
            switch (filter) {
                case 0: break;
                case 1: row[x] = static_cast<uint8_t>(row[x] + a); break;
                case 2: row[x] = static_cast<uint8_t>(row[x] + b); break;
                case 3: row[x] = static_cast<uint8_t>(row[x] + ((a + b) >> 1)); break;
                case 4: row[x] = static_cast<uint8_t>(row[x] + pngPaethPredictor(a, b, c)); break;
                default: return false;
            }
        }
        prev = row;
        data += 1 + rowBytes;
    }
    return true;
}

// Full PNG decoder: all color types, bit depths 1-16, tRNS and Adam7 interlacing.
// Returns nullptr for data it cannot decode; callers raise the Java exception MIDP requires.
std::shared_ptr<LcduiImage> LcduiImage::createImage(const uint8_t* rawData, size_t size) {
    static const uint8_t kMagic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (!rawData || size < 8 || std::memcmp(rawData, kMagic, 8) != 0) {
        if (rawData && size >= 4 && std::getenv("J2ME_TRACE")) {
            std::fprintf(stderr, "[J2ME TRACE] image decode failed, %zu bytes, head %02X %02X %02X %02X\n", size,
                         rawData[0], rawData[1], rawData[2], rawData[3]);
        }
        return nullptr;
    }

    auto be32 = [](const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    };

    uint32_t width = 0, height = 0;
    uint8_t bitDepth = 8, colorType = 6, interlace = 0;
    std::vector<uint8_t> palette, trns, idat;
    for (size_t offset = 8; offset + 8 <= size;) {
        uint32_t len = be32(rawData + offset);
        std::string type(reinterpret_cast<const char*>(rawData + offset + 4), 4);
        const uint8_t* body = rawData + offset + 8;
        if (len > size - offset - 8) break;
        if (type == "IHDR" && len >= 13) {
            width = be32(body);
            height = be32(body + 4);
            bitDepth = body[8];
            colorType = body[9];
            interlace = body[12];
        } else if (type == "PLTE") {
            palette.assign(body, body + len);
        } else if (type == "tRNS") {
            trns.assign(body, body + len);
        } else if (type == "IDAT") {
            idat.insert(idat.end(), body, body + len);
        } else if (type == "IEND") {
            break;
        }
        offset += 12 + static_cast<size_t>(len);
    }

    size_t channels = 0;
    switch (colorType) {
        case 0: channels = 1; break; // gray
        case 2: channels = 3; break; // RGB
        case 3: channels = 1; break; // indexed
        case 4: channels = 2; break; // gray + alpha
        case 6: channels = 4; break; // RGBA
        default: return nullptr;
    }
    if (width == 0 || height == 0 || width > 8192 || height > 8192 || idat.size() < 2) return nullptr;
    if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8 && bitDepth != 16) return nullptr;
    const size_t bitsPerPixel = channels * bitDepth;
    const size_t bytesPerPixel = std::max<size_t>(1, bitsPerPixel / 8);

    // Adam7 passes (x0, y0, dx, dy); a non-interlaced image is a single pass
    struct Pass { uint32_t x0, y0, dx, dy; };
    static const Pass kAdam7[7] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
    static const Pass kSingle[1] = {{0, 0, 1, 1}};
    const Pass* passes = interlace ? kAdam7 : kSingle;
    const int passCount = interlace ? 7 : 1;
    auto passWidth = [&](const Pass& ps) -> size_t { return ps.x0 >= width ? 0 : (width - ps.x0 + ps.dx - 1) / ps.dx; };
    auto passHeight = [&](const Pass& ps) -> size_t { return ps.y0 >= height ? 0 : (height - ps.y0 + ps.dy - 1) / ps.dy; };

    size_t total = 0;
    for (int i = 0; i < passCount; ++i) {
        size_t pw = passWidth(passes[i]), ph = passHeight(passes[i]);
        if (pw && ph) total += ph * (1 + (pw * bitsPerPixel + 7) / 8);
    }
    std::vector<uint8_t> raw(total, 0);
    // Skip the 2-byte zlib header and inflate the raw deflate stream
    if (!j2me::JarReader::inflateRaw(idat.data() + 2, idat.size() - 2, raw.data(), raw.size())) return nullptr;

    // Transparent color key for gray / RGB images, compared at the image's own bit depth
    const bool hasKey = (colorType == 0 && trns.size() >= 2) || (colorType == 2 && trns.size() >= 6);
    auto key = [&](int i) -> uint32_t { return (static_cast<uint32_t>(trns[i * 2]) << 8) | trns[i * 2 + 1]; };
    auto sample = [&](const uint8_t* row, size_t x, size_t ch) -> uint32_t {
        if (bitDepth == 16) {
            const uint8_t* p = row + (x * channels + ch) * 2;
            return (static_cast<uint32_t>(p[0]) << 8) | p[1];
        }
        if (bitDepth == 8) return row[x * channels + ch];
        size_t bit = x * bitDepth; // sub-byte depths only occur with a single channel
        return (row[bit >> 3] >> (8 - bitDepth - (bit & 7))) & ((1u << bitDepth) - 1);
    };
    auto to8 = [&](uint32_t v) -> uint32_t {
        switch (bitDepth) {
            case 1: return v * 255;
            case 2: return v * 85;
            case 4: return v * 17;
            case 16: return v >> 8;
            default: return v;
        }
    };

    auto img = std::make_shared<LcduiImage>(static_cast<int>(width), static_cast<int>(height), false);
    uint32_t* pixels = img->getPixelsMutable();
    uint8_t* data = raw.data();
    for (int i = 0; i < passCount; ++i) {
        const Pass& ps = passes[i];
        size_t pw = passWidth(ps), ph = passHeight(ps);
        if (!pw || !ph) continue;
        size_t rowBytes = (pw * bitsPerPixel + 7) / 8;
        if (!pngUnfilter(data, ph, rowBytes, bytesPerPixel)) return nullptr;
        for (size_t py = 0; py < ph; ++py) {
            const uint8_t* row = data + py * (1 + rowBytes) + 1;
            for (size_t px = 0; px < pw; ++px) {
                uint32_t r = 0, g = 0, b = 0, a = 255;
                if (colorType == 3) {
                    uint32_t idx = sample(row, px, 0);
                    if (idx * 3 + 2 < palette.size()) {
                        r = palette[idx * 3];
                        g = palette[idx * 3 + 1];
                        b = palette[idx * 3 + 2];
                    }
                    if (idx < trns.size()) a = trns[idx];
                } else if (colorType == 0 || colorType == 4) {
                    uint32_t v = sample(row, px, 0);
                    if (hasKey && v == key(0)) a = 0;
                    r = g = b = to8(v);
                    if (colorType == 4) a = to8(sample(row, px, 1));
                } else {
                    uint32_t rv = sample(row, px, 0), gv = sample(row, px, 1), bv = sample(row, px, 2);
                    if (hasKey && rv == key(0) && gv == key(1) && bv == key(2)) a = 0;
                    r = to8(rv);
                    g = to8(gv);
                    b = to8(bv);
                    if (colorType == 6) a = to8(sample(row, px, 3));
                }
                size_t x = ps.x0 + px * ps.dx, y = ps.y0 + py * ps.dy;
                pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        data += ph * (1 + rowBytes);
    }
    return img;
}

LcduiImage::LcduiImage(int width, int height, bool isMutable)
    : m_width(width), m_height(height), m_isMutable(isMutable) {
    m_pixels.assign((size_t)width * height, 0xFFFFFFFF);
}

LcduiImage::~LcduiImage() {}

std::shared_ptr<LcduiGraphics> LcduiImage::getGraphics() {
    if (!m_isMutable) return nullptr;
    if (!m_graphicsContext) {
        m_graphicsContext = std::make_shared<LcduiGraphics>(m_pixels.data(), m_width, m_height);
    }
    return m_graphicsContext;
}

void LcduiImage::getRGB(int32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height) const {
    if (!rgbData || x < 0 || y < 0 || x + width > m_width || y + height > m_height) return;

    for (int j = 0; j < height; ++j) {
        const uint32_t* srcRow = &m_pixels[(y + j) * m_width + x];
        int32_t* dstRow = rgbData + offset + (j * scanlength);
        for (int i = 0; i < width; ++i) {
            dstRow[i] = (int32_t)srcRow[i];
        }
    }
}

// ============================================================================
// 2. LcduiGraphics Triển Khai (MIDP 2.0 Graphics Specification)
// ============================================================================

LcduiGraphics::LcduiGraphics(uint32_t* targetBuffer, int bufferWidth, int bufferHeight)
    : m_target(targetBuffer), m_bufW(bufferWidth), m_bufH(bufferHeight) {
    m_clipX = 0;
    m_clipY = 0;
    m_clipW = bufferWidth;
    m_clipH = bufferHeight;
}

LcduiGraphics::~LcduiGraphics() {}

void LcduiGraphics::translate(int x, int y) {
    m_transX += x;
    m_transY += y;
}

void LcduiGraphics::setClip(int x, int y, int width, int height) {
    int absX = x + m_transX;
    int absY = y + m_transY;
    int cx = std::max(0, absX);
    int cy = std::max(0, absY);
    int cr = std::min(m_bufW, absX + width);
    int cb = std::min(m_bufH, absY + height);

    m_clipX = cx;
    m_clipY = cy;
    m_clipW = std::max(0, cr - cx);
    m_clipH = std::max(0, cb - cy);
}

void LcduiGraphics::clipRect(int x, int y, int width, int height) {
    int absX = x + m_transX;
    int absY = y + m_transY;
    int cx = std::max(m_clipX, absX);
    int cy = std::max(m_clipY, absY);
    int cr = std::min(m_clipX + m_clipW, absX + width);
    int cb = std::min(m_clipY + m_clipH, absY + height);

    m_clipX = cx;
    m_clipY = cy;
    m_clipW = std::max(0, cr - cx);
    m_clipH = std::max(0, cb - cy);
}

void LcduiGraphics::setColor(uint32_t argbColor) {
    m_currentColor = argbColor;
}

void LcduiGraphics::setColorRGB(int r, int g, int b) {
    m_currentColor = 0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

void LcduiGraphics::setGrayScale(int value) {
    int v = std::clamp(value, 0, 255);
    setColorRGB(v, v, v);
}

int LcduiGraphics::getGrayScale() const {
    int r = (m_currentColor >> 16) & 0xFF;
    int g = (m_currentColor >> 8) & 0xFF;
    int b = m_currentColor & 0xFF;
    // Chuẩn đặc tả upstream J2ME-Loader Graphics.java:
    // return 0x4CB2 * r + 0x9691 * g + 0x1D3E * b >> 16;
    return (0x4CB2 * r + 0x9691 * g + 0x1D3E * b) >> 16;
}

int LcduiGraphics::getRedComponent() const {
    return (m_currentColor >> 16) & 0xFF;
}

int LcduiGraphics::getGreenComponent() const {
    return (m_currentColor >> 8) & 0xFF;
}

int LcduiGraphics::getBlueComponent() const {
    return m_currentColor & 0xFF;
}

void LcduiGraphics::setStrokeStyle(int stroke) {
    m_strokeStyle = (stroke == DOTTED) ? DOTTED : SOLID;
}

int LcduiGraphics::getStrokeStyle() const {
    return m_strokeStyle;
}

void LcduiGraphics::drawPixelRaw(int x, int y, uint32_t color) {
    if (isClipped(x, y)) return;
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 255) {
        m_target[y * m_bufW + x] = color;
    } else if (a > 0) {
        uint32_t dst = m_target[y * m_bufW + x];
        uint32_t invA = 255 - a;
        uint32_t r = (((color >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * invA) / 255;
        uint32_t g = (((color >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * invA) / 255;
        uint32_t b = ((color & 0xFF) * a + (dst & 0xFF) * invA) / 255;
        m_target[y * m_bufW + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
}

void LcduiGraphics::drawLine(int x1, int y1, int x2, int y2) {
    int x0 = x1 + m_transX;
    int y0 = y1 + m_transY;
    int xTarget = x2 + m_transX;
    int yTarget = y2 + m_transY;

    int dx = std::abs(xTarget - x0);
    int dy = std::abs(yTarget - y0);
    int sx = (x0 < xTarget) ? 1 : -1;
    int sy = (y0 < yTarget) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawPixelRaw(x0, y0, m_currentColor);
        if (x0 == xTarget && y0 == yTarget) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void LcduiGraphics::drawRect(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) return;
    drawLine(x, y, x + width - 1, y);
    drawLine(x, y + height - 1, x + width - 1, y + height - 1);
    drawLine(x, y, x, y + height - 1);
    drawLine(x + width - 1, y, x + width - 1, y + height - 1);
}

void LcduiGraphics::fillRect(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) return;
    int absX = x + m_transX;
    int absY = y + m_transY;

    int rx = std::max(m_clipX, absX);
    int ry = std::max(m_clipY, absY);
    int rw = std::min(m_clipX + m_clipW, absX + width) - rx;
    int rh = std::min(m_clipY + m_clipH, absY + height) - ry;

    if (rw <= 0 || rh <= 0) return;

    uint32_t a = (m_currentColor >> 24) & 0xFF;
    if (a == 255) {
        for (int j = ry; j < ry + rh; ++j) {
            uint32_t* row = &m_target[j * m_bufW + rx];
            std::fill(row, row + rw, m_currentColor);
        }
    } else {
        for (int j = ry; j < ry + rh; ++j) {
            for (int i = rx; i < rx + rw; ++i) {
                drawPixelRaw(i, j, m_currentColor);
            }
        }
    }
}

void LcduiGraphics::drawRoundRect(int x, int y, int width, int height, int arcW, int arcH) {
    drawRect(x, y, width, height);
}

void LcduiGraphics::fillRoundRect(int x, int y, int width, int height, int arcW, int arcH) {
    fillRect(x, y, width, height);
}

void LcduiGraphics::drawArc(int x, int y, int width, int height, int startAngle, int arcAngle) {
    int cx = x + m_transX + width / 2;
    int cy = y + m_transY + height / 2;
    int rx = width / 2;
    int ry = height / 2;

    int steps = std::max(16, (rx + ry) / 2);
    double startRad = startAngle * M_PI / 180.0;
    double arcRad = arcAngle * M_PI / 180.0;

    int prevX = cx + (int)(rx * std::cos(startRad));
    int prevY = cy - (int)(ry * std::sin(startRad));

    for (int i = 1; i <= steps; ++i) {
        double currentRad = startRad + (arcRad * i / steps);
        int curX = cx + (int)(rx * std::cos(currentRad));
        int curY = cy - (int)(ry * std::sin(currentRad));
        drawLine(prevX - m_transX, prevY - m_transY, curX - m_transX, curY - m_transY);
        prevX = curX;
        prevY = curY;
    }
}

void LcduiGraphics::fillArc(int x, int y, int width, int height, int startAngle, int arcAngle) {
    drawArc(x, y, width, height, startAngle, arcAngle);
}

void LcduiGraphics::drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    drawLine(x1, y1, x2, y2);
    drawLine(x2, y2, x3, y3);
    drawLine(x3, y3, x1, y1);
}

// Thuật toán Scanline Rasterizer cho tam giác phẳng (Fill Triangle)
void LcduiGraphics::fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sắp xếp các đỉnh theo thứ tự y tăng dần: y1 <= y2 <= y3
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y1 > y3) { std::swap(x1, x3); std::swap(y1, y3); }
    if (y2 > y3) { std::swap(x2, x3); std::swap(y2, y3); }

    int totalHeight = y3 - y1;
    if (totalHeight == 0) return;

    for (int i = 0; i <= totalHeight; ++i) {
        bool secondHalf = i > (y2 - y1) || (y2 == y1);
        int segmentHeight = secondHalf ? (y3 - y2) : (y2 - y1);
        if (segmentHeight == 0) continue;

        float alpha = (float)i / totalHeight;
        float beta = (float)(i - (secondHalf ? (y2 - y1) : 0)) / segmentHeight;

        int ax = x1 + (int)((x3 - x1) * alpha);
        int bx = secondHalf ? (x2 + (int)((x3 - x2) * beta)) : (x1 + (int)((x2 - x1) * beta));

        if (ax > bx) std::swap(ax, bx);
        int py = y1 + i;
        for (int px = ax; px <= bx; ++px) {
            drawPixelRaw(px + m_transX, py + m_transY, m_currentColor);
        }
    }
}

void LcduiGraphics::drawPolygon(const int* xPoints, const int* yPoints, int nPoints) {
    if (!xPoints || !yPoints || nPoints < 2) return;
    for (int i = 0; i < nPoints - 1; ++i) {
        drawLine(xPoints[i], yPoints[i], xPoints[i + 1], yPoints[i + 1]);
    }
    drawLine(xPoints[nPoints - 1], yPoints[nPoints - 1], xPoints[0], yPoints[0]);
}

void LcduiGraphics::fillPolygon(const int* xPoints, const int* yPoints, int nPoints) {
    if (!xPoints || !yPoints || nPoints < 3) return;
    int minY = yPoints[0], maxY = yPoints[0];
    for (int i = 1; i < nPoints; ++i) {
        minY = std::min(minY, yPoints[i]);
        maxY = std::max(maxY, yPoints[i]);
    }

    std::vector<int> nodeX;
    for (int y = minY; y <= maxY; ++y) {
        nodeX.clear();
        int j = nPoints - 1;
        for (int i = 0; i < nPoints; ++i) {
            if ((yPoints[i] < y && yPoints[j] >= y) || (yPoints[j] < y && yPoints[i] >= y)) {
                int node = xPoints[i] + (y - yPoints[i]) * (xPoints[j] - xPoints[i]) / (yPoints[j] - yPoints[i]);
                nodeX.push_back(node);
            }
            j = i;
        }
        std::sort(nodeX.begin(), nodeX.end());
        for (size_t i = 0; i + 1 < nodeX.size(); i += 2) {
            if (nodeX[i] >= m_bufW) break;
            if (nodeX[i + 1] > 0) {
                int x1 = std::max(0, nodeX[i]);
                int x2 = std::min(m_bufW - 1, nodeX[i + 1]);
                for (int x = x1; x <= x2; ++x) {
                    drawPixelRaw(x + m_transX, y + m_transY, m_currentColor);
                }
            }
        }
    }
}

void LcduiGraphics::copyArea(int x_src, int y_src, int width, int height, int x_dest, int y_dest, int anchor) {
    if (width <= 0 || height <= 0) return;
    int absXSrc = x_src + m_transX;
    int absYSrc = y_src + m_transY;
    if (absXSrc < 0 || absYSrc < 0 || absXSrc + width > m_bufW || absYSrc + height > m_bufH) return;

    int dx = 0, dy = 0;
    calculateAnchor(x_dest, y_dest, width, height, anchor, dx, dy);

    std::vector<uint32_t> tempPixels((size_t)width * height);
    for (int j = 0; j < height; ++j) {
        std::memcpy(&tempPixels[j * width], &m_target[(absYSrc + j) * m_bufW + absXSrc], width * sizeof(uint32_t));
    }

    for (int j = 0; j < height; ++j) {
        int py = dy + j;
        for (int i = 0; i < width; ++i) {
            int px = dx + i;
            drawPixelRaw(px, py, tempPixels[j * width + i]);
        }
    }
}

void LcduiGraphics::calculateAnchor(int x, int y, int w, int h, int anchor, int& outX, int& outY) {
    outX = x;
    outY = y;
    if (anchor & ANCHOR_RIGHT) {
        outX -= w;
    } else if (anchor & ANCHOR_HCENTER) {
        outX -= w / 2;
    }

    if (anchor & ANCHOR_BOTTOM) {
        outY -= h;
    } else if (anchor & ANCHOR_VCENTER) {
        outY -= h / 2;
    }
}

void LcduiGraphics::drawRGB(const uint32_t* rgbData, int offset, int scanlength, int x, int y, int width, int height, bool processAlpha) {
    if (!rgbData || width <= 0 || height <= 0) return;
    int absX = x + m_transX;
    int absY = y + m_transY;

    for (int j = 0; j < height; ++j) {
        int py = absY + j;
        if (py < m_clipY || py >= m_clipY + m_clipH) continue;

        const uint32_t* srcRow = rgbData + offset + (j * scanlength);
        for (int i = 0; i < width; ++i) {
            int px = absX + i;
            if (px < m_clipX || px >= m_clipX + m_clipW) continue;

            uint32_t pixel = srcRow[i];
            if (processAlpha) {
                drawPixelRaw(px, py, pixel);
            } else {
                m_target[py * m_bufW + px] = 0xFF000000 | (pixel & 0x00FFFFFF);
            }
        }
    }
}

void LcduiGraphics::drawImage(LcduiImage* img, int x, int y, int anchor) {
    if (!img) return;
    drawRegion(img, 0, 0, img->getWidth(), img->getHeight(), TRANS_NONE, x, y, anchor);
}

// Triển khai toàn diện 8 phép biến hình (Transforms) chuẩn MIDP 2.0
void LcduiGraphics::drawRegion(LcduiImage* src, int x_src, int y_src, int width, int height,
                               int transform, int x_dest, int y_dest, int anchor) {
    if (!src || width <= 0 || height <= 0) return;
    if (x_src < 0 || y_src < 0 || x_src + width > src->getWidth() || y_src + height > src->getHeight()) return;

    bool swapAxes = (transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
                     transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270);

    int dstW = swapAxes ? height : width;
    int dstH = swapAxes ? width : height;

    int finalX = 0, finalY = 0;
    calculateAnchor(x_dest, y_dest, dstW, dstH, anchor, finalX, finalY);

    int absX = finalX + m_transX;
    int absY = finalY + m_transY;

    const uint32_t* srcPixels = src->getPixels();
    int srcStride = src->getWidth();

    for (int dy = 0; dy < dstH; ++dy) {
        int py = absY + dy;
        if (py < m_clipY || py >= m_clipY + m_clipH) continue;

        for (int dx = 0; dx < dstW; ++dx) {
            int px = absX + dx;
            if (px < m_clipX || px >= m_clipX + m_clipW) continue;

            int sx = 0, sy = 0;
            switch (transform) {
                case TRANS_NONE:
                    sx = x_src + dx;
                    sy = y_src + dy;
                    break;
                case TRANS_MIRROR:
                    sx = x_src + (width - 1 - dx);
                    sy = y_src + dy;
                    break;
                case TRANS_MIRROR_ROT180:
                    sx = x_src + dx;
                    sy = y_src + (height - 1 - dy);
                    break;
                case TRANS_ROT180:
                    sx = x_src + (width - 1 - dx);
                    sy = y_src + (height - 1 - dy);
                    break;
                case TRANS_ROT90:
                    sx = x_src + dy;
                    sy = y_src + (height - 1 - dx);
                    break;
                case TRANS_ROT270:
                    sx = x_src + (width - 1 - dy);
                    sy = y_src + dx;
                    break;
                case TRANS_MIRROR_ROT90:
                    sx = x_src + (width - 1 - dy);
                    sy = y_src + (height - 1 - dx);
                    break;
                case TRANS_MIRROR_ROT270:
                    sx = x_src + dy;
                    sy = y_src + dx;
                    break;
                default:
                    sx = x_src + dx;
                    sy = y_src + dy;
                    break;
            }

            uint32_t pixel = srcPixels[sy * srcStride + sx];
            drawPixelRaw(px, py, pixel);
        }
    }
}

void LcduiGraphics::setFont(std::shared_ptr<LcduiFont> font) {
    m_font = std::move(font);
}

std::shared_ptr<LcduiFont> LcduiGraphics::getFont() const {
    if (!m_font) {
        return LcduiFont::getDefaultFont();
    }
    return m_font;
}

void LcduiGraphics::drawString(const std::string& text, int x, int y, int anchor) {
    auto f = getFont();
    f->renderString(text, m_target, m_bufW, m_bufH, m_clipX, m_clipY, m_clipW, m_clipH, x + m_transX, y + m_transY, anchor, m_currentColor);
}

void LcduiGraphics::drawSubstring(const std::string& text, int offset, int len, int x, int y, int anchor) {
    auto f = getFont();
    f->renderSubstring(text, offset, len, m_target, m_bufW, m_bufH, m_clipX, m_clipY, m_clipW, m_clipH, x + m_transX, y + m_transY, anchor, m_currentColor);
}

void LcduiGraphics::drawChar(char c, int x, int y, int anchor) {
    std::string s(1, c);
    drawString(s, x, y, anchor);
}

void LcduiGraphics::drawChars(const char* data, int offset, int length, int x, int y, int anchor) {
    if (!data || offset < 0 || length <= 0) return;
    std::string s(data + offset, length);
    drawString(s, x, y, anchor);
}

// ============================================================================
// 3. GameCanvasEngine Triển Khai
// ============================================================================

GameCanvasEngine::GameCanvasEngine(int width, int height)
    : m_width(width), m_height(height) {
    size_t total = (size_t)width * height;
    m_drawBuffer.assign(total, 0xFF000000);
    m_displayBuffer.assign(total, 0xFF000000);
    m_graphics = std::make_shared<LcduiGraphics>(m_drawBuffer.data(), width, height);
}

GameCanvasEngine::~GameCanvasEngine() {}

int GameCanvasEngine::getKeyStates() {
    return m_keyStates.load();
}

void GameCanvasEngine::setKeyState(int j2meKeyCode, bool isPressed) {
    int mask = 0;
    switch (j2meKeyCode) {
        case J2ME_KEY_UP:
        case J2ME_KEY_NUM2:
            mask = UP_PRESSED;
            break;
        case J2ME_KEY_DOWN:
        case J2ME_KEY_NUM8:
            mask = DOWN_PRESSED;
            break;
        case J2ME_KEY_LEFT:
        case J2ME_KEY_NUM4:
            mask = LEFT_PRESSED;
            break;
        case J2ME_KEY_RIGHT:
        case J2ME_KEY_NUM6:
            mask = RIGHT_PRESSED;
            break;
        case J2ME_KEY_FIRE:
        case J2ME_KEY_NUM5:
            mask = FIRE_PRESSED;
            break;
        default:
            break;
    }

    if (mask != 0) {
        if (isPressed) {
            m_keyStates.fetch_or(mask);
        } else {
            m_keyStates.fetch_and(~mask);
        }
    }
}

std::shared_ptr<LcduiGraphics> GameCanvasEngine::getGraphics() {
    return m_graphics;
}

void GameCanvasEngine::flushGraphics() {
    std::memcpy(m_displayBuffer.data(), m_drawBuffer.data(), m_width * m_height * sizeof(uint32_t));
}

void GameCanvasEngine::flushGraphics(int x, int y, int width, int height) {
    flushGraphics();
}

// ============================================================================
// 4. SpriteEngine Triển Khai (Hoạt Cảnh & Kiểm Tra Va Chạm Chuẩn Pixel-Level)
// ============================================================================

SpriteEngine::SpriteEngine(std::shared_ptr<LcduiImage> image, int frameWidth, int frameHeight)
    : m_image(image), m_frameW(frameWidth), m_frameH(frameHeight) {
    if (m_image && frameWidth > 0 && frameHeight > 0) {
        m_cols = m_image->getWidth() / frameWidth;
        m_rows = m_image->getHeight() / frameHeight;
        m_totalFrames = std::max(1, m_cols * m_rows);
    }
}

SpriteEngine::~SpriteEngine() {}

void SpriteEngine::setFrame(int sequenceIndex) {
    if (m_sequence.empty()) {
        m_currentFrame = std::clamp(sequenceIndex, 0, m_totalFrames - 1);
    } else {
        m_seqIndex = std::clamp(sequenceIndex, 0, (int)m_sequence.size() - 1);
        m_currentFrame = m_sequence[m_seqIndex];
    }
}

void SpriteEngine::setFrameSequence(const std::vector<int>& sequence) {
    m_sequence = sequence;
    m_seqIndex = 0;
    if (!m_sequence.empty()) {
        m_currentFrame = m_sequence[0];
    }
}

void SpriteEngine::nextFrame() {
    if (m_sequence.empty()) {
        m_currentFrame = (m_currentFrame + 1) % m_totalFrames;
    } else {
        m_seqIndex = (m_seqIndex + 1) % (int)m_sequence.size();
        m_currentFrame = m_sequence[m_seqIndex];
    }
}

void SpriteEngine::prevFrame() {
    if (m_sequence.empty()) {
        m_currentFrame = (m_currentFrame - 1 + m_totalFrames) % m_totalFrames;
    } else {
        m_seqIndex = (m_seqIndex - 1 + (int)m_sequence.size()) % (int)m_sequence.size();
        m_currentFrame = m_sequence[m_seqIndex];
    }
}

uint32_t SpriteEngine::getFramePixel(int frame, int u, int v) const {
    if (!m_image || u < 0 || u >= m_frameW || v < 0 || v >= m_frameH) return 0;
    int col = frame % m_cols;
    int row = frame / m_cols;
    int px = col * m_frameW + u;
    int py = row * m_frameH + v;
    return m_image->getPixels()[py * m_image->getWidth() + px];
}

void SpriteEngine::paint(LcduiGraphics* g) {
    if (!g || !m_image) return;
    int col = m_currentFrame % m_cols;
    int row = m_currentFrame / m_cols;
    g->drawRegion(m_image.get(), col * m_frameW, row * m_frameH, m_frameW, m_frameH,
                  m_transform, m_x, m_y, ANCHOR_TOP | ANCHOR_LEFT);
}

// Kiểm tra va chạm hộp bao (Bounding Box) & va chạm điểm ảnh (Pixel-Level Collision)
bool SpriteEngine::collidesWith(const SpriteEngine& other, bool pixelLevel) const {
    // 1. Kiểm tra va chạm AABB (Bounding Box)
    int ax1 = m_x;
    int ay1 = m_y;
    int ax2 = m_x + m_frameW;
    int ay2 = m_y + m_frameH;

    int bx1 = other.m_x;
    int by1 = other.m_y;
    int bx2 = other.m_x + other.m_frameW;
    int by2 = other.m_y + other.m_frameH;

    if (ax1 >= bx2 || ax2 <= bx1 || ay1 >= by2 || ay2 <= by1) {
        return false; // Không giao nhau
    }

    if (!pixelLevel) return true; // Va chạm hộp bao thành công

    // 2. Kiểm tra va chạm cấp độ điểm ảnh (Pixel-Level Collision)
    int interX1 = std::max(ax1, bx1);
    int interY1 = std::max(ay1, by1);
    int interX2 = std::min(ax2, bx2);
    int interY2 = std::min(ay2, by2);

    for (int y = interY1; y < interY2; ++y) {
        for (int x = interX1; x < interX2; ++x) {
            uint32_t pA = getFramePixel(m_currentFrame, x - m_x, y - m_y);
            uint32_t pB = other.getFramePixel(other.m_currentFrame, x - other.m_x, y - other.m_y);

            // Cả hai điểm ảnh đều có độ mờ đục > 0 (không trong suốt)
            if (((pA >> 24) & 0xFF) > 0 && ((pB >> 24) & 0xFF) > 0) {
                return true;
            }
        }
    }
    return false;
}

bool SpriteEngine::collidesWithImage(const LcduiImage& image, int otherX, int otherY, bool pixelLevel) const {
    int ax1 = m_x, ay1 = m_y, ax2 = m_x + m_frameW, ay2 = m_y + m_frameH;
    int bx1 = otherX, by1 = otherY, bx2 = otherX + image.getWidth(), by2 = otherY + image.getHeight();

    if (ax1 >= bx2 || ax2 <= bx1 || ay1 >= by2 || ay2 <= by1) return false;
    if (!pixelLevel) return true;

    int interX1 = std::max(ax1, bx1);
    int interY1 = std::max(ay1, by1);
    int interX2 = std::min(ax2, bx2);
    int interY2 = std::min(ay2, by2);

    for (int y = interY1; y < interY2; ++y) {
        for (int x = interX1; x < interX2; ++x) {
            uint32_t pA = getFramePixel(m_currentFrame, x - m_x, y - m_y);
            uint32_t pB = image.getPixels()[(y - otherY) * image.getWidth() + (x - otherX)];
            if (((pA >> 24) & 0xFF) > 0 && ((pB >> 24) & 0xFF) > 0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace j2me
