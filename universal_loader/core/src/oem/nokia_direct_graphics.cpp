#include "nokia_direct_graphics.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace universal_loader {
namespace oem {

NokiaDirectGraphics::NokiaDirectGraphics(j2me::LcduiGraphics* targetGraphics)
    : m_target(targetGraphics) {
}

int32_t NokiaDirectGraphics::getTransformation(int32_t manipulation) {
    int32_t rotation = manipulation & (ROTATE_90 | ROTATE_180 | ROTATE_270);
    int32_t ret = j2me::TRANS_NONE;

    if ((manipulation & FLIP_HORIZONTAL) != 0) {
        if ((manipulation & FLIP_VERTICAL) != 0) {
            // Both horizontal & vertical flipping
            switch (rotation) {
                case 0:          ret = j2me::TRANS_ROT180; break;
                case ROTATE_90:  ret = j2me::TRANS_ROT90;  break;
                case ROTATE_180: ret = j2me::TRANS_NONE;   break;
                case ROTATE_270: ret = j2me::TRANS_ROT270; break;
                default:         ret = j2me::TRANS_NONE;   break;
            }
        } else {
            // Horizontal flipping only
            switch (rotation) {
                case 0:          ret = j2me::TRANS_MIRROR;        break;
                case ROTATE_90:  ret = j2me::TRANS_MIRROR_ROT90;  break;
                case ROTATE_180: ret = j2me::TRANS_MIRROR_ROT180; break;
                case ROTATE_270: ret = j2me::TRANS_MIRROR_ROT270; break;
                default:         ret = j2me::TRANS_MIRROR;        break;
            }
        }
    } else {
        if ((manipulation & FLIP_VERTICAL) != 0) {
            // Vertical flipping only
            switch (rotation) {
                case 0:          ret = j2me::TRANS_MIRROR_ROT180; break;
                case ROTATE_90:  ret = j2me::TRANS_MIRROR_ROT270; break;
                case ROTATE_180: ret = j2me::TRANS_MIRROR;        break;
                case ROTATE_270: ret = j2me::TRANS_MIRROR_ROT90;  break;
                default:         ret = j2me::TRANS_MIRROR_ROT180; break;
            }
        } else {
            // No flipping
            switch (rotation) {
                case 0:          ret = j2me::TRANS_NONE;   break;
                case ROTATE_90:  ret = j2me::TRANS_ROT270; break;
                case ROTATE_180: ret = j2me::TRANS_ROT180; break;
                case ROTATE_270: ret = j2me::TRANS_ROT90;  break;
                default:         ret = j2me::TRANS_NONE;   break;
            }
        }
    }
    return ret;
}

void NokiaDirectGraphics::setARGBColor(uint32_t argbColor) {
    m_currentArgbColor = argbColor;
    m_alphaComponent = static_cast<uint8_t>((argbColor >> 24) & 0xFF);
    if (m_target) {
        m_target->setColor(argbColor & 0x00FFFFFF);
    }
}

int32_t NokiaDirectGraphics::getAlphaComponent() const {
    return m_alphaComponent;
}

int32_t NokiaDirectGraphics::getNativePixelFormat() const {
    return TYPE_INT_8888_ARGB;
}

void NokiaDirectGraphics::drawImage(std::shared_ptr<j2me::LcduiImage> image, int32_t x, int32_t y,
                                    int32_t anchor, int32_t manipulation) {
    if (!m_target || !image) return;

    int32_t transform = getTransformation(manipulation);
    m_target->drawRegion(image.get(), 0, 0, image->getWidth(), image->getHeight(), transform, x, y, anchor);
}

uint32_t NokiaDirectGraphics::decodeShortPixel(uint16_t pixel, int32_t format) {
    switch (format) {
        case TYPE_USHORT_4444_ARGB: {
            uint32_t a = (pixel >> 12) & 0xF; a = (a << 4) | a;
            uint32_t r = (pixel >> 8)  & 0xF; r = (r << 4) | r;
            uint32_t g = (pixel >> 4)  & 0xF; g = (g << 4) | g;
            uint32_t b = pixel         & 0xF; b = (b << 4) | b;
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
        case TYPE_USHORT_444_RGB: {
            uint32_t r = (pixel >> 8)  & 0xF; r = (r << 4) | r;
            uint32_t g = (pixel >> 4)  & 0xF; g = (g << 4) | g;
            uint32_t b = pixel         & 0xF; b = (b << 4) | b;
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        case TYPE_USHORT_565_RGB: {
            uint32_t r = (pixel >> 11) & 0x1F; r = (r << 3) | (r >> 2);
            uint32_t g = (pixel >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
            uint32_t b = pixel         & 0x1F; b = (b << 3) | (b >> 2);
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        case TYPE_USHORT_1555_ARGB: {
            uint32_t a = (pixel & 0x8000) ? 0xFF : 0x00;
            uint32_t r = (pixel >> 10) & 0x1F; r = (r << 3) | (r >> 2);
            uint32_t g = (pixel >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
            uint32_t b = pixel         & 0x1F; b = (b << 3) | (b >> 2);
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
        default:
            return 0xFF000000;
    }
}

void NokiaDirectGraphics::drawPixels(const uint16_t* pixels, bool transparency, int32_t offset,
                                    int32_t scanlength, int32_t x, int32_t y, int32_t width,
                                    int32_t height, int32_t manipulation, int32_t format) {
    if (!m_target || !pixels || width <= 0 || height <= 0) return;

    std::vector<uint32_t> converted(static_cast<size_t>(width) * height);
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            uint16_t p = pixels[offset + row * scanlength + col];
            uint32_t argb = decodeShortPixel(p, format);
            if (!transparency) {
                argb |= 0xFF000000;
            }
            converted[static_cast<size_t>(row) * width + col] = argb;
        }
    }

    auto img = j2me::LcduiImage::createRGBImage(converted.data(), width, height, true);
    int32_t transform = getTransformation(manipulation);
    m_target->drawRegion(img.get(), 0, 0, width, height, transform, x, y, 0);
}

void NokiaDirectGraphics::drawPixels(const uint32_t* pixels, bool transparency, int32_t offset,
                                    int32_t scanlength, int32_t x, int32_t y, int32_t width,
                                    int32_t height, int32_t manipulation, int32_t format) {
    if (!m_target || !pixels || width <= 0 || height <= 0) return;

    std::vector<uint32_t> converted(static_cast<size_t>(width) * height);
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            uint32_t p = pixels[offset + row * scanlength + col];
            if (format == TYPE_INT_888_RGB || !transparency) {
                p |= 0xFF000000;
            }
            converted[static_cast<size_t>(row) * width + col] = p;
        }
    }

    auto img = j2me::LcduiImage::createRGBImage(converted.data(), width, height, true);
    int32_t transform = getTransformation(manipulation);
    m_target->drawRegion(img.get(), 0, 0, width, height, transform, x, y, 0);
}

void NokiaDirectGraphics::drawPixels(const uint8_t* pixels, const uint8_t* transparencyMask,
                                    int32_t offset, int32_t scanlength, int32_t x, int32_t y,
                                    int32_t width, int32_t height, int32_t manipulation, int32_t format) {
    if (!m_target || !pixels || width <= 0 || height <= 0) return;

    std::vector<uint32_t> converted(static_cast<size_t>(width) * height);

    if (format == TYPE_BYTE_1_GRAY) {
        for (int32_t row = 0; row < height; ++row) {
            int32_t line = offset + row * scanlength;
            for (int32_t col = 0; col < width; ++col) {
                int32_t bitIdx = line + col;
                int32_t byteIdx = bitIdx / 8;
                int32_t bitOffset = 7 - (bitIdx % 8);

                uint8_t bit = (pixels[byteIdx] >> bitOffset) & 1;
                uint32_t color = (bit == 1) ? 0xFFFFFFFF : 0xFF000000;

                if (transparencyMask != nullptr) {
                    uint8_t alphaBit = (transparencyMask[byteIdx] >> bitOffset) & 1;
                    if (alphaBit == 0) {
                        color = 0x00000000;
                    }
                }
                converted[static_cast<size_t>(row) * width + col] = color;
            }
        }
    } else {
        // Fallback default
        for (int32_t i = 0; i < width * height; ++i) {
            converted[i] = 0xFFFFFFFF;
        }
    }

    auto img = j2me::LcduiImage::createRGBImage(converted.data(), width, height, true);
    int32_t transform = getTransformation(manipulation);
    m_target->drawRegion(img.get(), 0, 0, width, height, transform, x, y, 0);
}

void NokiaDirectGraphics::getPixels(uint16_t* pixels, int32_t offset, int32_t scanlength,
                                   int32_t x, int32_t y, int32_t width, int32_t height, int32_t format) {
    if (!m_target || !pixels || width <= 0 || height <= 0) return;

    int32_t targetW = m_target->getBufferWidth();
    int32_t targetH = m_target->getBufferHeight();
    const uint32_t* srcBuffer = m_target->getRawBuffer();
    if (!srcBuffer) return;

    for (int32_t row = 0; row < height; ++row) {
        int32_t sy = y + row;
        if (sy < 0 || sy >= targetH) continue;

        for (int32_t col = 0; col < width; ++col) {
            int32_t sx = x + col;
            if (sx < 0 || sx >= targetW) continue;

            uint32_t argb = srcBuffer[sy * targetW + sx];
            uint16_t outPixel = 0;

            if (format == TYPE_USHORT_565_RGB) {
                uint32_t r = (argb >> 19) & 0x1F;
                uint32_t g = (argb >> 10) & 0x3F;
                uint32_t b = (argb >> 3)  & 0x1F;
                outPixel = static_cast<uint16_t>((r << 11) | (g << 5) | b);
            } else if (format == TYPE_USHORT_4444_ARGB) {
                uint32_t a = (argb >> 28) & 0xF;
                uint32_t r = (argb >> 20) & 0xF;
                uint32_t g = (argb >> 12) & 0xF;
                uint32_t b = (argb >> 4)  & 0xF;
                outPixel = static_cast<uint16_t>((a << 12) | (r << 8) | (g << 4) | b);
            }

            pixels[offset + row * scanlength + col] = outPixel;
        }
    }
}

void NokiaDirectGraphics::getPixels(uint32_t* pixels, int32_t offset, int32_t scanlength,
                                   int32_t x, int32_t y, int32_t width, int32_t height, int32_t format) {
    if (!m_target || !pixels || width <= 0 || height <= 0) return;

    int32_t targetW = m_target->getBufferWidth();
    int32_t targetH = m_target->getBufferHeight();
    const uint32_t* srcBuffer = m_target->getRawBuffer();
    if (!srcBuffer) return;

    for (int32_t row = 0; row < height; ++row) {
        int32_t sy = y + row;
        if (sy < 0 || sy >= targetH) continue;

        for (int32_t col = 0; col < width; ++col) {
            int32_t sx = x + col;
            if (sx < 0 || sx >= targetW) continue;

            uint32_t argb = srcBuffer[sy * targetW + sx];
            if (format == TYPE_INT_888_RGB) {
                argb &= 0x00FFFFFF;
            }
            pixels[offset + row * scanlength + col] = argb;
        }
    }
}

uint32_t NokiaDirectGraphics::blendColors(uint32_t src, uint32_t dst) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 255) return src;
    if (sa == 0) return dst;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8)  & 0xFF;
    uint32_t sb = src         & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8)  & 0xFF;
    uint32_t db = dst         & 0xFF;

    uint32_t r = (sr * sa + dr * (255 - sa)) / 255;
    uint32_t g = (sg * sa + dg * (255 - sa)) / 255;
    uint32_t b = (sb * sa + db * (255 - sa)) / 255;
    uint32_t a = std::min(255u, sa + ((dst >> 24) & 0xFF));

    return (a << 24) | (r << 16) | (g << 8) | b;
}

void NokiaDirectGraphics::drawPolygon(const int32_t* xPoints, const int32_t* yPoints,
                                     int32_t nPoints, uint32_t argbColor) {
    if (!m_target || !xPoints || !yPoints || nPoints < 2) return;

    uint32_t oldColor = m_target->getColor();
    m_target->setColor(argbColor & 0x00FFFFFF);

    for (int32_t i = 0; i < nPoints; ++i) {
        int32_t next = (i + 1) % nPoints;
        m_target->drawLine(xPoints[i], yPoints[i], xPoints[next], yPoints[next]);
    }

    m_target->setColor(oldColor);
}

void NokiaDirectGraphics::fillPolygon(const int32_t* xPoints, const int32_t* yPoints,
                                     int32_t nPoints, uint32_t argbColor) {
    if (!m_target || !xPoints || !yPoints || nPoints < 3) return;

    int32_t targetW = m_target->getBufferWidth();
    int32_t targetH = m_target->getBufferHeight();
    uint32_t* dstBuffer = m_target->getRawBuffer();
    if (!dstBuffer) return;

    // Bounding Box
    int32_t minY = yPoints[0], maxY = yPoints[0];
    for (int32_t i = 1; i < nPoints; ++i) {
        if (yPoints[i] < minY) minY = yPoints[i];
        if (yPoints[i] > maxY) maxY = yPoints[i];
    }

    minY = std::max(0, minY);
    maxY = std::min(targetH - 1, maxY);

    std::vector<int32_t> nodeX;
    nodeX.reserve(static_cast<size_t>(nPoints));

    // Scanline polygon fill (parity rule)
    for (int32_t y = minY; y <= maxY; ++y) {
        nodeX.clear();
        int32_t j = nPoints - 1;
        for (int32_t i = 0; i < nPoints; ++i) {
            if ((yPoints[i] < y && yPoints[j] >= y) || (yPoints[j] < y && yPoints[i] >= y)) {
                int32_t xIntersect = xPoints[i] + (y - yPoints[i]) * (xPoints[j] - xPoints[i]) / (yPoints[j] - yPoints[i]);
                nodeX.push_back(xIntersect);
            }
            j = i;
        }

        std::sort(nodeX.begin(), nodeX.end());

        for (size_t k = 0; k + 1 < nodeX.size(); k += 2) {
            int32_t xStart = std::max(0, nodeX[k]);
            int32_t xEnd = std::min(targetW - 1, nodeX[k + 1]);

            for (int32_t x = xStart; x <= xEnd; ++x) {
                size_t idx = static_cast<size_t>(y) * targetW + x;
                dstBuffer[idx] = blendColors(argbColor, dstBuffer[idx]);
            }
        }
    }
}

void NokiaDirectGraphics::drawTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                      int32_t x3, int32_t y3, uint32_t argbColor) {
    int32_t xs[3] = {x1, x2, x3};
    int32_t ys[3] = {y1, y2, y3};
    drawPolygon(xs, ys, 3, argbColor);
}

void NokiaDirectGraphics::fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                      int32_t x3, int32_t y3, uint32_t argbColor) {
    int32_t xs[3] = {x1, x2, x3};
    int32_t ys[3] = {y1, y2, y3};
    fillPolygon(xs, ys, 3, argbColor);
}

} // namespace oem
} // namespace universal_loader
