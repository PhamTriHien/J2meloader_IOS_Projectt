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
        uint8_t bitDepth = 0, colorType = 0, interlace = 0;
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
                interlace = chunkData[12];
                if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
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
        outPixels.assign((size_t)width * height, 0);

        auto paeth = [](int a, int b, int c) -> int {
            int p = a + b - c;
            int pa = std::abs(p - a);
            int pb = std::abs(p - b);
            int pc = std::abs(p - c);
            if (pa <= pb && pa <= pc) return a;
            if (pb <= pc) return b;
            return c;
        };

        // Unfilter one row of given byte length from the stream.
        std::vector<uint8_t> prevRow, curRow;
        size_t rawPos = 0;
        auto unfilterRow = [&](size_t rowBytes, std::vector<uint8_t>& out) -> bool {
            if (rawPos >= rawData.size()) return false;
            uint8_t filter = rawData[rawPos++];
            if (rawPos + rowBytes > rawData.size()) return false;
            out.assign(rowBytes, 0);
            size_t bpp = std::max<size_t>(1, bytesPerPixel);
            for (size_t i = 0; i < rowBytes; ++i) {
                uint8_t x = rawData[rawPos++];
                uint8_t a = (i >= bpp) ? out[i - bpp] : 0;
                uint8_t b = (i < prevRow.size()) ? prevRow[i] : 0;
                uint8_t c = (i >= bpp && i - bpp < prevRow.size()) ? prevRow[i - bpp] : 0;
                switch (filter) {
                case 0: out[i] = x; break;
                case 1: out[i] = x + a; break;
                case 2: out[i] = x + b; break;
                case 3: out[i] = x + ((a + b) >> 1); break;
                case 4: out[i] = x + paeth(a, b, c); break;
                default: out[i] = x; break;
                }
            }
            prevRow = out;
            return true;
        };

        // Sample grid: width*channels 8-bit samples per row (16-bit scaled down).
        std::vector<std::vector<uint8_t>> grid(height);
        auto unpackRow = [&](const std::vector<uint8_t>& row, int samples, std::vector<uint8_t>& out) {
            out.assign(samples, 0);
            if (bitDepth >= 8) {
                int step = bitDepth / 8;
                for (int s = 0; s < samples; ++s) {
                    size_t o = (size_t)s * step;
                    if (o >= row.size()) break;
                    out[s] = (bitDepth == 16) ? row[o] : row[o]; // 16-bit: high byte
                }
            } else {
                int mask = (1 << bitDepth) - 1;
                int scale = 255 / mask;
                for (int s = 0; s < samples; ++s) {
                    size_t bit = (size_t)s * bitDepth;
                    uint8_t v = (bit / 8 < row.size()) ? (uint8_t)((row[bit / 8] >> (8 - bitDepth - (bit % 8))) & mask) : 0;
                    out[s] = (uint8_t)(v * scale);
                }
            }
        };

        bool rowsOk = true;
        if (interlace == 0) {
            for (int y = 0; y < height; ++y) {
                std::vector<uint8_t> row;
                if (!unfilterRow(scanlineBytes, row)) { rowsOk = false; break; }
                unpackRow(row, width * channels, grid[y]);
            }
        } else if (interlace == 1) {
            // Adam7: 7 sub-images scattered onto the grid.
            for (int y = 0; y < height; ++y) grid[y].assign((size_t)width * channels, 0);
            static const int PX[7] = {0, 4, 0, 2, 0, 1, 0};
            static const int PY[7] = {0, 0, 4, 0, 2, 0, 1};
            static const int DX[7] = {8, 8, 4, 4, 2, 2, 1};
            static const int DY[7] = {8, 8, 8, 4, 4, 2, 2};
            for (int pass = 0; pass < 7; ++pass) {
                int pw = (width - PX[pass] + DX[pass] - 1) / DX[pass];
                int ph = (height - PY[pass] + DY[pass] - 1) / DY[pass];
                if (pw <= 0 || ph <= 0) continue;
                size_t prowBytes = ((size_t)pw * channels * bitDepth + 7) / 8;
                size_t saveBpp = bytesPerPixel;
                bytesPerPixel = (channels * bitDepth + 7) / 8;
                if (bytesPerPixel == 0) bytesPerPixel = 1;
                prevRow.assign(prowBytes, 0);
                for (int ry = 0; ry < ph; ++ry) {
                    std::vector<uint8_t> row;
                    if (!unfilterRow(prowBytes, row)) { rowsOk = false; break; }
                    std::vector<uint8_t> samples;
                    unpackRow(row, pw * channels, samples);
                    int fy = PY[pass] + ry * DY[pass];
                    if (fy < 0 || fy >= height) continue;
                    for (int rx = 0; rx < pw; ++rx) {
                        int fx = PX[pass] + rx * DX[pass];
                        if (fx < 0 || fx >= width) continue;
                        for (int c = 0; c < channels; ++c) {
                            size_t si = (size_t)rx * channels + c;
                            if (si < samples.size())
                                grid[fy][(size_t)fx * channels + c] = samples[si];
                        }
                    }
                }
                bytesPerPixel = saveBpp;
                if (!rowsOk) break;
            }
        } else {
            return false;
        }
        if (!rowsOk) return false;

        for (int y = 0; y < height; ++y) {
            const std::vector<uint8_t>& s = grid[y];
            for (int x = 0; x < width; ++x) {
                uint32_t argb = 0;
                size_t base = (size_t)x * channels;
                if (colorType == 6 && base + 3 < s.size()) {
                    argb = ((uint32_t)s[base + 3] << 24) | ((uint32_t)s[base] << 16) | ((uint32_t)s[base + 1] << 8) | s[base + 2];
                } else if (colorType == 2 && base + 2 < s.size()) {
                    argb = 0xFF000000 | ((uint32_t)s[base] << 16) | ((uint32_t)s[base + 1] << 8) | s[base + 2];
                } else if (colorType == 3 && (size_t)x < s.size()) {
                    uint8_t colorIdx = s[x];
                    if (colorIdx < palette.size()) {
                        argb = palette[colorIdx];
                        if (colorIdx < transPalette.size())
                            argb = ((uint32_t)transPalette[colorIdx] << 24) | (argb & 0x00FFFFFF);
                    }
                } else if (colorType == 4 && base + 1 < s.size()) {
                    uint8_t g = s[base], a = s[base + 1];
                    argb = ((uint32_t)a << 24) | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
                } else if ((size_t)x < s.size()) {
                    uint8_t g = s[x];
                    argb = 0xFF000000 | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
                }
                outPixels[(size_t)y * width + x] = argb;
            }
        }

        return true;
    }
};

#endif // PNG_DECODER_H
