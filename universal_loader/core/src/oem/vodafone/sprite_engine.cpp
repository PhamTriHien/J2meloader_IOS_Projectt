#include "sprite_engine.h"
#include <algorithm>
#include <cstring>

namespace universal_loader {
namespace oem {
namespace vodafone {

VodafoneSpriteCanvas::VodafoneSpriteCanvas(int32_t numPalettes, int32_t numPatterns)
    : m_numPatterns(numPatterns > 0 ? static_cast<size_t>(numPatterns) : 0) {
    if (numPalettes > 0) {
        m_palette.resize(static_cast<size_t>(numPalettes), 0xFF000000u);
    }
    if (numPatterns > 0) {
        m_patternData.resize(static_cast<size_t>(numPatterns) * SPRITE_PATTERN_SIZE, 0);
    }
}

void VodafoneSpriteCanvas::createFrameBuffer(int32_t fw, int32_t fh) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (fw <= 0 || fh <= 0) {
        m_fbWidth = 0;
        m_fbHeight = 0;
        m_frameBuffer.clear();
        return;
    }
    m_fbWidth = fw;
    m_fbHeight = fh;
    m_frameBuffer.assign(static_cast<size_t>(fw) * static_cast<size_t>(fh), 0);
}

void VodafoneSpriteCanvas::disposeFrameBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameBuffer.clear();
    m_fbWidth = 0;
    m_fbHeight = 0;
}

bool VodafoneSpriteCanvas::hasFrameBuffer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_frameBuffer.empty() && m_fbWidth > 0 && m_fbHeight > 0;
}

int32_t VodafoneSpriteCanvas::getFrameBufferWidth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fbWidth;
}

int32_t VodafoneSpriteCanvas::getFrameBufferHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fbHeight;
}

const uint32_t* VodafoneSpriteCanvas::getFrameBuffer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_frameBuffer.empty() ? nullptr : m_frameBuffer.data();
}

void VodafoneSpriteCanvas::setPalette(int32_t index, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && static_cast<size_t>(index) < m_palette.size()) {
        m_palette[static_cast<size_t>(index)] = color | 0xFF000000u;
    }
}

uint32_t VodafoneSpriteCanvas::getPalette(int32_t index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && static_cast<size_t>(index) < m_palette.size()) {
        return m_palette[static_cast<size_t>(index)];
    }
    return 0;
}

size_t VodafoneSpriteCanvas::getPaletteCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_palette.size();
}

void VodafoneSpriteCanvas::setPattern(int32_t index, const uint8_t* data, size_t length) {
    if (!data || length == 0 || index < 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t offset = static_cast<size_t>(index) * SPRITE_PATTERN_SIZE;
    if (offset + length <= m_patternData.size()) {
        std::memcpy(m_patternData.data() + offset, data, length);
    }
}

const uint8_t* VodafoneSpriteCanvas::getPatternData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_patternData.data();
}

size_t VodafoneSpriteCanvas::getPatternCount() const {
    return m_numPatterns;
}

int16_t VodafoneSpriteCanvas::createCharacterCommand(int32_t offset, bool transparent, int32_t rotation,
                                                    bool isUpsideDown, bool isRightsideLeft, int32_t patternNo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    VodafoneCharacterCommand cmd;
    cmd.offset = offset;
    cmd.transparent = transparent;
    cmd.rotation = (rotation >= 0 && rotation <= 3) ? rotation : 0;
    cmd.isUpsideDown = isUpsideDown;
    cmd.isRightsideLeft = isRightsideLeft;
    cmd.patternNo = patternNo;
    m_commands.push_back(cmd);
    return static_cast<int16_t>(m_commands.size() - 1);
}

const VodafoneCharacterCommand* VodafoneSpriteCanvas::getCommand(int16_t commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (commandId >= 0 && static_cast<size_t>(commandId) < m_commands.size()) {
        return &m_commands[static_cast<size_t>(commandId)];
    }
    return nullptr;
}

size_t VodafoneSpriteCanvas::getCommandCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.size();
}

void VodafoneSpriteCanvas::drawSpriteChar(int16_t commandId, int16_t x, int16_t y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameBuffer.empty() || m_fbWidth <= 0 || m_fbHeight <= 0) {
        return;
    }
    if (commandId < 0 || static_cast<size_t>(commandId) >= m_commands.size()) {
        return;
    }

    const VodafoneCharacterCommand& cmd = m_commands[static_cast<size_t>(commandId)];
    size_t basePatternOffset = static_cast<size_t>(cmd.patternNo) * SPRITE_PATTERN_SIZE + static_cast<size_t>(cmd.offset);

    if (basePatternOffset + SPRITE_PATTERN_SIZE > m_patternData.size()) {
        return;
    }

    // 8x8 character tile blitting with 8-directional geometric transform
    for (int32_t r = 0; r < 8; ++r) {
        for (int32_t c = 0; c < 8; ++c) {
            // Apply horizontal and vertical reflections
            int32_t xm = cmd.isRightsideLeft ? (7 - c) : c;
            int32_t ym = cmd.isUpsideDown ? (7 - r) : r;

            // Apply clockwise rotation
            int32_t dx = 0;
            int32_t dy = 0;
            switch (cmd.rotation) {
                case SPRITE_ROT_NONE:
                    dx = xm;
                    dy = ym;
                    break;
                case SPRITE_ROT_90:
                    dx = 7 - ym;
                    dy = xm;
                    break;
                case SPRITE_ROT_180:
                    dx = 7 - xm;
                    dy = 7 - ym;
                    break;
                case SPRITE_ROT_270:
                    dx = ym;
                    dy = 7 - xm;
                    break;
                default:
                    dx = xm;
                    dy = ym;
                    break;
            }

            int32_t destX = static_cast<int32_t>(x) + dx;
            int32_t destY = static_cast<int32_t>(y) + dy;

            // Boundary clipping
            if (destX >= 0 && destX < m_fbWidth && destY >= 0 && destY < m_fbHeight) {
                size_t patIdx = basePatternOffset + static_cast<size_t>(r * 8 + c);
                uint8_t colorId = m_patternData[patIdx];

                // Palette 0 is transparent if transparency is enabled
                if (cmd.transparent && colorId == 0) {
                    continue;
                }

                if (static_cast<size_t>(colorId) < m_palette.size()) {
                    uint32_t color = m_palette[static_cast<size_t>(colorId)];
                    m_frameBuffer[static_cast<size_t>(destY * m_fbWidth + destX)] = color;
                }
            }
        }
    }
}

void VodafoneSpriteCanvas::copyArea(int32_t sx, int32_t sy, int32_t fw, int32_t fh, int32_t tx, int32_t ty) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameBuffer.empty() || m_fbWidth <= 0 || m_fbHeight <= 0 || fw <= 0 || fh <= 0) {
        return;
    }

    // Temporary copy buffer to safely support overlapping source and destination rectangles
    std::vector<uint32_t> temp(static_cast<size_t>(fw) * static_cast<size_t>(fh), 0);
    for (int32_t r = 0; r < fh; ++r) {
        int32_t srcY = sy + r;
        for (int32_t c = 0; c < fw; ++c) {
            int32_t srcX = sx + c;
            if (srcX >= 0 && srcX < m_fbWidth && srcY >= 0 && srcY < m_fbHeight) {
                temp[static_cast<size_t>(r * fw + c)] = m_frameBuffer[static_cast<size_t>(srcY * m_fbWidth + srcX)];
            }
        }
    }

    // Blit temp buffer to destination
    for (int32_t r = 0; r < fh; ++r) {
        int32_t destY = ty + r;
        for (int32_t c = 0; c < fw; ++c) {
            int32_t destX = tx + c;
            if (destX >= 0 && destX < m_fbWidth && destY >= 0 && destY < m_fbHeight) {
                m_frameBuffer[static_cast<size_t>(destY * m_fbWidth + destX)] = temp[static_cast<size_t>(r * fw + c)];
            }
        }
    }
}

void VodafoneSpriteCanvas::drawFrameBuffer(uint32_t* targetPixels, int32_t targetWidth, int32_t targetHeight, int32_t tx, int32_t ty) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!targetPixels || targetWidth <= 0 || targetHeight <= 0 || m_frameBuffer.empty()) {
        return;
    }

    // Blit current framebuffer onto target
    for (int32_t r = 0; r < m_fbHeight; ++r) {
        int32_t destY = ty + r;
        if (destY < 0 || destY >= targetHeight) {
            continue;
        }
        for (int32_t c = 0; c < m_fbWidth; ++c) {
            int32_t destX = tx + c;
            if (destX < 0 || destX >= targetWidth) {
                continue;
            }
            uint32_t color = m_frameBuffer[static_cast<size_t>(r * m_fbWidth + c)];
            // Non-zero pixels overwrite target
            if (color != 0) {
                targetPixels[destY * targetWidth + destX] = color;
            }
        }
    }

    // Erase framebuffer to 0 after flushing as defined by Vodafone spec
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), 0);
}

} // namespace vodafone
} // namespace oem
} // namespace universal_loader
