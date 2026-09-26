#ifndef UNIVERSAL_LOADER_MICRO3D_LOADER_H
#define UNIVERSAL_LOADER_MICRO3D_LOADER_H

#include "j2me_core.h"
#include "micro3d_engine.h"
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>

namespace universal_loader {
namespace micro3d {

class J2ME_API Micro3dLoader {
public:
    // Identifies binary Micro3D format: 1 = MBAC (Figure), 2 = MTRA (ActionTable), 0 = Unknown
    static int identify(const uint8_t* data, size_t size);

    // MBAC Loading & Serialization
    static bool loadMbac(const uint8_t* data, size_t size, Micro3dFigure& outFigure);
    static bool loadMbacFromFile(const std::string& filePath, Micro3dFigure& outFigure);
    static std::vector<uint8_t> serializeMbac(const Micro3dFigure& figure, int version = 3);

    // MTRA Loading & Serialization
    static bool loadMtra(const uint8_t* data, size_t size, ActionTable& outActionTable);
    static bool loadMtraFromFile(const std::string& filePath, ActionTable& outActionTable);
    static std::vector<uint8_t> serializeMtra(const ActionTable& table, int version = 3);
};

} // namespace micro3d
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_MICRO3D_LOADER_H
