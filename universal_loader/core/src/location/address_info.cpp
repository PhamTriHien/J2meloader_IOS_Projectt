#include "address_info.h"

namespace universal_loader {
namespace location {

std::string AddressInfo::getField(int32_t field) const {
    auto it = m_fields.find(field);
    if (it != m_fields.end()) {
        return it->second;
    }
    return "";
}

void AddressInfo::setField(int32_t field, const std::string& value) {
    if (value.empty()) {
        m_fields.erase(field);
    } else {
        m_fields[field] = value;
    }
}

} // namespace location
} // namespace universal_loader
