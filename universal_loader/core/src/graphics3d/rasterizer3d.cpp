#include "rasterizer3d.h"
#include <algorithm>
#include <cmath>

namespace universal_loader {
namespace graphics3d {

Rasterizer3D::Rasterizer3D() {
    m_viewport = {0, 0, 0, 0};
    m_clipRect = {0, 0, 0, 0};
}

void Rasterizer3D::setTarget(uint32_t* colorBuffer, int32_t width, int32_t height) {
    m_colorBuffer = colorBuffer;
    m_targetWidth = width;
    m_targetHeight = height;

    if (width > 0 && height > 0) {
        m_depthBuffer.resize(static_cast<size_t>(width) * height, 1.0f);
        m_viewport = {0, 0, width, height};
        m_clipRect = {0, 0, width, height};
    } else {
        m_depthBuffer.clear();
    }
}

void Rasterizer3D::setViewport(int32_t x, int32_t y, int32_t width, int32_t height) {
    m_viewport = {x, y, width, height};
}

void Rasterizer3D::setClipRect(int32_t x, int32_t y, int32_t width, int32_t height) {
    int32_t x0 = std::clamp(x, 0, m_targetWidth);
    int32_t y0 = std::clamp(y, 0, m_targetHeight);
    int32_t x1 = std::clamp(x + width, 0, m_targetWidth);
    int32_t y1 = std::clamp(y + height, 0, m_targetHeight);

    m_clipRect = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

void Rasterizer3D::clear(uint32_t argbColor, float depth) {
    if (m_colorBuffer != nullptr && m_targetWidth > 0 && m_targetHeight > 0) {
        size_t total = static_cast<size_t>(m_targetWidth) * m_targetHeight;
        std::fill(m_colorBuffer, m_colorBuffer + total, argbColor);
        std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), depth);
    }
}

void Rasterizer3D::clearDepth(float depth) {
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), depth);
}

float Rasterizer3D::getDepthAt(int32_t x, int32_t y) const {
    if (x >= 0 && x < m_targetWidth && y >= 0 && y < m_targetHeight) {
        return m_depthBuffer[static_cast<size_t>(y) * m_targetWidth + x];
    }
    return 1.0f;
}

uint32_t Rasterizer3D::getColorAt(int32_t x, int32_t y) const {
    if (m_colorBuffer != nullptr && x >= 0 && x < m_targetWidth && y >= 0 && y < m_targetHeight) {
        return m_colorBuffer[static_cast<size_t>(y) * m_targetWidth + x];
    }
    return 0x00000000;
}

uint32_t Rasterizer3D::calculateLighting(const Vector3& normal, const Vector3& worldPos,
                                         uint32_t baseColor, const m3g::Material& mat,
                                         const std::vector<m3g::Light>& lights) const {
    float baseA = ((baseColor >> 24) & 0xFF) / 255.0f;
    float baseR = ((baseColor >> 16) & 0xFF) / 255.0f;
    float baseG = ((baseColor >> 8)  & 0xFF) / 255.0f;
    float baseB = (baseColor & 0xFF)         / 255.0f;

    float matDiffR = ((mat.diffuseColor >> 16) & 0xFF) / 255.0f;
    float matDiffG = ((mat.diffuseColor >> 8)  & 0xFF) / 255.0f;
    float matDiffB = (mat.diffuseColor & 0xFF)         / 255.0f;

    float matAmbR = ((mat.ambientColor >> 16) & 0xFF) / 255.0f;
    float matAmbG = ((mat.ambientColor >> 8)  & 0xFF) / 255.0f;
    float matAmbB = (mat.ambientColor & 0xFF)         / 255.0f;

    float totalR = matAmbR * 0.2f * baseR;
    float totalG = matAmbG * 0.2f * baseG;
    float totalB = matAmbB * 0.2f * baseB;

    if (lights.empty()) {
        totalR = baseR;
        totalG = baseG;
        totalB = baseB;
    } else {
        for (const auto& light : lights) {
            float lightR = ((light.color >> 16) & 0xFF) / 255.0f * light.intensity;
            float lightG = ((light.color >> 8)  & 0xFF) / 255.0f * light.intensity;
            float lightB = (light.color & 0xFF)         / 255.0f * light.intensity;

            if (light.type == m3g::LIGHT_AMBIENT) {
                totalR += lightR * baseR * matAmbR;
                totalG += lightG * baseG * matAmbG;
                totalB += lightB * baseB * matAmbB;
            } else if (light.type == m3g::LIGHT_DIRECTIONAL) {
                Vector3 lightDir = (light.direction * -1.0f).normalized();
                float NdotL = std::max(0.0f, normal.dot(lightDir));
                totalR += lightR * baseR * matDiffR * NdotL;
                totalG += lightG * baseG * matDiffG * NdotL;
                totalB += lightB * baseB * matDiffB * NdotL;
            } else if (light.type == m3g::LIGHT_POINT) {
                Vector3 lightVec = light.position - worldPos;
                float dist = lightVec.length();
                if (dist > 1e-4f) {
                    Vector3 lightDir = lightVec / dist;
                    float atten = 1.0f / (light.constantAttenuation +
                                          light.linearAttenuation * dist +
                                          light.quadraticAttenuation * dist * dist);
                    float NdotL = std::max(0.0f, normal.dot(lightDir)) * atten;
                    totalR += lightR * baseR * matDiffR * NdotL;
                    totalG += lightG * baseG * matDiffG * NdotL;
                    totalB += lightB * baseB * matDiffB * NdotL;
                }
            }
        }
    }

    uint32_t outA = static_cast<uint32_t>(std::clamp(baseA * 255.0f, 0.0f, 255.0f));
    uint32_t outR = static_cast<uint32_t>(std::clamp(totalR * 255.0f, 0.0f, 255.0f));
    uint32_t outG = static_cast<uint32_t>(std::clamp(totalG * 255.0f, 0.0f, 255.0f));
    uint32_t outB = static_cast<uint32_t>(std::clamp(totalB * 255.0f, 0.0f, 255.0f));

    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

void Rasterizer3D::drawTriangle(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                                const m3g::Appearance& app, const std::vector<m3g::Light>& lights) {
    if (m_colorBuffer == nullptr || m_targetWidth <= 0 || m_targetHeight <= 0) {
        return;
    }

    // 2D Signed area (determinant) for winding order
    float area = (v1.screenPos.x - v0.screenPos.x) * (v2.screenPos.y - v0.screenPos.y) -
                 (v1.screenPos.y - v0.screenPos.y) * (v2.screenPos.x - v0.screenPos.x);

    if (std::abs(area) < 1e-5f) {
        return; // Degenerate zero-area triangle
    }

    // Culling
    if (app.polygonMode.culling == m3g::CULL_BACK && area < 0.0f) {
        return;
    }
    if (app.polygonMode.culling == m3g::CULL_FRONT && area > 0.0f) {
        return;
    }

    float invArea = 1.0f / area;

    // Triangle Bounding Box clamped to clipRect
    int32_t minX = std::max(m_clipRect.x, static_cast<int32_t>(std::floor(std::min({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}))));
    int32_t maxX = std::min(m_clipRect.x + m_clipRect.width - 1, static_cast<int32_t>(std::ceil(std::max({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}))));
    int32_t minY = std::max(m_clipRect.y, static_cast<int32_t>(std::floor(std::min({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}))));
    int32_t maxY = std::min(m_clipRect.y + m_clipRect.height - 1, static_cast<int32_t>(std::ceil(std::max({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}))));

    if (minX > maxX || minY > maxY) {
        return;
    }

    for (int32_t py = minY; py <= maxY; ++py) {
        float y = py + 0.5f;
        for (int32_t px = minX; px <= maxX; ++px) {
            float x = px + 0.5f;

            // Barycentric edge functions
            float w0 = (v2.screenPos.x - v1.screenPos.x) * (y - v1.screenPos.y) -
                       (v2.screenPos.y - v1.screenPos.y) * (x - v1.screenPos.x);
            float w1 = (v0.screenPos.x - v2.screenPos.x) * (y - v2.screenPos.y) -
                       (v0.screenPos.y - v2.screenPos.y) * (x - v2.screenPos.x);
            float w2 = (v1.screenPos.x - v0.screenPos.x) * (y - v0.screenPos.y) -
                       (v1.screenPos.y - v0.screenPos.y) * (x - v0.screenPos.x);

            float l0 = w0 * invArea;
            float l1 = w1 * invArea;
            float l2 = w2 * invArea;

            // Point inside triangle check
            if (l0 >= -1e-5f && l1 >= -1e-5f && l2 >= -1e-5f) {
                // Perspective-correct interpolation weights
                float z = l0 * v0.screenPos.z + l1 * v1.screenPos.z + l2 * v2.screenPos.z;
                if (z < 0.0f || z > 1.0f) {
                    continue;
                }

                size_t pixelIdx = static_cast<size_t>(py) * m_targetWidth + px;

                // Depth test
                if (app.compositingMode.depthTestEnabled) {
                    if (z >= m_depthBuffer[pixelIdx] - 1e-6f) {
                        continue;
                    }
                }

                float invW = l0 * v0.invW + l1 * v1.invW + l2 * v2.invW;
                float w = (invW > 1e-6f) ? (1.0f / invW) : 1.0f;

                float u = (l0 * v0.u * v0.invW + l1 * v1.u * v1.invW + l2 * v2.u * v2.invW) * w;
                float v = (l0 * v0.v * v0.invW + l1 * v1.v * v1.invW + l2 * v2.v * v2.invW) * w;

                Vector3 norm = (v0.normal * l0 + v1.normal * l1 + v2.normal * l2).normalized();

                float vertR = (l0 * ((v0.color >> 16) & 0xFF) + l1 * ((v1.color >> 16) & 0xFF) + l2 * ((v2.color >> 16) & 0xFF)) / 255.0f;
                float vertG = (l0 * ((v0.color >> 8)  & 0xFF) + l1 * ((v1.color >> 8)  & 0xFF) + l2 * ((v2.color >> 8)  & 0xFF)) / 255.0f;
                float vertB = (l0 * (v0.color & 0xFF)         + l1 * (v1.color & 0xFF)         + l2 * (v2.color & 0xFF)) / 255.0f;
                float vertA = (l0 * ((v0.color >> 24) & 0xFF) + l1 * ((v1.color >> 24) & 0xFF) + l2 * ((v2.color >> 24) & 0xFF)) / 255.0f;

                uint32_t baseColor = 0xFFFFFFFF;
                if (app.texture != nullptr) {
                    uint32_t texColor = app.texture->sample(u, v);
                    float tA = ((texColor >> 24) & 0xFF) / 255.0f;
                    float tR = ((texColor >> 16) & 0xFF) / 255.0f;
                    float tG = ((texColor >> 8)  & 0xFF) / 255.0f;
                    float tB = (texColor & 0xFF)         / 255.0f;
                    baseColor = (static_cast<uint32_t>(std::clamp(tA * vertA * 255.0f, 0.0f, 255.0f)) << 24) |
                                (static_cast<uint32_t>(std::clamp(tR * vertR * 255.0f, 0.0f, 255.0f)) << 16) |
                                (static_cast<uint32_t>(std::clamp(tG * vertG * 255.0f, 0.0f, 255.0f)) << 8)  |
                                 static_cast<uint32_t>(std::clamp(tB * vertB * 255.0f, 0.0f, 255.0f));
                } else {
                    baseColor = (static_cast<uint32_t>(std::clamp(vertA * 255.0f, 0.0f, 255.0f)) << 24) |
                                (static_cast<uint32_t>(std::clamp(vertR * 255.0f, 0.0f, 255.0f)) << 16) |
                                (static_cast<uint32_t>(std::clamp(vertG * 255.0f, 0.0f, 255.0f)) << 8)  |
                                 static_cast<uint32_t>(std::clamp(vertB * 255.0f, 0.0f, 255.0f));
                }

                uint32_t litColor = calculateLighting(norm, Vector3(x, y, z), baseColor, app.material, lights);
                float alpha = ((litColor >> 24) & 0xFF) / 255.0f;

                if (alpha < app.compositingMode.alphaThreshold) {
                    continue;
                }

                // Blending
                if (app.compositingMode.blending == m3g::BLEND_REPLACE || alpha >= 0.999f) {
                    m_colorBuffer[pixelIdx] = litColor;
                } else if (app.compositingMode.blending == m3g::BLEND_ALPHA) {
                    uint32_t dstColor = m_colorBuffer[pixelIdx];
                    float dstR = ((dstColor >> 16) & 0xFF);
                    float dstG = ((dstColor >> 8)  & 0xFF);
                    float dstB = (dstColor & 0xFF);

                    float srcR = ((litColor >> 16) & 0xFF);
                    float srcG = ((litColor >> 8)  & 0xFF);
                    float srcB = (litColor & 0xFF);

                    uint32_t outR = static_cast<uint32_t>(srcR * alpha + dstR * (1.0f - alpha));
                    uint32_t outG = static_cast<uint32_t>(srcG * alpha + dstG * (1.0f - alpha));
                    uint32_t outB = static_cast<uint32_t>(srcB * alpha + dstB * (1.0f - alpha));

                    m_colorBuffer[pixelIdx] = 0xFF000000 | (outR << 16) | (outG << 8) | outB;
                } else if (app.compositingMode.blending == m3g::BLEND_ADD) {
                    uint32_t dstColor = m_colorBuffer[pixelIdx];
                    uint32_t outR = std::min(255u, ((litColor >> 16) & 0xFF) + ((dstColor >> 16) & 0xFF));
                    uint32_t outG = std::min(255u, ((litColor >> 8)  & 0xFF) + ((dstColor >> 8)  & 0xFF));
                    uint32_t outB = std::min(255u, (litColor & 0xFF)         + (dstColor & 0xFF));
                    m_colorBuffer[pixelIdx] = 0xFF000000 | (outR << 16) | (outG << 8) | outB;
                }

                // Depth write
                if (app.compositingMode.depthWriteEnabled) {
                    m_depthBuffer[pixelIdx] = z;
                }
            }
        }
    }
}

void Rasterizer3D::clipAndDrawTriangle(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                                       const m3g::Appearance& app, const std::vector<m3g::Light>& lights) {
    // Near plane clipping: check if all 3 vertices are behind near plane
    if (v0.clipPos.w <= 1e-4f && v1.clipPos.w <= 1e-4f && v2.clipPos.w <= 1e-4f) {
        return;
    }

    drawTriangle(v0, v1, v2, app, lights);
}

void Rasterizer3D::renderMesh(const m3g::Mesh& mesh, const m3g::Camera& camera,
                              const std::vector<m3g::Light>& lights) {
    if (mesh.vertexBuffer.vertices.empty() || mesh.submeshes.empty()) {
        return;
    }

    Matrix4x4 model = mesh.transform;
    Matrix4x4 view = camera.getViewMatrix();
    Matrix4x4 proj = camera.getProjectionMatrix();
    Matrix4x4 mvp = proj * view * model;

    // Transform all vertices to Clip Space and Screen Space
    std::vector<RasterVertex> rasterVertices(mesh.vertexBuffer.vertices.size());
    for (size_t i = 0; i < mesh.vertexBuffer.vertices.size(); ++i) {
        const auto& src = mesh.vertexBuffer.vertices[i];
        RasterVertex& dst = rasterVertices[i];

        dst.clipPos = mvp * Vector4(src.position, 1.0f);
        dst.normal = model.transformVector(src.normal).normalized();
        dst.u = src.u;
        dst.v = src.v;
        dst.color = src.color;

        float w = dst.clipPos.w;
        if (std::abs(w) > 1e-6f) {
            dst.invW = 1.0f / w;
            float ndcX = dst.clipPos.x * dst.invW;
            float ndcY = dst.clipPos.y * dst.invW;
            float ndcZ = dst.clipPos.z * dst.invW;

            dst.screenPos.x = m_viewport.x + (ndcX + 1.0f) * 0.5f * m_viewport.width;
            dst.screenPos.y = m_viewport.y + (1.0f - ndcY) * 0.5f * m_viewport.height; // Invert Y for screen
            dst.screenPos.z = (ndcZ + 1.0f) * 0.5f; // Map from [-1, 1] to [0, 1]
        } else {
            dst.invW = 1.0f;
            dst.screenPos = Vector3(0.0f, 0.0f, 1.0f);
        }
    }

    // Draw submeshes
    for (const auto& submesh : mesh.submeshes) {
        std::vector<uint32_t> triangles;
        submesh.indexBuffer.getTriangles(triangles);

        for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
            uint32_t idx0 = triangles[i];
            uint32_t idx1 = triangles[i + 1];
            uint32_t idx2 = triangles[i + 2];

            if (idx0 < rasterVertices.size() && idx1 < rasterVertices.size() && idx2 < rasterVertices.size()) {
                clipAndDrawTriangle(rasterVertices[idx0], rasterVertices[idx1], rasterVertices[idx2],
                                    submesh.appearance, lights);
            }
        }
    }
}

void Rasterizer3D::renderMicro3D(const micro3d::Micro3DVertex* vertices, int32_t vertexCount,
                                 const int32_t* indices, int32_t indexCount,
                                 const micro3d::FigureLayout& layout,
                                 const std::vector<m3g::Light>& lights,
                                 const m3g::Texture2D* texture) {
    if (vertices == nullptr || vertexCount <= 0 || indices == nullptr || indexCount <= 0) {
        return;
    }

    float pmElements[16];
    layout.computeProjectionMatrix(pmElements, 0, 0, static_cast<float>(m_viewport.width), static_cast<float>(m_viewport.height));
    Matrix4x4 proj(pmElements);
    Matrix4x4 model = layout.affine.toMatrix4x4();
    Matrix4x4 mvp = proj * model;

    std::vector<RasterVertex> rasterVertices(vertexCount);
    for (int32_t i = 0; i < vertexCount; ++i) {
        const auto& src = vertices[i];
        RasterVertex& dst = rasterVertices[i];

        dst.clipPos = mvp * Vector4(src.position, 1.0f);
        dst.normal = model.transformVector(src.normal).normalized();
        dst.u = src.u;
        dst.v = src.v;
        dst.color = src.color;

        float w = dst.clipPos.w;
        if (std::abs(w) > 1e-6f) {
            dst.invW = 1.0f / w;
            float ndcX = dst.clipPos.x * dst.invW;
            float ndcY = dst.clipPos.y * dst.invW;
            float ndcZ = dst.clipPos.z * dst.invW;

            dst.screenPos.x = m_viewport.x + (ndcX + 1.0f) * 0.5f * m_viewport.width;
            dst.screenPos.y = m_viewport.y + (1.0f - ndcY) * 0.5f * m_viewport.height;
            dst.screenPos.z = (ndcZ + 1.0f) * 0.5f;
        } else {
            dst.invW = 1.0f;
            dst.screenPos = Vector3(0.0f, 0.0f, 1.0f);
        }
    }

    m3g::Appearance app;
    if (texture != nullptr) {
        app.texture = std::make_shared<m3g::Texture2D>(*texture);
    }
    app.polygonMode.culling = m3g::CULL_BACK;

    for (int32_t i = 0; i + 2 < indexCount; i += 3) {
        int32_t idx0 = indices[i];
        int32_t idx1 = indices[i + 1];
        int32_t idx2 = indices[i + 2];

        if (idx0 >= 0 && idx0 < vertexCount &&
            idx1 >= 0 && idx1 < vertexCount &&
            idx2 >= 0 && idx2 < vertexCount) {
            clipAndDrawTriangle(rasterVertices[idx0], rasterVertices[idx1], rasterVertices[idx2], app, lights);
        }
    }
}

} // namespace graphics3d
} // namespace universal_loader
