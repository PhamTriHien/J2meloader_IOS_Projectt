#include "micro3d_engine.h"

namespace universal_loader {
namespace micro3d {

void Micro3dEngine::transformBones(const Bone* bones, int32_t boneCount,
                                   const AffineTrans* actionMatrices, int32_t actionCount,
                                   AffineTrans* outMatrices) {
    if (bones == nullptr || outMatrices == nullptr || boneCount <= 0) {
        return;
    }

    for (int32_t i = 0; i < boneCount; ++i) {
        const Bone& bone = bones[i];
        int32_t parent = bone.parent;
        AffineTrans& matrix = outMatrices[i];

        if (parent == -1 || parent >= i) {
            matrix = bone.matrix;
        } else {
            AffineTrans::multiplyMM(matrix, outMatrices[parent], bone.matrix);
        }

        if (actionMatrices != nullptr && i < actionCount) {
            AffineTrans::multiplyMM(matrix, matrix, actionMatrices[i]);
        }
    }
}

void Micro3dEngine::multiplyMV(graphics3d::Vector3& dst, const graphics3d::Vector3& src, const AffineTrans& m) {
    float x = src.x;
    float y = src.y;
    float z = src.z;
    dst.x = (x * m.m00 + y * m.m01 + z * m.m02) * TO_FLOAT + m.m03 * TO_FLOAT;
    dst.y = (x * m.m10 + y * m.m11 + z * m.m12) * TO_FLOAT + m.m13 * TO_FLOAT;
    dst.z = (x * m.m20 + y * m.m21 + z * m.m22) * TO_FLOAT + m.m23 * TO_FLOAT;
}

void Micro3dEngine::multiplyMN(graphics3d::Vector3& dst, const graphics3d::Vector3& src, const AffineTrans& m) {
    float x = src.x;
    float y = src.y;
    float z = src.z;
    dst.x = (x * m.m00 + y * m.m01 + z * m.m02) * TO_FLOAT;
    dst.y = (x * m.m10 + y * m.m11 + z * m.m12) * TO_FLOAT;
    dst.z = (x * m.m20 + y * m.m21 + z * m.m22) * TO_FLOAT;
}

void Micro3dEngine::transformVerticesAndNormals(const graphics3d::Vector3* srcVerts,
                                               graphics3d::Vector3* dstVerts,
                                               const graphics3d::Vector3* srcNorms,
                                               graphics3d::Vector3* dstNorms,
                                               const Bone* bones, int32_t boneCount,
                                               const AffineTrans* actionMatrices, int32_t actionCount) {
    if (srcVerts == nullptr || dstVerts == nullptr || bones == nullptr || boneCount <= 0) {
        return;
    }

    std::vector<AffineTrans> boneTransforms(boneCount);
    transformBones(bones, boneCount, actionMatrices, actionCount, boneTransforms.data());

    const graphics3d::Vector3* currentSrcVert = srcVerts;
    graphics3d::Vector3* currentDstVert = dstVerts;
    const graphics3d::Vector3* currentSrcNorm = srcNorms;
    graphics3d::Vector3* currentDstNorm = dstNorms;

    for (int32_t i = 0; i < boneCount; ++i) {
        const AffineTrans& matrix = boneTransforms[i];
        int32_t count = bones[i].length;

        for (int32_t j = 0; j < count; ++j) {
            multiplyMV(*currentDstVert++, *currentSrcVert++, matrix);

            if (currentSrcNorm != nullptr && currentDstNorm != nullptr) {
                multiplyMN(*currentDstNorm++, *currentSrcNorm++, matrix);
            }
        }
    }
}

void Micro3dEngine::fillBuffer(graphics3d::Vector3* dst,
                               const graphics3d::Vector3* src,
                               const int32_t* indices, int32_t indexCount) {
    if (dst == nullptr || src == nullptr || indices == nullptr || indexCount <= 0) {
        return;
    }

    for (int32_t i = 0; i < indexCount; ++i) {
        dst[i] = src[indices[i]];
    }
}

} // namespace micro3d
} // namespace universal_loader
