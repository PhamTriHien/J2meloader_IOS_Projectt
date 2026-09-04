#include "m3g_engine.h"
#include <cstring>
#include <algorithm>

Mat4::Mat4() {
    memset(m, 0, sizeof(m));
}

Mat4 Mat4::identity() {
    Mat4 res;
    res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
    return res;
}

Mat4 Mat4::perspective(float fovy, float aspect, float nearP, float farP) {
    Mat4 res;
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    res.m[0] = f / aspect;
    res.m[5] = f;
    res.m[10] = (farP + nearP) / (nearP - farP);
    res.m[11] = -1.0f;
    res.m[14] = (2.0f * farP * nearP) / (nearP - farP);
    return res;
}

Mat4 Mat4::multiply(const Mat4& a, const Mat4& b) {
    Mat4 res;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[r * 4 + k] * b.m[k * 4 + c];
            }
            res.m[r * 4 + c] = sum;
        }
    }
    return res;
}

Vec3 Mat4::transformPoint(const Vec3& v) const {
    float x = v.x * m[0] + v.y * m[4] + v.z * m[8] + m[12];
    float y = v.x * m[1] + v.y * m[5] + v.z * m[9] + m[13];
    float z = v.x * m[2] + v.y * m[6] + v.z * m[10] + m[14];
    float w = v.x * m[3] + v.y * m[7] + v.z * m[11] + m[15];
    if (w != 0.0f && w != 1.0f) {
        float invW = 1.0f / w;
        return Vec3(x * invW, y * invW, z * invW);
    }
    return Vec3(x, y, z);
}

M3GCamera::M3GCamera() {
    setPerspective(60.0f, 240.0f / 320.0f, 0.1f, 100.0f);
}

void M3GCamera::setPerspective(float fovy, float aspect, float nearP, float farP) {
    m_projMatrix = Mat4::perspective(fovy, aspect, nearP, farP);
}

M3GLight::M3GLight(Mode mode) : m_mode(mode) {}
void M3GLight::setColor(uint32_t rgb) { m_color = rgb; }
void M3GLight::setIntensity(float intensity) { m_intensity = intensity; }

M3GGraphics3D::M3GGraphics3D() {}

M3GGraphics3D& M3GGraphics3D::getInstance() {
    static M3GGraphics3D instance;
    return instance;
}

void M3GGraphics3D::bindTarget(LcduiDisplay* target) {
    m_target = target;
    if (m_target) {
        int w = m_target->getWidth();
        int h = m_target->getHeight();
        m_depthBuffer.resize(w * h, 1.0f);
    }
}

void M3GGraphics3D::releaseTarget() {
    m_target = nullptr;
}

void M3GGraphics3D::clear(uint32_t color) {
    if (m_target) {
        m_target->clear(color);
        std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);
    }
}

void M3GGraphics3D::setCamera(const M3GCamera& camera, const Mat4& viewMatrix) {
    m_viewProjMatrix = Mat4::multiply(camera.getProjectionMatrix(), viewMatrix);
}

void M3GGraphics3D::addLight(const M3GLight& light, const Mat4& transform) {
    m_lights.push_back(light);
    m_lightTransforms.push_back(transform);
}

void M3GGraphics3D::resetLights() {
    m_lights.clear();
    m_lightTransforms.clear();
}

void M3GGraphics3D::renderTriangle(const M3GVertex& v0, const M3GVertex& v1, const M3GVertex& v2, const uint32_t* texture, int texW, int texH) {
    if (!m_target) return;
    int screenW = m_target->getWidth();
    int screenH = m_target->getHeight();

    // Screen Space Coordinates
    int x0 = (int)((v0.position.x + 1.0f) * 0.5f * screenW);
    int y0 = (int)((1.0f - v0.position.y) * 0.5f * screenH);
    int x1 = (int)((v1.position.x + 1.0f) * 0.5f * screenW);
    int y1 = (int)((1.0f - v1.position.y) * 0.5f * screenH);
    int x2 = (int)((v2.position.x + 1.0f) * 0.5f * screenW);
    int y2 = (int)((1.0f - v2.position.y) * 0.5f * screenH);

    // Bounding Box
    int minX = std::max(0, std::min({x0, x1, x2}));
    int maxX = std::min(screenW - 1, std::max({x0, x1, x2}));
    int minY = std::max(0, std::min({y0, y1, y2}));
    int maxY = std::min(screenH - 1, std::max({y0, y1, y2}));

    float det = (float)((y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2));
    if (std::abs(det) < 1e-5f) return;
    float invDet = 1.0f / det;

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            float w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * invDet;
            float w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * invDet;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float z = w0 * v0.position.z + w1 * v1.position.z + w2 * v2.position.z;
                int idx = py * screenW + px;
                if (z >= 0.0f && z < m_depthBuffer[idx]) {
                    m_depthBuffer[idx] = z;

                    uint32_t color = v0.color;
                    if (texture && texW > 0 && texH > 0) {
                        float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
                        float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;
                        int tx = (int)(u * (texW - 1)) % texW;
                        int ty = (int)(v * (texH - 1)) % texH;
                        if (tx < 0) tx += texW;
                        if (ty < 0) ty += texH;
                        color = texture[ty * texW + tx];
                    }
                    m_target->drawRGB((const int32_t*)&color, 0, 1, px, py, 1, 1, false);
                }
            }
        }
    }
}

void M3GGraphics3D::renderMesh(const std::vector<M3GVertex>& vertices, const std::vector<uint16_t>& indices, const Mat4& modelMatrix, const uint32_t* texture, int texW, int texH) {
    Mat4 mvp = Mat4::multiply(m_viewProjMatrix, modelMatrix);

    std::vector<M3GVertex> projected(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        projected[i] = vertices[i];
        projected[i].position = mvp.transformPoint(vertices[i].position);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        renderTriangle(projected[indices[i]], projected[indices[i + 1]], projected[indices[i + 2]], texture, texW, texH);
    }
}