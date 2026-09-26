#ifndef UNIVERSAL_LOADER_SKINNED_MESH_H
#define UNIVERSAL_LOADER_SKINNED_MESH_H

#include <vector>
#include <memory>
#include "../../include/j2me_core.h"
#include "m3g_engine.h"
#include "m3g_node.h"

namespace universal_loader {
namespace m3g {

struct BoneRecord {
    std::shared_ptr<Node> node;
    graphics3d::Matrix4x4 inverseBindPose{graphics3d::Matrix4x4::identity()};
};

struct VertexWeight {
    int boneIndex{0};
    float weight{0.0f};
};

class J2ME_API SkinnedMesh : public Mesh {
public:
    SkinnedMesh(const VertexBuffer& base, const std::vector<Submesh>& inSubmeshes, std::shared_ptr<Group> skeleton);
    ~SkinnedMesh() = default;

    void addTransform(std::shared_ptr<Node> bone, int weight, int firstVertex, int numVertices);

    std::shared_ptr<Group> getSkeleton() const { return m_skeleton; }
    size_t getBoneCount() const { return m_bones.size(); }
    std::shared_ptr<Node> getBone(size_t index) const;

    void skin();
    int animate(int worldTime);

    const VertexBuffer& getBase() const { return m_baseVertexBuffer; }

private:
    int findOrAddBone(const std::shared_ptr<Node>& bone);

    VertexBuffer m_baseVertexBuffer;
    std::shared_ptr<Group> m_skeleton;
    std::vector<BoneRecord> m_bones;
    std::vector<std::vector<VertexWeight>> m_vertexWeights;
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SKINNED_MESH_H
