#include "micro3d_loader.h"
#include <fstream>
#include <cstring>
#include <cmath>

namespace universal_loader {
namespace micro3d {

namespace {

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size)
        : data_(data), size_(size), pos_(0) {}

    bool hasRemaining() const { return pos_ < size_ || cached_ > 0; }
    size_t available() const { return (pos_ < size_) ? (size_ - pos_) : 0; }

    uint8_t readByte() {
        if (pos_ >= size_) return 0;
        return data_[pos_++];
    }

    uint16_t readUShortLE() {
        if (pos_ + 2 > size_) return 0;
        uint16_t val = static_cast<uint16_t>(data_[pos_]) |
                      (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return val;
    }

    int16_t readShortLE() {
        return static_cast<int16_t>(readUShortLE());
    }

    int32_t readIntLE() {
        if (pos_ + 4 > size_) return 0;
        uint32_t val = static_cast<uint32_t>(data_[pos_]) |
                      (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
                      (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
                      (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return static_cast<int32_t>(val);
    }

    int32_t readUBits(int bits) {
        if (bits <= 0) return 0;
        while (bits > cached_) {
            if (pos_ >= size_) break;
            cache_ |= static_cast<uint32_t>(data_[pos_++]) << cached_;
            cached_ += 8;
        }
        int32_t mask = (bits >= 32) ? 0xFFFFFFFF : ((1 << bits) - 1);
        int32_t result = static_cast<int32_t>(cache_ & mask);
        cached_ -= bits;
        if (cached_ < 0) cached_ = 0;
        cache_ >>= bits;
        return result;
    }

    int32_t readBits(int bits) {
        int32_t u = readUBits(bits);
        int lzb = 32 - bits;
        return (u << lzb) >> lzb;
    }

    void clearCache() {
        cache_ = 0;
        cached_ = 0;
    }

    size_t getPosition() const { return pos_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;
    uint32_t cache_{0};
    int cached_{0};
};

static inline void writeUShortLE(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

static inline void writeShortLE(std::vector<uint8_t>& buf, int16_t val) {
    writeUShortLE(buf, static_cast<uint16_t>(val));
}

static inline void writeIntLE(std::vector<uint8_t>& buf, int32_t val) {
    uint32_t u = static_cast<uint32_t>(val);
    buf.push_back(static_cast<uint8_t>(u & 0xFF));
    buf.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
}

} // namespace

int Micro3dLoader::identify(const uint8_t* data, size_t size) {
    if (!data || size < 4) return 0;
    if (data[0] == 'M' && data[1] == 'B') return 1; // MBAC
    if (data[0] == 'M' && data[1] == 'T') return 2; // MTRA
    return 0;
}

bool Micro3dLoader::loadMbac(const uint8_t* data, size_t size, Micro3dFigure& outFigure) {
    outFigure.clear();
    if (!data || size < 12) return false;

    BitReader reader(data, size);
    if (reader.readByte() != 'M' || reader.readByte() != 'B') {
        return false;
    }

    int version = reader.readByte();
    int zeroByte = reader.readByte();
    if (zeroByte != 0 || version < 2 || version > 5) {
        return false;
    }

    int vertexFormat = 1;
    int normalFormat = 0;
    int polygonFormat = 1;
    int boneFormat = 1;

    if (version > 3) {
        vertexFormat = reader.readByte();
        normalFormat = reader.readByte();
        polygonFormat = reader.readByte();
        boneFormat = reader.readByte();
    }

    if (boneFormat != 1) {
        return false;
    }

    uint16_t numVertices = reader.readUShortLE();
    uint16_t numPolyT3   = reader.readUShortLE();
    uint16_t numPolyT4   = reader.readUShortLE();
    uint16_t numBones    = reader.readUShortLE();

    uint16_t numPolyC3 = 0;
    uint16_t numPolyC4 = 0;
    uint16_t numTextures = 1;
    uint16_t numPatterns = 1;
    uint16_t numColors = 0;

    if (polygonFormat >= 3) {
        numPolyC3   = reader.readUShortLE();
        numPolyC4   = reader.readUShortLE();
        numTextures = reader.readUShortLE();
        numPatterns = reader.readUShortLE();
        numColors   = reader.readUShortLE();
    }

    if (numVertices > 21845 || numTextures > 16 || numPatterns > 33 || numColors > 256) {
        return false;
    }

    // Version 5 patterns
    if (version == 5) {
        for (int i = 0; i < numPatterns; ++i) {
            reader.readUShortLE();
            reader.readUShortLE();
            for (int j = 1; j <= numTextures; ++j) {
                reader.readUShortLE();
                reader.readUShortLE();
            }
        }
    }

    // Vertices
    outFigure.vertices.resize(numVertices);
    if (vertexFormat == 1) {
        for (int i = 0; i < numVertices; ++i) {
            int16_t x = reader.readShortLE();
            int16_t y = reader.readShortLE();
            int16_t z = reader.readShortLE();
            outFigure.vertices[i].position = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
        }
    } else if (vertexFormat == 2) {
        static const int SIZES[4] = {8, 10, 13, 16};
        int idx = 0;
        while (idx < numVertices) {
            int chunk = reader.readUBits(8);
            int type = (chunk >> 6) & 3;
            int sizeBits = SIZES[type];
            int count = (chunk & 0x3F) + 1;
            for (int i = 0; i < count && idx < numVertices; ++i, ++idx) {
                int32_t x = reader.readBits(sizeBits);
                int32_t y = reader.readBits(sizeBits);
                int32_t z = reader.readBits(sizeBits);
                outFigure.vertices[idx].position = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
            }
        }
    } else {
        return false;
    }
    reader.clearCache();

    // Normals
    if (normalFormat == 1) {
        for (int i = 0; i < numVertices; ++i) {
            int16_t nx = reader.readShortLE();
            int16_t ny = reader.readShortLE();
            int16_t nz = reader.readShortLE();
            outFigure.vertices[i].normal = { static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz) };
        }
    } else if (normalFormat == 2) {
        static const int POOL_NORMALS[8] = {0, 0, 64, 0, 0, -64, 0, 0};
        for (int i = 0; i < numVertices; ++i) {
            int x = reader.readUBits(7);
            int y = 0, z = 0;
            if (x == 64) {
                int type = reader.readUBits(3);
                if (type <= 5) {
                    z = POOL_NORMALS[type];
                    y = POOL_NORMALS[type + 1];
                    x = POOL_NORMALS[type + 2];
                }
            } else {
                x = (x << 25) >> 25;
                y = (reader.readUBits(7) << 25) >> 25;
                int sign = reader.readUBits(1);
                int dq = 4096 - x * x - y * y;
                z = (dq > 0) ? static_cast<int>(std::round(std::sqrt(static_cast<double>(dq)))) : 0;
                if (sign == 1) z = -z;
            }
            outFigure.vertices[i].normal = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
        }
    }
    reader.clearCache();

    // Polygons
    if (polygonFormat == 1) {
        for (int i = 0; i < numPolyT3; ++i) {
            uint16_t material = reader.readUShortLE();
            uint16_t a = reader.readUShortLE();
            uint16_t b = reader.readUShortLE();
            uint16_t c = reader.readUShortLE();
            uint8_t uA = reader.readByte(), vA = reader.readByte();
            uint8_t uB = reader.readByte(), vB = reader.readByte();
            uint8_t uC = reader.readByte(), vC = reader.readByte();

            Micro3DPolygon poly;
            poly.indices = { a, b, c };
            poly.blendMode = (material & 4) << 2 | (material & 2) >> 1;
            outFigure.polygons.push_back(poly);

            if (a < numVertices) { outFigure.vertices[a].u = uA / 255.0f; outFigure.vertices[a].v = vA / 255.0f; }
            if (b < numVertices) { outFigure.vertices[b].u = uB / 255.0f; outFigure.vertices[b].v = vB / 255.0f; }
            if (c < numVertices) { outFigure.vertices[c].u = uC / 255.0f; outFigure.vertices[c].v = vC / 255.0f; }
        }

        for (int i = 0; i < numPolyT4; ++i) {
            uint16_t material = reader.readUShortLE();
            uint16_t a = reader.readUShortLE();
            uint16_t b = reader.readUShortLE();
            uint16_t c = reader.readUShortLE();
            uint16_t d = reader.readUShortLE();
            uint8_t uA = reader.readByte(), vA = reader.readByte();
            uint8_t uB = reader.readByte(), vB = reader.readByte();
            uint8_t uC = reader.readByte(), vC = reader.readByte();
            uint8_t uD = reader.readByte(), vD = reader.readByte();

            Micro3DPolygon poly;
            poly.indices = { a, b, c, d };
            poly.blendMode = (material & 4) << 2 | (material & 2) >> 1;
            outFigure.polygons.push_back(poly);

            if (a < numVertices) { outFigure.vertices[a].u = uA / 255.0f; outFigure.vertices[a].v = vA / 255.0f; }
            if (b < numVertices) { outFigure.vertices[b].u = uB / 255.0f; outFigure.vertices[b].v = vB / 255.0f; }
            if (c < numVertices) { outFigure.vertices[c].u = uC / 255.0f; outFigure.vertices[c].v = vC / 255.0f; }
            if (d < numVertices) { outFigure.vertices[d].u = uD / 255.0f; outFigure.vertices[d].v = vD / 255.0f; }
        }
    }
    reader.clearCache();

    // Bones
    outFigure.bones.resize(numBones);
    for (int b = 0; b < numBones; ++b) {
        uint16_t boneVerts = reader.readUShortLE();
        int16_t parent = reader.readShortLE();
        outFigure.bones[b].length = boneVerts;
        outFigure.bones[b].parent = parent;

        int16_t m00 = reader.readShortLE(), m01 = reader.readShortLE(), m02 = reader.readShortLE(), m03 = reader.readShortLE();
        int16_t m10 = reader.readShortLE(), m11 = reader.readShortLE(), m12 = reader.readShortLE(), m13 = reader.readShortLE();
        int16_t m20 = reader.readShortLE(), m21 = reader.readShortLE(), m22 = reader.readShortLE(), m23 = reader.readShortLE();

        outFigure.bones[b].matrix = AffineTrans(
            m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23
        );
    }

    outFigure.numVertices = numVertices;
    outFigure.numPolyT3 = numPolyT3;
    outFigure.numPolyT4 = numPolyT4;
    outFigure.numBones = numBones;
    outFigure.numTextures = numTextures;
    outFigure.numColors = numColors;

    return true;
}

bool Micro3dLoader::loadMbacFromFile(const std::string& filePath, Micro3dFigure& outFigure) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadMbac(buffer.data(), buffer.size(), outFigure);
}

std::vector<uint8_t> Micro3dLoader::serializeMbac(const Micro3dFigure& figure, int version) {
    std::vector<uint8_t> buf;
    buf.reserve(1024);

    // Magic 'M', 'B'
    buf.push_back('M');
    buf.push_back('B');
    buf.push_back(static_cast<uint8_t>(version));
    buf.push_back(0); // must be 0

    if (version > 3) {
        buf.push_back(1); // vertexFormat = 1
        buf.push_back(0); // normalFormat = 0
        buf.push_back(1); // polygonFormat = 1
        buf.push_back(1); // boneFormat = 1
    }

    uint16_t numVertices = static_cast<uint16_t>(figure.vertices.size());
    uint16_t numPolyT3 = 0;
    uint16_t numPolyT4 = 0;

    for (const auto& poly : figure.polygons) {
        if (poly.indices.size() == 3) numPolyT3++;
        else if (poly.indices.size() == 4) numPolyT4++;
    }

    uint16_t numBones = static_cast<uint16_t>(figure.bones.size());

    writeUShortLE(buf, numVertices);
    writeUShortLE(buf, numPolyT3);
    writeUShortLE(buf, numPolyT4);
    writeUShortLE(buf, numBones);

    // Vertices (vertexFormat = 1)
    for (const auto& v : figure.vertices) {
        writeShortLE(buf, static_cast<int16_t>(std::round(v.position.x)));
        writeShortLE(buf, static_cast<int16_t>(std::round(v.position.y)));
        writeShortLE(buf, static_cast<int16_t>(std::round(v.position.z)));
    }

    // Polygons (polygonFormat = 1)
    // T3
    for (const auto& poly : figure.polygons) {
        if (poly.indices.size() != 3) continue;
        uint16_t mat = static_cast<uint16_t>((poly.blendMode & 0x10) >> 2 | (poly.blendMode & 0x01) << 1);
        writeUShortLE(buf, mat);
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[0]));
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[1]));
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[2]));
        // UV coordinates (uA, vA, uB, vB, uC, vC)
        for (int k = 0; k < 3; ++k) {
            int idx = poly.indices[k];
            uint8_t u = (idx < numVertices) ? static_cast<uint8_t>(std::clamp(figure.vertices[idx].u * 255.0f, 0.0f, 255.0f)) : 0;
            uint8_t v = (idx < numVertices) ? static_cast<uint8_t>(std::clamp(figure.vertices[idx].v * 255.0f, 0.0f, 255.0f)) : 0;
            buf.push_back(u);
            buf.push_back(v);
        }
    }

    // T4
    for (const auto& poly : figure.polygons) {
        if (poly.indices.size() != 4) continue;
        uint16_t mat = static_cast<uint16_t>((poly.blendMode & 0x10) >> 2 | (poly.blendMode & 0x01) << 1 | 1);
        writeUShortLE(buf, mat);
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[0]));
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[1]));
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[2]));
        writeUShortLE(buf, static_cast<uint16_t>(poly.indices[3]));
        // UV coordinates (uA, vA, uB, vB, uC, vC, uD, vD)
        for (int k = 0; k < 4; ++k) {
            int idx = poly.indices[k];
            uint8_t u = (idx < numVertices) ? static_cast<uint8_t>(std::clamp(figure.vertices[idx].u * 255.0f, 0.0f, 255.0f)) : 0;
            uint8_t v = (idx < numVertices) ? static_cast<uint8_t>(std::clamp(figure.vertices[idx].v * 255.0f, 0.0f, 255.0f)) : 0;
            buf.push_back(u);
            buf.push_back(v);
        }
    }

    // Bones
    for (const auto& bone : figure.bones) {
        writeUShortLE(buf, static_cast<uint16_t>(bone.length));
        writeShortLE(buf, static_cast<int16_t>(bone.parent));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m00));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m01));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m02));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m03));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m10));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m11));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m12));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m13));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m20));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m21));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m22));
        writeShortLE(buf, static_cast<int16_t>(bone.matrix.m23));
    }

    return buf;
}

bool Micro3dLoader::loadMtra(const uint8_t* data, size_t size, ActionTable& outActionTable) {
    outActionTable.clear();
    if (!data || size < 28) return false;

    BitReader reader(data, size);
    if (reader.readByte() != 'M' || reader.readByte() != 'T') {
        return false;
    }

    int version = reader.readByte();
    int zeroByte = reader.readByte();
    if (zeroByte != 0 || version < 2 || version > 5) {
        return false;
    }

    uint16_t numActions = reader.readUShortLE();
    uint16_t numBones   = reader.readUShortLE();

    // 8 transform type counts
    for (int i = 0; i < 8; ++i) {
        reader.readUShortLE();
    }

    int32_t dataSize = reader.readIntLE();
    (void)dataSize;

    outActionTable.actions.resize(numActions);
    for (int actIdx = 0; actIdx < numActions; ++actIdx) {
        uint16_t keyframes = reader.readUShortLE();
        Action& act = outActionTable.actions[actIdx];
        act.keyframes = keyframes;
        act.numBones = numBones;
        act.boneActions.resize(numBones);

        for (int b = 0; b < numBones; ++b) {
            uint8_t type = reader.readByte();
            BoneAction& ba = act.boneActions[b];
            ba.keyframes = keyframes;

            if (type == 0) {
                int16_t m00 = reader.readShortLE(), m01 = reader.readShortLE(), m02 = reader.readShortLE(), m03 = reader.readShortLE();
                int16_t m10 = reader.readShortLE(), m11 = reader.readShortLE(), m12 = reader.readShortLE(), m13 = reader.readShortLE();
                int16_t m20 = reader.readShortLE(), m21 = reader.readShortLE(), m22 = reader.readShortLE(), m23 = reader.readShortLE();
                ba.matrices.push_back(AffineTrans(
                    m00, m01, m02, m03,
                    m10, m11, m12, m13,
                    m20, m21, m22, m23
                ));
            } else if (type == 1) {
                AffineTrans id;
                id.setIdentity();
                ba.matrices.push_back(id);
            } else if (type == 2) {
                // translate
                uint16_t countT = reader.readUShortLE();
                for (int k = 0; k < countT; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                // scale
                uint16_t countS = reader.readUShortLE();
                for (int k = 0; k < countS; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                // rotate
                uint16_t countR = reader.readUShortLE();
                for (int k = 0; k < countR; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                // roll
                uint16_t countRoll = reader.readUShortLE();
                for (int k = 0; k < countRoll; ++k) {
                    reader.readUShortLE(); reader.readShortLE();
                }
                ba.matrices.push_back(AffineTrans());
            } else if (type == 3) {
                reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                uint16_t countR = reader.readUShortLE();
                for (int k = 0; k < countR; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                reader.readShortLE();
                ba.matrices.push_back(AffineTrans());
            } else if (type == 4) {
                uint16_t countR = reader.readUShortLE();
                for (int k = 0; k < countR; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                uint16_t countRoll = reader.readUShortLE();
                for (int k = 0; k < countRoll; ++k) {
                    reader.readUShortLE(); reader.readShortLE();
                }
                ba.matrices.push_back(AffineTrans());
            } else if (type == 5) {
                uint16_t countR = reader.readUShortLE();
                for (int k = 0; k < countR; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                ba.matrices.push_back(AffineTrans());
            } else if (type == 6) {
                uint16_t countT = reader.readUShortLE();
                for (int k = 0; k < countT; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                uint16_t countR = reader.readUShortLE();
                for (int k = 0; k < countR; ++k) {
                    reader.readUShortLE(); reader.readShortLE(); reader.readShortLE(); reader.readShortLE();
                }
                uint16_t countRoll = reader.readUShortLE();
                for (int k = 0; k < countRoll; ++k) {
                    reader.readUShortLE(); reader.readShortLE();
                }
                ba.matrices.push_back(AffineTrans());
            }
        }

        if (version == 5) {
            uint16_t dynCount = reader.readUShortLE();
            for (int d = 0; d < dynCount; ++d) {
                reader.readUShortLE();
                reader.readIntLE();
            }
        }
    }

    return true;
}

bool Micro3dLoader::loadMtraFromFile(const std::string& filePath, ActionTable& outActionTable) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadMtra(buffer.data(), buffer.size(), outActionTable);
}

std::vector<uint8_t> Micro3dLoader::serializeMtra(const ActionTable& table, int version) {
    std::vector<uint8_t> buf;
    buf.reserve(1024);

    // Magic 'M', 'T'
    buf.push_back('M');
    buf.push_back('T');
    buf.push_back(static_cast<uint8_t>(version));
    buf.push_back(0);

    uint16_t numActions = static_cast<uint16_t>(table.actions.size());
    uint16_t numBones = numActions > 0 ? static_cast<uint16_t>(table.actions[0].numBones) : 0;

    writeUShortLE(buf, numActions);
    writeUShortLE(buf, numBones);

    // 8 transform type counts (default type 0 for all bones)
    writeUShortLE(buf, numBones); // type 0
    for (int i = 1; i < 8; ++i) {
        writeUShortLE(buf, 0);
    }

    // dataSize offset: 2 + 2 + 2 + 2 + 16 = 24
    size_t dataSizeOffset = buf.size();
    writeIntLE(buf, 0); // placeholder for dataSize

    size_t dataPayloadStart = buf.size();

    for (const auto& act : table.actions) {
        writeUShortLE(buf, static_cast<uint16_t>(act.keyframes));
        for (const auto& ba : act.boneActions) {
            if (ba.matrices.empty()) {
                buf.push_back(1); // type 1: identity
            } else {
                buf.push_back(0); // type 0: raw 12 shorts matrix
                const auto& m = ba.matrices[0];
                writeShortLE(buf, static_cast<int16_t>(m.m00));
                writeShortLE(buf, static_cast<int16_t>(m.m01));
                writeShortLE(buf, static_cast<int16_t>(m.m02));
                writeShortLE(buf, static_cast<int16_t>(m.m03));
                writeShortLE(buf, static_cast<int16_t>(m.m10));
                writeShortLE(buf, static_cast<int16_t>(m.m11));
                writeShortLE(buf, static_cast<int16_t>(m.m12));
                writeShortLE(buf, static_cast<int16_t>(m.m13));
                writeShortLE(buf, static_cast<int16_t>(m.m20));
                writeShortLE(buf, static_cast<int16_t>(m.m21));
                writeShortLE(buf, static_cast<int16_t>(m.m22));
                writeShortLE(buf, static_cast<int16_t>(m.m23));
            }
        }
    }

    // Backpatch dataSize
    uint32_t totalPayload = static_cast<uint32_t>(buf.size() - dataPayloadStart);
    buf[dataSizeOffset]     = static_cast<uint8_t>(totalPayload & 0xFF);
    buf[dataSizeOffset + 1] = static_cast<uint8_t>((totalPayload >> 8) & 0xFF);
    buf[dataSizeOffset + 2] = static_cast<uint8_t>((totalPayload >> 16) & 0xFF);
    buf[dataSizeOffset + 3] = static_cast<uint8_t>((totalPayload >> 24) & 0xFF);

    return buf;
}

} // namespace micro3d
} // namespace universal_loader
