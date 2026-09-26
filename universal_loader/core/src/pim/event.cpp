#include "event.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace universal_loader {
namespace pim {

static std::string formatVcalDate(int64_t ms) {
    if (ms <= 0) return "";
    time_t sec = static_cast<time_t>(ms / 1000);
    struct tm tmVal{};
#if defined(_WIN32)
    gmtime_s(&tmVal, &sec);
#else
    gmtime_r(&sec, &tmVal);
#endif
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02dZ",
             tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday,
             tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);
    return std::string(buf);
}

static int64_t parseVcalDate(const std::string& str) {
    if (str.size() < 15) return 0;
    struct tm tmVal{};
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (sscanf(str.c_str(), "%04d%02d%02dT%02d%02d%02d", &year, &mon, &day, &hour, &min, &sec) >= 6) {
        tmVal.tm_year = year - 1900;
        tmVal.tm_mon  = mon - 1;
        tmVal.tm_mday = day;
        tmVal.tm_hour = hour;
        tmVal.tm_min  = min;
        tmVal.tm_sec  = sec;
#if defined(_WIN32)
        time_t t = _mkgmtime(&tmVal);
#else
        time_t t = timegm(&tmVal);
#endif
        if (t != static_cast<time_t>(-1)) {
            return static_cast<int64_t>(t) * 1000;
        }
    }
    return 0;
}

Event::Event(PIMList* list)
    : PIMItem(list)
{
}

int Event::getDataType(int field) const {
    switch (field) {
        case EVENT_ALARM:
        case EVENT_CLASS:
            return PIM_TYPE_INT;

        case EVENT_START:
        case EVENT_END:
        case EVENT_REVISION:
            return PIM_TYPE_DATE;

        case EVENT_LOCATION:
        case EVENT_NOTE:
        case EVENT_SUMMARY:
        case EVENT_UID:
        default:
            return PIM_TYPE_STRING;
    }
}

void Event::setRepeatRule(const RepeatRule& rule) {
    m_repeatRule = rule;
    m_hasRepeatRule = true;
    m_modified = true;
}

void Event::removeRepeatRule() {
    m_hasRepeatRule = false;
    m_modified = true;
}

std::string Event::toVCalendar() const {
    std::ostringstream ss;
    ss << "BEGIN:VCALENDAR\r\n";
    ss << "VERSION:1.0\r\n";
    ss << "BEGIN:VEVENT\r\n";

    if (!m_uid.empty()) {
        ss << "UID:" << m_uid << "\r\n";
    }
    if (countValues(EVENT_SUMMARY) > 0) {
        ss << "SUMMARY:" << getString(EVENT_SUMMARY, 0) << "\r\n";
    }
    if (countValues(EVENT_NOTE) > 0) {
        ss << "DESCRIPTION:" << getString(EVENT_NOTE, 0) << "\r\n";
    }
    if (countValues(EVENT_LOCATION) > 0) {
        ss << "LOCATION:" << getString(EVENT_LOCATION, 0) << "\r\n";
    }
    if (countValues(EVENT_START) > 0) {
        ss << "DTSTART:" << formatVcalDate(getDate(EVENT_START, 0)) << "\r\n";
    }
    if (countValues(EVENT_END) > 0) {
        ss << "DTEND:" << formatVcalDate(getDate(EVENT_END, 0)) << "\r\n";
    }
    if (countValues(EVENT_ALARM) > 0) {
        ss << "AALARM:" << getInt(EVENT_ALARM, 0) << "\r\n";
    }

    if (m_hasRepeatRule) {
        int freq = m_repeatRule.getInt(REPEAT_FREQUENCY);
        int interval = m_repeatRule.getInt(REPEAT_INTERVAL);
        ss << "RRULE:";
        switch (freq) {
            case REPEAT_FREQ_DAILY:   ss << "D" << interval; break;
            case REPEAT_FREQ_WEEKLY:  ss << "W" << interval; break;
            case REPEAT_FREQ_MONTHLY: ss << "M" << interval; break;
            case REPEAT_FREQ_YEARLY:  ss << "Y" << interval; break;
            default: ss << "D" << interval; break;
        }
        int count = m_repeatRule.getInt(REPEAT_COUNT);
        if (count > 0) {
            ss << " #" << count;
        }
        int64_t rEnd = m_repeatRule.getDate(REPEAT_END);
        if (rEnd > 0) {
            ss << " " << formatVcalDate(rEnd);
        }
        ss << "\r\n";
    }

    ss << "END:VEVENT\r\n";
    ss << "END:VCALENDAR\r\n";
    return ss.str();
}

std::shared_ptr<Event> Event::fromVCalendar(const std::string& vcalText, PIMList* list) {
    auto ev = std::make_shared<Event>(list);
    std::istringstream stream(vcalText);
    std::string line;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string tag = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        std::string tagUpper = tag;
        std::transform(tagUpper.begin(), tagUpper.end(), tagUpper.begin(), ::toupper);

        if (tagUpper == "UID") {
            ev->setUid(val);
        } else if (tagUpper == "SUMMARY") {
            ev->addString(EVENT_SUMMARY, ATTR_NONE, val);
        } else if (tagUpper == "DESCRIPTION" || tagUpper == "NOTE") {
            ev->addString(EVENT_NOTE, ATTR_NONE, val);
        } else if (tagUpper == "LOCATION") {
            ev->addString(EVENT_LOCATION, ATTR_NONE, val);
        } else if (tagUpper == "DTSTART") {
            ev->addDate(EVENT_START, ATTR_NONE, parseVcalDate(val));
        } else if (tagUpper == "DTEND") {
            ev->addDate(EVENT_END, ATTR_NONE, parseVcalDate(val));
        } else if (tagUpper == "AALARM") {
            ev->addInt(EVENT_ALARM, ATTR_NONE, std::atoi(val.c_str()));
        } else if (tagUpper == "RRULE") {
            RepeatRule rr;
            if (!val.empty()) {
                char freqChar = val[0];
                int interval = 1;
                size_t numStart = 1;
                while (numStart < val.size() && (val[numStart] == ' ' || val[numStart] == '=')) numStart++;
                if (numStart < val.size() && std::isdigit(val[numStart])) {
                    interval = std::atoi(val.c_str() + numStart);
                }
                switch (freqChar) {
                    case 'D': rr.setInt(REPEAT_FREQUENCY, REPEAT_FREQ_DAILY); break;
                    case 'W': rr.setInt(REPEAT_FREQUENCY, REPEAT_FREQ_WEEKLY); break;
                    case 'M': rr.setInt(REPEAT_FREQUENCY, REPEAT_FREQ_MONTHLY); break;
                    case 'Y': rr.setInt(REPEAT_FREQUENCY, REPEAT_FREQ_YEARLY); break;
                    default: break;
                }
                rr.setInt(REPEAT_INTERVAL, interval);
                ev->setRepeatRule(rr);
            }
        }
    }

    return ev;
}

} // namespace pim
} // namespace universal_loader
