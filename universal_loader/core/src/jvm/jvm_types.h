#ifndef J2ME_JVM_TYPES_H
#define J2ME_JVM_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>

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

    // Zero the full 64-bit union so a default value also reads as a null ref / 0L / 0.0
    JavaValue() : type(JavaValueType::INT), l(0) {}
    explicit JavaValue(int32_t val) : type(JavaValueType::INT), i(val) {}
    explicit JavaValue(float val) : type(JavaValueType::FLOAT), f(val) {}
    explicit JavaValue(int64_t val) : type(JavaValueType::LONG), l(val) {}
    explicit JavaValue(double val) : type(JavaValueType::DOUBLE), d(val) {}
    explicit JavaValue(JavaObject* val) : type(JavaValueType::REF), l(0) { ref = val; }

    // Category-2 values (long/double) occupy two JVM slots
    bool isWide() const { return type == JavaValueType::LONG || type == JavaValueType::DOUBLE; }
};

// Base for native (C++) state attached to instances of system classes
// such as StringBuffer, Vector or Hashtable.
struct NativePayload {
    virtual ~NativePayload() = default;
    // Reports Java objects referenced from native state (for the garbage collector)
    virtual void trace(std::vector<JavaObject*>&) const {}
};

class JavaObject {
public:
    JavaClass* clazz{nullptr};
    std::vector<JavaValue> fields;
    void* nativeHandle{nullptr};
    // Class name for instances of system classes that have no bytecode (clazz == nullptr)
    std::string nativeClassName;
    // Detail message for java/lang/Throwable instances
    std::string throwableMessage;
    std::string throwableTrace; // frames unwound before the exception was caught
    std::shared_ptr<NativePayload> payload;
    // Monitor state; only touched while holding the VM's GIL
    std::thread::id monitorOwner{};
    int32_t monitorCount{0};
    uint32_t notifyGen{0};
    bool gcMark{false};

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
        : elementTypeCode(typeCode), length(len), elements(len > 0 ? len : 0) {
        JavaValue init;
        if (typeCode == 'J') init = JavaValue(int64_t(0));
        else if (typeCode == 'D') init = JavaValue(0.0);
        else if (typeCode == 'F') init = JavaValue(0.0f);
        else if (typeCode == 'L' || typeCode == '[') init = JavaValue(static_cast<JavaObject*>(nullptr));
        for (auto& e : elements) e = init;
    }

    bool isArray() const override { return true; }
};

class JavaString : public JavaObject {
public:
    std::string value; // UTF-8

    // Lazily built UTF-16 view used for Java index semantics (charAt, length, substring)
    std::u16string utf16Cache;
    bool utf16Valid{false};

    JavaString(const std::string& str = "") : value(str) {}

    void setValue(const std::string& str) {
        value = str;
        utf16Valid = false;
    }

    bool isString() const override { return true; }
};

inline void gcTraceValue(const JavaValue& v, std::vector<JavaObject*>& out) {
    if (v.type == JavaValueType::REF && v.ref) out.push_back(v.ref);
}

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
