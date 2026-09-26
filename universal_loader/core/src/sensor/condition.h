#ifndef UNIVERSAL_LOADER_CONDITION_H
#define UNIVERSAL_LOADER_CONDITION_H

#include "sensor_types.h"
#include <string>
#include <memory>

namespace universal_loader {
namespace sensor {

class SensorConnection;
class Channel;

// Abstract Condition Interface (JSR-256 Condition.java)
class J2ME_API Condition {
public:
    virtual ~Condition() = default;
    virtual bool isMet(double value) const = 0;
    virtual bool isMet(const std::string& value) const = 0;
};

// Condition Listener Interface (JSR-256 ConditionListener.java)
class J2ME_API ConditionListener {
public:
    virtual ~ConditionListener() = default;
    virtual void conditionMet(SensorConnection* sensor, Channel* channel, Condition* condition, double value) = 0;
};

// Limit Condition (JSR-256 LimitCondition.java)
class J2ME_API LimitCondition : public Condition {
public:
    LimitCondition(double limit, std::string op);
    ~LimitCondition() override = default;

    double getLimit() const { return m_limit; }
    const std::string& getOperator() const { return m_op; }

    bool isMet(double value) const override;
    bool isMet(const std::string& value) const override;

private:
    double m_limit;
    std::string m_op;
};

// Range Condition (JSR-256 RangeCondition.java)
class J2ME_API RangeCondition : public Condition {
public:
    RangeCondition(double lowerLimit, std::string lowerOp, double upperLimit, std::string upperOp);
    ~RangeCondition() override = default;

    double getLowerLimit() const { return m_lowerLimit; }
    const std::string& getLowerOp() const { return m_lowerOp; }
    double getUpperLimit() const { return m_upperLimit; }
    const std::string& getUpperOp() const { return m_upperOp; }

    bool isMet(double value) const override;
    bool isMet(const std::string& value) const override;

private:
    double m_lowerLimit;
    std::string m_lowerOp;
    double m_upperLimit;
    std::string m_upperOp;
};

// Object Condition (JSR-256 ObjectCondition.java)
class J2ME_API ObjectCondition : public Condition {
public:
    explicit ObjectCondition(std::string limit);
    ~ObjectCondition() override = default;

    const std::string& getLimit() const { return m_limit; }

    bool isMet(double value) const override;
    bool isMet(const std::string& value) const override;

private:
    std::string m_limit;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_CONDITION_H
