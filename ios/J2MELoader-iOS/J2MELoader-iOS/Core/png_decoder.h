#ifndef PNG_DECODER_H
#define PNG_DECODER_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <zlib.h>

class PngDecoder {
public:
    static bool decode(const uint8_t* data, size_t size, int& outW, int& outH, std::vector<uint32_t>& outPixels) {
        if (!data || size < 8) return false;

        const uint8_t pngSig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
        if (std::memcmp(data, pngSig, 8) != 0) return false;

        size_t pos = 8;
        int width = 0, height = 0;
        uint8_t bitDepth = 0, colorType = 0;
        std::vector<uint8_t> idatData;
        std::vector<uint32_t> palette;
        std::vector<uint8_t> transPalette;

        auto readU4 = [&](size_t p) -> uint32_t {
            if (p + 4 > size) return 0;
            return ((uint32_t)data[p] << 24) | ((uint32_t)data[p + 1] << 16) | ((uint32_t)data[p + 2] << 8) | data[p + 3];
        };

        while (pos + 8 <= size) {
            uint32_t length = readU4(pos);
            pos += 4;
            if (pos + 4 + length > size) break;

            char chunkType[5] = { (char)data[pos], (char)data[pos + 1], (char)data[pos + 2], (char)data[pos + 3], '\0' };
            pos += 4;
            const uint8_t* chunkData = data + pos;
            pos += length;
            pos += 4; // Skip CRC

            if (std::strcmp(chunkType, "IHDR") == 0 && length >= 13) {
                width = ((uint32_t)chunkData[0] << 24) | ((uint32_t)chunkData[1] << 16) | ((uint32_t)chunkData[2] << 8) | chunkData[3];
                height = ((uint32_t)chunkData[4] << 24) | ((uint32_t)chunkData[5] << 16) | ((uint32_t)chunkData[6] << 8) | chunkData[7];
                bitDepth = chunkData[8];
                colorType = chunkData[9];
            } else if (std::strcmp(chunkType, "PLTE") == 0) {
                size_t numColors = length / 3;
                palette.resize(numColors);
                for (size_t i = 0; i < numColors; ++i) {
                    uint8_t r = chunkData[i * 3];
                    uint8_t g = chunkData[i * 3 + 1];
                    uint8_t b = chunkData[i * 3 + 2];
                    palette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
                }
            } else if (std::strcmp(chunkType, "tRNS") == 0) {
                transPalette.assign(chunkData, chunkData + length);
            } else if (std::strcmp(chunkType, "IDAT") == 0) {
                idatData.insert(idatData.end(), chunkData, chunkData + length);
            } else if (std::strcmp(chunkType, "IEND") == 0) {
                break;
            }
        }

        if (width <= 0 || height <= 0 || idatData.empty()) return false;

        int channels = 1;
        if (colorType == 2) channels = 3;
        else if (colorType == 3) channels = 1;
        else if (colorType == 4) channels = 2;
        else if (colorType == 6) channels = 4;

        size_t bytesPerPixel = (channels * bitDepth + 7) / 8;
        size_t scanlineBytes = (width * channels * bitDepth + 7) / 8;
        size_t rawSize = (scanlineBytes + 1) * height;

        std::vector<uint8_t> rawData(rawSize);
        uLongf destLen = (uLongf)rawSize;
        if (uncompress(rawData.data(), &destLen, idatData.data(), (uLong)idatData.size()) != Z_OK) {
            return false;
        }

        outW = width;
        outH = height;
        outPixels.resize(width * height, 0);

        auto paeth = [](int a, int b, int c) -> int {
            int p = a + b - c;
            int pa = std::abs(p - a);
            int pb = std::abs(p - b);
            int pc = std::abs(p - c);
            if (pa <= pb && pa <= pc) return a;
            if (pb <= pc) return b;
            return c;
        };

        std::vector<uint8_t> prevScanline(scanlineBytes, 0);
        std::vector<uint8_t> currScanline(scanlineBytes, 0);
        size_t rawPos = 0;

        for (int y = 0; y < height; ++y) {
            if (rawPos >= rawData.size()) break;
            uint8_t filter = rawData[rawPos++];

            for (size_t i = 0; i < scanlineBytes; ++i) {
                if (rawPos >= rawData.size()) break;
                uint8_t x = rawData[rawPos++];
                uint8_t a = (i >= bytesPerPixel) ? currScanline[i - bytesPerPixel] : 0;
                uint8_t b = prevScanline[i];
                uint8_t c = (i >= bytesPerPixel) ? prevScanline[i - bytesPerPixel] : 0;

                switch (filter) {
                case 0: currScanline[i] = x; break;
                case 1: currScanline[i] = x + a; break;
                case 2: currScanline[i] = x + b; break;
                case 3: currScanline[i] = x + ((a + b) >> 1); break;
                case 4: currScanline[i] = x + paeth(a, b, c); break;
                default: currScanline[i] = x; break;
                }
            }

            for (int x = 0; x < width; ++x) {
                uint32_t argb = 0;
                if (colorType == 6) {
                    size_t idx = x * 4;
                    if (idx + 3 < currScanline.size()) {
                        uint8_t r = currScanline[idx], g = currScanline[idx + 1], b = currScanline[idx + 2], a = currScanline[idx + 3];
                        argb = (a << 24) | (r << 16) | (g << 8) | b;
                    }
                } else if (colorType == 2) {
                    size_t idx = x * 3;
                    if (idx + 2 < currScanline.size()) {
                        uint8_t r = currScanline[idx], g = currScanline[idx + 1], b = currScanline[idx + 2];
                        argb = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                } else if (colorType == 3) {
                    if ((size_t)x < currScanline.size()) {
                        uint8_t colorIdx = currScanline[x];
                        if (colorIdx < palette.size()) {
                            argb = palette[colorIdx];
                            if (colorIdx < transPalette.size()) {
                                argb = (transPalette[colorIdx] << 24) | (argb & 0x00FFFFFF);
                            }
                        }
                    }
                } else {
                    if ((size_t)x < currScanline.size()) {
                        uint8_t gray = currScanline[x];
                        argb = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
                    }
                }
                if ((size_t)(y * width + x) < outPixels.size()) {
                    outPixels[y * width + x] = argb;
                }
            }

            prevScanline = currScanline;
        }

        return true;
    }
};

#endif // PNG_DECODER_H
