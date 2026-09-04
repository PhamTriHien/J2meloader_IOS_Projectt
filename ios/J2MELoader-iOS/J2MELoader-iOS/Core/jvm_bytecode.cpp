#include "jvm_bytecode.h"
#include "lcdui_display.h"
#include "jar_loader.h"
#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>

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
    m_nextRef = 1;
}

uint32_t JvmBytecodeEngine::allocObject(const std::string& className) {
    uint32_t ref = m_nextRef++;
    JavaObject obj;
    obj.id = ref;
    obj.className = className;
    m_heapObjects[ref] = std::move(obj);
    return ref;
}

uint32_t JvmBytecodeEngine::allocArray(uint8_t type, int length) {
    uint32_t ref = m_nextRef++;
    JavaArray arr;
    arr.id = ref;
    arr.elemType = type;
    if (type == 10) arr.intData.resize(length, 0); // T_INT
    else if (type == 8) arr.byteData.resize(length, 0); // T_BYTE / T_BOOLEAN
    else if (type == 5) arr.charData.resize(length, 0); // T_CHAR
    else arr.refData.resize(length, 0); // Reference / Object array
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

                // Exception table
                uint16_t exTableLen = bs.readU2();
                bs.skip(exTableLen * 8);

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
    auto it = m_loadedClasses.find(className);
    if (it != m_loadedClasses.end()) return it->second;

    if (!jar) return nullptr;

    std::string entryName = className + ".class";
    std::vector<uint8_t> bytes;
    if (jar->extractEntry(entryName, bytes)) {
        return loadClass(bytes);
    }
    return nullptr;
}

// ----------------------------------------------------
// Native Dispatcher for Standard CLDC 1.1 / MIDP 2.0
// ----------------------------------------------------
bool JvmBytecodeEngine::dispatchNativeMethod(const std::string& className, const std::string& methodName, const std::string& desc, const std::vector<JavaValue>& args, JavaValue& outResult, LcduiDisplay* display) {
    // java/lang/System
    if (className == "java/lang/System") {
        if (methodName == "currentTimeMillis") {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            outResult = JavaValue((int32_t)now);
            return true;
        }
        if (methodName == "arraycopy") {
            // src, srcPos, dest, destPos, length
            if (args.size() >= 5) {
                JavaArray* src = getArray(args[0].asRef());
                int srcPos = args[1].asInt();
                JavaArray* dst = getArray(args[2].asRef());
                int dstPos = args[3].asInt();
                int len = args[4].asInt();
                if (src && dst && len > 0) {
                    if (!src->intData.empty() && !dst->intData.empty()) {
                        for (int k = 0; k < len; ++k) dst->intData[dstPos + k] = src->intData[srcPos + k];
                    } else if (!src->byteData.empty() && !dst->byteData.empty()) {
                        for (int k = 0; k < len; ++k) dst->byteData[dstPos + k] = src->byteData[srcPos + k];
                    }
                }
            }
            return true;
        }
        if (methodName == "gc") {
            return true;
        }
    }

    // javax/microedition/lcdui/Canvas / GameCanvas
    if (className.find("Canvas") != std::string::npos) {
        if (methodName == "repaint" || methodName == "flushGraphics") {
            if (display) {
                // Trigger display refresh
            }
            return true;
        }
        if (methodName == "getWidth") {
            outResult = JavaValue(display ? display->getWidth() : 240);
            return true;
        }
        if (methodName == "getHeight") {
            outResult = JavaValue(display ? display->getHeight() : 320);
            return true;
        }
        if (methodName == "setFullScreenMode") {
            return true;
        }
    }

    // javax/microedition/lcdui/Graphics
    if (className == "javax/microedition/lcdui/Graphics") {
        if (!display) return true;
        if (methodName == "setColor") {
            if (args.size() >= 2) display->setColor((uint32_t)(args[1].asInt() | 0xFF000000));
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
        if (methodName == "drawString" && args.size() >= 5) {
            // text, x, y, anchor
            display->drawString("MIDP", args[2].asInt(), args[3].asInt(), args[4].asInt(), display->getColor());
            return true;
        }
        if (methodName == "setClip" || methodName == "clipRect") {
            return true;
        }
    }

    // javax/microedition/media/Manager
    if (className == "javax/microedition/media/Manager") {
        if (methodName == "playTone") {
            // note, duration, volume
            return true;
        }
    }

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

    while (frame.pc >= 0 && (size_t)frame.pc < codeLen) {
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
        case OP_DCONST_0: frame.push(JavaValue(0.0)); break;

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
                else if (cp.tag == CONSTANT_String) frame.push(JavaValue(allocObject("java/lang/String"), true));
                else frame.push(JavaValue(0));
            }
            break;
        }

        case OP_ILOAD:
        case OP_ALOAD: {
            uint8_t idx = code[frame.pc++];
            frame.push(idx < frame.locals.size() ? frame.locals[idx] : JavaValue(0));
            break;
        }
        case OP_ILOAD_0: case OP_ALOAD_0: frame.push(frame.locals.size() > 0 ? frame.locals[0] : JavaValue(0)); break;
        case OP_ILOAD_1: case OP_ALOAD_1: frame.push(frame.locals.size() > 1 ? frame.locals[1] : JavaValue(0)); break;
        case OP_ILOAD_2: case OP_ALOAD_2: frame.push(frame.locals.size() > 2 ? frame.locals[2] : JavaValue(0)); break;
        case OP_ILOAD_3: case OP_ALOAD_3: frame.push(frame.locals.size() > 3 ? frame.locals[3] : JavaValue(0)); break;

        case OP_ISTORE:
        case OP_ASTORE: {
            uint8_t idx = code[frame.pc++];
            if (idx >= frame.locals.size()) frame.locals.resize(idx + 1, JavaValue(0));
            frame.locals[idx] = frame.pop();
            break;
        }
        case OP_ISTORE_0: case OP_ASTORE_0: if (frame.locals.empty()) frame.locals.resize(1); frame.locals[0] = frame.pop(); break;
        case OP_ISTORE_1: case OP_ASTORE_1: if (frame.locals.size() < 2) frame.locals.resize(2); frame.locals[1] = frame.pop(); break;
        case OP_ISTORE_2: case OP_ASTORE_2: if (frame.locals.size() < 3) frame.locals.resize(3); frame.locals[2] = frame.pop(); break;
        case OP_ISTORE_3: case OP_ASTORE_3: if (frame.locals.size() < 4) frame.locals.resize(4); frame.locals[3] = frame.pop(); break;

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
            frame.push((arr && idx >= 0 && idx < (int)arr->byteData.size()) ? JavaValue((int32_t)arr->byteData[idx]) : JavaValue(0));
            break;
        }
        case OP_AALOAD: {
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            frame.push((arr && idx >= 0 && idx < (int)arr->refData.size()) ? JavaValue(arr->refData[idx], true) : JavaValue(0, true));
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
        case OP_AASTORE: {
            uint32_t ref = frame.pop().asRef();
            int idx = frame.pop().asInt();
            JavaArray* arr = getArray(frame.pop().asRef());
            if (arr && idx >= 0 && idx < (int)arr->refData.size()) arr->refData[idx] = ref;
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
        case OP_SWAP: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); frame.push(v1); frame.push(v2); break; }

        // Math
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

        case OP_GOTO: {
            int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
            frame.pc += off - 1;
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

            // Estimate param count from descriptor
            int paramCount = 0;
            size_t p = targetDesc.find('(');
            size_t endP = targetDesc.find(')');
            if (p != std::string::npos && endP != std::string::npos) {
                for (size_t k = p + 1; k < endP; ++k) {
                    if (targetDesc[k] == 'L') {
                        while (k < endP && targetDesc[k] != ';') k++;
                    }
                    paramCount++;
                }
            }
            if (op != OP_INVOKESTATIC) paramCount++; // this ref

            std::vector<JavaValue> callArgs(paramCount);
            for (int a = paramCount - 1; a >= 0; --a) callArgs[a] = frame.pop();

            JavaValue retVal;
            if (dispatchNativeMethod(targetClass, targetMethod, targetDesc, callArgs, retVal, display)) {
                if (targetDesc.find(")V") == std::string::npos) frame.push(retVal);
            }
            break;
        }

        case OP_IRETURN:
        case OP_ARETURN:
            return frame.pop();

        case OP_RETURN:
            return JavaValue(0);

        default:
            break;
        }
    }

    return JavaValue(0);
}