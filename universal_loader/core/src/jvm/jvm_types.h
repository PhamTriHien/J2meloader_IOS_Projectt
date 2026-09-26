#ifndef J2ME_JVM_TYPES_H
#define J2ME_JVM_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace universal_loader::jvm {

class JavaClass;
class JavaMethod;
class JavaObject;

enum class JavaValueType {
    INT,
    FLOAT,
    LONG,
    DOUBLE,
    REF
};

struct JavaValue {
    JavaValueType type{JavaValueType::INT};
    union {
        int32_t i;
        float f;
        int64_t l;
        double d;
        JavaObject* ref;
    };

    JavaValue() : type(JavaValueType::INT), i(0) {}
    explicit JavaValue(int32_t val) : type(JavaValueType::INT), i(val) {}
    explicit JavaValue(float val) : type(JavaValueType::FLOAT), f(val) {}
    explicit JavaValue(int64_t val) : type(JavaValueType::LONG), l(val) {}
    explicit JavaValue(double val) : type(JavaValueType::DOUBLE), d(val) {}
    explicit JavaValue(JavaObject* val) : type(JavaValueType::REF), ref(val) {}
};

class JavaObject {
public:
    JavaClass* clazz{nullptr};
    std::vector<JavaValue> fields;
    void* nativeHandle{nullptr};

    JavaObject(JavaClass* c = nullptr) : clazz(c) {}
    virtual ~JavaObject() = default;

    virtual bool isArray() const { return false; }
    virtual bool isString() const { return false; }
};

class JavaArray : public JavaObject {
public:
    char elementTypeCode{'I'}; // 'Z', 'B', 'C', 'S', 'I', 'J', 'F', 'D', 'L'
    int32_t length{0};
    std::vector<JavaValue> elements;

    JavaArray(char typeCode, int32_t len)
        : elementTypeCode(typeCode), length(len), elements(len > 0 ? len : 0) {}

    bool isArray() const override { return true; }
};

class JavaString : public JavaObject {
public:
    std::string value;

    JavaString(const std::string& str = "") : value(str) {}

    bool isString() const override { return true; }
};

struct StackFrame {
    JavaMethod* method{nullptr};
    std::vector<JavaValue> locals;
    std::vector<JavaValue> operandStack;
    size_t sp{0};
    size_t pc{0};

    StackFrame(JavaMethod* m = nullptr, size_t maxLocals = 0, size_t maxStack = 0)
        : method(m), locals(maxLocals), operandStack(maxStack), sp(0), pc(0) {}

    void push(const JavaValue& val) {
        if (sp >= operandStack.size()) {
            operandStack.resize(sp + 16);
        }
        operandStack[sp++] = val;
    }

    JavaValue pop() {
        if (sp == 0) {
            throw std::runtime_error("JVM Stack underflow");
        }
        return operandStack[--sp];
    }

    JavaValue peek() const {
        if (sp == 0) {
            throw std::runtime_error("JVM Stack underflow on peek");
        }
        return operandStack[sp - 1];
    }
};

} // namespace universal_loader::jvm

#endif // J2ME_JVM_TYPES_H
