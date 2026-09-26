#ifndef UNIVERSAL_LOADER_TODO_H
#define UNIVERSAL_LOADER_TODO_H

#include "pim_item.h"

namespace universal_loader {
namespace pim {

class J2ME_API ToDo : public PIMItem {
public:
    explicit ToDo(PIMList* list = nullptr);
    ~ToDo() override = default;

    int getType() const override { return PIM_TODO_LIST; }
    int getDataType(int field) const override;

    // vCalendar 1.0 serialization
    std::string toVCalendar() const;
    static std::shared_ptr<ToDo> fromVCalendar(const std::string& vcalText, PIMList* list = nullptr);
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_TODO_H
