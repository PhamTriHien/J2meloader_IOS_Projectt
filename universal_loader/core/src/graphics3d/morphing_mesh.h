#ifndef UNIVERSAL_LOADER_MORPHING_MESH_H
#define UNIVERSAL_LOADER_MORPHING_MESH_H

#include <vector>
#include <memory>
#include "../../include/j2me_core.h"
#include "m3g_engine.h"
#include "animation_track.h"

namespace universal_loader {
namespace m3g {

class J2ME_API MorphingMesh : public Mesh {
public:
    MorphingMesh(const VertexBuffer& base, const std::vector<VertexBuffer>& targets, const std::vector<Submesh>& inSubmeshes);
    ~MorphingMesh() = default;

    void setWeights(const float* inWeights, size_t count);
    void getWeights(float* outWeights, size_t maxCount) const;
    size_t getTargetCount() const;

    const VertexBuffer& getBase() const;
    const VertexBuffer& getTarget(size_t index) const;

    void morph();

    void addAnimationTrack(std::shared_ptr<AnimationTrack> track);
    void removeAnimationTrack(std::shared_ptr<AnimationTrack> track);
    size_t getAnimationTrackCount() const;
    std::shared_ptr<AnimationTrack> getAnimationTrack(size_t index) const;

    int animate(int worldTime);

    void setTranslation(float tx, float ty, float tz);
    void setOrientation(const graphics3d::Quaternion& q);
    void setScale(float sx, float sy, float sz);

    const graphics3d::Vector3& getTranslation() const { return m_translation; }
    const graphics3d::Quaternion& getOrientation() const { return m_orientation; }
    const graphics3d::Vector3& getScale() const { return m_scale; }

private:
    void updateTransform();

    VertexBuffer m_baseVertexBuffer;
    std::vector<VertexBuffer> m_targets;
    std::vector<float> m_weights;
    std::vector<std::shared_ptr<AnimationTrack>> m_tracks;

    graphics3d::Vector3 m_translation{0.0f, 0.0f, 0.0f};
    graphics3d::Quaternion m_orientation{graphics3d::Quaternion::identity()};
    graphics3d::Vector3 m_scale{1.0f, 1.0f, 1.0f};
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_MORPHING_MESH_H
