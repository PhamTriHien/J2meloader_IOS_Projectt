#ifndef UNIVERSAL_LOADER_M3G_NODE_H
#define UNIVERSAL_LOADER_M3G_NODE_H

#include <vector>
#include <memory>
#include "../../include/j2me_core.h"
#include "math3d.h"
#include "animation_track.h"

namespace universal_loader {
namespace m3g {

class J2ME_API Node {
public:
    Node();
    virtual ~Node() = default;

    void setTranslation(float tx, float ty, float tz);
    const graphics3d::Vector3& getTranslation() const { return m_translation; }

    void setOrientation(const graphics3d::Quaternion& q);
    const graphics3d::Quaternion& getOrientation() const { return m_orientation; }

    void setScale(float sx, float sy, float sz);
    const graphics3d::Vector3& getScale() const { return m_scale; }

    graphics3d::Matrix4x4 getLocalTransform() const;
    graphics3d::Matrix4x4 getGlobalTransform() const;

    void setParent(Node* parent) { m_parent = parent; }
    Node* getParent() const { return m_parent; }

    void addAnimationTrack(std::shared_ptr<AnimationTrack> track);
    void removeAnimationTrack(std::shared_ptr<AnimationTrack> track);
    size_t getAnimationTrackCount() const { return m_tracks.size(); }
    std::shared_ptr<AnimationTrack> getAnimationTrack(size_t index) const;

    virtual int animate(int worldTime);

protected:
    Node* m_parent{nullptr};
    graphics3d::Vector3 m_translation{0.0f, 0.0f, 0.0f};
    graphics3d::Quaternion m_orientation{graphics3d::Quaternion::identity()};
    graphics3d::Vector3 m_scale{1.0f, 1.0f, 1.0f};
    std::vector<std::shared_ptr<AnimationTrack>> m_tracks;
};

class J2ME_API Group : public Node {
public:
    Group() = default;
    ~Group() override = default;

    void addChild(std::shared_ptr<Node> child);
    void removeChild(std::shared_ptr<Node> child);
    size_t getChildCount() const { return m_children.size(); }
    std::shared_ptr<Node> getChild(size_t index) const;

    int animate(int worldTime) override;

private:
    std::vector<std::shared_ptr<Node>> m_children;
};

class Camera;

class J2ME_API World : public Group {
public:
    World() = default;
    ~World() override = default;

    void setActiveCamera(std::shared_ptr<Camera> cam) { m_activeCamera = cam; }
    std::shared_ptr<Camera> getActiveCamera() const { return m_activeCamera; }

    void setBackground(uint32_t argb) { m_backgroundColor = argb; }
    uint32_t getBackground() const { return m_backgroundColor; }

private:
    std::shared_ptr<Camera> m_activeCamera{nullptr};
    uint32_t m_backgroundColor{0xFF000000};
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_M3G_NODE_H
