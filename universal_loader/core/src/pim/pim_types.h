#ifndef UNIVERSAL_LOADER_PIM_TYPES_H
#define UNIVERSAL_LOADER_PIM_TYPES_H

#include "j2me_core.h"
#include <string>
#include <vector>
#include <cstdint>

namespace universal_loader {
namespace pim {

// PIM List Types (PIM.java)
constexpr int32_t PIM_CONTACT_LIST = 1;
constexpr int32_t PIM_EVENT_LIST   = 2;
constexpr int32_t PIM_TODO_LIST    = 3;

// PIM Access Modes (PIM.java)
constexpr int32_t PIM_READ_ONLY  = 1;
constexpr int32_t PIM_WRITE_ONLY = 2;
constexpr int32_t PIM_READ_WRITE = 3;

// PIM Field Data Types (PIMItem.java)
constexpr int32_t PIM_TYPE_BINARY       = 0;
constexpr int32_t PIM_TYPE_BOOLEAN      = 1;
constexpr int32_t PIM_TYPE_DATE         = 2;
constexpr int32_t PIM_TYPE_INT          = 3;
constexpr int32_t PIM_TYPE_STRING       = 4;
constexpr int32_t PIM_TYPE_STRING_ARRAY = 5;

// Common Attributes (PIMItem.java, Contact.java)
constexpr int32_t ATTR_NONE      = 0;
constexpr int32_t ATTR_ASST      = 1;
constexpr int32_t ATTR_AUTO      = 2;
constexpr int32_t ATTR_FAX       = 4;
constexpr int32_t ATTR_HOME      = 8;
constexpr int32_t ATTR_MOBILE    = 16;
constexpr int32_t ATTR_OTHER     = 32;
constexpr int32_t ATTR_PAGER     = 64;
constexpr int32_t ATTR_PREFERRED = 128;
constexpr int32_t ATTR_SMS       = 256;
constexpr int32_t ATTR_WORK      = 512;

// Security Classification
constexpr int32_t CLASS_CONFIDENTIAL = 200;
constexpr int32_t CLASS_PRIVATE      = 201;
constexpr int32_t CLASS_PUBLIC       = 202;

// Contact Fields (Contact.java)
constexpr int32_t CONTACT_ADDR               = 100;
constexpr int32_t CONTACT_BIRTHDAY           = 101;
constexpr int32_t CONTACT_CLASS              = 102;
constexpr int32_t CONTACT_EMAIL              = 103;
constexpr int32_t CONTACT_FORMATTED_ADDR     = 104;
constexpr int32_t CONTACT_FORMATTED_NAME     = 105;
constexpr int32_t CONTACT_NAME               = 106;
constexpr int32_t CONTACT_NICKNAME           = 107;
constexpr int32_t CONTACT_NOTE               = 108;
constexpr int32_t CONTACT_ORG                = 109;
constexpr int32_t CONTACT_PHOTO              = 110;
constexpr int32_t CONTACT_PHOTO_URL          = 111;
constexpr int32_t CONTACT_PUBLIC_KEY         = 112;
constexpr int32_t CONTACT_PUBLIC_KEY_STRING  = 113;
constexpr int32_t CONTACT_REVISION           = 114;
constexpr int32_t CONTACT_TEL                = 115;
constexpr int32_t CONTACT_TITLE              = 116;
constexpr int32_t CONTACT_UID                = 117;
constexpr int32_t CONTACT_URL                = 118;

// Contact String Array Field Elements
constexpr int32_t NAME_FAMILY = 0;
constexpr int32_t NAME_GIVEN  = 1;
constexpr int32_t NAME_OTHER  = 2;
constexpr int32_t NAME_PREFIX = 3;
constexpr int32_t NAME_SUFFIX = 4;
constexpr int32_t NAME_ARRAY_SIZE = 5;

constexpr int32_t ADDR_POBOX      = 0;
constexpr int32_t ADDR_EXTRA      = 1;
constexpr int32_t ADDR_STREET     = 2;
constexpr int32_t ADDR_LOCALITY   = 3;
constexpr int32_t ADDR_REGION     = 4;
constexpr int32_t ADDR_POSTALCODE = 5;
constexpr int32_t ADDR_COUNTRY    = 6;
constexpr int32_t ADDR_ARRAY_SIZE = 7;

// Event Fields (Event.java)
constexpr int32_t EVENT_ALARM    = 100;
constexpr int32_t EVENT_CLASS    = 101;
constexpr int32_t EVENT_END      = 102;
constexpr int32_t EVENT_LOCATION = 103;
constexpr int32_t EVENT_NOTE     = 104;
constexpr int32_t EVENT_REVISION = 105;
constexpr int32_t EVENT_START    = 106;
constexpr int32_t EVENT_SUMMARY  = 107;
constexpr int32_t EVENT_UID      = 108;

// Event Search Types (EventList.java)
constexpr int32_t EVENT_STARTING  = 0;
constexpr int32_t EVENT_ENDING    = 1;
constexpr int32_t EVENT_OCCURRING = 2;

// ToDo Fields (ToDo.java)
constexpr int32_t TODO_CLASS           = 100;
constexpr int32_t TODO_COMPLETED       = 101;
constexpr int32_t TODO_COMPLETION_DATE = 102;
constexpr int32_t TODO_DUE             = 103;
constexpr int32_t TODO_NOTE            = 104;
constexpr int32_t TODO_PRIORITY        = 105;
constexpr int32_t TODO_REVISION        = 106;
constexpr int32_t TODO_SUMMARY         = 107;
constexpr int32_t TODO_UID             = 108;

// RepeatRule Constants (RepeatRule.java)
constexpr int32_t REPEAT_FREQUENCY     = 0;
constexpr int32_t REPEAT_DAY_IN_MONTH  = 1;
constexpr int32_t REPEAT_DAY_IN_WEEK   = 2;
constexpr int32_t REPEAT_DAY_IN_YEAR   = 4;
constexpr int32_t REPEAT_MONTH_IN_YEAR = 8;
constexpr int32_t REPEAT_WEEK_IN_MONTH = 16;
constexpr int32_t REPEAT_COUNT         = 32;
constexpr int32_t REPEAT_END           = 64;
constexpr int32_t REPEAT_INTERVAL      = 128;

// Frequencies
constexpr int32_t REPEAT_FREQ_DAILY   = 16;
constexpr int32_t REPEAT_FREQ_WEEKLY  = 17;
constexpr int32_t REPEAT_FREQ_MONTHLY = 18;
constexpr int32_t REPEAT_FREQ_YEARLY  = 19;

// Days of Week bitmasks
constexpr int32_t DOW_SUNDAY    = 65536;
constexpr int32_t DOW_MONDAY    = 32768;
constexpr int32_t DOW_TUESDAY   = 16384;
constexpr int32_t DOW_WEDNESDAY = 8192;
constexpr int32_t DOW_THURSDAY  = 4096;
constexpr int32_t DOW_FRIDAY    = 2048;
constexpr int32_t DOW_SATURDAY  = 1024;

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PIM_TYPES_H
