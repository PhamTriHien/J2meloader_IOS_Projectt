#include "vodafone_image_encoder.h"
#include <algorithm>
#include <cstring>

namespace universal_loader {
namespace oem {
namespace vodafone {

uint32_t VodafoneImageEncoder::calcCrc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t VodafoneImageEncoder::calcAdler32(const uint8_t* data, size_t length) {
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    const uint32_t MOD_ADLER = 65521u;
    for (size_t i = 0; i < length; ++i) {
        s1 = (s1 + data[i]) % MOD_ADLER;
        s2 = (s2 + s1) % MOD_ADLER;
    }
    return (s2 << 16) | s1;
}

void VodafoneImageEncoder::writeU32Be(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void VodafoneImageEncoder::writeChunk(std::vector<uint8_t>& buf, const char* type, const uint8_t* data, uint32_t length) {
    writeU32Be(buf, length);
    size_t crcStart = buf.size();
    buf.insert(buf.end(), type, type + 4);
    if (data && length > 0) {
        buf.insert(buf.end(), data, data + length);
    }
    uint32_t crc = calcCrc32(buf.data() + crcStart, length + 4);
    writeU32Be(buf, crc);
}

std::vector<uint8_t> VodafoneImageEncoder::encodeOffscreen(
    const uint32_t* srcPixels,
    int32_t srcW,
    int32_t srcH,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t format
) {
    (void)format; // Default and primary supported offscreen encoding is PNG
    if (!srcPixels || srcW <= 0 || srcH <= 0 || width <= 0 || height <= 0) {
        return {};
    }

    // 1. Prepare raw uncompressed scanlines with filter type 0 (None)
    // Scanline size = 1 filter byte + width * 4 bytes (RGBA)
    size_t scanlineBytes = 1 + static_cast<size_t>(width) * 4;
    size_t rawSize = scanlineBytes * static_cast<size_t>(height);
    std::vector<uint8_t> rawData;
    rawData.reserve(rawSize);

    for (int32_t r = 0; r < height; ++r) {
        rawData.push_back(0); // Filter byte: 0 = None
        int32_t sy = y + r;
        for (int32_t c = 0; c < width; ++c) {
            int32_t sx = x + c;
            uint32_t pixel = 0;
            if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                pixel = srcPixels[sy * srcW + sx];
            }
            uint8_t a = static_cast<uint8_t>((pixel >> 24) & 0xFF);
            uint8_t red = static_cast<uint8_t>((pixel >> 16) & 0xFF);
            uint8_t grn = static_cast<uint8_t>((pixel >> 8) & 0xFF);
            uint8_t blu = static_cast<uint8_t>(pixel & 0xFF);

            rawData.push_back(red);
            rawData.push_back(grn);
            rawData.push_back(blu);
            rawData.push_back(a);
        }
    }

    // 2. Wrap raw data into an RFC 1950 zlib stream with uncompressed Deflate blocks
    std::vector<uint8_t> zlibStream;
    // zlib header (CMF=0x78, FLG=0x01: Deflate, default compression, valid check bits)
    zlibStream.push_back(0x78);
    zlibStream.push_back(0x01);

    const size_t MAX_BLOCK_SIZE = 65535;
    size_t offset = 0;
    while (offset < rawData.size()) {
        size_t chunkSize = (rawData.size() - offset < MAX_BLOCK_SIZE) ? (rawData.size() - offset) : MAX_BLOCK_SIZE;
        bool isFinal = (offset + chunkSize >= rawData.size());

        zlibStream.push_back(isFinal ? 0x01 : 0x00); // BFINAL and BTYPE (00 = uncompressed)

        uint16_t len = static_cast<uint16_t>(chunkSize);
        uint16_t nlen = ~len;
        zlibStream.push_back(static_cast<uint8_t>(len & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));

        zlibStream.insert(zlibStream.end(), rawData.begin() + offset, rawData.begin() + offset + chunkSize);
        offset += chunkSize;
    }

    // Append Adler-32 checksum (Big Endian)
    uint32_t adler = calcAdler32(rawData.data(), rawData.size());
    writeU32Be(zlibStream, adler);

    // 3. Assemble PNG binary file
    std::vector<uint8_t> png;
    // Standard PNG signature
    const uint8_t pngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    png.insert(png.end(), pngSig, pngSig + 8);

    // IHDR chunk: 13 bytes
    std::vector<uint8_t> ihdr;
    writeU32Be(ihdr, static_cast<uint32_t>(width));
    writeU32Be(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8); // Bit depth: 8 bits per channel
    ihdr.push_back(6); // Color type: 6 = RGBA
    ihdr.push_back(0); // Compression method: 0
    ihdr.push_back(0); // Filter method: 0
    ihdr.push_back(0); // Interlace: 0 (No interlace)
    writeChunk(png, "IHDR", ihdr.data(), static_cast<uint32_t>(ihdr.size()));

    // IDAT chunk
    writeChunk(png, "IDAT", zlibStream.data(), static_cast<uint32_t>(zlibStream.size()));

    // IEND chunk
    writeChunk(png, "IEND", nullptr, 0);

    return png;
}

} // namespace vodafone
} // namespace oem
} // namespace universal_loader
