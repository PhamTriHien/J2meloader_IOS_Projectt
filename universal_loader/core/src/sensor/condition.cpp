#include "condition.h"
#include <cmath>
#include <sstream>

namespace universal_loader {
namespace sensor {

static bool evaluateOp(double val, double limit, const std::string& op) {
    if (op == OP_EQUALS) {
        return std::abs(val - limit) < 1e-9;
    } else if (op == OP_GREATER_THAN) {
        return val > limit;
    } else if (op == OP_GREATER_THAN_OR_EQUALS) {
        return val >= limit;
    } else if (op == OP_LESS_THAN) {
        return val < limit;
    } else if (op == OP_LESS_THAN_OR_EQUALS) {
        return val <= limit;
    }
    return false;
}

static bool isValidOp(const std::string& op) {
    return (op == OP_EQUALS || op == OP_GREATER_THAN || op == OP_GREATER_THAN_OR_EQUALS ||
            op == OP_LESS_THAN || op == OP_LESS_THAN_OR_EQUALS);
}

// ==================== LimitCondition ====================

LimitCondition::LimitCondition(double limit, std::string op)
    : m_limit(limit), m_op(std::move(op)) {
    if (!isValidOp(m_op)) {
        throw std::invalid_argument("Invalid operator for LimitCondition: " + m_op);
    }
}

bool LimitCondition::isMet(double value) const {
    return evaluateOp(value, m_limit, m_op);
}

bool LimitCondition::isMet(const std::string& value) const {
    try {
        double d = std::stod(value);
        return isMet(d);
    } catch (...) {
        return false;
    }
}

// ==================== RangeCondition ====================

RangeCondition::RangeCondition(double lowerLimit, std::string lowerOp, double upperLimit, std::string upperOp)
    : m_lowerLimit(lowerLimit), m_lowerOp(std::move(lowerOp)),
      m_upperLimit(upperLimit), m_upperOp(std::move(upperOp)) {
    if (m_lowerOp != OP_GREATER_THAN && m_lowerOp != OP_GREATER_THAN_OR_EQUALS) {
        throw std::invalid_argument("Invalid lower operator for RangeCondition: " + m_lowerOp);
    }
    if (m_upperOp != OP_LESS_THAN && m_upperOp != OP_LESS_THAN_OR_EQUALS) {
        throw std::invalid_argument("Invalid upper operator for RangeCondition: " + m_upperOp);
    }
    if (m_lowerLimit > m_upperLimit) {
        throw std::invalid_argument("Lower limit cannot exceed upper limit in RangeCondition");
    }
}

bool RangeCondition::isMet(double value) const {
    return evaluateOp(value, m_lowerLimit, m_lowerOp) && evaluateOp(value, m_upperLimit, m_upperOp);
}

bool RangeCondition::isMet(const std::string& value) const {
    try {
        double d = std::stod(value);
        return isMet(d);
    } catch (...) {
        return false;
    }
}

// ==================== ObjectCondition ====================

ObjectCondition::ObjectCondition(std::string limit)
    : m_limit(std::move(limit)) {
}

bool ObjectCondition::isMet(double value) const {
    std::ostringstream ss;
    ss << value;
    return ss.str() == m_limit;
}

bool ObjectCondition::isMet(const std::string& value) const {
    return value == m_limit;
}

} // namespace sensor
} // namespace universal_loader
