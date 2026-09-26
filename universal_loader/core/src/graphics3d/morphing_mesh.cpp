#include "morphing_mesh.h"
#include <algorithm>
#include <cmath>

namespace universal_loader {
namespace m3g {

MorphingMesh::MorphingMesh(const VertexBuffer& base,
                           const std::vector<VertexBuffer>& targets,
                           const std::vector<Submesh>& inSubmeshes)
    : m_baseVertexBuffer(base),
      m_targets(targets) {
    m_weights.resize(targets.size(), 0.0f);
    submeshes = inSubmeshes;
    vertexBuffer = base;
    updateTransform();
}

void MorphingMesh::setWeights(const float* inWeights, size_t count) {
    for (size_t i = 0; i < m_weights.size(); ++i) {
        m_weights[i] = (i < count && inWeights != nullptr) ? inWeights[i] : 0.0f;
    }
}

void MorphingMesh::getWeights(float* outWeights, size_t maxCount) const {
    if (outWeights == nullptr) {
        return;
    }
    size_t count = std::min(m_weights.size(), maxCount);
    for (size_t i = 0; i < count; ++i) {
        outWeights[i] = m_weights[i];
    }
}

size_t MorphingMesh::getTargetCount() const {
    return m_targets.size();
}

const VertexBuffer& MorphingMesh::getBase() const {
    return m_baseVertexBuffer;
}

const VertexBuffer& MorphingMesh::getTarget(size_t index) const {
    static const VertexBuffer s_empty;
    if (index < m_targets.size()) {
        return m_targets[index];
    }
    return s_empty;
}

void MorphingMesh::morph() {
    if (m_baseVertexBuffer.vertices.empty()) {
        return;
    }

    if (vertexBuffer.vertices.size() != m_baseVertexBuffer.vertices.size()) {
        vertexBuffer.vertices.resize(m_baseVertexBuffer.vertices.size());
    }

    float sumWeights = 0.0f;
    for (float w : m_weights) {
        sumWeights += w;
    }
    float baseWeight = 1.0f - sumWeights;

    size_t numVerts = m_baseVertexBuffer.vertices.size();
    for (size_t i = 0; i < numVerts; ++i) {
        const auto& baseV = m_baseVertexBuffer.vertices[i];
        graphics3d::Vector3 blendedPos = baseV.position * baseWeight;
        graphics3d::Vector3 blendedNorm = baseV.normal * baseWeight;

        for (size_t t = 0; t < m_targets.size(); ++t) {
            float wt = m_weights[t];
            if (wt != 0.0f && i < m_targets[t].vertices.size()) {
                const auto& targetV = m_targets[t].vertices[i];
                blendedPos += targetV.position * wt;
                blendedNorm += targetV.normal * wt;
            }
        }

        vertexBuffer.vertices[i].position = blendedPos;
        vertexBuffer.vertices[i].normal = blendedNorm.normalized();
        vertexBuffer.vertices[i].u = baseV.u;
        vertexBuffer.vertices[i].v = baseV.v;
        vertexBuffer.vertices[i].color = baseV.color;
    }
}

void MorphingMesh::addAnimationTrack(std::shared_ptr<AnimationTrack> track) {
    if (track) {
        m_tracks.push_back(track);
    }
}

void MorphingMesh::removeAnimationTrack(std::shared_ptr<AnimationTrack> track) {
    m_tracks.erase(std::remove(m_tracks.begin(), m_tracks.end(), track), m_tracks.end());
}

size_t MorphingMesh::getAnimationTrackCount() const {
    return m_tracks.size();
}

std::shared_ptr<AnimationTrack> MorphingMesh::getAnimationTrack(size_t index) const {
    if (index < m_tracks.size()) {
        return m_tracks[index];
    }
    return nullptr;
}

void MorphingMesh::setTranslation(float tx, float ty, float tz) {
    m_translation = graphics3d::Vector3(tx, ty, tz);
    updateTransform();
}

void MorphingMesh::setOrientation(const graphics3d::Quaternion& q) {
    m_orientation = q.normalized();
    updateTransform();
}

void MorphingMesh::setScale(float sx, float sy, float sz) {
    m_scale = graphics3d::Vector3(sx, sy, sz);
    updateTransform();
}

void MorphingMesh::updateTransform() {
    transform = graphics3d::Matrix4x4::translation(m_translation.x, m_translation.y, m_translation.z)
              * graphics3d::Matrix4x4::fromQuaternion(m_orientation)
              * graphics3d::Matrix4x4::scaling(m_scale.x, m_scale.y, m_scale.z);
}

int MorphingMesh::animate(int worldTime) {
    if (m_tracks.empty()) {
        return 0;
    }

    // 1. Morph Weights Accumulation
    std::vector<float> accumWeights(m_targets.size(), 0.0f);
    float totalMorphWeight = 0.0f;
    bool hasMorphAnim = false;

    // 2. Translation Accumulation
    graphics3d::Vector3 accumTrans{0.0f, 0.0f, 0.0f};
    float totalTransWeight = 0.0f;
    bool hasTransAnim = false;

    // 3. Orientation Accumulation
    graphics3d::Quaternion accumQuat{0.0f, 0.0f, 0.0f, 0.0f};
    float totalOrientWeight = 0.0f;
    bool hasOrientAnim = false;

    // 4. Scale Accumulation
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
        if (prop == ANIM_MORPH_WEIGHTS && !m_targets.empty()) {
            hasMorphAnim = true;
            totalMorphWeight += trackWeight;
            for (size_t t = 0; t < m_targets.size(); ++t) {
                if (t < sample.size()) {
                    accumWeights[t] += sample[t] * trackWeight;
                }
            }
        } else if (prop == ANIM_TRANSLATION && sample.size() >= 3) {
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

    if (hasMorphAnim && totalMorphWeight > 0.0f) {
        for (size_t t = 0; t < accumWeights.size(); ++t) {
            accumWeights[t] /= totalMorphWeight;
        }
        setWeights(accumWeights.data(), accumWeights.size());
        morph();
    }

    bool transformChanged = false;
    if (hasTransAnim && totalTransWeight > 0.0f) {
        m_translation = accumTrans / totalTransWeight;
        transformChanged = true;
    }

    if (hasOrientAnim && totalOrientWeight > 0.0f) {
        m_orientation = accumQuat.normalized();
        transformChanged = true;
    }

    if (hasScaleAnim && totalScaleWeight > 0.0f) {
        m_scale = accumScale / totalScaleWeight;
        transformChanged = true;
    }

    if (transformChanged) {
        updateTransform();
    }

    return 0;
}

} // namespace m3g
} // namespace universal_loader
