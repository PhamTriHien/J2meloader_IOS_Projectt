#ifndef UNIVERSAL_LOADER_RASTERIZER3D_H
#define UNIVERSAL_LOADER_RASTERIZER3D_H

#include <vector>
#include <cstdint>
#include "../../include/j2me_core.h"
#include "math3d.h"
#include "m3g_engine.h"
#include "micro3d_math.h"
#include "micro3d_engine.h"

namespace universal_loader {
namespace graphics3d {

struct Viewport {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
};

struct Rect2D {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
};

struct RasterVertex {
    Vector4 clipPos;      // Clip space x, y, z, w
    Vector3 screenPos;    // Screen space x, y, z (z in [0, 1])
    Vector3 normal;       // World space or camera space normal
    float u{0.0f};
    float v{0.0f};
    uint32_t color{0xFFFFFFFF};
    float invW{1.0f};     // 1.0f / clipPos.w
};

class J2ME_API Rasterizer3D {
public:
    Rasterizer3D();
    ~Rasterizer3D() = default;

    void setTarget(uint32_t* colorBuffer, int32_t width, int32_t height);
    void setViewport(int32_t x, int32_t y, int32_t width, int32_t height);
    void setClipRect(int32_t x, int32_t y, int32_t width, int32_t height);

    void clear(uint32_t argbColor = 0x00000000, float depth = 1.0f);
    void clearDepth(float depth = 1.0f);

    void renderMesh(const m3g::Mesh& mesh, const m3g::Camera& camera,
                    const std::vector<m3g::Light>& lights);

    void renderMicro3D(const micro3d::Micro3DVertex* vertices, int32_t vertexCount,
                       const int32_t* indices, int32_t indexCount,
                       const micro3d::FigureLayout& layout,
                       const std::vector<m3g::Light>& lights,
                       const m3g::Texture2D* texture);

    void drawTriangle(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                      const m3g::Appearance& app, const std::vector<m3g::Light>& lights);

    float getDepthAt(int32_t x, int32_t y) const;
    uint32_t getColorAt(int32_t x, int32_t y) const;

private:
    uint32_t* m_colorBuffer{nullptr};
    int32_t m_targetWidth{0};
    int32_t m_targetHeight{0};

    std::vector<float> m_depthBuffer;

    Viewport m_viewport;
    Rect2D m_clipRect;

    uint32_t calculateLighting(const Vector3& normal, const Vector3& worldPos,
                               uint32_t baseColor, const m3g::Material& mat,
                               const std::vector<m3g::Light>& lights) const;

    void clipAndDrawTriangle(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                             const m3g::Appearance& app, const std::vector<m3g::Light>& lights);
};

} // namespace graphics3d
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_RASTERIZER3D_H
