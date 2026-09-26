#ifndef UNIVERSAL_LOADER_CONTACT_H
#define UNIVERSAL_LOADER_CONTACT_H

#include "pim_item.h"

namespace universal_loader {
namespace pim {

class J2ME_API Contact : public PIMItem {
public:
    explicit Contact(PIMList* list = nullptr);
    ~Contact() override = default;

    int getType() const override { return PIM_CONTACT_LIST; }
    int getDataType(int field) const override;

    int getPreferredIndex(int field) const;

    // High level helpers
    std::string getFormattedName() const;
    void setName(const std::string& family, const std::string& given,
                 const std::string& other = "", const std::string& prefix = "", const std::string& suffix = "");

    void setAddress(int index, int attributes,
                    const std::string& street, const std::string& locality,
                    const std::string& region, const std::string& postalCode,
                    const std::string& country, const std::string& poBox = "",
                    const std::string& extra = "");

    // vCard 2.1 Serialization
    std::string toVCard() const;
    static std::shared_ptr<Contact> fromVCard(const std::string& vcardText, PIMList* list = nullptr);
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CONTACT_H
