#include "repeat_rule.h"
#include <ctime>
#include <chrono>

namespace universal_loader {
namespace pim {

RepeatRule::RepeatRule()
    : m_frequency(REPEAT_FREQ_DAILY)
    , m_interval(1)
    , m_count(-1)
    , m_end(0)
    , m_dayInWeek(0)
    , m_dayInMonth(0)
    , m_monthInYear(0)
{
}

int RepeatRule::getInt(int field) const {
    switch (field) {
        case REPEAT_FREQUENCY:     return m_frequency;
        case REPEAT_INTERVAL:      return m_interval;
        case REPEAT_COUNT:         return m_count;
        case REPEAT_DAY_IN_WEEK:   return m_dayInWeek;
        case REPEAT_DAY_IN_MONTH:  return m_dayInMonth;
        case REPEAT_MONTH_IN_YEAR: return m_monthInYear;
        default: return 0;
    }
}

void RepeatRule::setInt(int field, int value) {
    switch (field) {
        case REPEAT_FREQUENCY:     m_frequency = value; break;
        case REPEAT_INTERVAL:      m_interval = (value > 0 ? value : 1); break;
        case REPEAT_COUNT:         m_count = value; break;
        case REPEAT_DAY_IN_WEEK:   m_dayInWeek = value; break;
        case REPEAT_DAY_IN_MONTH:  m_dayInMonth = value; break;
        case REPEAT_MONTH_IN_YEAR: m_monthInYear = value; break;
        default: break;
    }
}

int64_t RepeatRule::getDate(int field) const {
    if (field == REPEAT_END) {
        return m_end;
    }
    return 0;
}

void RepeatRule::setDate(int field, int64_t value) {
    if (field == REPEAT_END) {
        m_end = value;
    }
}

void RepeatRule::addExceptDate(int64_t date) {
    if (std::find(m_exceptDates.begin(), m_exceptDates.end(), date) == m_exceptDates.end()) {
        m_exceptDates.push_back(date);
    }
}

void RepeatRule::removeExceptDate(int64_t date) {
    auto it = std::remove(m_exceptDates.begin(), m_exceptDates.end(), date);
    if (it != m_exceptDates.end()) {
        m_exceptDates.erase(it, m_exceptDates.end());
    }
}

std::vector<int> RepeatRule::getFields() const {
    std::vector<int> fields;
    fields.push_back(REPEAT_FREQUENCY);
    fields.push_back(REPEAT_INTERVAL);
    if (m_count > 0) fields.push_back(REPEAT_COUNT);
    if (m_end > 0) fields.push_back(REPEAT_END);
    if (m_dayInWeek > 0) fields.push_back(REPEAT_DAY_IN_WEEK);
    if (m_dayInMonth > 0) fields.push_back(REPEAT_DAY_IN_MONTH);
    if (m_monthInYear > 0) fields.push_back(REPEAT_MONTH_IN_YEAR);
    return fields;
}

static bool isExceptDate(int64_t curMs, const std::vector<int64_t>& exceptDates) {
    // Check if curMs falls on same day (86400000ms) as any except date
    int64_t curDay = curMs / 86400000LL;
    for (int64_t ex : exceptDates) {
        if (ex == curMs || (ex / 86400000LL) == curDay) {
            return true;
        }
    }
    return false;
}

std::vector<int64_t> RepeatRule::dates(int64_t start, int64_t rangeStart, int64_t rangeEnd) const {
    std::vector<int64_t> result;
    if (start <= 0 || rangeStart > rangeEnd) {
        return result;
    }

    int occurrences = 0;
    int interval = (m_interval > 0) ? m_interval : 1;

    // Convert start to calendar breakdown
    time_t sec = static_cast<time_t>(start / 1000);
    struct tm tmStart{};
#if defined(_WIN32)
    gmtime_s(&tmStart, &sec);
#else
    gmtime_r(&sec, &tmStart);
#endif
    int64_t msOffset = start % 1000;

    int maxCount = (m_count > 0) ? m_count : 10000; // safety bound
    struct tm curTm = tmStart;

#if defined(_WIN32)
#define timegm_compat _mkgmtime
#else
#define timegm_compat timegm
#endif

    while (occurrences < maxCount) {
        time_t curSec = timegm_compat(&curTm);
        if (curSec == static_cast<time_t>(-1)) {
            // fallback using direct ms calculation
            break;
        }
        int64_t curMs = static_cast<int64_t>(curSec) * 1000 + msOffset;

        if (m_end > 0 && curMs > m_end) {
            break;
        }

        if (curMs > rangeEnd && occurrences > 0) {
            break;
        }

        if (!isExceptDate(curMs, m_exceptDates)) {
            if (curMs >= rangeStart && curMs <= rangeEnd) {
                result.push_back(curMs);
            }
        }
        occurrences++;

        // Step by frequency and interval
        switch (m_frequency) {
            case REPEAT_FREQ_DAILY:
                curTm.tm_mday += interval;
                break;
            case REPEAT_FREQ_WEEKLY:
                curTm.tm_mday += (7 * interval);
                break;
            case REPEAT_FREQ_MONTHLY:
                curTm.tm_mon += interval;
                break;
            case REPEAT_FREQ_YEARLY:
                curTm.tm_year += interval;
                break;
            default:
                curTm.tm_mday += interval;
                break;
        }
        // Normalize tm
        timegm_compat(&curTm);
    }

    return result;
}

bool RepeatRule::equals(const RepeatRule& other) const {
    return m_frequency == other.m_frequency &&
           m_interval == other.m_interval &&
           m_count == other.m_count &&
           m_end == other.m_end &&
           m_dayInWeek == other.m_dayInWeek &&
           m_dayInMonth == other.m_dayInMonth &&
           m_monthInYear == other.m_monthInYear &&
           m_exceptDates == other.m_exceptDates;
}

} // namespace pim
} // namespace universal_loader
