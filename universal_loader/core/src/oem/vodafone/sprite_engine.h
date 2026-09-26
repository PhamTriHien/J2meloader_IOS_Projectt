#ifndef UNIVERSAL_LOADER_VODAFONE_SPRITE_ENGINE_H
#define UNIVERSAL_LOADER_VODAFONE_SPRITE_ENGINE_H

#include "vodafone_types.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <mutex>

namespace universal_loader {
namespace oem {
namespace vodafone {

struct VodafoneCharacterCommand {
    int32_t offset{0};
    bool transparent{false};
    int32_t rotation{0};        // 0: none, 1: 90 deg, 2: 180 deg, 3: 270 deg
    bool isUpsideDown{false};    // vertical reflection
    bool isRightsideLeft{false}; // horizontal reflection
    int32_t patternNo{0};
};

class J2ME_API VodafoneSpriteCanvas {
public:
    VodafoneSpriteCanvas(int32_t numPalettes, int32_t numPatterns);
    ~VodafoneSpriteCanvas() = default;

    // Framebuffer lifecycle
    void createFrameBuffer(int32_t fw, int32_t fh);
    void disposeFrameBuffer();
    bool hasFrameBuffer() const;
    int32_t getFrameBufferWidth() const;
    int32_t getFrameBufferHeight() const;
    const uint32_t* getFrameBuffer() const;

    // Palette management
    void setPalette(int32_t index, uint32_t color);
    uint32_t getPalette(int32_t index) const;
    size_t getPaletteCount() const;

    // Pattern data management (8x8 pixel patterns, 64 bytes each)
    void setPattern(int32_t index, const uint8_t* data, size_t length);
    const uint8_t* getPatternData() const;
    size_t getPatternCount() const;

    // Character commands
    int16_t createCharacterCommand(int32_t offset, bool transparent, int32_t rotation,
                                   bool isUpsideDown, bool isRightsideLeft, int32_t patternNo);
    const VodafoneCharacterCommand* getCommand(int16_t commandId) const;
    size_t getCommandCount() const;

    // Sprite drawing and transformation
    void drawSpriteChar(int16_t commandId, int16_t x, int16_t y);

    // Framebuffer operations
    void copyArea(int32_t sx, int32_t sy, int32_t fw, int32_t fh, int32_t tx, int32_t ty);
    void drawFrameBuffer(uint32_t* targetPixels, int32_t targetWidth, int32_t targetHeight, int32_t tx, int32_t ty);

private:
    mutable std::mutex m_mutex;
    std::vector<uint32_t> m_palette;
    std::vector<uint8_t> m_patternData;
    std::vector<VodafoneCharacterCommand> m_commands;

    std::vector<uint32_t> m_frameBuffer;
    int32_t m_fbWidth{0};
    int32_t m_fbHeight{0};
    size_t m_numPatterns{0};
};

} // namespace vodafone
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_VODAFONE_SPRITE_ENGINE_H
