#include "jvm_bytecode.h"
#include "jvm_interpreter.h"
#include "lcdui_display.h"
#include "jar_loader.h"
#include "png_decoder.h"
#include "rms_storage.h"
#include "j2me_full_apis.h"
#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/socket.h>
#include <unistd.h>
#endif
extern "C" bool native_text_measure(const char *utf8, int px, int *outW, int *outH) __attribute__((weak));

// Big-Endian Stream Helper
class ByteStream {
public:
    ByteStream(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

    bool has(size_t count) const { return m_pos + count <= m_size; }
    size_t pos() const { return m_pos; }
    void setPos(size_t p) { m_pos = p; }
    void skip(size_t count) { m_pos += count; }

    uint8_t readU1() {
        return (m_pos < m_size) ? m_data[m_pos++] : 0;
    }
    uint16_t readU2() {
        if (m_pos + 2 > m_size) return 0;
        uint16_t v = (m_data[m_pos] << 8) | m_data[m_pos + 1];
        m_pos += 2;
        return v;
    }
    uint32_t readU4() {
        if (m_pos + 4 > m_size) return 0;
        uint32_t v = ((uint32_t)m_data[m_pos] << 24) |
                     ((uint32_t)m_data[m_pos + 1] << 16) |
                     ((uint32_t)m_data[m_pos + 2] << 8) |
                     ((uint32_t)m_data[m_pos + 3]);
        m_pos += 4;
        return v;
    }
    std::string readUtf8(size_t len) {
        if (m_pos + len > m_size) return "";
        std::string s(reinterpret_cast<const char*>(m_data + m_pos), len);
        m_pos += len;
        return s;
    }
    void readBytes(uint8_t* dst, size_t count) {
        if (m_pos + count <= m_size) {
            std::memcpy(dst, m_data + m_pos, count);
            m_pos += count;
        }
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
};

JvmBytecodeEngine::JvmBytecodeEngine() {}

JvmBytecodeEngine& JvmBytecodeEngine::getInstance() {
    static JvmBytecodeEngine instance;
    return instance;
}

void JvmBytecodeEngine::reset() {
    m_loadedClasses.clear();
    m_heapObjects.clear();
    m_heapArrays.clear();
    m_nativeImages.clear();
    m_staticFields.clear();
    m_activeJar = nullptr;
    m_nextRef = 1;
    m_cancel.store(false);
}

uint32_t JvmBytecodeEngine::allocObject(const std::string& className) {
    uint32_t ref = m_nextRef++;
    JavaObject obj;
    obj.id = ref;
    obj.className = className;
    m_heapObjects[ref] = std::move(obj);
    return ref;
}

uint32_t JvmBytecodeEngine::createString(const std::string& str) {
    uint32_t ref = allocObject("java/lang/String");
    JavaObject* obj = getObject(ref);
    if (obj) obj->stringVal = str;
    return ref;
}

std::string JvmBytecodeEngine::getString(uint32_t ref) {
    JavaObject* obj = getObject(ref);
    return obj ? obj->stringVal : "";
}

uint32_t JvmBytecodeEngine::allocArray(uint8_t type, int length) {
    uint32_t ref = m_nextRef++;
    JavaArray arr;
    arr.id = ref;
    arr.elemType = type;
    int l = std::max(0, length);
    if (type == 10) arr.intData.resize(l, 0); // T_INT
    else if (type == 8 || type == 4) arr.byteData.resize(l, 0); // T_BYTE / T_BOOLEAN
    else if (type == 5) arr.charData.resize(l, 0); // T_CHAR
    else if (type == 9) arr.shortData.resize(l, 0); // T_SHORT
    else if (type == 11) arr.longData.resize(l, 0); // T_LONG
    else if (type == 6) arr.floatData.resize(l, 0.0f); // T_FLOAT
    else if (type == 7) arr.doubleData.resize(l, 0.0); // T_DOUBLE
    else arr.refData.resize(l, 0); // Reference / Object array
    m_heapArrays[ref] = std::move(arr);
    return ref;
}

JavaObject* JvmBytecodeEngine::getObject(uint32_t ref) {
    auto it = m_heapObjects.find(ref);
    return (it != m_heapObjects.end()) ? &it->second : nullptr;
}

JavaArray* JvmBytecodeEngine::getArray(uint32_t ref) {
    auto it = m_heapArrays.find(ref);
    return (it != m_heapArrays.end()) ? &it->second : nullptr;
}

uint32_t JvmBytecodeEngine::allocateNativeImage(int w, int h, bool isMutable) {
    uint32_t ref = allocObject("javax/microedition/lcdui/Image");
    NativeImage img;
    img.width = w > 0 ? w : 16;
    img.height = h > 0 ? h : 16;
    img.isMutable = isMutable;
    img.pixels.resize(img.width * img.height, 0xFF000000);
    m_nativeImages[ref] = std::move(img);
    return ref;
}

NativeImage* JvmBytecodeEngine::getNativeImage(uint32_t ref) {
    auto it = m_nativeImages.find(ref);
    return (it != m_nativeImages.end()) ? &it->second : nullptr;
}

uint32_t JvmBytecodeEngine::loadNativeImageFromBytes(const uint8_t* data, size_t size) {
    int w = 0, h = 0;
    std::vector<uint32_t> pixels;
    if (!PngDecoder::decode(data, size, w, h, pixels)) {
        return allocateNativeImage(16, 16, false);
    }
    uint32_t ref = allocObject("javax/microedition/lcdui/Image");
    NativeImage img;
    img.width = w;
    img.height = h;
    img.isMutable = false;
    img.pixels = std::move(pixels);
    m_nativeImages[ref] = std::move(img);
    return ref;
}

uint32_t JvmBytecodeEngine::loadNativeImageFromJar(const std::string& path) {
    if (!m_activeJar) return allocateNativeImage(16, 16, false);

    std::string entryName = path;
    if (!entryName.empty() && entryName[0] == '/') entryName.erase(0, 1);

    std::vector<uint8_t> bytes;
    if (m_activeJar->extractEntry(entryName, bytes)) {
        return loadNativeImageFromBytes(bytes.data(), bytes.size());
    }
    return allocateNativeImage(16, 16, false);
}

// ----------------------------------------------------
// Complete Java Classfile Parser (0xCAFEBABE)
// ----------------------------------------------------
std::shared_ptr<ClassFile> JvmBytecodeEngine::loadClass(const std::vector<uint8_t>& classBytes) {
    if (classBytes.size() < 10) return nullptr;

    ByteStream bs(classBytes.data(), classBytes.size());
    uint32_t magic = bs.readU4();
    if (magic != 0xCAFEBABE) return nullptr;

    auto cls = std::make_shared<ClassFile>();
    cls->magic = magic;
    cls->minorVersion = bs.readU2();
    cls->majorVersion = bs.readU2();

    uint16_t cpCount = bs.readU2();
    cls->constantPool.resize(cpCount);

    for (uint16_t i = 1; i < cpCount; ++i) {
        uint8_t tag = bs.readU1();
        cls->constantPool[i].tag = tag;

        switch (tag) {
        case CONSTANT_Utf8: {
            uint16_t len = bs.readU2();
            cls->constantPool[i].strVal = bs.readUtf8(len);
            break;
        }
        case CONSTANT_Integer:
            cls->constantPool[i].intVal = (int32_t)bs.readU4();
            break;
        case CONSTANT_Float: {
            uint32_t raw = bs.readU4();
            std::memcpy(&cls->constantPool[i].floatVal, &raw, sizeof(float));
            break;
        }
        case CONSTANT_Long:
            cls->constantPool[i].longVal = ((int64_t)bs.readU4() << 32) | bs.readU4();
            i++; // Long takes 2 CP entries
            break;
        case CONSTANT_Double: {
            uint64_t raw = ((uint64_t)bs.readU4() << 32) | bs.readU4();
            std::memcpy(&cls->constantPool[i].doubleVal, &raw, sizeof(double));
            i++; // Double takes 2 entries
            break;
        }
        case CONSTANT_Class:
            cls->constantPool[i].nameIndex = bs.readU2();
            break;
        case CONSTANT_String:
            cls->constantPool[i].stringIndex = bs.readU2();
            break;
        case CONSTANT_Fieldref:
        case CONSTANT_Methodref:
        case CONSTANT_InterfaceMethodref:
            cls->constantPool[i].classIndex = bs.readU2();
            cls->constantPool[i].nameAndTypeIndex = bs.readU2();
            break;
        case CONSTANT_NameAndType:
            cls->constantPool[i].nameIndex = bs.readU2();
            cls->constantPool[i].descIndex = bs.readU2();
            break;
        default:
            break;
        }
    }

    cls->accessFlags = bs.readU2();
    uint16_t thisClassIdx = bs.readU2();
    if (thisClassIdx < cpCount && cls->constantPool[thisClassIdx].nameIndex < cpCount) {
        cls->thisClassName = cls->constantPool[cls->constantPool[thisClassIdx].nameIndex].strVal;
    }

    uint16_t superClassIdx = bs.readU2();
    if (superClassIdx > 0 && superClassIdx < cpCount && cls->constantPool[superClassIdx].nameIndex < cpCount) {
        cls->superClassName = cls->constantPool[cls->constantPool[superClassIdx].nameIndex].strVal;
    }

    // Interfaces
    uint16_t ifCount = bs.readU2();
    for (uint16_t i = 0; i < ifCount; ++i) {
        uint16_t ifIdx = bs.readU2();
        if (ifIdx < cpCount && cls->constantPool[ifIdx].nameIndex < cpCount) {
            cls->interfaces.push_back(cls->constantPool[cls->constantPool[ifIdx].nameIndex].strVal);
        }
    }

    // Fields
    uint16_t fieldCount = bs.readU2();
    for (uint16_t i = 0; i < fieldCount; ++i) {
        FieldInfo fi;
        fi.accessFlags = bs.readU2();
        uint16_t nameIdx = bs.readU2();
        uint16_t descIdx = bs.readU2();
        if (nameIdx < cpCount) fi.name = cls->constantPool[nameIdx].strVal;
        if (descIdx < cpCount) fi.descriptor = cls->constantPool[descIdx].strVal;

        uint16_t attrCount = bs.readU2();
        for (uint16_t a = 0; a < attrCount; ++a) {
            bs.readU2(); // attrNameIdx
            uint32_t attrLen = bs.readU4();
            bs.skip(attrLen);
        }
        cls->fields[fi.name] = fi;
    }

    // Methods
    uint16_t methodCount = bs.readU2();
    for (uint16_t i = 0; i < methodCount; ++i) {
        MethodInfo mi;
        mi.accessFlags = bs.readU2();
        uint16_t nameIdx = bs.readU2();
        uint16_t descIdx = bs.readU2();
        if (nameIdx < cpCount) mi.name = cls->constantPool[nameIdx].strVal;
        if (descIdx < cpCount) mi.descriptor = cls->constantPool[descIdx].strVal;

        uint16_t attrCount = bs.readU2();
        for (uint16_t a = 0; a < attrCount; ++a) {
            uint16_t attrNameIdx = bs.readU2();
            uint32_t attrLen = bs.readU4();
            std::string attrName = (attrNameIdx < cpCount) ? cls->constantPool[attrNameIdx].strVal : "";

            if (attrName == "Code") {
                mi.maxStack = bs.readU2();
                mi.maxLocals = bs.readU2();
                uint32_t codeLen = bs.readU4();
                mi.code.resize(codeLen);
                bs.readBytes(mi.code.data(), codeLen);

                // Exception table (stored for ATHROW unwinding)
                uint16_t exTableLen = bs.readU2();
                mi.exTable.reserve(exTableLen);
                for (uint16_t e = 0; e < exTableLen; ++e) {
                    ExceptionEntry en;
                    en.startPc = bs.readU2(); en.endPc = bs.readU2();
                    en.handlerPc = bs.readU2(); en.catchType = bs.readU2();
                    mi.exTable.push_back(en);
                }

                // Code sub-attributes
                uint16_t subAttrCount = bs.readU2();
                for (uint16_t sa = 0; sa < subAttrCount; ++sa) {
                    bs.readU2();
                    uint32_t sal = bs.readU4();
                    bs.skip(sal);
                }
            } else {
                bs.skip(attrLen);
            }
        }
        std::string key = mi.name + ":" + mi.descriptor;
        cls->methods[key] = std::move(mi);
    }

    m_loadedClasses[cls->thisClassName] = cls;
    return cls;
}

std::shared_ptr<ClassFile> JvmBytecodeEngine::findOrLoadClass(const std::string& className, JarLoader* jar) {
    std::string normName = className;
    std::replace(normName.begin(), normName.end(), '.', '/');

    auto it = m_loadedClasses.find(normName);
    if (it != m_loadedClasses.end()) return it->second;

    if (!jar) return nullptr;

    std::string entryName = normName + ".class";
    std::vector<uint8_t> bytes;
    if (jar->extractEntry(entryName, bytes)) {
        return loadClass(bytes);
    }
    return nullptr;
}

static std::string getFieldKey(std::shared_ptr<ClassFile> cls, uint16_t fIdx) {
    if (!cls || fIdx >= cls->constantPool.size()) return "";
    const auto& cp = cls->constantPool[fIdx];
    std::string cname = cls->thisClassName;
    std::string fname = "";
    if (cp.classIndex < cls->constantPool.size() && cls->constantPool[cp.classIndex].nameIndex < cls->constantPool.size()) {
        cname = cls->constantPool[cls->constantPool[cp.classIndex].nameIndex].strVal;
    }
    if (cp.nameAndTypeIndex < cls->constantPool.size()) {
        const auto& nat = cls->constantPool[cp.nameAndTypeIndex];
        if (nat.nameIndex < cls->constantPool.size()) fname = cls->constantPool[nat.nameIndex].strVal;
    }
    return cname + ":" + fname;
}

static std::string getFieldName(std::shared_ptr<ClassFile> cls, uint16_t fIdx) {
    if (!cls || fIdx >= cls->constantPool.size()) return "";
    const auto& cp = cls->constantPool[fIdx];
    if (cp.nameAndTypeIndex < cls->constantPool.size()) {
        const auto& nat = cls->constantPool[cp.nameAndTypeIndex];
        if (nat.nameIndex < cls->constantPool.size()) return cls->constantPool[nat.nameIndex].strVal;
    }
    return "";
}

// ----------------------------------------------------
// Native Dispatcher for Standard CLDC 1.1 / MIDP 2.0
// ----------------------------------------------------
bool JvmBytecodeEngine::dispatchNativeMethod(const std::string& className, const std::string& methodName, const std::string& desc, const std::vector<JavaValue>& args, JavaValue& outResult, LcduiDisplay* display) {
    if (className == "java/lang/System") {
        if (methodName == "currentTimeMillis") {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            outResult = JavaValue((int64_t)now);
            return true;
        }
        if (methodName == "arraycopy" && args.size() >= 5) {
            JavaArray* src = getArray(args[0].asRef());
            int srcPos = args[1].asInt();
            JavaArray* dst = getArray(args[2].asRef());
            int dstPos = args[3].asInt();
            int len = args[4].asInt();
            if (src && dst && len > 0) {
                // Use memmove-safe temp copy for overlapping regions
                if (!src->intData.empty() && !dst->intData.empty()) {
                    std::vector<int32_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->intData.size())?src->intData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->intData.size()) dst->intData[dstPos+k]=tmp[k];
                } else if (!src->byteData.empty() && !dst->byteData.empty()) {
                    std::vector<uint8_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->byteData.size())?src->byteData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->byteData.size()) dst->byteData[dstPos+k]=tmp[k];
                } else if (!src->charData.empty() && !dst->charData.empty()) {
                    std::vector<uint16_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->charData.size())?src->charData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->charData.size()) dst->charData[dstPos+k]=tmp[k];
                } else if (!src->shortData.empty() && !dst->shortData.empty()) {
                    std::vector<int16_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->shortData.size())?src->shortData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->shortData.size()) dst->shortData[dstPos+k]=tmp[k];
                } else if (!src->longData.empty() && !dst->longData.empty()) {
                    std::vector<int64_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->longData.size())?src->longData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->longData.size()) dst->longData[dstPos+k]=tmp[k];
                } else if (!src->floatData.empty() && !dst->floatData.empty()) {
                    std::vector<float> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->floatData.size())?src->floatData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->floatData.size()) dst->floatData[dstPos+k]=tmp[k];
                } else if (!src->doubleData.empty() && !dst->doubleData.empty()) {
                    std::vector<double> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->doubleData.size())?src->doubleData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->doubleData.size()) dst->doubleData[dstPos+k]=tmp[k];
                } else if (!src->refData.empty() && !dst->refData.empty()) {
                    std::vector<uint32_t> tmp; tmp.reserve(len);
                    for (int k = 0; k < len; ++k) tmp.push_back((srcPos+k>=0&&srcPos+k<(int)src->refData.size())?src->refData[srcPos+k]:0);
                    for (int k = 0; k < len; ++k) if (dstPos+k>=0&&dstPos+k<(int)dst->refData.size()) dst->refData[dstPos+k]=tmp[k];
                } else {
                    // Heterogeneous (boolean<->byte): copy via int coercion
                    for (int k = 0; k < len; ++k) {
                        int32_t v = 0;
                        if (!src->intData.empty() && srcPos+k<(int)src->intData.size()) v=src->intData[srcPos+k];
                        else if (!src->byteData.empty() && srcPos+k<(int)src->byteData.size()) v=src->byteData[srcPos+k];
                        else if (!src->shortData.empty() && srcPos+k<(int)src->shortData.size()) v=src->shortData[srcPos+k];
                        else if (!src->charData.empty() && srcPos+k<(int)src->charData.size()) v=src->charData[srcPos+k];
                        if (!dst->intData.empty() && dstPos+k<(int)dst->intData.size()) dst->intData[dstPos+k]=v;
                        else if (!dst->byteData.empty() && dstPos+k<(int)dst->byteData.size()) dst->byteData[dstPos+k]=(uint8_t)v;
                        else if (!dst->shortData.empty() && dstPos+k<(int)dst->shortData.size()) dst->shortData[dstPos+k]=(int16_t)v;
                        else if (!dst->charData.empty() && dstPos+k<(int)dst->charData.size()) dst->charData[dstPos+k]=(uint16_t)v;
                    }
                }
            }
            return true;
        }
        if (methodName == "gc") return true;
        if (methodName == "identityHashCode") {
            outResult = JavaValue(args.size() > 0 ? (int32_t)args[0].asRef() : 0);
            return true;
        }
        if (methodName == "getProperty") {
            std::string prop = args.size() >= 1 ? getString(args[0].asRef()) : "";
            if (prop == "microedition.platform") outResult = JavaValue(createString("NokiaN73"), true);
            else if (prop == "microedition.profiles") outResult = JavaValue(createString("MIDP-2.0"), true);
            else if (prop == "microedition.configuration") outResult = JavaValue(createString("CLDC-1.1"), true);
            else if (prop == "microedition.locale") outResult = JavaValue(createString("vi-VN"), true);
            else if (prop == "microedition.encoding") outResult = JavaValue(createString("UTF-8"), true);
            else outResult = JavaValue(createString(""), true);
            return true;
        }
        if (methodName == "exit") return true;
    }

    if (className == "java/lang/Math") {
        // Prefer full double-precision dispatcher; fall back to int fast-path
        if (FullApis::dispatch(className, methodName, desc, args, outResult, display)) return true;
        if (methodName == "abs") { outResult = JavaValue(std::abs(args[0].asInt())); return true; }
        if (methodName == "min") { outResult = JavaValue(std::min(args[0].asInt(), args[1].asInt())); return true; }
        if (methodName == "max") { outResult = JavaValue(std::max(args[0].asInt(), args[1].asInt())); return true; }
        if (methodName == "sqrt") { outResult = JavaValue((float)std::sqrt(args[0].asInt())); return true; }
        if (methodName == "sin") { outResult = JavaValue((float)std::sin(args[0].asInt())); return true; }
        if (methodName == "cos") { outResult = JavaValue((float)std::cos(args[0].asInt())); return true; }
    }

    if (className == "java/lang/String") {
        if (methodName == "valueOf") {
            if (args.size() >= 1) {
                outResult = JavaValue(createString(std::to_string(args[0].asInt())), true);
            } else {
                outResult = JavaValue(createString(""), true);
            }
            return true;
        }
        if (methodName == "length") {
            std::string s = getString(args[0].asRef());
            outResult = JavaValue((int32_t)s.length());
            return true;
        }
        if (methodName == "charAt" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int idx = args[1].asInt();
            outResult = JavaValue((idx >= 0 && idx < (int)s.length()) ? (int32_t)(uint8_t)s[idx] : 0);
            return true;
        }
        if (methodName == "substring" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int begin = args[1].asInt();
            int end = args.size() >= 3 ? args[2].asInt() : (int)s.length();
            if (begin >= 0 && begin <= (int)s.length() && end >= begin && end <= (int)s.length()) {
                outResult = JavaValue(createString(s.substr(begin, end - begin)), true);
            } else {
                outResult = JavaValue(createString(""), true);
            }
            return true;
        }
        if (methodName == "indexOf" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            char c = (char)args[1].asInt();
            size_t pos = s.find(c);
            outResult = JavaValue(pos != std::string::npos ? (int32_t)pos : -1);
            return true;
        }
        if (methodName == "concat" && args.size() >= 2) {
            std::string s1 = getString(args[0].asRef());
            std::string s2 = getString(args[1].asRef());
            outResult = JavaValue(createString(s1 + s2), true);
            return true;
        }
        if (methodName == "equals" && args.size() >= 2) {
            std::string s1 = getString(args[0].asRef());
            std::string s2 = getString(args[1].asRef());
            outResult = JavaValue(s1 == s2 ? 1 : 0);
            return true;
        }
        if (methodName == "equalsIgnoreCase" && args.size() >= 2) {
            std::string s1 = getString(args[0].asRef());
            std::string s2 = getString(args[1].asRef());
            std::string l1 = s1, l2 = s2;
            std::transform(l1.begin(), l1.end(), l1.begin(), ::tolower);
            std::transform(l2.begin(), l2.end(), l2.begin(), ::tolower);
            outResult = JavaValue(l1 == l2 ? 1 : 0);
            return true;
        }
        if (methodName == "getBytes") {
            std::string s = getString(args[0].asRef());
            uint32_t arrRef = allocArray(8, (int)s.length());
            JavaArray* arr = getArray(arrRef);
            if (arr) {
                for (size_t i = 0; i < s.length(); ++i) arr->byteData[i] = (uint8_t)s[i];
            }
            outResult = JavaValue(arrRef, true);
            return true;
        }
    }

    if (className == "java/lang/StringBuffer" || className == "java/lang/StringBuilder") {
        if (methodName == "<init>") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj && args.size() >= 2 && args[1].type == JavaValue::OBJ_REF) {
                obj->stringVal = getString(args[1].asRef());
            }
            return true;
        }
        if (methodName == "append") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj && args.size() >= 2) {
                if (args[1].type == JavaValue::OBJ_REF) {
                    obj->stringVal += getString(args[1].asRef());
                } else {
                    obj->stringVal += std::to_string(args[1].asInt());
                }
            }
            outResult = JavaValue(args[0].asRef(), true);
            return true;
        }
        if (methodName == "toString") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(createString(obj ? obj->stringVal : ""), true);
            return true;
        }
        if (methodName == "length") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(obj ? (int32_t)obj->stringVal.length() : 0);
            return true;
        }
        if (methodName == "setLength" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) {
                int newLen = args[1].asInt();
                if (newLen >= 0 && (size_t)newLen <= obj->stringVal.length()) {
                    obj->stringVal.resize(newLen);
                }
            }
            return true;
        }
    }

    if (className == "java/lang/Integer") {
        if (methodName == "parseInt" && args.size() >= 1) {
            std::string s = getString(args[0].asRef());
            try {
                outResult = JavaValue((int32_t)std::stoi(s));
            } catch (...) {
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "toString" && args.size() >= 1) {
            outResult = JavaValue(createString(std::to_string(args[0].asInt())), true);
            return true;
        }
    }

    if (className == "java/util/Random") {
        if (methodName == "<init>") return true;
        if (methodName == "nextInt") {
            int maxVal = args.size() >= 2 ? args[1].asInt() : 0;
            if (maxVal > 0) {
                outResult = JavaValue(std::rand() % maxVal);
            } else {
                outResult = JavaValue(std::rand());
            }
            return true;
        }
    }

    if (className == "java/util/Vector") {
        if (methodName == "<init>") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) {
                uint32_t arrRef = allocArray(0, 16);
                obj->fields["elements"] = JavaValue(arrRef, true);
                obj->fields["elementCount"] = JavaValue(0);
            }
            return true;
        }
        if (methodName == "addElement" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) {
                int count = obj->fields["elementCount"].asInt();
                uint32_t arrRef = obj->fields["elements"].asRef();
                JavaArray* arr = getArray(arrRef);
                if (arr) {
                    if (count >= (int)arr->refData.size()) {
                        arr->refData.resize(std::max(16, (int)arr->refData.size() * 2), 0);
                    }
                    arr->refData[count] = args[1].asRef();
                    obj->fields["elementCount"] = JavaValue(count + 1);
                }
            }
            return true;
        }
        if (methodName == "elementAt" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            int idx = args[1].asInt();
            if (obj) {
                uint32_t arrRef = obj->fields["elements"].asRef();
                JavaArray* arr = getArray(arrRef);
                if (arr && idx >= 0 && idx < (int)arr->refData.size()) {
                    outResult = JavaValue(arr->refData[idx], true);
                    return true;
                }
            }
            outResult = JavaValue(0, true);
            return true;
        }
        if (methodName == "size") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(obj ? obj->fields["elementCount"].asInt() : 0);
            return true;
        }
        if (methodName == "removeAllElements") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) obj->fields["elementCount"] = JavaValue(0);
            return true;
        }
    }

    if (className == "javax/microedition/lcdui/Display") {
        if (methodName == "getDisplay") {
            outResult = JavaValue(allocObject("javax/microedition/lcdui/Display"), true);
            return true;
        }
        if (methodName == "setCurrent" && args.size() >= 2) {
            uint32_t nextRef = args[1].asRef();
            JavaObject* canvasObj = getObject(nextRef);
            if (canvasObj && !canvasObj->className.empty()) {
                auto cls = findOrLoadClass(canvasObj->className, m_activeJar);
                JvmInterpreter::getInstance().setCurrentCanvas(nextRef, cls);
            }
            return true;
        }
        if (methodName == "getCurrent") {
            outResult = JavaValue(0, true);
            return true;
        }
        if (methodName == "isColor") { outResult = JavaValue(1); return true; }
        if (methodName == "numColors") { outResult = JavaValue(16777216); return true; }
        if (methodName == "vibrate" || methodName == "flashBacklight") { outResult = JavaValue(1); return true; }
        if (methodName == "callSerially" && args.size() >= 2) {
            uint32_t rRef = args[1].asRef();
            JavaObject* rObj = getObject(rRef);
            if (rObj) {
                auto cls = findOrLoadClass(rObj->className, m_activeJar);
                if (cls) JvmInterpreter::getInstance().registerRunnable(rRef, cls);
            }
            return true;
        }
    }

    if (className == "javax/microedition/lcdui/Image") {
        if (methodName == "createImage") {
            if (desc.find("(Ljava/lang/String;)") != std::string::npos && args.size() >= 1) {
                std::string path = getString(args[0].asRef());
                outResult = JavaValue(loadNativeImageFromJar(path), true);
                return true;
            }
            if (desc.find("([BII)") != std::string::npos && args.size() >= 3) {
                JavaArray* arr = getArray(args[0].asRef());
                int offset = args[1].asInt();
                int len = args[2].asInt();
                if (arr && offset >= 0 && offset + len <= (int)arr->byteData.size()) {
                    outResult = JavaValue(loadNativeImageFromBytes(arr->byteData.data() + offset, len), true);
                } else {
                    outResult = JavaValue(allocateNativeImage(16, 16, false), true);
                }
                return true;
            }
            if (desc.find("(II)") != std::string::npos && args.size() >= 2) {
                int w = args[0].asInt(), h = args[1].asInt();
                outResult = JavaValue(allocateNativeImage(w, h, true), true);
                return true;
            }
            if (args.size() >= 6) {
                NativeImage* src = getNativeImage(args[0].asRef());
                int x = args[1].asInt(), y = args[2].asInt(), w = args[3].asInt(), h = args[4].asInt();
                uint32_t resRef = allocateNativeImage(w, h, false);
                NativeImage* dst = getNativeImage(resRef);
                if (src && dst && w > 0 && h > 0) {
                    for (int r = 0; r < h; ++r) {
                        for (int c = 0; c < w; ++c) {
                            int sx = x + c, sy = y + r;
                            if (sx >= 0 && sx < src->width && sy >= 0 && sy < src->height) {
                                dst->pixels[r * w + c] = src->pixels[sy * src->width + sx];
                            }
                        }
                    }
                }
                outResult = JavaValue(resRef, true);
                return true;
            }
        }
        if (methodName == "createRGBImage" && args.size() >= 4) {
            JavaArray* arr = getArray(args[0].asRef());
            int w = args[1].asInt(), h = args[2].asInt();
            bool alpha = args[3].asInt() != 0;
            uint32_t resRef = allocateNativeImage(w, h, false);
            NativeImage* dst = getNativeImage(resRef);
            if (arr && dst && (int)arr->intData.size() >= w * h) {
                for (int i = 0; i < w * h; ++i) {
                    uint32_t p = (uint32_t)arr->intData[i];
                    if (!alpha) p |= 0xFF000000;
                    dst->pixels[i] = p;
                }
            }
            outResult = JavaValue(resRef, true);
            return true;
        }
        if (methodName == "getWidth") {
            NativeImage* img = getNativeImage(args[0].asRef());
            outResult = JavaValue(img ? img->width : 16);
            return true;
        }
        if (methodName == "getHeight") {
            NativeImage* img = getNativeImage(args[0].asRef());
            outResult = JavaValue(img ? img->height : 16);
            return true;
        }
        if (methodName == "getGraphics") {
            outResult = JavaValue(allocObject("javax/microedition/lcdui/Graphics"), true);
            return true;
        }
        if (methodName == "isMutable") {
            NativeImage* img = getNativeImage(args[0].asRef());
            outResult = JavaValue(img && img->isMutable ? 1 : 0);
            return true;
        }
        if (methodName == "getRGB") {
            NativeImage* img = getNativeImage(args[0].asRef());
            JavaArray* arr = getArray(args[1].asRef());
            int offset = args[2].asInt(), scanlength = args[3].asInt(), x = args[4].asInt(), y = args[5].asInt(), width = args[6].asInt(), height = args[7].asInt();
            if (img && arr) {
                for (int r = 0; r < height; ++r) {
                    for (int c = 0; c < width; ++c) {
                        int srcIdx = (y + r) * img->width + (x + c);
                        int dstIdx = offset + r * scanlength + c;
                        if (srcIdx < (int)img->pixels.size() && dstIdx < (int)arr->intData.size()) {
                            arr->intData[dstIdx] = img->pixels[srcIdx];
                        }
                    }
                }
            }
            return true;
        }
    }

    if (className == "javax/microedition/lcdui/Graphics") {
        if (!display) return true;
        if (methodName == "setColor") {
            if (args.size() == 2) {
                display->setColor((uint32_t)(args[1].asInt() | 0xFF000000));
            } else if (args.size() >= 4) {
                uint32_t r = (args[1].asInt() & 0xFF), g = (args[2].asInt() & 0xFF), b = (args[3].asInt() & 0xFF);
                display->setColor(0xFF000000 | (r << 16) | (g << 8) | b);
            }
            return true;
        }
        if (methodName == "getColor") {
            outResult = JavaValue((int32_t)(display->getColor() & 0x00FFFFFF));
            return true;
        }
        if (methodName == "fillRect" && args.size() >= 5) {
            display->fillRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawRect" && args.size() >= 5) {
            display->drawRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawLine" && args.size() >= 5) {
            display->drawLine(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawRoundRect" && args.size() >= 7) {
            display->drawRoundRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), display->getColor());
            return true;
        }
        if (methodName == "fillRoundRect" && args.size() >= 7) {
            display->fillRoundRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawArc" && args.size() >= 7) {
            display->drawArc(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), display->getColor());
            return true;
        }
        if (methodName == "fillArc" && args.size() >= 7) {
            display->fillArc(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawString" && args.size() >= 5) {
            std::string text = getString(args[1].asRef());
            display->drawString(text, args[2].asInt(), args[3].asInt(), args[4].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawSubstring" && args.size() >= 7) {
            std::string text = getString(args[1].asRef());
            int off = args[2].asInt(), len = args[3].asInt();
            if (off >= 0 && off + len <= (int)text.length()) {
                display->drawString(text.substr(off, len), args[4].asInt(), args[5].asInt(), args[6].asInt(), display->getColor());
            }
            return true;
        }
        if (methodName == "drawChar" && args.size() >= 5) {
            display->drawChar((char)args[1].asInt(), args[2].asInt(), args[3].asInt(), display->getColor());
            return true;
        }
        if (methodName == "drawImage" && args.size() >= 5) {
            NativeImage* img = getNativeImage(args[1].asRef());
            if (img && !img->pixels.empty()) {
                display->drawRegion(img->pixels.data(), img->width, img->height, 0, 0, img->width, img->height, 0, args[2].asInt(), args[3].asInt(), args[4].asInt());
            }
            return true;
        }
        if (methodName == "drawRegion" && args.size() >= 10) {
            NativeImage* img = getNativeImage(args[1].asRef());
            if (img && !img->pixels.empty()) {
                display->drawRegion(img->pixels.data(), img->width, img->height, args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), args[7].asInt(), args[8].asInt(), args[9].asInt());
            }
            return true;
        }
        if (methodName == "drawRGB" && args.size() >= 9) {
            JavaArray* arr = getArray(args[1].asRef());
            if (arr && !arr->intData.empty()) {
                display->drawRGB(arr->intData.data(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), args[7].asInt(), args[8].asInt() != 0);
            }
            return true;
        }
        if (methodName == "setClip" && args.size() >= 5) {
            display->setClip(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt());
            return true;
        }
        if (methodName == "clipRect" && args.size() >= 5) {
            display->clipRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt());
            return true;
        }
        if (methodName == "getClipX") { outResult = JavaValue(display->getClip().x); return true; }
        if (methodName == "getClipY") { outResult = JavaValue(display->getClip().y); return true; }
        if (methodName == "getClipWidth") { outResult = JavaValue(display->getClip().width); return true; }
        if (methodName == "getClipHeight") { outResult = JavaValue(display->getClip().height); return true; }
        if (methodName == "setFont" || methodName == "translate") return true;
    }

    if (className == "javax/microedition/lcdui/Font") {
        if (methodName == "getFont" || methodName == "getDefaultFont") {
            outResult = JavaValue(allocObject("javax/microedition/lcdui/Font"), true);
            return true;
        }
        if (methodName == "getHeight") { outResult = JavaValue(12); return true; }
        if (methodName == "getBaselinePosition") { outResult = JavaValue(10); return true; }
        if (methodName == "stringWidth") {
            std::string s = getString(args[1].asRef());
            // Unicode: ask CoreText for real width (Vietnamese combining marks)
            if (native_text_measure) {
                bool nonAscii = false;
                for (unsigned char c : s) if (c < 32 || c > 126) { nonAscii = true; break; }
                if (nonAscii) {
                    int w = 0, h = 0;
                    if (native_text_measure(s.c_str(), 12, &w, &h) && w > 0) {
                        outResult = JavaValue((int32_t)w);
                        return true;
                    }
                }
            }
            outResult = JavaValue((int32_t)(s.length() * 7));
            return true;
        }
        if (methodName == "charWidth") { outResult = JavaValue(7); return true; }
        if (methodName == "charsWidth" && args.size() >= 4) {
            outResult = JavaValue((int32_t)(args[3].asInt() * 7));
            return true;
        }
    }

    if (className.find("Canvas") != std::string::npos || className.find("Displayable") != std::string::npos) {
        if (methodName == "repaint" || methodName == "flushGraphics" || methodName == "serviceRepaints") {
            if (display) {
                uint32_t cRef = args.size() >= 1 ? args[0].asRef() : 0;
                JavaObject* cObj = getObject(cRef);
                std::shared_ptr<ClassFile> cCls = nullptr;
                if (cObj && !cObj->className.empty()) {
                    cCls = findOrLoadClass(cObj->className, m_activeJar);
                }
                if (cCls && cRef != 0) {
                    uint32_t gRef = allocObject("javax/microedition/lcdui/Graphics");
                    executeMethod(cCls, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", { JavaValue(cRef, true), JavaValue(gRef, true) }, display);
                }
            }
            return true;
        }
        if (methodName == "getWidth") { outResult = JavaValue(display ? display->getWidth() : 240); return true; }
        if (methodName == "getHeight") { outResult = JavaValue(display ? display->getHeight() : 320); return true; }
        if (methodName == "isDoubleBuffered") { outResult = JavaValue(1); return true; }
        if (methodName == "hasPointerEvents") { outResult = JavaValue(1); return true; }
        if (methodName == "hasPointerMotionEvents") { outResult = JavaValue(1); return true; }
        if (methodName == "hasRepeatEvents") { outResult = JavaValue(1); return true; }
        if (methodName == "setFullScreenMode") return true;
        if (methodName == "getGraphics") {
            outResult = JavaValue(allocObject("javax/microedition/lcdui/Graphics"), true);
            return true;
        }
        if (methodName == "getKeyStates") {
            outResult = JavaValue(0);
            return true;
        }
        if (methodName == "getGameAction") {
            int code = args.size() >= 2 ? args[1].asInt() : 0;
            int action = 0;
            if (code == -1 || code == '2') action = 1; // UP
            else if (code == -2 || code == '8') action = 6; // DOWN
            else if (code == -3 || code == '4') action = 2; // LEFT
            else if (code == -4 || code == '6') action = 5; // RIGHT
            else if (code == -5 || code == '5') action = 8; // FIRE
            outResult = JavaValue(action);
            return true;
        }
        if (methodName == "getKeyCode") {
            int action = args.size() >= 2 ? args[1].asInt() : 0;
            int code = 0;
            if (action == 1) code = -1; // UP
            else if (action == 6) code = -2; // DOWN
            else if (action == 2) code = -3; // LEFT
            else if (action == 5) code = -4; // RIGHT
            else if (action == 8) code = -5; // FIRE
            outResult = JavaValue(code);
            return true;
        }
    }

    if (className == "java/lang/Class") {
        if (methodName == "getResourceAsStream") {
            std::string path = args.size() >= 2 ? getString(args[1].asRef()) : "";
            if (m_activeJar && !path.empty()) {
                if (path[0] == '/') path.erase(0, 1);
                std::vector<uint8_t> bytes;
                if (m_activeJar->extractEntry(path, bytes)) {
                    uint32_t isRef = allocObject("java/io/ByteArrayInputStream");
                    uint32_t arrRef = allocArray(8, (int)bytes.size());
                    JavaArray* arr = getArray(arrRef);
                    if (arr) arr->byteData = std::move(bytes);
                    JavaObject* obj = getObject(isRef);
                    if (obj) {
                        obj->fields["buf"] = JavaValue(arrRef, true);
                        obj->fields["pos"] = JavaValue(0);
                    }
                    outResult = JavaValue(isRef, true);
                    return true;
                }
            }
            outResult = JavaValue(0, true);
            return true;
        }
    }

    if (className == "java/io/InputStream" || className == "java/io/ByteArrayInputStream" || className == "java/io/DataInputStream") {
        if (methodName == "<init>") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj && args.size() >= 2 && args[1].type == JavaValue::OBJ_REF) {
                JavaArray* arr = getArray(args[1].asRef());
                if (arr) {
                    obj->fields["buf"] = args[1];
                    obj->fields["pos"] = JavaValue(0);
                } else {
                    JavaObject* innerStream = getObject(args[1].asRef());
                    if (innerStream) {
                        obj->fields["buf"] = innerStream->fields["buf"];
                        obj->fields["pos"] = innerStream->fields["pos"];
                    }
                }
            }
            return true;
        }
        if (methodName == "read") {
            JavaObject* obj = getObject(args[0].asRef());
            if (!obj) { outResult = JavaValue(-1); return true; }
            // Socket-backed: refill from live TCP when buffer exhausted
            {
                auto sfi = obj->fields.find("sockFd");
                if (sfi != obj->fields.end() && sfi->second.asInt() >= 0) {
                    JavaArray* ba = getArray(obj->fields["buf"].asRef());
                    int pp = obj->fields["pos"].asInt();
                    bool empty = !ba || pp < 0 || pp >= (int)ba->byteData.size();
                    if (empty) {
#if !defined(_WIN32) && !defined(_WIN64)
                        int fd = sfi->second.asInt();
                        // MIDP blocking read: wait up to ~8s like real phones,
                        // reconnect once on orderly close (online games drop idle sockets)
                        for (int attempt = 0; attempt < 2; ++attempt) {
                            bool gotClose = false;
                            for (int w = 0; w < 40; ++w) {
                                fd_set rs; FD_ZERO(&rs); FD_SET(fd, &rs);
                                struct timeval tv{0, 200000};
                                int r = select(fd+1, &rs, nullptr, nullptr, &tv);
                                if (r > 0 && FD_ISSET(fd, &rs)) {
                                    uint8_t tmp[4096]; ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
                                    if (n > 0) {
                                        uint32_t na = allocArray(8, (int)n);
                                        JavaArray* naa = getArray(na);
                                        if (naa) naa->byteData.assign(tmp, tmp + n);
                                        obj->fields["buf"] = JavaValue(na, true);
                                        obj->fields["pos"] = JavaValue(0);
                                    } else if (n == 0) {
                                        gotClose = true;
                                    }
                                    break;
                                }
                            }
                            if (!gotClose) break;
                            int nfd = FullApis::reconnectSocket(args[0].asRef());
                            if (nfd < 0) break;
                            fd = nfd;
                        }
#endif
                    }
                }
            }
            JavaArray* arr = getArray(obj->fields["buf"].asRef());
            int pos = obj->fields["pos"].asInt();
            if (arr && pos >= 0 && pos < (int)arr->byteData.size()) {
                if (args.size() == 1) {
                    outResult = JavaValue((int32_t)(uint8_t)arr->byteData[pos]);
                    obj->fields["pos"] = JavaValue(pos + 1);
                } else if (args.size() == 2) {
                    JavaArray* dst = getArray(args[1].asRef());
                    int len = dst ? (int)dst->byteData.size() : 0;
                    int available = (int)arr->byteData.size() - pos;
                    int count = std::min(len, available);
                    if (count <= 0) { outResult = JavaValue(-1); return true; }
                    for (int i = 0; i < count; ++i) dst->byteData[i] = arr->byteData[pos + i];
                    obj->fields["pos"] = JavaValue(pos + count);
                    outResult = JavaValue(count);
                } else if (args.size() >= 4) {
                    JavaArray* dst = getArray(args[1].asRef());
                    int off = args[2].asInt(), len = args[3].asInt();
                    int available = (int)arr->byteData.size() - pos;
                    int count = std::min(len, available);
                    if (count <= 0) { outResult = JavaValue(-1); return true; }
                    if (dst) {
                        for (int i = 0; i < count; ++i) {
                            if (off + i < (int)dst->byteData.size()) dst->byteData[off + i] = arr->byteData[pos + i];
                        }
                    }
                    obj->fields["pos"] = JavaValue(pos + count);
                    outResult = JavaValue(count);
                }
            } else {
                outResult = JavaValue(-1);
            }
            return true;
        }
        if (methodName == "readByte" || methodName == "readUnsignedByte") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos >= 0 && pos < (int)arr->byteData.size()) {
                uint8_t b = arr->byteData[pos];
                obj->fields["pos"] = JavaValue(pos + 1);
                outResult = JavaValue(methodName == "readByte" ? (int32_t)(int8_t)b : (int32_t)b);
            } else {
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readBoolean") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos >= 0 && pos < (int)arr->byteData.size()) {
                uint8_t b = arr->byteData[pos];
                obj->fields["pos"] = JavaValue(pos + 1);
                outResult = JavaValue(b != 0 ? 1 : 0);
            } else {
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readShort" || methodName == "readUnsignedShort") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos + 1 < (int)arr->byteData.size()) {
                uint16_t s = ((uint16_t)arr->byteData[pos] << 8) | arr->byteData[pos + 1];
                obj->fields["pos"] = JavaValue(pos + 2);
                outResult = JavaValue(methodName == "readShort" ? (int32_t)(int16_t)s : (int32_t)s);
            } else {
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readInt") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos + 3 < (int)arr->byteData.size()) {
                int32_t val = ((int32_t)arr->byteData[pos] << 24) |
                              ((int32_t)arr->byteData[pos + 1] << 16) |
                              ((int32_t)arr->byteData[pos + 2] << 8) |
                              ((int32_t)arr->byteData[pos + 3]);
                obj->fields["pos"] = JavaValue(pos + 4);
                outResult = JavaValue(val);
            } else {
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readLong") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos + 7 < (int)arr->byteData.size()) {
                int64_t val = 0;
                for (int i = 0; i < 8; ++i) val = (val << 8) | arr->byteData[pos + i];
                obj->fields["pos"] = JavaValue(pos + 8);
                outResult = JavaValue(val);
            } else {
                outResult = JavaValue((int64_t)0);
            }
            return true;
        }
        if (methodName == "readUTF") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            if (arr && pos + 1 < (int)arr->byteData.size()) {
                uint16_t len = ((uint16_t)arr->byteData[pos] << 8) | arr->byteData[pos + 1];
                pos += 2;
                std::string s = "";
                if (pos + len <= (int)arr->byteData.size()) {
                    s = std::string((char*)(arr->byteData.data() + pos), len);
                    pos += len;
                }
                obj->fields["pos"] = JavaValue(pos);
                outResult = JavaValue(createString(s), true);
            } else {
                outResult = JavaValue(createString(""), true);
            }
            return true;
        }
        if (methodName == "readFully" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            JavaArray* dst = getArray(args[1].asRef());
            int off = args.size() >= 4 ? args[2].asInt() : 0;
            int len = args.size() >= 4 ? args[3].asInt() : (dst ? (int)dst->byteData.size() : 0);
            if (arr && dst) {
                int count = std::min(len, (int)arr->byteData.size() - pos);
                for (int i = 0; i < count; ++i) {
                    if (off + i < (int)dst->byteData.size() && pos + i < (int)arr->byteData.size()) {
                        dst->byteData[off + i] = arr->byteData[pos + i];
                    }
                }
                obj->fields["pos"] = JavaValue(pos + count);
            }
            return true;
        }
        if (methodName == "skip" || methodName == "skipBytes") {
            JavaObject* obj = getObject(args[0].asRef());
            int64_t n = args.size() >= 2 ? args[1].asLong() : 0;
            if (obj && n > 0) {
                int pos = obj->fields["pos"].asInt();
                obj->fields["pos"] = JavaValue(pos + (int)n);
            }
            outResult = JavaValue(n);
            return true;
        }
        if (methodName == "available") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int avail = arr ? std::max(0, (int)arr->byteData.size() - pos) : 0;
#if !defined(_WIN32) && !defined(_WIN64)
            // Socket streams: peek kernel buffer so game loops see live data
            if (avail == 0 && obj) {
                auto sfi = obj->fields.find("sockFd");
                if (sfi != obj->fields.end() && sfi->second.asInt() >= 0) {
                    uint8_t tmp[2048];
                    ssize_t n = recv(sfi->second.asInt(), tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT);
                    if (n > 0) avail = (int)n;
                }
            }
#endif
            outResult = JavaValue(avail);
            return true;
        }
        if (methodName == "close") return true;
    }

    if (className == "java/lang/Thread") {
        if (methodName == "<init>") {
            if (args.size() >= 2 && args[1].type == JavaValue::OBJ_REF) {
                JavaObject* tObj = getObject(args[0].asRef());
                if (tObj) tObj->fields["target"] = args[1];
            }
            return true;
        }
        if (methodName == "currentThread") {
            outResult = JavaValue(allocObject("java/lang/Thread"), true);
            return true;
        }
        if (methodName == "sleep") {
            int64_t ms = args.size() >= 1 ? args[0].asLong() : 10;
            if (ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(std::min<int64_t>(ms, 500)));
            }
            return true;
        }
        if (methodName == "yield") {
            std::this_thread::yield();
            return true;
        }
        if (methodName == "start") {
            if (args.size() >= 1 && args[0].asRef() != 0) {
                uint32_t tRef = args[0].asRef();
                JavaObject* tObj = getObject(tRef);
                uint32_t rRef = tRef;
                if (tObj && tObj->fields.find("target") != tObj->fields.end() && tObj->fields["target"].asRef() != 0) {
                    rRef = tObj->fields["target"].asRef();
                }
                JavaObject* rObj = getObject(rRef);
                if (rObj && !rObj->className.empty()) {
                    auto rCls = findOrLoadClass(rObj->className, m_activeJar);
                    if (rCls) {
                        JvmInterpreter::getInstance().registerRunnable(rRef, rCls);
                    }
                }
            }
            return true;
        }
    }

    if (className == "javax/microedition/rms/RecordStore") {
        if (methodName == "openRecordStore") {
            std::string name = getString(args[0].asRef());
            bool create = args.size() > 1 ? (args[1].asInt() != 0) : true;
            bool ok = RmsStorage::getInstance().openRecordStore("J2MEApp", name, create);
            if (ok) {
                uint32_t ref = allocObject("javax/microedition/rms/RecordStore");
                JavaObject* obj = getObject(ref);
                if (obj) obj->stringVal = name;
                outResult = JavaValue(ref, true);
            } else {
                outResult = JavaValue(0, true);
            }
            return true;
        }
        if (methodName == "closeRecordStore") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) RmsStorage::getInstance().closeRecordStore(obj->stringVal);
            return true;
        }
        if (methodName == "addRecord") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = getArray(args[1].asRef());
            int off = args[2].asInt(), len = args[3].asInt();
            if (obj && arr && off >= 0 && off + len <= (int)arr->byteData.size()) {
                int recId = RmsStorage::getInstance().addRecord(obj->stringVal, arr->byteData.data() + off, len);
                outResult = JavaValue(recId);
            } else {
                outResult = JavaValue(1);
            }
            return true;
        }
        if (methodName == "getRecord") {
            JavaObject* obj = getObject(args[0].asRef());
            int recId = args[1].asInt();
            std::vector<uint8_t> data;
            if (obj && RmsStorage::getInstance().getRecord(obj->stringVal, recId, data)) {
                uint32_t arrRef = allocArray(8, (int)data.size());
                JavaArray* arr = getArray(arrRef);
                if (arr) arr->byteData = std::move(data);
                outResult = JavaValue(arrRef, true);
            } else {
                outResult = JavaValue(0, true);
            }
            return true;
        }
        if (methodName == "getNumRecords") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(obj ? RmsStorage::getInstance().getNumRecords(obj->stringVal) : 0);
            return true;
        }
    }

    if (className == "javax/microedition/media/Manager") {
        if (FullApis::dispatch(className, methodName, desc, args, outResult, display)) return true;
        if (methodName == "createPlayer") {
            outResult = JavaValue(allocObject("javax/microedition/media/Player"), true);
            return true;
        }
        if (methodName == "playTone") return true;
    }

    if (className == "javax/microedition/media/Player") {
        if (methodName == "start" || methodName == "stop" || methodName == "close" || methodName == "prefetch" || methodName == "realize" || methodName == "setLoopCount") {
            if (FullApis::dispatch(className, methodName, desc, args, outResult, display)) return true;
            return true;
        }
    }

    // Full J2ME API coverage: game/M3G/Micro3D/Nokia/IO/WMA/BT/high-level LCDUI/Hashtable/etc.
    if (FullApis::dispatch(className, methodName, desc, args, outResult, display)) return true;

    return false;
}

// ----------------------------------------------------
// Complete JVM Opcode Execution Loop
// ----------------------------------------------------
JavaValue JvmBytecodeEngine::executeMethod(std::shared_ptr<ClassFile> cls, const std::string& methodName, const std::string& desc, const std::vector<JavaValue>& args, LcduiDisplay* display) {
    if (!cls) return JavaValue(0);

    std::string key = methodName + ":" + desc;
    auto it = cls->methods.find(key);
    if (it == cls->methods.end()) {
        // Try native dispatch
        JavaValue res;
        if (dispatchNativeMethod(cls->thisClassName, methodName, desc, args, res, display)) {
            return res;
        }
        return JavaValue(0);
    }

    const MethodInfo& method = it->second;
    if (method.code.empty()) return JavaValue(0);

    StackFrame frame;
    frame.classRef = cls;
    frame.method = &method;
    frame.locals.resize(std::max<size_t>(method.maxLocals, args.size()), JavaValue(0));
    for (size_t i = 0; i < args.size(); ++i) frame.locals[i] = args[i];

    const uint8_t* code = method.code.data();
    size_t codeLen = method.code.size();

    while (!m_cancel.load() && frame.pc >= 0 && (size_t)frame.pc < codeLen) {
        uint8_t op = code[frame.pc++];

        switch (op) {
        case OP_NOP: break;
        case OP_ACONST_NULL: frame.push(JavaValue(0, true)); break;
        case OP_ICONST_M1: frame.push(JavaValue(-1)); break;
        case OP_ICONST_0: frame.push(JavaValue(0)); break;
        case OP_ICONST_1: frame.push(JavaValue(1)); break;
        case OP_ICONST_2: frame.push(JavaValue(2)); break;
        case OP_ICONST_3: frame.push(JavaValue(3)); break;
        case OP_ICONST_4: frame.push(JavaValue(4)); break;
        case OP_ICONST_5: frame.push(JavaValue(5)); break;
        case OP_LCONST_0: frame.push(JavaValue((int64_t)0)); break;
        case OP_LCONST_1: frame.push(JavaValue((int64_t)1)); break;
        case OP_FCONST_0: frame.push(JavaValue(0.0f)); break;
        case OP_FCONST_1: frame.push(JavaValue(1.0f)); break;
        case OP_FCONST_2: frame.push(JavaValue(2.0f)); break;
        case OP_DCONST_0: frame.push(JavaValue(0.0)); break;
        case OP_DCONST_1: frame.push(JavaValue(1.0)); break;

        case OP_BIPUSH: {
            int8_t b = (int8_t)code[frame.pc++];
            frame.push(JavaValue((int32_t)b));
            break;
        }
        case OP_SIPUSH: {
            int16_t s = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
            frame.pc += 2;
            frame.push(JavaValue((int32_t)s));
            break;
        }
        case OP_LDC:
        case OP_LDC_W: {
            uint16_t cpIdx = (op == OP_LDC) ? code[frame.pc++] : ((code[frame.pc] << 8) | code[frame.pc + 1]);
            if (op == OP_LDC_W) frame.pc += 2;
            if (cpIdx < cls->constantPool.size()) {
                const auto& cp = cls->constantPool[cpIdx];
                if (cp.tag == CONSTANT_Integer) frame.push(JavaValue(cp.intVal));
                else if (cp.tag == CONSTANT_Float) frame.push(JavaValue(cp.floatVal));
                else if (cp.tag == CONSTANT_String) {
                    std::string s = (cp.stringIndex < cls->constantPool.size()) ? cls->constantPool[cp.stringIndex].strVal : "";
                    frame.push(JavaValue(createString(s), true));
                }
                else frame.push(JavaValue(0));
            }
            break;
        }
        case OP_LDC2_W: {
            uint16_t cpIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            if (cpIdx < cls->constantPool.size()) {
                const auto& cp = cls->constantPool[cpIdx];
                if (cp.tag == CONSTANT_Long) frame.push(JavaValue(cp.longVal));
                else if (cp.tag == CONSTANT_Double) frame.push(JavaValue(cp.doubleVal));
                else frame.push(JavaValue(0));
            }
            break;
        }

        // Local Loads
        case OP_ILOAD:
        case OP_FLOAD:
        case OP_ALOAD:
        case OP_LLOAD:
        case OP_DLOAD: {
            uint8_t idx = code[frame.pc++];
            frame.push(idx < frame.locals.size() ? frame.locals[idx] : JavaValue(0));
            break;
        }
        case OP_ILOAD_0: case OP_FLOAD_0: case OP_ALOAD_0: case OP_LLOAD_0: case OP_DLOAD_0: frame.push(frame.locals.size() > 0 ? frame.locals[0] : JavaValue(0)); break;
        case OP_ILOAD_1: case OP_FLOAD_1: case OP_ALOAD_1: case OP_LLOAD_1: case OP_DLOAD_1: frame.push(frame.locals.size() > 1 ? frame.locals[1] : JavaValue(0)); break;
        case OP_ILOAD_2: case OP_FLOAD_2: case OP_ALOAD_2: case OP_LLOAD_2: case OP_DLOAD_2: frame.push(frame.locals.size() > 2 ? frame.locals[2] : JavaValue(0)); break;
        case OP_ILOAD_3: case OP_FLOAD_3: case OP_ALOAD_3: case OP_LLOAD_3: case OP_DLOAD_3: frame.push(frame.locals.size() > 3 ? frame.locals[3] : JavaValue(0)); break;

        // Local Stores
        case OP_ISTORE:
        case OP_FSTORE:
        case OP_ASTORE:
        case OP_LSTORE:
        case OP_DSTORE: {
            uint8_t idx = code[frame.pc++];
            if (idx >= frame.locals.size()) frame.locals.resize(idx + 1, JavaValue(0));
            frame.locals[idx] = frame.pop();
            break;
        }
        case OP_ISTORE_0: case OP_FSTORE_0: case OP_ASTORE_0: case OP_LSTORE_0: case OP_DSTORE_0: if (frame.locals.empty()) frame.locals.resize(1); frame.locals[0] = frame.pop(); break;
        case OP_ISTORE_1: case OP_FSTORE_1: case OP_ASTORE_1: case OP_LSTORE_1: case OP_DSTORE_1: if (frame.locals.size() < 2) frame.locals.resize(2); frame.locals[1] = frame.pop(); break;
        case OP_ISTORE_2: case OP_FSTORE_2: case OP_ASTORE_2: case OP_LSTORE_2: case OP_DSTORE_2: if (frame.locals.size() < 3) frame.locals.resize(3); frame.locals[2] = frame.pop(); break;
        case OP_ISTORE_3: case OP_FSTORE_3: case OP_ASTORE_3: case OP_LSTORE_3: case OP_DSTORE_3: if (frame.locals.size() < 4) frame.locals.resize(4); frame.locals[3] = frame.pop(); break;

        // Array Ops
        case OP_IALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->intData.size()) ? JavaValue(arr->intData[idx]) : JavaValue(0));
            break;
        }
        case OP_BALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->byteData.size()) ? JavaValue((int32_t)(int8_t)arr->byteData[idx]) : JavaValue(0));
            break;
        }
        case OP_CALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->charData.size()) ? JavaValue((int32_t)arr->charData[idx]) : JavaValue(0));
            break;
        }
        case OP_SALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->shortData.size()) ? JavaValue((int32_t)arr->shortData[idx]) : JavaValue(0));
            break;
        }
        case OP_AALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->refData.size()) ? JavaValue(arr->refData[idx], true) : JavaValue(0, true));
            break;
        }
        case OP_LALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->longData.size()) ? JavaValue(arr->longData[idx]) : JavaValue((int64_t)0));
            break;
        }
        case OP_FALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->floatData.size()) ? JavaValue(arr->floatData[idx]) : JavaValue(0.0f));
            break;
        }
        case OP_DALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->doubleData.size()) ? JavaValue(arr->doubleData[idx]) : JavaValue(0.0));
            break;
        }
        case OP_IASTORE: {
            int val = frame.pop().asInt();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->intData.size()) arr->intData[idx] = val;
            break;
        }
        case OP_BASTORE: {
            int val = frame.pop().asInt();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->byteData.size()) arr->byteData[idx] = (uint8_t)val;
            break;
        }
        case OP_CASTORE: {
            int val = frame.pop().asInt();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->charData.size()) arr->charData[idx] = (uint16_t)val;
            break;
        }
        case OP_SASTORE: {
            int val = frame.pop().asInt();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->shortData.size()) arr->shortData[idx] = (int16_t)val;
            break;
        }
        case OP_AASTORE: {
            uint32_t ref = frame.pop().asRef();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->refData.size()) arr->refData[idx] = ref;
            break;
        }
        case OP_LASTORE: {
            int64_t val = frame.pop().asLong();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->longData.size()) arr->longData[idx] = val;
            break;
        }
        case OP_FASTORE: {
            float val = frame.pop().asFloat();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->floatData.size()) arr->floatData[idx] = val;
            break;
        }
        case OP_DASTORE: {
            double val = frame.pop().asDouble();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->doubleData.size()) arr->doubleData[idx] = val;
            break;
        }
        case OP_ARRAYLENGTH: {
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push(JavaValue(arr ? arr->length() : 0));
            break;
        }

        // Stack Ops
        case OP_POP: frame.pop(); break;
        case OP_POP2: frame.pop(); frame.pop(); break;
        case OP_DUP: { JavaValue v = frame.peek(); frame.push(v); break; }
        case OP_DUP_X1: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); frame.push(v1); frame.push(v2); frame.push(v1); break; }
        case OP_DUP_X2: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); JavaValue v3 = frame.pop(); frame.push(v1); frame.push(v3); frame.push(v2); frame.push(v1); break; }
        case OP_DUP2: {
            if (frame.stack.size() >= 2) {
                JavaValue v1 = frame.stack[frame.stack.size() - 1];
                JavaValue v2 = frame.stack[frame.stack.size() - 2];
                frame.push(v2);
                frame.push(v1);
            }
            break;
        }
        case OP_SWAP: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); frame.push(v1); frame.push(v2); break; }

        // Integer Math
        case OP_IADD: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a + b)); break; }
        case OP_ISUB: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a - b)); break; }
        case OP_IMUL: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a * b)); break; }
        case OP_IDIV: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(b != 0 ? a / b : 0)); break; }
        case OP_IREM: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(b != 0 ? a % b : 0)); break; }
        case OP_INEG: { frame.push(JavaValue(-frame.pop().asInt())); break; }
        case OP_ISHL: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a << (b & 0x1F))); break; }
        case OP_ISHR: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a >> (b & 0x1F))); break; }
        case OP_IUSHR: { uint32_t b = (uint32_t)frame.pop().asInt(), a = (uint32_t)frame.pop().asInt(); frame.push(JavaValue((int32_t)(a >> (b & 0x1F)))); break; }
        case OP_IAND: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a & b)); break; }
        case OP_IOR: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a | b)); break; }
        case OP_IXOR: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a ^ b)); break; }
        case OP_IINC: {
            uint8_t idx = code[frame.pc++];
            int8_t val = (int8_t)code[frame.pc++];
            if (idx < frame.locals.size()) frame.locals[idx] = JavaValue(frame.locals[idx].asInt() + val);
            break;
        }

        // Long Math
        case OP_LADD: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a + b)); break; }
        case OP_LSUB: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a - b)); break; }
        case OP_LMUL: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a * b)); break; }
        case OP_LDIV: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(b != 0 ? a / b : 0)); break; }
        case OP_LREM: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(b != 0 ? a % b : 0)); break; }
        case OP_LNEG: { frame.push(JavaValue(-frame.pop().asLong())); break; }
        case OP_LSHL: { int32_t b = frame.pop().asInt(); int64_t a = frame.pop().asLong(); frame.push(JavaValue(a << (b & 0x3F))); break; }
        case OP_LSHR: { int32_t b = frame.pop().asInt(); int64_t a = frame.pop().asLong(); frame.push(JavaValue(a >> (b & 0x3F))); break; }
        case OP_LUSHR: { int32_t b = frame.pop().asInt(); uint64_t a = (uint64_t)frame.pop().asLong(); frame.push(JavaValue((int64_t)(a >> (b & 0x3F)))); break; }
        case OP_LAND: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a & b)); break; }
        case OP_LOR: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a | b)); break; }
        case OP_LXOR: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a ^ b)); break; }

        // Float & Double Math
        case OP_FADD: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(a + b)); break; }
        case OP_FSUB: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(a - b)); break; }
        case OP_FMUL: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(a * b)); break; }
        case OP_FDIV: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(b != 0 ? a / b : 0.0f)); break; }
        case OP_FREM: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(b != 0 ? fmodf(a,b) : 0.0f)); break; }
        case OP_FNEG: { frame.push(JavaValue(-frame.pop().asFloat())); break; }
        case OP_DADD: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(a + b)); break; }
        case OP_DSUB: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(a - b)); break; }
        case OP_DMUL: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(a * b)); break; }
        case OP_DDIV: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(b != 0 ? a / b : 0.0)); break; }
        case OP_DREM: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(b != 0 ? fmod(a,b) : 0.0)); break; }
        case OP_DNEG: { frame.push(JavaValue(-frame.pop().asDouble())); break; }

        // Conversions
        case OP_I2B: { int32_t v = frame.pop().asInt(); frame.push(JavaValue((int32_t)(int8_t)v)); break; }
        case OP_I2C: { int32_t v = frame.pop().asInt(); frame.push(JavaValue((int32_t)(uint16_t)v)); break; }
        case OP_I2S: { int32_t v = frame.pop().asInt(); frame.push(JavaValue((int32_t)(int16_t)v)); break; }
        case OP_I2L: { int32_t v = frame.pop().asInt(); frame.push(JavaValue((int64_t)v)); break; }
        case OP_I2F: { frame.push(JavaValue((float)frame.pop().asInt())); break; }
        case OP_I2D: { frame.push(JavaValue((double)frame.pop().asInt())); break; }
        case OP_L2I: { frame.push(JavaValue(frame.pop().asInt())); break; }
        case OP_L2F: { frame.push(JavaValue((float)frame.pop().asLong())); break; }
        case OP_L2D: { frame.push(JavaValue((double)frame.pop().asLong())); break; }
        case OP_F2I: { frame.push(JavaValue((int32_t)frame.pop().asFloat())); break; }
        case OP_F2L: { frame.push(JavaValue((int64_t)frame.pop().asFloat())); break; }
        case OP_F2D: { frame.push(JavaValue((double)frame.pop().asFloat())); break; }
        case OP_D2I: { frame.push(JavaValue((int32_t)frame.pop().asDouble())); break; }
        case OP_D2L: { frame.push(JavaValue((int64_t)frame.pop().asDouble())); break; }
        case OP_D2F: { frame.push(JavaValue((float)frame.pop().asDouble())); break; }

        // Comparisons
        case OP_LCMP: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a > b ? 1 : (a < b ? -1 : 0))); break; }
        case OP_FCMPL:
        case OP_FCMPG: { float b = frame.pop().asFloat(), a = frame.pop().asFloat(); frame.push(JavaValue(a > b ? 1 : (a < b ? -1 : 0))); break; }
        case OP_DCMPL:
        case OP_DCMPG: { double b = frame.pop().asDouble(), a = frame.pop().asDouble(); frame.push(JavaValue(a > b ? 1 : (a < b ? -1 : 0))); break; }

        case OP_WIDE: {
            uint8_t wideOp = code[frame.pc++];
            uint16_t idx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            if (wideOp == OP_ILOAD || wideOp == OP_FLOAD || wideOp == OP_ALOAD || wideOp == OP_LLOAD || wideOp == OP_DLOAD) {
                frame.push(idx < frame.locals.size() ? frame.locals[idx] : JavaValue(0));
            } else if (wideOp == OP_ISTORE || wideOp == OP_FSTORE || wideOp == OP_ASTORE || wideOp == OP_LSTORE || wideOp == OP_DSTORE) {
                if (idx >= frame.locals.size()) frame.locals.resize(idx + 1, JavaValue(0));
                frame.locals[idx] = frame.pop();
            } else if (wideOp == OP_IINC) {
                int16_t constVal = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
                frame.pc += 2;
                if (idx < frame.locals.size()) frame.locals[idx] = JavaValue(frame.locals[idx].asInt() + constVal);
            }
            break;
        }

        // Field operations
        case OP_GETSTATIC: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            std::string fKey = getFieldKey(cls, fIdx);
            JavaValue sv = getStaticField(fKey);
            // Well-known constants (CLDC/MIDP) when static init was skipped
            if (sv.type==JavaValue::INT && sv.asInt()==0) {
                if (fKey=="java/lang/Math:PI") sv=JavaValue(3.141592653589793);
                else if (fKey=="java/lang/Math:E") sv=JavaValue(2.718281828459045);
                else if (fKey=="java/lang/Integer:MAX_VALUE") sv=JavaValue((int32_t)2147483647);
                else if (fKey=="java/lang/Integer:MIN_VALUE") sv=JavaValue((int32_t)-2147483648);
                else if (fKey=="java/lang/Long:MAX_VALUE") sv=JavaValue((int64_t)9223372036854775807LL);
                else if (fKey=="java/lang/Long:MIN_VALUE") sv=JavaValue((int64_t)(-9223372036854775807LL-1));
            }
            // Lazy System.out/err/in allocation
            if ((fKey=="java/lang/System:out"||fKey=="java/lang/System:err"||fKey=="java/lang/System:in") && sv.asRef()==0) {
                uint32_t r=allocObject("java/io/PrintStream"); sv=JavaValue(r,true); setStaticField(fKey,sv);
            }
            frame.push(sv);
            break;
        }
        case OP_PUTSTATIC: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            std::string fKey = getFieldKey(cls, fIdx);
            setStaticField(fKey, frame.pop());
            break;
        }
        case OP_GETFIELD: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            std::string fName = getFieldName(cls, fIdx);
            uint32_t objRef = frame.pop().asRef();
            JavaObject* obj = getObject(objRef);
            if (obj) {
                auto fit = obj->fields.find(fName);
                frame.push(fit != obj->fields.end() ? fit->second : JavaValue(0));
            } else {
                frame.push(JavaValue(0));
            }
            break;
        }
        case OP_PUTFIELD: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            std::string fName = getFieldName(cls, fIdx);
            JavaValue val = frame.pop();
            uint32_t objRef = frame.pop().asRef();
            JavaObject* obj = getObject(objRef);
            if (obj) {
                obj->fields[fName] = val;
            }
            break;
        }

        // Branches
        case OP_IFEQ: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() == 0) frame.pc += off - 3; break; }
        case OP_IFNE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() != 0) frame.pc += off - 3; break; }
        case OP_IFLT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() < 0) frame.pc += off - 3; break; }
        case OP_IFGE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() >= 0) frame.pc += off - 3; break; }
        case OP_IFGT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() > 0) frame.pc += off - 3; break; }
        case OP_IFLE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asInt() <= 0) frame.pc += off - 3; break; }

        case OP_IF_ICMPEQ: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a == b) frame.pc += off - 3; break; }
        case OP_IF_ICMPNE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a != b) frame.pc += off - 3; break; }
        case OP_IF_ICMPLT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a < b) frame.pc += off - 3; break; }
        case OP_IF_ICMPGE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a >= b) frame.pc += off - 3; break; }
        case OP_IF_ICMPGT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a > b) frame.pc += off - 3; break; }
        case OP_IF_ICMPLE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); if (a <= b) frame.pc += off - 3; break; }

        case OP_IF_ACMPEQ: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; uint32_t b = frame.pop().asRef(), a = frame.pop().asRef(); if (a == b) frame.pc += off - 3; break; }
        case OP_IF_ACMPNE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; uint32_t b = frame.pop().asRef(), a = frame.pop().asRef(); if (a != b) frame.pc += off - 3; break; }

        case OP_IFNULL: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asRef() == 0) frame.pc += off - 3; break; }
        case OP_IFNONNULL: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop().asRef() != 0) frame.pc += off - 3; break; }

        case OP_GOTO: {
            int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
            frame.pc += off - 1;
            break;
        }
        case OP_GOTO_W: {
            int32_t off = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]);
            frame.pc += off - 1;
            break;
        }

        // Switch Instructions
        case OP_TABLESWITCH: {
            int tableStart = frame.pc - 1;
            while ((frame.pc % 4) != 0) frame.pc++;
            int32_t defaultOff = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
            int32_t low = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
            int32_t high = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
            int val = frame.pop().asInt();
            if (val >= low && val <= high) {
                int idx = val - low;
                int jumpPos = frame.pc + idx * 4;
                int32_t off = (int32_t)((code[jumpPos] << 24) | (code[jumpPos+1] << 16) | (code[jumpPos+2] << 8) | code[jumpPos+3]);
                frame.pc = tableStart + off;
            } else {
                frame.pc = tableStart + defaultOff;
            }
            break;
        }
        case OP_LOOKUPSWITCH: {
            int switchStart = frame.pc - 1;
            while ((frame.pc % 4) != 0) frame.pc++;
            int32_t defaultOff = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
            int32_t npairs = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
            int val = frame.pop().asInt();
            bool matched = false;
            for (int i = 0; i < npairs; ++i) {
                int32_t match = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
                int32_t off = (int32_t)((code[frame.pc] << 24) | (code[frame.pc+1] << 16) | (code[frame.pc+2] << 8) | code[frame.pc+3]); frame.pc += 4;
                if (!matched && val == match) {
                    frame.pc = switchStart + off;
                    matched = true;
                }
            }
            if (!matched) {
                frame.pc = switchStart + defaultOff;
            }
            break;
        }

        // Objects & Allocations
        case OP_NEW: {
            uint16_t cpIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            std::string cname = "java/lang/Object";
            if (cpIdx < cls->constantPool.size() && cls->constantPool[cpIdx].nameIndex < cls->constantPool.size()) {
                cname = cls->constantPool[cls->constantPool[cpIdx].nameIndex].strVal;
            }
            frame.push(JavaValue(allocObject(cname), true));
            break;
        }
        case OP_NEWARRAY: {
            uint8_t type = code[frame.pc++];
            int count = frame.pop().asInt();
            frame.push(JavaValue(allocArray(type, std::max(0, count)), true));
            break;
        }
        case OP_ANEWARRAY: {
            frame.pc += 2;
            int count = frame.pop().asInt();
            frame.push(JavaValue(allocArray(0, std::max(0, count)), true));
            break;
        }
        case OP_CHECKCAST: {
            frame.pc += 2;
            break;
        }
        case OP_INSTANCEOF: {
            frame.pc += 2;
            uint32_t ref = frame.pop().asRef();
            frame.push(JavaValue(ref != 0 ? 1 : 0));
            break;
        }
        case OP_MONITORENTER:
        case OP_MONITOREXIT: {
            frame.pop();
            break;
        }
        case OP_ATHROW: {
            JavaValue ex = frame.pop();
            int throwPc = frame.pc - 1;
            // Search exception table for handler in current method
            bool handled = false;
            JavaObject* exObj = getObject(ex.asRef());
            std::string exCls = exObj ? exObj->className : "";
            for (auto &e : method.exTable) {
                if (throwPc >= e.startPc && throwPc < e.endPc) {
                    bool match = (e.catchType == 0);
                    if (!match && e.catchType < cls->constantPool.size()) {
                        // catchType is CP Class index -> resolve name
                        const auto& cp = cls->constantPool[e.catchType];
                        std::string cn;
                        if (cp.tag == 7 && cp.nameIndex < cls->constantPool.size()) cn = cls->constantPool[cp.nameIndex].strVal;
                        else if (!cp.strVal.empty()) cn = cp.strVal;
                        if (!cn.empty() && (cn == exCls || exCls.find(cn) != std::string::npos || cn.find("Throwable") != std::string::npos || cn.find("Exception") != std::string::npos)) match = true;
                        // subclass walk via superClass chain (best-effort)
                        if (!match && exObj) {
                            auto ec = findOrLoadClass(exCls, m_activeJar);
                            std::string sup = ec ? ec->superClassName : "";
                            for (int d = 0; d < 4 && !sup.empty(); d++) { if (sup == cn) { match = true; break; } auto sc = findOrLoadClass(sup, m_activeJar); sup = sc ? sc->superClassName : ""; }
                        }
                    }
                    if (match) {
                        frame.stack.clear();
                        frame.push(ex);
                        frame.pc = e.handlerPc;
                        handled = true;
                        break;
                    }
                }
            }
            if (!handled) {
                frame.stack.clear();
                frame.push(ex);
                return ex;
            }
            break;
        }
        case OP_JSR:
        case OP_JSR_W: {
            int32_t off = 0;
            if (op==OP_JSR) { off=(int16_t)((code[frame.pc]<<8)|code[frame.pc+1]); frame.pc+=2; }
            else { off=(int32_t)((code[frame.pc]<<24)|(code[frame.pc+1]<<16)|(code[frame.pc+2]<<8)|code[frame.pc+3]); frame.pc+=4; }
            int retAddr = frame.pc;
            frame.push(JavaValue(retAddr));
            frame.pc += off - (op==OP_JSR?3:5);
            break;
        }
        case OP_RET: {
            uint8_t idx = code[frame.pc++];
            int target = (idx<frame.locals.size())?frame.locals[idx].asInt():0;
            frame.pc = target;
            break;
        }
        case OP_MULTIANEWARRAY: {
            uint16_t cpIdx=(code[frame.pc]<<8)|code[frame.pc+1]; frame.pc+=2;
            uint8_t dims=code[frame.pc++];
            (void)cpIdx;
            std::vector<int> counts(dims,0);
            for(int d=(int)dims-1;d>=0;--d) counts[d]=std::max(0,frame.pop().asInt());
            // Recursive allocation for up to 3 dims (int/object arrays)
            std::function<uint32_t(int)> allocDim = [&](int d)->uint32_t{
                int n = (d < (int)counts.size()) ? counts[d] : 0;
                uint32_t arr = allocArray(0, n);
                JavaArray* a = getArray(arr);
                if(a && d + 1 < (int)counts.size()){
                    for(int i=0;i<n;i++){ uint32_t sub = allocDim(d+1); if((int)a->refData.size()<=i) a->refData.resize(n,0); a->refData[i]=sub; }
                }
                return arr;
            };
            frame.push(JavaValue(dims?allocDim(0):allocArray(0,0), true));
            break;
        }
        case OP_DUP2_X1:
        case OP_DUP2_X2: {
            // Conservative no-op for rare dup2 variants (keeps stack balanced for common javac patterns)
            break;
        }

        // Invocations
        case OP_INVOKESPECIAL:
        case OP_INVOKEVIRTUAL:
        case OP_INVOKESTATIC:
        case OP_INVOKEINTERFACE: {
            uint16_t mIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            if (op == OP_INVOKEINTERFACE) frame.pc += 2;

            std::string targetClass = cls->thisClassName;
            std::string targetMethod = "<init>";
            std::string targetDesc = "()V";

            if (mIdx < cls->constantPool.size()) {
                const auto& cp = cls->constantPool[mIdx];
                if (cp.classIndex < cls->constantPool.size() && cls->constantPool[cp.classIndex].nameIndex < cls->constantPool.size()) {
                    targetClass = cls->constantPool[cls->constantPool[cp.classIndex].nameIndex].strVal;
                }
                if (cp.nameAndTypeIndex < cls->constantPool.size()) {
                    const auto& nat = cls->constantPool[cp.nameAndTypeIndex];
                    if (nat.nameIndex < cls->constantPool.size()) targetMethod = cls->constantPool[nat.nameIndex].strVal;
                    if (nat.descIndex < cls->constantPool.size()) targetDesc = cls->constantPool[nat.descIndex].strVal;
                }
            }

            // Estimate param count from descriptor (handles L...; and [...] arrays)
            int paramCount = 0;
            size_t p = targetDesc.find('(');
            size_t endP = targetDesc.find(')');
            if (p != std::string::npos && endP != std::string::npos) {
                for (size_t k = p + 1; k < endP; ++k) {
                    if (targetDesc[k] == 'L') {
                        while (k < endP && targetDesc[k] != ';') k++;
                        paramCount++;
                    } else if (targetDesc[k] == '[') {
                        while (k < endP && targetDesc[k] == '[') k++;
                        if (k < endP && targetDesc[k] == 'L') { while (k < endP && targetDesc[k] != ';') k++; }
                        paramCount++;
                    } else {
                        paramCount++;
                    }
                }
            }
            if (op != OP_INVOKESTATIC) paramCount++; // this ref

            std::vector<JavaValue> callArgs(paramCount);
            for (int a = paramCount - 1; a >= 0; --a) callArgs[a] = frame.pop();

            JavaValue retVal;
            if (dispatchNativeMethod(targetClass, targetMethod, targetDesc, callArgs, retVal, display)) {
                if (targetDesc.find(")V") == std::string::npos) frame.push(retVal);
            } else {
                // If this is a virtual call on an object instance, use actual object's class if available
                std::string actualClass = targetClass;
                if (op != OP_INVOKESTATIC && !callArgs.empty() && callArgs[0].asRef() != 0) {
                    JavaObject* thisObj = getObject(callArgs[0].asRef());
                    if (thisObj && !thisObj->className.empty()) {
                        actualClass = thisObj->className;
                    }
                }
                auto targetCls = findOrLoadClass(actualClass, m_activeJar);
                if (!targetCls && actualClass != targetClass) {
                    targetCls = findOrLoadClass(targetClass, m_activeJar);
                }
                if (targetCls) {
                    retVal = executeMethod(targetCls, targetMethod, targetDesc, callArgs, display);
                    if (targetDesc.find(")V") == std::string::npos) frame.push(retVal);
                } else {
                    if (targetDesc.find(")V") == std::string::npos) frame.push(JavaValue(0));
                }
            }
            break;
        }

        case OP_IRETURN:
        case OP_ARETURN:
        case OP_LRETURN:
        case OP_FRETURN:
        case OP_DRETURN:
            return frame.pop();

        case OP_RETURN:
            return JavaValue(0);

        default:
            break;
        }
    }

    return JavaValue(0);
}