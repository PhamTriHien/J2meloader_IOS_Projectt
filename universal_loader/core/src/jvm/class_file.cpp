#include "class_file.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace universal_loader::jvm {

class BinaryReader {
public:
    const uint8_t* ptr;
    const uint8_t* end;

    BinaryReader(const uint8_t* data, size_t size)
        : ptr(data), end(data + size) {}

    bool has(size_t n) const {
        return ptr + n <= end;
    }

    uint8_t readU1() {
        if (!has(1)) throw std::runtime_error("Unexpected EOF reading U1");
        return *ptr++;
    }

    uint16_t readU2() {
        if (!has(2)) throw std::runtime_error("Unexpected EOF reading U2");
        uint16_t val = (static_cast<uint16_t>(ptr[0]) << 8) | static_cast<uint16_t>(ptr[1]);
        ptr += 2;
        return val;
    }

    uint32_t readU4() {
        if (!has(4)) throw std::runtime_error("Unexpected EOF reading U4");
        uint32_t val = (static_cast<uint32_t>(ptr[0]) << 24) |
                       (static_cast<uint32_t>(ptr[1]) << 16) |
                       (static_cast<uint32_t>(ptr[2]) << 8)  |
                       static_cast<uint32_t>(ptr[3]);
        ptr += 4;
        return val;
    }

    void readBytes(uint8_t* out, size_t n) {
        if (!has(n)) throw std::runtime_error("Unexpected EOF reading bytes");
        std::memcpy(out, ptr, n);
        ptr += n;
    }

    void skip(size_t n) {
        if (!has(n)) throw std::runtime_error("Unexpected EOF skipping bytes");
        ptr += n;
    }
};

std::string JavaClass::getUtf8FromCp(uint16_t index) const {
    if (index == 0 || index >= constantPool.size()) return "";
    if (constantPool[index].tag != CONSTANT_Utf8) return "";
    return constantPool[index].utf8Val;
}

std::string JavaClass::getClassNameFromCp(uint16_t classIndex) const {
    if (classIndex == 0 || classIndex >= constantPool.size()) return "";
    if (constantPool[classIndex].tag != CONSTANT_Class) return "";
    return getUtf8FromCp(constantPool[classIndex].nameIndex);
}

JavaMethod* JavaClass::findMethod(const std::string& methodName, const std::string& methodDesc) {
    for (auto& m : methods) {
        if (m.name == methodName) {
            if (methodDesc.empty() || m.descriptor == methodDesc) {
                return &m;
            }
        }
    }
    return nullptr;
}

JavaField* JavaClass::findField(const std::string& fieldName, const std::string& fieldDesc) {
    for (auto& f : fields) {
        if (f.name == fieldName) {
            if (fieldDesc.empty() || f.descriptor == fieldDesc) {
                return &f;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<JavaClass> ClassFileParser::parse(const uint8_t* data, size_t size) {
    if (!data || size < 10) return nullptr;

    try {
        BinaryReader r(data, size);
        auto clazz = std::make_shared<JavaClass>();

        clazz->magic = r.readU4();
        if (clazz->magic != 0xCAFEBABE) {
            return nullptr;
        }

        clazz->minorVersion = r.readU2();
        clazz->majorVersion = r.readU2();

        uint16_t cpCount = r.readU2();
        clazz->constantPool.resize(cpCount);

        for (uint16_t i = 1; i < cpCount; ++i) {
            uint8_t tag = r.readU1();
            clazz->constantPool[i].tag = tag;

            switch (tag) {
                case CONSTANT_Utf8: {
                    uint16_t len = r.readU2();
                    std::vector<char> buf(len);
                    if (len > 0) {
                        r.readBytes(reinterpret_cast<uint8_t*>(buf.data()), len);
                    }
                    clazz->constantPool[i].utf8Val = std::string(buf.data(), len);
                    break;
                }
                case CONSTANT_Integer: {
                    clazz->constantPool[i].intVal = static_cast<int32_t>(r.readU4());
                    break;
                }
                case CONSTANT_Float: {
                    uint32_t raw = r.readU4();
                    float f;
                    std::memcpy(&f, &raw, 4);
                    clazz->constantPool[i].floatVal = f;
                    break;
                }
                case CONSTANT_Long: {
                    uint32_t hi = r.readU4();
                    uint32_t lo = r.readU4();
                    clazz->constantPool[i].longVal = (static_cast<int64_t>(hi) << 32) | lo;
                    ++i; // Long consumes two entries in constant pool
                    break;
                }
                case CONSTANT_Double: {
                    uint32_t hi = r.readU4();
                    uint32_t lo = r.readU4();
                    uint64_t raw = (static_cast<uint64_t>(hi) << 32) | lo;
                    double d;
                    std::memcpy(&d, &raw, 8);
                    clazz->constantPool[i].doubleVal = d;
                    ++i; // Double consumes two entries in constant pool
                    break;
                }
                case CONSTANT_Class: {
                    clazz->constantPool[i].nameIndex = r.readU2();
                    break;
                }
                case CONSTANT_String: {
                    clazz->constantPool[i].stringIndex = r.readU2();
                    break;
                }
                case CONSTANT_Fieldref:
                case CONSTANT_Methodref:
                case CONSTANT_InterfaceMethodref: {
                    clazz->constantPool[i].classIndex = r.readU2();
                    clazz->constantPool[i].nameAndTypeIndex = r.readU2();
                    break;
                }
                case CONSTANT_NameAndType: {
                    clazz->constantPool[i].nameIndex = r.readU2();
                    clazz->constantPool[i].descriptorIndex = r.readU2();
                    break;
                }
                default:
                    // Unknown tag, stop parsing safely
                    return nullptr;
            }
        }

        clazz->accessFlags = r.readU2();
        uint16_t thisClassIdx = r.readU2();
        clazz->thisClassName = clazz->getClassNameFromCp(thisClassIdx);

        uint16_t superClassIdx = r.readU2();
        if (superClassIdx > 0) {
            clazz->superClassName = clazz->getClassNameFromCp(superClassIdx);
        }

        uint16_t interfacesCount = r.readU2();
        clazz->interfaceNames.reserve(interfacesCount);
        for (uint16_t i = 0; i < interfacesCount; ++i) {
            uint16_t ifIdx = r.readU2();
            clazz->interfaceNames.push_back(clazz->getClassNameFromCp(ifIdx));
        }

        // Fields
        uint16_t fieldsCount = r.readU2();
        clazz->fields.reserve(fieldsCount);
        for (uint16_t i = 0; i < fieldsCount; ++i) {
            JavaField f;
            f.accessFlags = r.readU2();
            f.name = clazz->getUtf8FromCp(r.readU2());
            f.descriptor = clazz->getUtf8FromCp(r.readU2());

            if (f.isStatic()) {
                f.slotIndex = clazz->staticFieldValues.size();
                clazz->staticFieldValues.emplace_back();
            } else {
                f.slotIndex = clazz->instanceFieldCount++;
            }

            uint16_t attrsCount = r.readU2();
            for (uint16_t a = 0; a < attrsCount; ++a) {
                r.skip(2); // attr_name_index
                uint32_t len = r.readU4();
                r.skip(len);
            }
            clazz->fields.push_back(f);
        }

        // Methods
        uint16_t methodsCount = r.readU2();
        clazz->methods.reserve(methodsCount);
        for (uint16_t i = 0; i < methodsCount; ++i) {
            JavaMethod m;
            m.clazz = clazz.get();
            m.accessFlags = r.readU2();
            m.name = clazz->getUtf8FromCp(r.readU2());
            m.descriptor = clazz->getUtf8FromCp(r.readU2());

            uint16_t attrsCount = r.readU2();
            for (uint16_t a = 0; a < attrsCount; ++a) {
                uint16_t attrNameIdx = r.readU2();
                uint32_t attrLen = r.readU4();
                std::string attrName = clazz->getUtf8FromCp(attrNameIdx);

                if (attrName == "Code") {
                    m.maxStack = r.readU2();
                    m.maxLocals = r.readU2();
                    uint32_t codeLen = r.readU4();
                    m.code.resize(codeLen);
                    if (codeLen > 0) {
                        r.readBytes(m.code.data(), codeLen);
                    }

                    uint16_t exceptionLen = r.readU2();
                    m.exceptionTable.reserve(exceptionLen);
                    for (uint16_t e = 0; e < exceptionLen; ++e) {
                        JavaExceptionCatch ex;
                        ex.startPc = r.readU2();
                        ex.endPc = r.readU2();
                        ex.handlerPc = r.readU2();
                        ex.catchType = r.readU2();
                        if (ex.catchType > 0) {
                            ex.catchClassName = clazz->getClassNameFromCp(ex.catchType);
                        }
                        m.exceptionTable.push_back(ex);
                    }

                    uint16_t codeAttrsCount = r.readU2();
                    for (uint16_t ca = 0; ca < codeAttrsCount; ++ca) {
                        r.skip(2); // sub-attr name index
                        uint32_t subLen = r.readU4();
                        r.skip(subLen);
                    }
                } else {
                    r.skip(attrLen);
                }
            }
            clazz->methods.push_back(m);
        }

        // Class-level attributes
        uint16_t classAttrsCount = r.readU2();
        for (uint16_t a = 0; a < classAttrsCount; ++a) {
            r.skip(2);
            uint32_t len = r.readU4();
            r.skip(len);
        }

        return clazz;
    } catch (...) {
        return nullptr;
    }
}

} // namespace universal_loader::jvm
