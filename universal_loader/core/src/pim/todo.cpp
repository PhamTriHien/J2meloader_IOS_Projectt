#include "todo.h"
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

ToDo::ToDo(PIMList* list)
    : PIMItem(list)
{
}

int ToDo::getDataType(int field) const {
    switch (field) {
        case TODO_CLASS:
        case TODO_PRIORITY:
            return PIM_TYPE_INT;

        case TODO_COMPLETED:
            return PIM_TYPE_BOOLEAN;

        case TODO_COMPLETION_DATE:
        case TODO_DUE:
        case TODO_REVISION:
            return PIM_TYPE_DATE;

        case TODO_NOTE:
        case TODO_SUMMARY:
        case TODO_UID:
        default:
            return PIM_TYPE_STRING;
    }
}

std::string ToDo::toVCalendar() const {
    std::ostringstream ss;
    ss << "BEGIN:VCALENDAR\r\n";
    ss << "VERSION:1.0\r\n";
    ss << "BEGIN:VTODO\r\n";

    if (!m_uid.empty()) {
        ss << "UID:" << m_uid << "\r\n";
    }
    if (countValues(TODO_SUMMARY) > 0) {
        ss << "SUMMARY:" << getString(TODO_SUMMARY, 0) << "\r\n";
    }
    if (countValues(TODO_NOTE) > 0) {
        ss << "DESCRIPTION:" << getString(TODO_NOTE, 0) << "\r\n";
    }
    if (countValues(TODO_PRIORITY) > 0) {
        ss << "PRIORITY:" << getInt(TODO_PRIORITY, 0) << "\r\n";
    }
    if (countValues(TODO_COMPLETED) > 0) {
        bool comp = getBoolean(TODO_COMPLETED, 0);
        ss << "STATUS:" << (comp ? "COMPLETED" : "NEEDS ACTION") << "\r\n";
    }
    if (countValues(TODO_COMPLETION_DATE) > 0) {
        ss << "COMPLETED:" << formatVcalDate(getDate(TODO_COMPLETION_DATE, 0)) << "\r\n";
    }
    if (countValues(TODO_DUE) > 0) {
        ss << "DUE:" << formatVcalDate(getDate(TODO_DUE, 0)) << "\r\n";
    }

    ss << "END:VTODO\r\n";
    ss << "END:VCALENDAR\r\n";
    return ss.str();
}

std::shared_ptr<ToDo> ToDo::fromVCalendar(const std::string& vcalText, PIMList* list) {
    auto todo = std::make_shared<ToDo>(list);
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
            todo->setUid(val);
        } else if (tagUpper == "SUMMARY") {
            todo->addString(TODO_SUMMARY, ATTR_NONE, val);
        } else if (tagUpper == "DESCRIPTION" || tagUpper == "NOTE") {
            todo->addString(TODO_NOTE, ATTR_NONE, val);
        } else if (tagUpper == "PRIORITY") {
            todo->addInt(TODO_PRIORITY, ATTR_NONE, std::atoi(val.c_str()));
        } else if (tagUpper == "STATUS") {
            bool comp = (val.find("COMPLETED") != std::string::npos);
            todo->addBoolean(TODO_COMPLETED, ATTR_NONE, comp);
        } else if (tagUpper == "COMPLETED") {
            todo->addDate(TODO_COMPLETION_DATE, ATTR_NONE, parseVcalDate(val));
        } else if (tagUpper == "DUE") {
            todo->addDate(TODO_DUE, ATTR_NONE, parseVcalDate(val));
        }
    }

    return todo;
}

} // namespace pim
} // namespace universal_loader
