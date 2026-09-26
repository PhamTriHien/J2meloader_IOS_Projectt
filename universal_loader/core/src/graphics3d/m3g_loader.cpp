#include "m3g_loader.h"
#include <fstream>
#include <cstring>
#include <cmath>

namespace universal_loader {
namespace m3g {

static const uint8_t M3G_SIG[12] = {
    0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static const uint8_t PNG_SIG[8] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static const uint8_t JPEG_SIG[2] = {
    0xFF, 0xD8
};

FileType M3gLoader::identify(const uint8_t* data, size_t size) {
    if (!data || size == 0) return M3G_FILE_UNKNOWN;

    if (size >= sizeof(M3G_SIG) && std::memcmp(data, M3G_SIG, sizeof(M3G_SIG)) == 0) {
        return M3G_FILE_M3G;
    }
    if (size >= sizeof(PNG_SIG) && std::memcmp(data, PNG_SIG, sizeof(PNG_SIG)) == 0) {
        return M3G_FILE_PNG;
    }
    if (size >= sizeof(JPEG_SIG) && data[0] == JPEG_SIG[0] && data[1] == JPEG_SIG[1]) {
        return M3G_FILE_JPEG;
    }
    return M3G_FILE_UNKNOWN;
}

uint32_t M3gLoader::computeAdler32(const uint8_t* data, size_t len) {
    uint32_t a = 1;
    uint32_t b = 0;
    constexpr uint32_t MOD_ADLER = 65521;

    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

static inline uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           (static_cast<uint32_t>(p[3]));
}

static inline void writeU32BE(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

static inline float readFloatBE(const uint8_t* p) {
    uint32_t u = readU32BE(p);
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(float));
    return f;
}

static inline void writeFloatBE(std::vector<uint8_t>& buf, float val) {
    uint32_t u = 0;
    std::memcpy(&u, &val, sizeof(float));
    writeU32BE(buf, u);
}

bool M3gLoader::load(const uint8_t* data, size_t size, M3gLoadedScene& outScene) {
    outScene.clear();
    if (!data || size < 25) return false;

    if (identify(data, size) != M3G_FILE_M3G) {
        return false;
    }

    size_t offset = sizeof(M3G_SIG);

    // --- Section 1: Header Section ---
    if (offset + 9 > size) return false;

    uint8_t headerCompression = data[offset++];
    uint32_t headerTotalLength = readU32BE(data + offset); offset += 4;
    uint32_t headerUncompressedLength = readU32BE(data + offset); offset += 4;

    (void)headerCompression;
    (void)headerUncompressedLength;

    if (offset + headerTotalLength > size) return false;

    size_t headerSectionStart = offset;
    size_t headerBodyLength = (headerTotalLength >= 4) ? (headerTotalLength - 4) : 0;

    // Check Adler32 of Header Section
    uint32_t expectedHeaderChecksum = readU32BE(data + headerSectionStart + headerBodyLength);
    uint32_t calculatedHeaderChecksum = computeAdler32(data + headerSectionStart, headerBodyLength);
    if (expectedHeaderChecksum != calculatedHeaderChecksum) {
        return false;
    }

    // Parse Header Object
    if (headerBodyLength < 1 + 4 + 1 + 1 + 1 + 4 + 4) return false;
    uint8_t objType = data[offset++];
    if (objType != M3G_OBJ_HEADER) return false;

    uint32_t objLength = readU32BE(data + offset); offset += 4;
    (void)objLength;

    outScene.versionMajor = data[offset++];
    outScene.versionMinor = data[offset++];
    uint8_t hasExternalLinks = data[offset++];
    (void)hasExternalLinks;

    outScene.totalFileSize = readU32BE(data + offset); offset += 4;
    uint32_t approxContentSize = readU32BE(data + offset); offset += 4;
    (void)approxContentSize;

    // Read authoring tool string
    std::string author;
    while (offset < headerSectionStart + headerBodyLength && data[offset] != '\0') {
        author.push_back(static_cast<char>(data[offset++]));
    }
    if (offset < headerSectionStart + headerBodyLength && data[offset] == '\0') {
        offset++;
    }
    outScene.authoringTool = author;

    // Advance past header section checksum
    offset = headerSectionStart + headerTotalLength;

    // --- Section 2: Data Section(s) ---
    while (offset + 9 <= size) {
        uint8_t dataCompression = data[offset++];
        uint32_t dataTotalLength = readU32BE(data + offset); offset += 4;
        uint32_t dataUncompressedLength = readU32BE(data + offset); offset += 4;
        (void)dataCompression;
        (void)dataUncompressedLength;

        if (offset + dataTotalLength > size) break;

        size_t dataSectionStart = offset;
        size_t dataBodyLength = (dataTotalLength >= 4) ? (dataTotalLength - 4) : 0;

        uint32_t expectedDataChecksum = readU32BE(data + dataSectionStart + dataBodyLength);
        uint32_t calculatedDataChecksum = computeAdler32(data + dataSectionStart, dataBodyLength);
        if (expectedDataChecksum != calculatedDataChecksum) {
            return false;
        }

        size_t dataEnd = dataSectionStart + dataBodyLength;
        while (offset + 5 <= dataEnd) {
            uint8_t curType = data[offset++];
            uint32_t curLength = readU32BE(data + offset); offset += 4;
            if (offset + curLength > dataEnd) break;

            size_t nextObj = offset + curLength;

            switch (curType) {
                case M3G_OBJ_WORLD: {
                    auto world = std::make_shared<World>();
                    if (curLength >= 4) {
                        uint32_t bg = readU32BE(data + offset);
                        world->setBackground(bg);
                    }
                    outScene.world = world;
                    break;
                }
                case M3G_OBJ_CAMERA: {
                    auto cam = std::make_shared<Camera>();
                    if (curLength >= 1 + 16) {
                        cam->projection = (data[offset] == 1) ? PROJECTION_PARALLEL : PROJECTION_PERSPECTIVE;
                        cam->fovY = readFloatBE(data + offset + 1);
                        cam->aspectRatio = readFloatBE(data + offset + 5);
                        cam->nearDistance = readFloatBE(data + offset + 9);
                        cam->farDistance = readFloatBE(data + offset + 13);
                    }
                    outScene.cameras.push_back(cam);
                    if (outScene.world && !outScene.world->getActiveCamera()) {
                        outScene.world->setActiveCamera(cam);
                    }
                    break;
                }
                case M3G_OBJ_MESH: {
                    auto mesh = std::make_shared<Mesh>();
                    size_t p = offset;
                    if (p + 4 <= nextObj) {
                        uint32_t vCount = readU32BE(data + p); p += 4;
                        std::vector<graphics3d::Vector3> positions;
                        for (uint32_t i = 0; i < vCount && p + 12 <= nextObj; ++i) {
                            float x = readFloatBE(data + p); p += 4;
                            float y = readFloatBE(data + p); p += 4;
                            float z = readFloatBE(data + p); p += 4;
                            positions.emplace_back(x, y, z);
                        }
                        mesh->vertexBuffer.setPositions(positions);

                        if (p + 4 <= nextObj) {
                            uint32_t idxCount = readU32BE(data + p); p += 4;
                            std::vector<uint32_t> indices;
                            for (uint32_t i = 0; i < idxCount && p + 4 <= nextObj; ++i) {
                                indices.push_back(readU32BE(data + p));
                                p += 4;
                            }
                            Submesh sub;
                            sub.indexBuffer = IndexBuffer(PRIMITIVE_TRIANGLES, indices);
                            mesh->submeshes.push_back(sub);
                        }
                    }
                    outScene.meshes.push_back(mesh);
                    break;
                }
                case M3G_OBJ_MATERIAL: {
                    auto mat = std::make_shared<Material>();
                    if (curLength >= 16) {
                        mat->ambientColor = readU32BE(data + offset);
                        mat->diffuseColor = readU32BE(data + offset + 4);
                        mat->specularColor = readU32BE(data + offset + 8);
                        mat->shininess = readFloatBE(data + offset + 12);
                    }
                    outScene.materials.push_back(mat);
                    break;
                }
                case M3G_OBJ_KEYFRAME_SEQUENCE: {
                    if (curLength >= 16) {
                        uint32_t numKeyframes = readU32BE(data + offset);
                        uint32_t numComponents = readU32BE(data + offset + 4);
                        uint32_t interpolation = readU32BE(data + offset + 8);
                        uint32_t duration = readU32BE(data + offset + 12);
                        auto seq = std::make_shared<KeyframeSequence>(numKeyframes, numComponents, interpolation);
                        seq->setDuration(static_cast<int>(duration));
                        outScene.keyframeSequences.push_back(seq);
                    }
                    break;
                }
                case M3G_OBJ_GROUP: {
                    auto grp = std::make_shared<Group>();
                    if (curLength >= 40) {
                        float tx = readFloatBE(data + offset);
                        float ty = readFloatBE(data + offset + 4);
                        float tz = readFloatBE(data + offset + 8);
                        grp->setTranslation(tx, ty, tz);

                        float qx = readFloatBE(data + offset + 12);
                        float qy = readFloatBE(data + offset + 16);
                        float qz = readFloatBE(data + offset + 20);
                        float qw = readFloatBE(data + offset + 24);
                        grp->setOrientation(graphics3d::Quaternion(qx, qy, qz, qw));

                        float sx = readFloatBE(data + offset + 28);
                        float sy = readFloatBE(data + offset + 32);
                        float sz = readFloatBE(data + offset + 36);
                        grp->setScale(sx, sy, sz);
                    }
                    outScene.rootNodes.push_back(grp);
                    break;
                }
                default:
                    break;
            }

            offset = nextObj;
        }

        offset = dataSectionStart + dataTotalLength;
    }

    return true;
}

bool M3gLoader::loadFromFile(const std::string& filePath, M3gLoadedScene& outScene) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) return false;

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return false;
    }

    return load(buffer.data(), buffer.size(), outScene);
}

std::vector<uint8_t> M3gLoader::serializeScene(const M3gLoadedScene& scene) {
    std::vector<uint8_t> output;

    // 1. File Identifier (12 bytes)
    output.insert(output.end(), M3G_SIG, M3G_SIG + sizeof(M3G_SIG));

    // 2. Header Section
    std::vector<uint8_t> headerBody;
    headerBody.push_back(M3G_OBJ_HEADER); // objectType = 0
    // placeholder for header obj length (4 bytes)
    size_t headerLenPos = headerBody.size();
    writeU32BE(headerBody, 0);

    headerBody.push_back(static_cast<uint8_t>(scene.versionMajor));
    headerBody.push_back(static_cast<uint8_t>(scene.versionMinor));
    headerBody.push_back(0); // externalLinks = false
    writeU32BE(headerBody, 0); // totalFileSize placeholder
    writeU32BE(headerBody, 1024); // approxContentSize

    std::string tool = scene.authoringTool.empty() ? "UniversalLoader-J2ME" : scene.authoringTool;
    for (char c : tool) {
        headerBody.push_back(static_cast<uint8_t>(c));
    }
    headerBody.push_back(0); // null terminator

    // Back-patch header object length
    uint32_t headerObjPayloadLen = static_cast<uint32_t>(headerBody.size() - headerLenPos - 4);
    headerBody[headerLenPos]     = (headerObjPayloadLen >> 24) & 0xFF;
    headerBody[headerLenPos + 1] = (headerObjPayloadLen >> 16) & 0xFF;
    headerBody[headerLenPos + 2] = (headerObjPayloadLen >> 8)  & 0xFF;
    headerBody[headerLenPos + 3] = (headerObjPayloadLen)       & 0xFF;

    // Header section wrapper
    output.push_back(0); // compression = uncompressed
    uint32_t headerTotalLen = static_cast<uint32_t>(headerBody.size() + 4); // body + checksum
    writeU32BE(output, headerTotalLen);
    writeU32BE(output, static_cast<uint32_t>(headerBody.size())); // uncompressedLength
    output.insert(output.end(), headerBody.begin(), headerBody.end());

    uint32_t headerChecksum = computeAdler32(headerBody.data(), headerBody.size());
    writeU32BE(output, headerChecksum);

    // 3. Data Section
    std::vector<uint8_t> dataBody;

    // World Object
    if (scene.world) {
        dataBody.push_back(M3G_OBJ_WORLD);
        writeU32BE(dataBody, 4); // length: 4 bytes bg color
        writeU32BE(dataBody, scene.world->getBackground());
    }

    // Camera Objects
    for (const auto& cam : scene.cameras) {
        if (!cam) continue;
        dataBody.push_back(M3G_OBJ_CAMERA);
        writeU32BE(dataBody, 1 + 16);
        dataBody.push_back(cam->projection == PROJECTION_PARALLEL ? 1 : 0);
        writeFloatBE(dataBody, cam->fovY);
        writeFloatBE(dataBody, cam->aspectRatio);
        writeFloatBE(dataBody, cam->nearDistance);
        writeFloatBE(dataBody, cam->farDistance);
    }

    // Mesh Objects
    for (const auto& mesh : scene.meshes) {
        if (!mesh) continue;
        dataBody.push_back(M3G_OBJ_MESH);
        size_t meshLenPos = dataBody.size();
        writeU32BE(dataBody, 0); // placeholder for length

        size_t meshStart = dataBody.size();
        uint32_t vCount = static_cast<uint32_t>(mesh->vertexBuffer.vertices.size());
        writeU32BE(dataBody, vCount);
        for (const auto& v : mesh->vertexBuffer.vertices) {
            writeFloatBE(dataBody, v.position.x);
            writeFloatBE(dataBody, v.position.y);
            writeFloatBE(dataBody, v.position.z);
        }

        uint32_t idxCount = 0;
        if (!mesh->submeshes.empty()) {
            idxCount = static_cast<uint32_t>(mesh->submeshes[0].indexBuffer.indices.size());
        }
        writeU32BE(dataBody, idxCount);
        if (!mesh->submeshes.empty()) {
            for (uint32_t idx : mesh->submeshes[0].indexBuffer.indices) {
                writeU32BE(dataBody, idx);
            }
        }

        uint32_t meshLen = static_cast<uint32_t>(dataBody.size() - meshStart);
        dataBody[meshLenPos]     = (meshLen >> 24) & 0xFF;
        dataBody[meshLenPos + 1] = (meshLen >> 16) & 0xFF;
        dataBody[meshLenPos + 2] = (meshLen >> 8)  & 0xFF;
        dataBody[meshLenPos + 3] = (meshLen)       & 0xFF;
    }

    // Material Objects
    for (const auto& mat : scene.materials) {
        if (!mat) continue;
        dataBody.push_back(M3G_OBJ_MATERIAL);
        writeU32BE(dataBody, 16);
        writeU32BE(dataBody, mat->ambientColor);
        writeU32BE(dataBody, mat->diffuseColor);
        writeU32BE(dataBody, mat->specularColor);
        writeFloatBE(dataBody, mat->shininess);
    }

    // Keyframe Sequences
    for (const auto& seq : scene.keyframeSequences) {
        if (!seq) continue;
        dataBody.push_back(M3G_OBJ_KEYFRAME_SEQUENCE);
        writeU32BE(dataBody, 16);
        writeU32BE(dataBody, static_cast<uint32_t>(seq->getKeyframeCount()));
        writeU32BE(dataBody, static_cast<uint32_t>(seq->getComponentCount()));
        writeU32BE(dataBody, static_cast<uint32_t>(seq->getInterpolationType()));
        writeU32BE(dataBody, static_cast<uint32_t>(seq->getDuration()));
    }

    // Group Objects
    for (const auto& grp : scene.rootNodes) {
        if (!grp) continue;
        dataBody.push_back(M3G_OBJ_GROUP);
        writeU32BE(dataBody, 40);
        const auto& t = grp->getTranslation();
        writeFloatBE(dataBody, t.x);
        writeFloatBE(dataBody, t.y);
        writeFloatBE(dataBody, t.z);

        const auto& q = grp->getOrientation();
        writeFloatBE(dataBody, q.x);
        writeFloatBE(dataBody, q.y);
        writeFloatBE(dataBody, q.z);
        writeFloatBE(dataBody, q.w);

        const auto& s = grp->getScale();
        writeFloatBE(dataBody, s.x);
        writeFloatBE(dataBody, s.y);
        writeFloatBE(dataBody, s.z);
    }

    // Wrap data section
    output.push_back(0); // uncompressed
    uint32_t dataTotalLen = static_cast<uint32_t>(dataBody.size() + 4);
    writeU32BE(output, dataTotalLen);
    writeU32BE(output, static_cast<uint32_t>(dataBody.size()));
    output.insert(output.end(), dataBody.begin(), dataBody.end());

    uint32_t dataChecksum = computeAdler32(dataBody.data(), dataBody.size());
    writeU32BE(output, dataChecksum);

    // Backpatch totalFileSize in header object
    uint32_t totalSize = static_cast<uint32_t>(output.size());
    // offset 12 is section wrapper start, header body starts at 12 + 9 = 21.
    // In headerBody: objType(1) + len(4) + vMajor(1) + vMinor(1) + extLinks(1) = 8 bytes.
    // So totalFileSize is at 21 + 8 = 29.
    if (output.size() >= 33) {
        output[29] = (totalSize >> 24) & 0xFF;
        output[30] = (totalSize >> 16) & 0xFF;
        output[31] = (totalSize >> 8)  & 0xFF;
        output[32] = (totalSize)       & 0xFF;
        // Recompute header section checksum
        uint32_t updatedHeaderChecksum = computeAdler32(output.data() + 21, headerBody.size());
        size_t headerChecksumPos = 21 + headerBody.size();
        output[headerChecksumPos]     = (updatedHeaderChecksum >> 24) & 0xFF;
        output[headerChecksumPos + 1] = (updatedHeaderChecksum >> 16) & 0xFF;
        output[headerChecksumPos + 2] = (updatedHeaderChecksum >> 8)  & 0xFF;
        output[headerChecksumPos + 3] = (updatedHeaderChecksum)       & 0xFF;
    }

    return output;
}

} // namespace m3g
} // namespace universal_loader
