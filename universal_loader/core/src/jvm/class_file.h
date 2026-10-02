#ifndef J2ME_CLASS_FILE_H
#define J2ME_CLASS_FILE_H

#include "jvm_types.h"
#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace universal_loader::jvm {

constexpr uint16_t ACC_PUBLIC       = 0x0001;
constexpr uint16_t ACC_PRIVATE      = 0x0002;
constexpr uint16_t ACC_PROTECTED    = 0x0004;
constexpr uint16_t ACC_STATIC       = 0x0008;
constexpr uint16_t ACC_FINAL        = 0x0010;
constexpr uint16_t ACC_SYNCHRONIZED = 0x0020;
constexpr uint16_t ACC_SUPER        = 0x0020;
constexpr uint16_t ACC_INTERFACE    = 0x0200;
constexpr uint16_t ACC_ABSTRACT     = 0x0400;
constexpr uint16_t ACC_NATIVE       = 0x0100;

enum ConstantPoolTag : uint8_t {
    CONSTANT_Utf8               = 1,
    CONSTANT_Integer            = 3,
    CONSTANT_Float              = 4,
    CONSTANT_Long               = 5,
    CONSTANT_Double             = 6,
    CONSTANT_Class              = 7,
    CONSTANT_String             = 8,
    CONSTANT_Fieldref           = 9,
    CONSTANT_Methodref          = 10,
    CONSTANT_InterfaceMethodref = 11,
    CONSTANT_NameAndType        = 12
};

struct CpEntry {
    uint8_t tag{0};
    std::string utf8Val;
    int32_t intVal{0};
    float floatVal{0.0f};
    int64_t longVal{0};
    double doubleVal{0.0};
    uint16_t classIndex{0};
    uint16_t nameAndTypeIndex{0};
    uint16_t stringIndex{0};
    uint16_t nameIndex{0};
    uint16_t descriptorIndex{0};
};

struct JavaExceptionCatch {
    uint16_t startPc{0};
    uint16_t endPc{0};
    uint16_t handlerPc{0};
    uint16_t catchType{0};
    std::string catchClassName;
};

struct JavaField {
    uint16_t accessFlags{0};
    std::string name;
    std::string descriptor;
    size_t slotIndex{0};

    bool isStatic() const { return (accessFlags & ACC_STATIC) != 0; }
    bool isFinal() const { return (accessFlags & ACC_FINAL) != 0; }
};

class J2ME_API JavaMethod {
public:
    JavaClass* clazz{nullptr};
    uint16_t accessFlags{0};
    std::string name;
    std::string descriptor;
    uint16_t maxStack{0};
    uint16_t maxLocals{0};
    std::vector<uint8_t> code;
    std::vector<JavaExceptionCatch> exceptionTable;
    // Interpreter caches
    const void* nativeHandler{nullptr};
    bool nativeResolved{false};
    std::vector<char> paramTypes;
    bool paramsParsed{false};
    // Trivial static no-arg methods (obfuscator getters / opaque predicates)
    int8_t trivialKind{0}; // 0 unknown, 1 not trivial, 2 constant, 3 static field getter
    JavaValue trivialConst;
    JavaClass* trivialOwner{nullptr};
    size_t trivialSlot{0};

    bool isNative() const { return (accessFlags & ACC_NATIVE) != 0; }
    bool isStatic() const { return (accessFlags & ACC_STATIC) != 0; }
    bool isAbstract() const { return (accessFlags & ACC_ABSTRACT) != 0; }
};

class J2ME_API JavaClass {
public:
    uint32_t magic{0};
    uint16_t minorVersion{0};
    uint16_t majorVersion{0};
    uint16_t accessFlags{0};
    std::string thisClassName;
    std::string superClassName;
    std::vector<std::string> interfaceNames;

    std::vector<CpEntry> constantPool;
    std::vector<JavaField> fields;
    std::vector<JavaMethod> methods;
    std::vector<JavaValue> staticFieldValues;

    size_t instanceFieldCount{0};
    // Index of this class's first instance field inside an object's field vector
    // (superclass fields come first). Valid once layoutResolved is set.
    size_t instanceFieldBase{0};
    bool layoutResolved{false};
    bool staticInitDone{false};

    std::string getUtf8FromCp(uint16_t index) const;
    std::string getClassNameFromCp(uint16_t classIndex) const;
    JavaMethod* findMethod(const std::string& methodName, const std::string& methodDesc);
    JavaField* findField(const std::string& fieldName, const std::string& fieldDesc);
};

class J2ME_API ClassFileParser {
public:
    static std::shared_ptr<JavaClass> parse(const uint8_t* data, size_t size);
};

} // namespace universal_loader::jvm

#endif // J2ME_CLASS_FILE_H
