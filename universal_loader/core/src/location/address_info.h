#ifndef UNIVERSAL_LOADER_ADDRESS_INFO_H
#define UNIVERSAL_LOADER_ADDRESS_INFO_H

#include "location_types.h"
#include <string>
#include <unordered_map>

namespace universal_loader {
namespace location {

class J2ME_API AddressInfo {
public:
    static constexpr int32_t EXTENSION      = 1;
    static constexpr int32_t STREET         = 2;
    static constexpr int32_t POSTAL_CODE    = 3;
    static constexpr int32_t CITY           = 4;
    static constexpr int32_t COUNTY         = 5;
    static constexpr int32_t STATE          = 6;
    static constexpr int32_t COUNTRY        = 7;
    static constexpr int32_t COUNTRY_CODE   = 8;
    static constexpr int32_t DISTRICT       = 9;
    static constexpr int32_t BUILDING_NAME  = 10;
    static constexpr int32_t BUILDING_FLOOR = 11;
    static constexpr int32_t BUILDING_ROOM  = 12;
    static constexpr int32_t BUILDING_ZONE  = 13;
    static constexpr int32_t CROSSING1      = 14;
    static constexpr int32_t CROSSING2      = 15;
    static constexpr int32_t URL            = 16;
    static constexpr int32_t PHONE_NUMBER   = 17;

    AddressInfo() = default;
    ~AddressInfo() = default;

    std::string getField(int32_t field) const;
    void setField(int32_t field, const std::string& value);

private:
    std::unordered_map<int32_t, std::string> m_fields;
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_ADDRESS_INFO_H
