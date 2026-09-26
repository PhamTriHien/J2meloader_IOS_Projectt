#include "m3g_engine.h"
#include <cstring>
#include <algorithm>

namespace universal_loader {
namespace m3g {

void IndexBuffer::getTriangles(std::vector<uint32_t>& outTriangles) const {
    outTriangles.clear();
    if (indices.size() < 3) {
        return;
    }

    if (primitiveType == PRIMITIVE_TRIANGLES) {
        size_t count = (indices.size() / 3) * 3;
        outTriangles.assign(indices.begin(), indices.begin() + count);
    } else if (primitiveType == PRIMITIVE_TRIANGLE_STRIP) {
        outTriangles.reserve((indices.size() - 2) * 3);
        for (size_t i = 0; i + 2 < indices.size(); ++i) {
            uint32_t v0, v1, v2;
            if ((i % 2) == 0) {
                v0 = indices[i];
                v1 = indices[i + 1];
                v2 = indices[i + 2];
            } else {
                v0 = indices[i + 1];
                v1 = indices[i];
                v2 = indices[i + 2];
            }

            // Skip degenerate triangles used for strip stitching
            if (v0 != v1 && v1 != v2 && v0 != v2) {
                outTriangles.push_back(v0);
                outTriangles.push_back(v1);
                outTriangles.push_back(v2);
            }
        }
    }
}

void VertexBuffer::setPositions(const std::vector<graphics3d::Vector3>& pos) {
    if (vertices.size() < pos.size()) {
        vertices.resize(pos.size());
    }
    for (size_t i = 0; i < pos.size(); ++i) {
        vertices[i].position = pos[i];
    }
}

void VertexBuffer::setNormals(const std::vector<graphics3d::Vector3>& norm) {
    if (vertices.size() < norm.size()) {
        vertices.resize(norm.size());
    }
    for (size_t i = 0; i < norm.size(); ++i) {
        vertices[i].normal = norm[i];
    }
}

void VertexBuffer::setTexCoords(const std::vector<std::pair<float, float>>& uvs) {
    if (vertices.size() < uvs.size()) {
        vertices.resize(uvs.size());
    }
    for (size_t i = 0; i < uvs.size(); ++i) {
        vertices[i].u = uvs[i].first;
        vertices[i].v = uvs[i].second;
    }
}

void VertexBuffer::setColors(const std::vector<uint32_t>& colors) {
    if (vertices.size() < colors.size()) {
        vertices.resize(colors.size());
    }
    for (size_t i = 0; i < colors.size(); ++i) {
        vertices[i].color = colors[i];
    }
}

Texture2D::Texture2D(int32_t w, int32_t h, const uint32_t* srcPixels)
    : width(w), height(h) {
    if (w > 0 && h > 0) {
        pixels.resize(static_cast<size_t>(w) * h, 0xFFFFFFFF);
        if (srcPixels != nullptr) {
            std::memcpy(pixels.data(), srcPixels, pixels.size() * sizeof(uint32_t));
        }
    }
}

void Texture2D::setPixel(int32_t x, int32_t y, uint32_t argb) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        pixels[static_cast<size_t>(y) * width + x] = argb;
    }
}

static float wrapCoord(float val, WrapMode mode) {
    if (mode == WRAP_REPEAT) {
        float frac = val - std::floor(val);
        return (frac < 0.0f) ? (frac + 1.0f) : frac;
    }
    return std::clamp(val, 0.0f, 1.0f);
}

uint32_t Texture2D::sample(float u, float v) const {
    if (width <= 0 || height <= 0 || pixels.empty()) {
        return 0xFFFFFFFF;
    }

    float wrappedU = wrapCoord(u, wrapS);
    float wrappedV = wrapCoord(v, wrapT);

    if (filter == FILTER_NEAREST) {
        int32_t px = std::clamp(static_cast<int32_t>(wrappedU * width), 0, width - 1);
        int32_t py = std::clamp(static_cast<int32_t>(wrappedV * height), 0, height - 1);
        return pixels[static_cast<size_t>(py) * width + px];
    }

    // Bilinear Filtering
    float fx = wrappedU * width - 0.5f;
    float fy = wrappedV * height - 0.5f;
    int32_t x0 = static_cast<int32_t>(std::floor(fx));
    int32_t y0 = static_cast<int32_t>(std::floor(fy));
    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;

    float dx = fx - x0;
    float dy = fy - y0;

    auto getTexel = [this](int32_t x, int32_t y) -> uint32_t {
        if (wrapS == WRAP_REPEAT) {
            x = (x % width + width) % width;
        } else {
            x = std::clamp(x, 0, width - 1);
        }
        if (wrapT == WRAP_REPEAT) {
            y = (y % height + height) % height;
        } else {
            y = std::clamp(y, 0, height - 1);
        }
        return pixels[static_cast<size_t>(y) * width + x];
    };

    uint32_t c00 = getTexel(x0, y0);
    uint32_t c10 = getTexel(x1, y0);
    uint32_t c01 = getTexel(x0, y1);
    uint32_t c11 = getTexel(x1, y1);

    auto extract = [](uint32_t argb, int shift) -> float {
        return static_cast<float>((argb >> shift) & 0xFF);
    };

    auto bilerp = [dx, dy](float f00, float f10, float f01, float f11) -> uint32_t {
        float top = f00 * (1.0f - dx) + f10 * dx;
        float btm = f01 * (1.0f - dx) + f11 * dx;
        float val = top * (1.0f - dy) + btm * dy;
        return static_cast<uint32_t>(std::clamp(val, 0.0f, 255.0f));
    };

    uint32_t a = bilerp(extract(c00, 24), extract(c10, 24), extract(c01, 24), extract(c11, 24));
    uint32_t r = bilerp(extract(c00, 16), extract(c10, 16), extract(c01, 16), extract(c11, 16));
    uint32_t g = bilerp(extract(c00, 8),  extract(c10, 8),  extract(c01, 8),  extract(c11, 8));
    uint32_t b = bilerp(extract(c00, 0),  extract(c10, 0),  extract(c01, 0),  extract(c11, 0));

    return (a << 24) | (r << 16) | (g << 8) | b;
}

graphics3d::Matrix4x4 Camera::getProjectionMatrix() const {
    if (projection == PROJECTION_PERSPECTIVE) {
        return graphics3d::Matrix4x4::perspective(fovY, aspectRatio, nearDistance, farDistance);
    }
    float halfW = parallelWidth * 0.5f;
    float halfH = parallelHeight * 0.5f;
    return graphics3d::Matrix4x4::ortho(-halfW, halfW, -halfH, halfH, nearDistance, farDistance);
}

graphics3d::Matrix4x4 Camera::getViewMatrix() const {
    return graphics3d::Matrix4x4::lookAt(eye, target, up);
}

} // namespace m3g
} // namespace universal_loader
