#ifndef UNIVERSAL_LOADER_REPEAT_RULE_H
#define UNIVERSAL_LOADER_REPEAT_RULE_H

#include "pim_types.h"
#include <vector>
#include <algorithm>

namespace universal_loader {
namespace pim {

// JSR-75 RepeatRule (RepeatRule.java)
class J2ME_API RepeatRule {
public:
    RepeatRule();
    ~RepeatRule() = default;

    int getInt(int field) const;
    void setInt(int field, int value);

    int64_t getDate(int field) const;
    void setDate(int field, int64_t value);

    void addExceptDate(int64_t date);
    void removeExceptDate(int64_t date);
    const std::vector<int64_t>& getExceptDates() const { return m_exceptDates; }

    std::vector<int> getFields() const;

    // Evaluates occurrences between rangeStart and rangeEnd
    std::vector<int64_t> dates(int64_t start, int64_t rangeStart, int64_t rangeEnd) const;

    bool equals(const RepeatRule& other) const;

private:
    int m_frequency{REPEAT_FREQ_DAILY};
    int m_interval{1};
    int m_count{-1};
    int64_t m_end{0};
    int m_dayInWeek{0};
    int m_dayInMonth{0};
    int m_monthInYear{0};
    std::vector<int64_t> m_exceptDates;
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_REPEAT_RULE_H
