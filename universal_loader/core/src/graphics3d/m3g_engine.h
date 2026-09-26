#ifndef UNIVERSAL_LOADER_M3G_ENGINE_H
#define UNIVERSAL_LOADER_M3G_ENGINE_H

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include "../../include/j2me_core.h"
#include "math3d.h"

namespace universal_loader {
namespace m3g {

struct Vertex3D {
    graphics3d::Vector3 position;
    graphics3d::Vector3 normal{0.0f, 1.0f, 0.0f};
    float u{0.0f};
    float v{0.0f};
    uint32_t color{0xFFFFFFFF}; // ARGB 32-bit
};

enum PrimitiveType {
    PRIMITIVE_TRIANGLES = 0,
    PRIMITIVE_TRIANGLE_STRIP = 1
};

class J2ME_API IndexBuffer {
public:
    PrimitiveType primitiveType{PRIMITIVE_TRIANGLES};
    std::vector<uint32_t> indices;

    IndexBuffer() = default;
    IndexBuffer(PrimitiveType type, const std::vector<uint32_t>& idxs)
        : primitiveType(type), indices(idxs) {}

    void getTriangles(std::vector<uint32_t>& outTriangles) const;
};

class J2ME_API VertexBuffer {
public:
    std::vector<Vertex3D> vertices;

    VertexBuffer() = default;

    void setPositions(const std::vector<graphics3d::Vector3>& pos);
    void setNormals(const std::vector<graphics3d::Vector3>& norm);
    void setTexCoords(const std::vector<std::pair<float, float>>& uvs);
    void setColors(const std::vector<uint32_t>& colors);
    size_t size() const { return vertices.size(); }
};

enum WrapMode {
    WRAP_REPEAT = 0,
    WRAP_CLAMP = 1
};

enum FilterMode {
    FILTER_NEAREST = 0,
    FILTER_LINEAR = 1
};

class J2ME_API Texture2D {
public:
    int32_t width{0};
    int32_t height{0};
    std::vector<uint32_t> pixels; // 32-bit ARGB
    WrapMode wrapS{WRAP_REPEAT};
    WrapMode wrapT{WRAP_REPEAT};
    FilterMode filter{FILTER_NEAREST};

    Texture2D() = default;
    Texture2D(int32_t w, int32_t h, const uint32_t* srcPixels = nullptr);

    uint32_t sample(float u, float v) const;
    void setPixel(int32_t x, int32_t y, uint32_t argb);
};

class Material {
public:
    uint32_t ambientColor{0x00333333};
    uint32_t diffuseColor{0x00CCCCCC};
    uint32_t specularColor{0x00000000};
    uint32_t emissiveColor{0x00000000};
    float shininess{0.0f};

    Material() = default;
};

enum BlendingMode {
    BLEND_REPLACE = 0,
    BLEND_MODULATE = 1,
    BLEND_ADD = 2,
    BLEND_ALPHA = 3
};

class CompositingMode {
public:
    BlendingMode blending{BLEND_ALPHA};
    float alphaThreshold{0.0f};
    bool depthTestEnabled{true};
    bool depthWriteEnabled{true};

    CompositingMode() = default;
};

enum CullingMode {
    CULL_NONE = 0,
    CULL_BACK = 1,
    CULL_FRONT = 2
};

enum ShadingMode {
    SHADE_FLAT = 0,
    SHADE_SMOOTH = 1
};

class PolygonMode {
public:
    CullingMode culling{CULL_BACK};
    ShadingMode shading{SHADE_SMOOTH};
    bool twoSidedLighting{false};

    PolygonMode() = default;
};

class Appearance {
public:
    Material material;
    std::shared_ptr<Texture2D> texture{nullptr};
    CompositingMode compositingMode;
    PolygonMode polygonMode;

    Appearance() = default;
};

enum LightType {
    LIGHT_AMBIENT = 0,
    LIGHT_DIRECTIONAL = 1,
    LIGHT_POINT = 2,
    LIGHT_SPOT = 3
};

class Light {
public:
    LightType type{LIGHT_DIRECTIONAL};
    uint32_t color{0x00FFFFFF};
    float intensity{1.0f};
    graphics3d::Vector3 position{0.0f, 0.0f, 0.0f};
    graphics3d::Vector3 direction{0.0f, 0.0f, -1.0f};
    float spotAngle{45.0f * graphics3d::DEG_TO_RAD};
    float spotExponent{0.0f};
    float constantAttenuation{1.0f};
    float linearAttenuation{0.0f};
    float quadraticAttenuation{0.0f};

    Light() = default;
};

enum CameraProjection {
    PROJECTION_PERSPECTIVE = 0,
    PROJECTION_PARALLEL = 1
};

class J2ME_API Camera {
public:
    CameraProjection projection{PROJECTION_PERSPECTIVE};
    float fovY{60.0f * graphics3d::DEG_TO_RAD};
    float aspectRatio{1.0f};
    float nearDistance{0.1f};
    float farDistance{1000.0f};

    float parallelWidth{2.0f};
    float parallelHeight{2.0f};

    graphics3d::Vector3 eye{0.0f, 0.0f, 5.0f};
    graphics3d::Vector3 target{0.0f, 0.0f, 0.0f};
    graphics3d::Vector3 up{0.0f, 1.0f, 0.0f};

    Camera() = default;

    graphics3d::Matrix4x4 getProjectionMatrix() const;
    graphics3d::Matrix4x4 getViewMatrix() const;
};

struct Submesh {
    IndexBuffer indexBuffer;
    Appearance appearance;
};

class J2ME_API Mesh {
public:
    VertexBuffer vertexBuffer;
    std::vector<Submesh> submeshes;
    graphics3d::Matrix4x4 transform{graphics3d::Matrix4x4::identity()};

    Mesh() = default;
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_M3G_ENGINE_H
