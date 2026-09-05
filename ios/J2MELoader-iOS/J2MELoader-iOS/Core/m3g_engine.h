#ifndef M3G_ENGINE_H
#define M3G_ENGINE_H

#include "lcdui_display.h"
#include <vector>
#include <memory>
#include <cmath>

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct Mat4 {
    float m[16];
    Mat4();
    static Mat4 identity();
    static Mat4 perspective(float fovy, float aspect, float nearP, float farP);
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
    static Mat4 multiply(const Mat4& a, const Mat4& b);
    Vec3 transformPoint(const Vec3& v) const;
};

struct M3GVertex {
    Vec3 position;
    Vec3 normal;
    float u = 0.0f, v = 0.0f;
    uint32_t color = 0xFFFFFFFF;
};

class M3GCamera {
public:
    M3GCamera();
    void setPerspective(float fovy, float aspect, float nearP, float farP);
    void setParallel(float height, float aspect, float nearP, float farP);
    Mat4 getProjectionMatrix() const { return m_projMatrix; }

private:
    Mat4 m_projMatrix;
};

class M3GLight {
public:
    enum Mode { AMBIENT, DIRECTIONAL, POINT, SPOT };
    M3GLight(Mode mode = DIRECTIONAL);
    void setColor(uint32_t rgb);
    void setIntensity(float intensity);
    uint32_t getColor() const { return m_color; }
    float getIntensity() const { return m_intensity; }
    Mode getMode() const { return m_mode; }

private:
    Mode m_mode;
    uint32_t m_color = 0xFFFFFFFF;
    float m_intensity = 1.0f;
};

class M3GLoader {
public:
    // Parse real .m3g file (JSR-184 v1.0). Returns true if at least header valid.
    // Extracts first texture PNG/JPEG + first vertex mesh for immediate render.
    static bool parse(const uint8_t* data, size_t size,
                      std::vector<M3GVertex>& outVerts,
                      std::vector<uint16_t>& outIndices,
                      std::vector<uint32_t>& outTex, int& texW, int& texH,
                      uint32_t& bgColor);
};

class M3GGraphics3D {
public:
    static M3GGraphics3D& getInstance();

    void bindTarget(LcduiDisplay* target);
    void releaseTarget();

    void clear(uint32_t color = 0xFF000000);
    void setCamera(const M3GCamera& camera, const Mat4& viewMatrix);
    void addLight(const M3GLight& light, const Mat4& transform);
    void resetLights();

    void renderTriangle(const M3GVertex& v0, const M3GVertex& v1, const M3GVertex& v2, const uint32_t* texture, int texW, int texH);
    void renderMesh(const std::vector<M3GVertex>& vertices, const std::vector<uint16_t>& indices, const Mat4& modelMatrix, const uint32_t* texture = nullptr, int texW = 0, int texH = 0);
    // Render parsed M3G world (real mesh + texture if available, else fallback)
    void renderWorld(const std::vector<M3GVertex>& verts, const std::vector<uint16_t>& idx,
                     const uint32_t* tex, int tw, int th, uint32_t bg);

private:
    M3GGraphics3D();
    LcduiDisplay* m_target = nullptr;
    Mat4 m_viewProjMatrix;
    std::vector<float> m_depthBuffer;
    std::vector<M3GLight> m_lights;
    std::vector<Mat4> m_lightTransforms;
};

#endif // M3G_ENGINE_H