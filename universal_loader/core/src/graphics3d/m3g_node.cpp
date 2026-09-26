#include "m3g_node.h"
#include <algorithm>

namespace universal_loader {
namespace m3g {

Node::Node() = default;

void Node::setTranslation(float tx, float ty, float tz) {
    m_translation = graphics3d::Vector3(tx, ty, tz);
}

void Node::setOrientation(const graphics3d::Quaternion& q) {
    m_orientation = q.normalized();
}

void Node::setScale(float sx, float sy, float sz) {
    m_scale = graphics3d::Vector3(sx, sy, sz);
}

graphics3d::Matrix4x4 Node::getLocalTransform() const {
    return graphics3d::Matrix4x4::translation(m_translation.x, m_translation.y, m_translation.z)
         * graphics3d::Matrix4x4::fromQuaternion(m_orientation)
         * graphics3d::Matrix4x4::scaling(m_scale.x, m_scale.y, m_scale.z);
}

graphics3d::Matrix4x4 Node::getGlobalTransform() const {
    if (m_parent != nullptr) {
        return m_parent->getGlobalTransform() * getLocalTransform();
    }
    return getLocalTransform();
}

void Node::addAnimationTrack(std::shared_ptr<AnimationTrack> track) {
    if (track) {
        m_tracks.push_back(track);
    }
}

void Node::removeAnimationTrack(std::shared_ptr<AnimationTrack> track) {
    m_tracks.erase(std::remove(m_tracks.begin(), m_tracks.end(), track), m_tracks.end());
}

std::shared_ptr<AnimationTrack> Node::getAnimationTrack(size_t index) const {
    if (index < m_tracks.size()) {
        return m_tracks[index];
    }
    return nullptr;
}

int Node::animate(int worldTime) {
    if (m_tracks.empty()) {
        return 0;
    }

    graphics3d::Vector3 accumTrans{0.0f, 0.0f, 0.0f};
    float totalTransWeight = 0.0f;
    bool hasTransAnim = false;

    graphics3d::Quaternion accumQuat{0.0f, 0.0f, 0.0f, 0.0f};
    float totalOrientWeight = 0.0f;
    bool hasOrientAnim = false;

    graphics3d::Vector3 accumScale{0.0f, 0.0f, 0.0f};
    float totalScaleWeight = 0.0f;
    bool hasScaleAnim = false;

    for (const auto& track : m_tracks) {
        if (!track) continue;

        std::vector<float> sample;
        float trackWeight = 0.0f;
        if (!track->sample(worldTime, sample, trackWeight)) {
            continue;
        }

        int prop = track->getTargetProperty();
        if (prop == ANIM_TRANSLATION && sample.size() >= 3) {
            hasTransAnim = true;
            totalTransWeight += trackWeight;
            accumTrans.x += sample[0] * trackWeight;
            accumTrans.y += sample[1] * trackWeight;
            accumTrans.z += sample[2] * trackWeight;
        } else if (prop == ANIM_ORIENTATION && sample.size() >= 4) {
            hasOrientAnim = true;
            totalOrientWeight += trackWeight;
            accumQuat.x += sample[0] * trackWeight;
            accumQuat.y += sample[1] * trackWeight;
            accumQuat.z += sample[2] * trackWeight;
            accumQuat.w += sample[3] * trackWeight;
        } else if (prop == ANIM_SCALE && sample.size() >= 1) {
            hasScaleAnim = true;
            totalScaleWeight += trackWeight;
            if (sample.size() == 1) {
                accumScale.x += sample[0] * trackWeight;
                accumScale.y += sample[0] * trackWeight;
                accumScale.z += sample[0] * trackWeight;
            } else if (sample.size() >= 3) {
                accumScale.x += sample[0] * trackWeight;
                accumScale.y += sample[1] * trackWeight;
                accumScale.z += sample[2] * trackWeight;
            }
        }
    }

    if (hasTransAnim && totalTransWeight > 0.0f) {
        m_translation = accumTrans / totalTransWeight;
    }
    if (hasOrientAnim && totalOrientWeight > 0.0f) {
        m_orientation = accumQuat.normalized();
    }
    if (hasScaleAnim && totalScaleWeight > 0.0f) {
        m_scale = accumScale / totalScaleWeight;
    }

    return 0;
}

void Group::addChild(std::shared_ptr<Node> child) {
    if (child) {
        child->setParent(this);
        m_children.push_back(child);
    }
}

void Group::removeChild(std::shared_ptr<Node> child) {
    if (!child) return;
    auto it = std::remove(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        child->setParent(nullptr);
        m_children.erase(it, m_children.end());
    }
}

std::shared_ptr<Node> Group::getChild(size_t index) const {
    if (index < m_children.size()) {
        return m_children[index];
    }
    return nullptr;
}

int Group::animate(int worldTime) {
    Node::animate(worldTime);
    for (auto& child : m_children) {
        if (child) {
            child->animate(worldTime);
        }
    }
    return 0;
}

} // namespace m3g
} // namespace universal_loader
