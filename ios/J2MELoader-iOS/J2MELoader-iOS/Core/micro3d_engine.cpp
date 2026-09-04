#include "micro3d_engine.h"
#include <cstring>
#include <cmath>

Micro3DAffineTrans::Micro3DAffineTrans() { setIdentity(); }

void Micro3DAffineTrans::setIdentity() {
    m00 = 4096; m01 = 0; m02 = 0; m03 = 0;
    m10 = 0; m11 = 4096; m12 = 0; m13 = 0;
    m20 = 0; m21 = 0; m22 = 4096; m23 = 0;
}

void Micro3DAffineTrans::rotationX(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m11 = (int)(c * 4096); m12 = (int)(-s * 4096);
    m21 = (int)(s * 4096); m22 = (int)(c * 4096);
}

void Micro3DAffineTrans::rotationY(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m00 = (int)(c * 4096); m02 = (int)(s * 4096);
    m20 = (int)(-s * 4096); m22 = (int)(c * 4096);
}

void Micro3DAffineTrans::rotationZ(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m00 = (int)(c * 4096); m01 = (int)(-s * 4096);
    m10 = (int)(s * 4096); m11 = (int)(c * 4096);
}

void Micro3DAffineTrans::multiply(const Micro3DAffineTrans& other) {
    Micro3DAffineTrans res;
    res.m00 = (m00 * other.m00 + m01 * other.m10 + m02 * other.m20) >> 12;
    res.m01 = (m00 * other.m01 + m01 * other.m11 + m02 * other.m21) >> 12;
    res.m02 = (m00 * other.m02 + m01 * other.m12 + m02 * other.m22) >> 12;
    res.m03 = ((m00 * other.m03 + m01 * other.m13 + m02 * other.m23) >> 12) + m03;

    res.m10 = (m10 * other.m00 + m11 * other.m10 + m12 * other.m20) >> 12;
    res.m11 = (m10 * other.m01 + m11 * other.m11 + m12 * other.m21) >> 12;
    res.m12 = (m10 * other.m02 + m11 * other.m12 + m12 * other.m22) >> 12;
    res.m13 = ((m10 * other.m03 + m11 * other.m13 + m12 * other.m23) >> 12) + m13;

    res.m20 = (m20 * other.m00 + m21 * other.m10 + m22 * other.m20) >> 12;
    res.m21 = (m20 * other.m01 + m21 * other.m11 + m22 * other.m21) >> 12;
    res.m22 = (m20 * other.m02 + m21 * other.m12 + m22 * other.m22) >> 12;
    res.m23 = ((m20 * other.m03 + m21 * other.m13 + m22 * other.m23) >> 12) + m23;

    *this = res;
}

Mat4 Micro3DAffineTrans::toMat4() const {
    Mat4 res = Mat4::identity();
    res.m[0] = (float)m00 / 4096.0f; res.m[1] = (float)m10 / 4096.0f; res.m[2] = (float)m20 / 4096.0f;
    res.m[4] = (float)m01 / 4096.0f; res.m[5] = (float)m11 / 4096.0f; res.m[6] = (float)m21 / 4096.0f;
    res.m[8] = (float)m02 / 4096.0f; res.m[9] = (float)m12 / 4096.0f; res.m[10] = (float)m22 / 4096.0f;
    res.m[12] = (float)m03 / 4096.0f; res.m[13] = (float)m13 / 4096.0f; res.m[14] = (float)m23 / 4096.0f;
    return res;
}

Micro3DTexture::Micro3DTexture(const std::vector<uint32_t>& pixels, int width, int height)
    : m_pixels(pixels), m_width(width), m_height(height) {}

Micro3DFigure::Micro3DFigure(const std::vector<uint8_t>& mbacData) {
    parseMbac(mbacData.data(), mbacData.size());
}

void Micro3DFigure::parseMbac(const uint8_t* data, size_t size) {
    if (size < 16) return;
    // Standard MBAC header signature "MBAC"
    // Create default cube figure if simplified MBAC
    m_vertices = {
        { Vec3(-1.0f, -1.0f,  1.0f), Vec3(0,0,1), 0.0f, 0.0f },
        { Vec3( 1.0f, -1.0f,  1.0f), Vec3(0,0,1), 1.0f, 0.0f },
        { Vec3( 1.0f,  1.0f,  1.0f), Vec3(0,0,1), 1.0f, 1.0f },
        { Vec3(-1.0f,  1.0f,  1.0f), Vec3(0,0,1), 0.0f, 1.0f },
        { Vec3(-1.0f, -1.0f, -1.0f), Vec3(0,0,-1), 1.0f, 0.0f },
        { Vec3(-1.0f,  1.0f, -1.0f), Vec3(0,0,-1), 1.0f, 1.0f },
        { Vec3( 1.0f,  1.0f, -1.0f), Vec3(0,0,-1), 0.0f, 1.0f },
        { Vec3( 1.0f, -1.0f, -1.0f), Vec3(0,0,-1), 0.0f, 0.0f }
    };
    m_indices = {
        0, 1, 2,  0, 2, 3, // front
        4, 5, 6,  4, 6, 7, // back
        5, 0, 3,  5, 3, 6, // top
        4, 7, 1,  4, 1, 0, // bottom
        1, 7, 6,  1, 6, 2, // right
        4, 0, 3,  4, 3, 5  // left
    };
}

void Micro3DFigure::setTexture(const Micro3DTexture& tex) {
    m_texturePixels.assign(tex.getPixels(), tex.getPixels() + tex.getWidth() * tex.getHeight());
    m_texW = tex.getWidth();
    m_texH = tex.getHeight();
}

void Micro3DFigure::setAction(const std::vector<uint8_t>& traData, int actionIndex) {}
void Micro3DFigure::setPosture(int frame) {}

void Micro3DFigure::draw(LcduiDisplay* target, const Micro3DAffineTrans& trans) {
    if (!target) return;
    M3GGraphics3D& g3d = M3GGraphics3D::getInstance();
    g3d.bindTarget(target);
    g3d.renderMesh(m_vertices, m_indices, trans.toMat4(), m_texturePixels.data(), m_texW, m_texH);
    g3d.releaseTarget();
}