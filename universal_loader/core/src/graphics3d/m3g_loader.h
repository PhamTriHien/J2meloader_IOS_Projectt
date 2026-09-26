#ifndef UNIVERSAL_LOADER_M3G_LOADER_H
#define UNIVERSAL_LOADER_M3G_LOADER_H

#include "j2me_core.h"
#include "m3g_engine.h"
#include "m3g_node.h"
#include "keyframe_sequence.h"
#include "animation_controller.h"
#include "animation_track.h"
#include "morphing_mesh.h"
#include "skinned_mesh.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace universal_loader {
namespace m3g {

enum FileType {
    M3G_FILE_UNKNOWN = 0,
    M3G_FILE_M3G     = 1,
    M3G_FILE_PNG     = 2,
    M3G_FILE_JPEG    = 3
};

// Object types defined by JSR-184 Specification
enum M3gObjectType : uint8_t {
    M3G_OBJ_HEADER               = 0,
    M3G_OBJ_ANIMATION_CONTROLLER = 1,
    M3G_OBJ_ANIMATION_TRACK      = 2,
    M3G_OBJ_APPEARANCE           = 3,
    M3G_OBJ_BACKGROUND           = 4,
    M3G_OBJ_CAMERA               = 5,
    M3G_OBJ_COMPOSITING_MODE     = 6,
    M3G_OBJ_FOG                  = 7,
    M3G_OBJ_POLYGON_MODE         = 8,
    M3G_OBJ_GROUP                = 9,
    M3G_OBJ_IMAGE2D              = 10,
    M3G_OBJ_TRIANGLE_STRIP_ARRAY = 11,
    M3G_OBJ_LIGHT                = 12,
    M3G_OBJ_MATERIAL             = 13,
    M3G_OBJ_MESH                 = 14,
    M3G_OBJ_MORPHING_MESH        = 15,
    M3G_OBJ_SKINNED_MESH         = 16,
    M3G_OBJ_TEXTURE2D            = 17,
    M3G_OBJ_SPRITE3D             = 18,
    M3G_OBJ_KEYFRAME_SEQUENCE    = 19,
    M3G_OBJ_VERTEX_ARRAY         = 20,
    M3G_OBJ_VERTEX_BUFFER        = 21,
    M3G_OBJ_WORLD                = 22
};

struct J2ME_API M3gLoadedScene {
    std::shared_ptr<World> world;
    std::vector<std::shared_ptr<Node>> rootNodes;
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Camera>> cameras;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<AnimationTrack>> animationTracks;
    std::vector<std::shared_ptr<KeyframeSequence>> keyframeSequences;
    std::string authoringTool;
    int32_t versionMajor{1};
    int32_t versionMinor{0};
    uint32_t totalFileSize{0};

    void clear() {
        world.reset();
        rootNodes.clear();
        meshes.clear();
        cameras.clear();
        materials.clear();
        animationTracks.clear();
        keyframeSequences.clear();
        authoringTool.clear();
        versionMajor = 1;
        versionMinor = 0;
        totalFileSize = 0;
    }
};

class J2ME_API M3gLoader {
public:
    static FileType identify(const uint8_t* data, size_t size);

    static bool load(const uint8_t* data, size_t size, M3gLoadedScene& outScene);
    static bool loadFromFile(const std::string& filePath, M3gLoadedScene& outScene);

    // Serialization helper to generate valid binary M3G streams (Header + Data + Adler32 checksums)
    static std::vector<uint8_t> serializeScene(const M3gLoadedScene& scene);

    static uint32_t computeAdler32(const uint8_t* data, size_t len);
};

} // namespace m3g
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_M3G_LOADER_H
