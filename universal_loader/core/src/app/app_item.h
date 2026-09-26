#ifndef UNIVERSAL_LOADER_APP_ITEM_H
#define UNIVERSAL_LOADER_APP_ITEM_H

#include <string>
#include <cstdint>
#include "../../include/j2me_core.h"

namespace universal_loader {
namespace app {

struct J2ME_API AppItem {
    int id{0};
    std::string path;              // Subdirectory name within apps folder, e.g. "DragonBoy"
    std::string title;             // MIDlet-Name, e.g. "DragonBoy"
    std::string author;            // MIDlet-Vendor, e.g. "Team"
    std::string version;           // MIDlet-Version, e.g. "2.4.7"
    std::string imagePath;         // Relative path to icon, e.g. "icon.png"
    int64_t installedTimestamp{0}; // Milliseconds since Unix epoch
    int64_t lastPlayedTimestamp{0};// Milliseconds since Unix epoch
    int playCount{0};

    std::string serializeJson() const;
    bool deserializeJson(const std::string& json);
};

} // namespace app
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_APP_ITEM_H
