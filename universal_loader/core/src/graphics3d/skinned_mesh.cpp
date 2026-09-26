#include "skinned_mesh.h"
#include <algorithm>
#include <cmath>

namespace universal_loader {
namespace m3g {

SkinnedMesh::SkinnedMesh(const VertexBuffer& base,
                         const std::vector<Submesh>& inSubmeshes,
                         std::shared_ptr<Group> skeleton)
    : m_baseVertexBuffer(base),
      m_skeleton(std::move(skeleton)) {
    submeshes = inSubmeshes;
    vertexBuffer = base;
    m_vertexWeights.resize(base.vertices.size());
}

int SkinnedMesh::findOrAddBone(const std::shared_ptr<Node>& bone) {
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].node == bone) {
            return static_cast<int>(i);
        }
    }

    BoneRecord rec;
    rec.node = bone;

    graphics3d::Matrix4x4 globalBind = bone->getGlobalTransform();
    graphics3d::Matrix4x4 invBind;
    if (globalBind.invert(invBind)) {
        rec.inverseBindPose = invBind;
    } else {
        rec.inverseBindPose = graphics3d::Matrix4x4::identity();
    }

    m_bones.push_back(rec);
    return static_cast<int>(m_bones.size() - 1);
}

void SkinnedMesh::addTransform(std::shared_ptr<Node> bone, int weight, int firstVertex, int numVertices) {
    if (!bone || weight <= 0 || numVertices <= 0) {
        return;
    }

    int bIdx = findOrAddBone(bone);
    size_t endVertex = static_cast<size_t>(firstVertex + numVertices);
    if (endVertex > m_vertexWeights.size()) {
        endVertex = m_vertexWeights.size();
    }

    for (size_t v = static_cast<size_t>(firstVertex); v < endVertex; ++v) {
        bool found = false;
        for (auto& vw : m_vertexWeights[v]) {
            if (vw.boneIndex == bIdx) {
                vw.weight += static_cast<float>(weight);
                found = true;
                break;
            }
        }
        if (!found) {
            m_vertexWeights[v].push_back({bIdx, static_cast<float>(weight)});
        }
    }
}

std::shared_ptr<Node> SkinnedMesh::getBone(size_t index) const {
    if (index < m_bones.size()) {
        return m_bones[index].node;
    }
    return nullptr;
}

void SkinnedMesh::skin() {
    if (m_baseVertexBuffer.vertices.empty()) {
        return;
    }

    if (vertexBuffer.vertices.size() != m_baseVertexBuffer.vertices.size()) {
        vertexBuffer.vertices.resize(m_baseVertexBuffer.vertices.size());
    }

    // 1. Compute skinning matrix for each bone: S_k = M_k * B_k^-1
    std::vector<graphics3d::Matrix4x4> skinningMatrices(m_bones.size());
    for (size_t k = 0; k < m_bones.size(); ++k) {
        if (m_bones[k].node) {
            skinningMatrices[k] = m_bones[k].node->getGlobalTransform() * m_bones[k].inverseBindPose;
        } else {
            skinningMatrices[k] = graphics3d::Matrix4x4::identity();
        }
    }

    // 2. Linear Blend Skinning for each vertex
    size_t numVerts = m_baseVertexBuffer.vertices.size();
    for (size_t i = 0; i < numVerts; ++i) {
        const auto& baseV = m_baseVertexBuffer.vertices[i];
        const auto& weights = m_vertexWeights[i];

        if (weights.empty()) {
            vertexBuffer.vertices[i] = baseV;
            continue;
        }

        float totalW = 0.0f;
        for (const auto& vw : weights) {
            totalW += vw.weight;
        }

        if (totalW <= 0.0f) {
            vertexBuffer.vertices[i] = baseV;
            continue;
        }

        graphics3d::Vector3 blendedPos{0.0f, 0.0f, 0.0f};
        graphics3d::Vector3 blendedNorm{0.0f, 0.0f, 0.0f};

        for (const auto& vw : weights) {
            float normW = vw.weight / totalW;
            const auto& mat = skinningMatrices[vw.boneIndex];

            graphics3d::Vector3 pTrans = mat.transformPoint(baseV.position);
            graphics3d::Vector3 nTrans = mat.transformVector(baseV.normal);

            blendedPos += pTrans * normW;
            blendedNorm += nTrans * normW;
        }

        vertexBuffer.vertices[i].position = blendedPos;
        vertexBuffer.vertices[i].normal = blendedNorm.normalized();
        vertexBuffer.vertices[i].u = baseV.u;
        vertexBuffer.vertices[i].v = baseV.v;
        vertexBuffer.vertices[i].color = baseV.color;
    }
}

int SkinnedMesh::animate(int worldTime) {
    if (m_skeleton) {
        m_skeleton->animate(worldTime);
    }
    skin();
    return 0;
}

} // namespace m3g
} // namespace universal_loader
