#ifndef UNIVERSAL_LOADER_VODAFONE_IMAGE_ENCODER_H
#define UNIVERSAL_LOADER_VODAFONE_IMAGE_ENCODER_H

#include "vodafone_types.h"
#include <vector>
#include <cstdint>

namespace universal_loader {
namespace oem {
namespace vodafone {

class J2ME_API VodafoneImageEncoder {
public:
    static std::vector<uint8_t> encodeOffscreen(
        const uint32_t* srcPixels,
        int32_t srcW,
        int32_t srcH,
        int32_t x,
        int32_t y,
        int32_t width,
        int32_t height,
        int32_t format = IMAGE_FORMAT_PNG
    );

private:
    static uint32_t calcCrc32(const uint8_t* data, size_t length);
    static uint32_t calcAdler32(const uint8_t* data, size_t length);
    static void writeU32Be(std::vector<uint8_t>& buf, uint32_t val);
    static void writeChunk(std::vector<uint8_t>& buf, const char* type, const uint8_t* data, uint32_t length);
};

} // namespace vodafone
} // namespace oem
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_VODAFONE_IMAGE_ENCODER_H
