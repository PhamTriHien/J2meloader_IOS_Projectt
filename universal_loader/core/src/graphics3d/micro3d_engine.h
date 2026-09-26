#ifndef UNIVERSAL_LOADER_MICRO3D_ENGINE_H
#define UNIVERSAL_LOADER_MICRO3D_ENGINE_H

#include <vector>
#include <string>
#include <cstdint>
#include "micro3d_math.h"
#include "math3d.h"

namespace universal_loader {
namespace micro3d {

struct Bone {
    int32_t parent{-1};
    AffineTrans matrix;
    int32_t length{0}; // Number of vertices bound to this bone
};

struct Micro3DVertex {
    graphics3d::Vector3 position;
    graphics3d::Vector3 normal;
    float u{0.0f};
    float v{0.0f};
    uint32_t color{0xFFFFFFFF};
};

struct Micro3DPolygon {
    int32_t blendMode{0};
    int32_t face{0};
    int32_t doubleFace{0};
    std::vector<int32_t> indices;
};

class J2ME_API Micro3dEngine {
public:
    static void transformBones(const Bone* bones, int32_t boneCount,
                               const AffineTrans* actionMatrices, int32_t actionCount,
                               AffineTrans* outMatrices);

    static void transformVerticesAndNormals(const graphics3d::Vector3* srcVerts,
                                           graphics3d::Vector3* dstVerts,
                                           const graphics3d::Vector3* srcNorms,
                                           graphics3d::Vector3* dstNorms,
                                           const Bone* bones, int32_t boneCount,
                                           const AffineTrans* actionMatrices, int32_t actionCount);

    static void fillBuffer(graphics3d::Vector3* dst,
                           const graphics3d::Vector3* src,
                           const int32_t* indices, int32_t indexCount);

    static void multiplyMV(graphics3d::Vector3& dst, const graphics3d::Vector3& src, const AffineTrans& m);
    static void multiplyMN(graphics3d::Vector3& dst, const graphics3d::Vector3& src, const AffineTrans& m);
};

struct J2ME_API Micro3dFigure {
    int32_t numVertices{0};
    int32_t numPolyT3{0};
    int32_t numPolyT4{0};
    int32_t numBones{0};
    int32_t numTextures{1};
    int32_t numColors{0};
    std::vector<Micro3DVertex> vertices;
    std::vector<Micro3DPolygon> polygons;
    std::vector<Bone> bones;
    std::string name;

    void clear() {
        numVertices = numPolyT3 = numPolyT4 = numBones = numColors = 0;
        numTextures = 1;
        vertices.clear();
        polygons.clear();
        bones.clear();
        name.clear();
    }
};

struct J2ME_API BoneAction {
    int32_t keyframes{0};
    std::vector<AffineTrans> matrices;
};

struct J2ME_API Action {
    int32_t keyframes{0};
    int32_t numBones{0};
    std::vector<BoneAction> boneActions;
};

class J2ME_API ActionTable {
public:
    std::vector<Action> actions;

    ActionTable() = default;
    size_t getActionCount() const { return actions.size(); }
    const Action* getAction(size_t index) const {
        return index < actions.size() ? &actions[index] : nullptr;
    }
    void clear() { actions.clear(); }
};

} // namespace micro3d
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_MICRO3D_ENGINE_H
