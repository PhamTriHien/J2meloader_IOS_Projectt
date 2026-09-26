#ifndef UNIVERSAL_LOADER_SENSOR_TYPES_H
#define UNIVERSAL_LOADER_SENSOR_TYPES_H

#include "j2me_core.h"
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace universal_loader {
namespace sensor {

// Connection Types (SensorInfo)
constexpr int32_t SENSOR_CONN_EMBEDDED             = 1;
constexpr int32_t SENSOR_CONN_REMOTE               = 2;
constexpr int32_t SENSOR_CONN_SHORT_RANGE_WIRELESS = 4;
constexpr int32_t SENSOR_CONN_WIRED                = 8;

// Context Types (SensorInfo)
inline const char* const CONTEXT_TYPE_AMBIENT = "ambient";
inline const char* const CONTEXT_TYPE_DEVICE  = "device";
inline const char* const CONTEXT_TYPE_USER    = "user";
inline const char* const CONTEXT_TYPE_VEHICLE = "vehicle";

// Channel Data Types (ChannelInfo)
constexpr int32_t CHANNEL_TYPE_DOUBLE = 1;
constexpr int32_t CHANNEL_TYPE_INT    = 2;
constexpr int32_t CHANNEL_TYPE_OBJECT = 4;

// SensorConnection States
constexpr int32_t SENSOR_STATE_OPENED    = 1;
constexpr int32_t SENSOR_STATE_LISTENING = 2;
constexpr int32_t SENSOR_STATE_CLOSED    = 4;

// Condition Comparison Operators
inline const char* const OP_EQUALS                 = "eq";
inline const char* const OP_GREATER_THAN           = "gt";
inline const char* const OP_GREATER_THAN_OR_EQUALS = "ge";
inline const char* const OP_LESS_THAN              = "lt";
inline const char* const OP_LESS_THAN_OR_EQUALS    = "le";

// Physical Measurement Unit (JSR-256 Unit.java)
class J2ME_API Unit {
public:
    Unit() : m_symbol("") {}
    explicit Unit(std::string symbol) : m_symbol(std::move(symbol)) {}

    const std::string& getSymbol() const { return m_symbol; }
    std::string toString() const { return m_symbol; }

    static Unit getUnit(const std::string& symbol) {
        return Unit(symbol);
    }

    bool operator==(const Unit& other) const { return m_symbol == other.m_symbol; }
    bool operator!=(const Unit& other) const { return !(*this == other); }

private:
    std::string m_symbol;
};

// Measurement Range (JSR-256 MeasurementRange.java)
class J2ME_API MeasurementRange {
public:
    MeasurementRange(double smallest, double largest, double resolution)
        : m_smallest(smallest), m_largest(largest), m_resolution(resolution) {
        if (smallest > largest || resolution < 0.0) {
            throw std::invalid_argument("Invalid measurement range parameters");
        }
    }

    double getSmallestValue() const { return m_smallest; }
    double getLargestValue() const { return m_largest; }
    double getResolution() const { return m_resolution; }

private:
    double m_smallest;
    double m_largest;
    double m_resolution;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SENSOR_TYPES_H
