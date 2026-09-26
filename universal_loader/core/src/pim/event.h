#ifndef UNIVERSAL_LOADER_EVENT_H
#define UNIVERSAL_LOADER_EVENT_H

#include "pim_item.h"
#include "repeat_rule.h"

namespace universal_loader {
namespace pim {

class J2ME_API Event : public PIMItem {
public:
    explicit Event(PIMList* list = nullptr);
    ~Event() override = default;

    int getType() const override { return PIM_EVENT_LIST; }
    int getDataType(int field) const override;

    bool hasRepeatRule() const { return m_hasRepeatRule; }
    const RepeatRule& getRepeatRule() const { return m_repeatRule; }
    void setRepeatRule(const RepeatRule& rule);
    void removeRepeatRule();

    // vCalendar 1.0 serialization
    std::string toVCalendar() const;
    static std::shared_ptr<Event> fromVCalendar(const std::string& vcalText, PIMList* list = nullptr);

private:
    bool m_hasRepeatRule{false};
    RepeatRule m_repeatRule;
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_EVENT_H
