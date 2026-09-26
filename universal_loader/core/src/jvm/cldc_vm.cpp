#include "cldc_vm.h"
#include "../lcdui/frame_buffer.h"
#include "../storage/rms_storage.h"
#include "../audio/mmapi_audio.h"
#include "../oem/device_control.h"
#include "../system/system_properties.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>
#include <cstring>

namespace universal_loader::jvm {

CldcVirtualMachine::CldcVirtualMachine() {
    registerStandardNatives();
}

CldcVirtualMachine::~CldcVirtualMachine() {
    m_heap.clear();
    m_classes.clear();
}

std::string CldcVirtualMachine::makeNativeKey(const std::string& className, const std::string& methodName, const std::string& descriptor) {
    if (descriptor.empty()) {
        return className + "." + methodName;
    }
    return className + "." + methodName + ":" + descriptor;
}

bool CldcVirtualMachine::loadClass(const uint8_t* data, size_t size) {
    auto clazz = ClassFileParser::parse(data, size);
    if (!clazz) return false;
    return loadClass(clazz);
}

bool CldcVirtualMachine::loadClass(std::shared_ptr<JavaClass> clazz) {
    if (!clazz || clazz->thisClassName.empty()) return false;
    m_classes[clazz->thisClassName] = clazz;
    return true;
}

std::shared_ptr<JavaClass> CldcVirtualMachine::findClass(const std::string& className) {
    auto it = m_classes.find(className);
    if (it != m_classes.end()) return it->second;
    return nullptr;
}

void CldcVirtualMachine::registerNative(const std::string& className, const std::string& methodName, const std::string& descriptor, NativeMethodHandler handler) {
    m_nativeMethods[makeNativeKey(className, methodName, descriptor)] = handler;
    if (!descriptor.empty()) {
        m_nativeMethods[makeNativeKey(className, methodName, "")] = handler;
    }
}

JavaObject* CldcVirtualMachine::allocateObject(JavaClass* clazz) {
    auto obj = std::make_unique<JavaObject>(clazz);
    if (clazz && clazz->instanceFieldCount > 0) {
        obj->fields.resize(clazz->instanceFieldCount);
    }
    JavaObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    return ptr;
}

JavaArray* CldcVirtualMachine::allocateArray(char typeCode, int32_t length) {
    auto arr = std::make_unique<JavaArray>(typeCode, length);
    JavaArray* ptr = arr.get();
    m_heap.push_back(std::move(arr));
    return ptr;
}

JavaString* CldcVirtualMachine::allocateString(const std::string& str) {
    auto s = std::make_unique<JavaString>(str);
    JavaString* ptr = s.get();
    m_heap.push_back(std::move(s));
    return ptr;
}

void CldcVirtualMachine::gc() {
    // Basic sweep: in production VM, mark-and-sweep marks roots from stack frames
}

static size_t parseMethodArgCount(const std::string& desc, bool isStatic) {
    size_t count = isStatic ? 0 : 1; // 'this'
    size_t i = 0;
    if (i < desc.size() && desc[i] == '(') ++i;
    while (i < desc.size() && desc[i] != ')') {
        if (desc[i] == 'L') {
            while (i < desc.size() && desc[i] != ';') ++i;
            if (i < desc.size()) ++i;
            count++;
        } else if (desc[i] == '[') {
            while (i < desc.size() && desc[i] == '[') ++i;
            if (i < desc.size() && desc[i] == 'L') {
                while (i < desc.size() && desc[i] != ';') ++i;
                if (i < desc.size()) ++i;
            } else if (i < desc.size()) {
                ++i;
            }
            count++;
        } else {
            ++i;
            count++;
        }
    }
    return count;
}

JavaValue CldcVirtualMachine::executeMethodByName(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args) {
    auto clazz = findClass(className);
    if (!clazz) {
        // Try native lookup directly
        auto it = m_nativeMethods.find(makeNativeKey(className, methodName, descriptor));
        if (it != m_nativeMethods.end()) {
            return it->second(this, args);
        }
        return JavaValue();
    }
    auto method = clazz->findMethod(methodName, descriptor);
    if (!method) return JavaValue();
    return executeMethod(method, args);
}

JavaValue CldcVirtualMachine::executeMethod(JavaMethod* method, const std::vector<JavaValue>& args) {
    if (!method) return JavaValue();

    if (method->isNative()) {
        std::string cName = method->clazz ? method->clazz->thisClassName : "";
        std::string key = makeNativeKey(cName, method->name, method->descriptor);
        auto it = m_nativeMethods.find(key);
        if (it == m_nativeMethods.end()) {
            it = m_nativeMethods.find(makeNativeKey(cName, method->name, ""));
        }
        if (it != m_nativeMethods.end()) {
            return it->second(this, args);
        }
        return JavaValue();
    }

    if (method->code.empty()) return JavaValue();

    StackFrame frame(method, std::max<size_t>(method->maxLocals, args.size()), std::max<size_t>(method->maxStack, 16));
    for (size_t i = 0; i < args.size(); ++i) {
        frame.locals[i] = args[i];
    }

    const uint8_t* code = method->code.data();
    size_t codeLen = method->code.size();
    size_t& pc = frame.pc;
    pc = 0;

    auto readU1 = [&]() -> uint8_t {
        if (pc >= codeLen) throw std::runtime_error("Bytecode out of range");
        return code[pc++];
    };
    auto readI1 = [&]() -> int8_t {
        if (pc >= codeLen) throw std::runtime_error("Bytecode out of range");
        return static_cast<int8_t>(code[pc++]);
    };
    auto readU2 = [&]() -> uint16_t {
        if (pc + 2 > codeLen) throw std::runtime_error("Bytecode out of range");
        uint16_t v = (static_cast<uint16_t>(code[pc]) << 8) | code[pc + 1];
        pc += 2;
        return v;
    };
    auto readI2 = [&]() -> int16_t {
        if (pc + 2 > codeLen) throw std::runtime_error("Bytecode out of range");
        int16_t v = static_cast<int16_t>((static_cast<uint16_t>(code[pc]) << 8) | code[pc + 1]);
        pc += 2;
        return v;
    };
    auto readI4 = [&]() -> int32_t {
        if (pc + 4 > codeLen) throw std::runtime_error("Bytecode out of range");
        int32_t v = (static_cast<int32_t>(code[pc]) << 24) |
                    (static_cast<int32_t>(code[pc + 1]) << 16) |
                    (static_cast<int32_t>(code[pc + 2]) << 8)  |
                    static_cast<int32_t>(code[pc + 3]);
        pc += 4;
        return v;
    };

    while (pc < codeLen) {
        size_t currentInstrPc = pc;
        uint8_t opcode = readU1();

        switch (opcode) {
            case 0x00: // nop
                break;
            case 0x01: // aconst_null
                frame.push(JavaValue(static_cast<JavaObject*>(nullptr)));
                break;
            case 0x02: frame.push(JavaValue(-1)); break; // iconst_m1
            case 0x03: frame.push(JavaValue(0)); break;  // iconst_0
            case 0x04: frame.push(JavaValue(1)); break;  // iconst_1
            case 0x05: frame.push(JavaValue(2)); break;  // iconst_2
            case 0x06: frame.push(JavaValue(3)); break;  // iconst_3
            case 0x07: frame.push(JavaValue(4)); break;  // iconst_4
            case 0x08: frame.push(JavaValue(5)); break;  // iconst_5
            case 0x09: frame.push(JavaValue(int64_t(0))); break; // lconst_0
            case 0x0A: frame.push(JavaValue(int64_t(1))); break; // lconst_1
            case 0x0B: frame.push(JavaValue(0.0f)); break;       // fconst_0
            case 0x0C: frame.push(JavaValue(1.0f)); break;       // fconst_1
            case 0x0D: frame.push(JavaValue(2.0f)); break;       // fconst_2
            case 0x0E: frame.push(JavaValue(0.0)); break;        // dconst_0
            case 0x0F: frame.push(JavaValue(1.0)); break;        // dconst_1
            case 0x10: { // bipush
                int8_t b = readI1();
                frame.push(JavaValue(static_cast<int32_t>(b)));
                break;
            }
            case 0x11: { // sipush
                int16_t s = readI2();
                frame.push(JavaValue(static_cast<int32_t>(s)));
                break;
            }
            case 0x12: // ldc
            case 0x13: { // ldc_w
                uint16_t cpIdx = (opcode == 0x12) ? readU1() : readU2();
                if (method->clazz && cpIdx < method->clazz->constantPool.size()) {
                    const auto& cp = method->clazz->constantPool[cpIdx];
                    if (cp.tag == CONSTANT_Integer) frame.push(JavaValue(cp.intVal));
                    else if (cp.tag == CONSTANT_Float) frame.push(JavaValue(cp.floatVal));
                    else if (cp.tag == CONSTANT_String) {
                        std::string strVal = method->clazz->getUtf8FromCp(cp.stringIndex);
                        frame.push(JavaValue(static_cast<JavaObject*>(allocateString(strVal))));
                    } else if (cp.tag == CONSTANT_Class) {
                        std::string cName = method->clazz->getUtf8FromCp(cp.nameIndex);
                        frame.push(JavaValue(static_cast<JavaObject*>(allocateString(cName))));
                    }
                }
                break;
            }
            case 0x14: { // ldc2_w
                uint16_t cpIdx = readU2();
                if (method->clazz && cpIdx < method->clazz->constantPool.size()) {
                    const auto& cp = method->clazz->constantPool[cpIdx];
                    if (cp.tag == CONSTANT_Long) frame.push(JavaValue(cp.longVal));
                    else if (cp.tag == CONSTANT_Double) frame.push(JavaValue(cp.doubleVal));
                }
                break;
            }
            case 0x15: // iload
            case 0x19: { // aload
                uint8_t idx = readU1();
                frame.push(frame.locals[idx]);
                break;
            }
            case 0x1A: frame.push(frame.locals[0]); break; // iload_0
            case 0x1B: frame.push(frame.locals[1]); break; // iload_1
            case 0x1C: frame.push(frame.locals[2]); break; // iload_2
            case 0x1D: frame.push(frame.locals[3]); break; // iload_3
            case 0x1E: frame.push(frame.locals[0]); break; // lload_0
            case 0x1F: frame.push(frame.locals[1]); break; // lload_1
            case 0x20: frame.push(frame.locals[2]); break; // lload_2
            case 0x21: frame.push(frame.locals[3]); break; // lload_3
            case 0x22: frame.push(frame.locals[0]); break; // fload_0
            case 0x23: frame.push(frame.locals[1]); break; // fload_1
            case 0x24: frame.push(frame.locals[2]); break; // fload_2
            case 0x25: frame.push(frame.locals[3]); break; // fload_3
            case 0x26: frame.push(frame.locals[0]); break; // aload_0
            case 0x27: frame.push(frame.locals[1]); break; // aload_1
            case 0x28: frame.push(frame.locals[2]); break; // aload_2
            case 0x29: frame.push(frame.locals[3]); break; // aload_3

            case 0x2E: // iaload
            case 0x32: // aaload
            case 0x33: // baload
            case 0x34: // caload
            case 0x35: { // saload
                int32_t idx = frame.pop().i;
                JavaObject* obj = frame.pop().ref;
                auto* arr = dynamic_cast<JavaArray*>(obj);
                if (!arr || idx < 0 || idx >= arr->length) {
                    throw std::runtime_error("ArrayIndexOutOfBoundsException");
                }
                frame.push(arr->elements[idx]);
                break;
            }

            case 0x36: // istore
            case 0x3A: { // astore
                uint8_t idx = readU1();
                frame.locals[idx] = frame.pop();
                break;
            }
            case 0x3B: frame.locals[0] = frame.pop(); break; // istore_0
            case 0x3C: frame.locals[1] = frame.pop(); break; // istore_1
            case 0x3D: frame.locals[2] = frame.pop(); break; // istore_2
            case 0x3E: frame.locals[3] = frame.pop(); break; // istore_3
            case 0x47: frame.locals[0] = frame.pop(); break; // astore_0
            case 0x48: frame.locals[1] = frame.pop(); break; // astore_1
            case 0x49: frame.locals[2] = frame.pop(); break; // astore_2
            case 0x4A: frame.locals[3] = frame.pop(); break; // astore_3

            case 0x4F: // iastore
            case 0x53: // aastore
            case 0x54: // bastore
            case 0x55: // castore
            case 0x56: { // sastore
                JavaValue val = frame.pop();
                int32_t idx = frame.pop().i;
                JavaObject* obj = frame.pop().ref;
                auto* arr = dynamic_cast<JavaArray*>(obj);
                if (!arr || idx < 0 || idx >= arr->length) {
                    throw std::runtime_error("ArrayIndexOutOfBoundsException");
                }
                arr->elements[idx] = val;
                break;
            }

            case 0x57: frame.pop(); break; // pop
            case 0x58: frame.pop(); frame.pop(); break; // pop2
            case 0x59: { // dup
                JavaValue v = frame.peek();
                frame.push(v);
                break;
            }
            case 0x5A: { // dup_x1
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                frame.push(v1);
                frame.push(v2);
                frame.push(v1);
                break;
            }
            case 0x5F: { // swap
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                frame.push(v1);
                frame.push(v2);
                break;
            }

            case 0x60: { // iadd
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a + b));
                break;
            }
            case 0x61: { // ladd
                int64_t b = frame.pop().l;
                int64_t a = frame.pop().l;
                frame.push(JavaValue(a + b));
                break;
            }
            case 0x62: { // fadd
                float b = frame.pop().f;
                float a = frame.pop().f;
                frame.push(JavaValue(a + b));
                break;
            }
            case 0x64: { // isub
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a - b));
                break;
            }
            case 0x68: { // imul
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a * b));
                break;
            }
            case 0x6C: { // idiv
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                if (b == 0) throw std::runtime_error("ArithmeticException: / by zero");
                frame.push(JavaValue(a / b));
                break;
            }
            case 0x70: { // irem
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                if (b == 0) throw std::runtime_error("ArithmeticException: % by zero");
                frame.push(JavaValue(a % b));
                break;
            }
            case 0x74: { // ineg
                frame.push(JavaValue(-frame.pop().i));
                break;
            }
            case 0x78: { // ishl
                int32_t s = frame.pop().i & 0x1F;
                int32_t v = frame.pop().i;
                frame.push(JavaValue(v << s));
                break;
            }
            case 0x7A: { // ishr
                int32_t s = frame.pop().i & 0x1F;
                int32_t v = frame.pop().i;
                frame.push(JavaValue(v >> s));
                break;
            }
            case 0x7C: { // iushr
                int32_t s = frame.pop().i & 0x1F;
                uint32_t v = static_cast<uint32_t>(frame.pop().i);
                frame.push(JavaValue(static_cast<int32_t>(v >> s)));
                break;
            }
            case 0x7E: { // iand
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a & b));
                break;
            }
            case 0x80: { // ior
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a | b));
                break;
            }
            case 0x82: { // ixor
                int32_t b = frame.pop().i;
                int32_t a = frame.pop().i;
                frame.push(JavaValue(a ^ b));
                break;
            }
            case 0x84: { // iinc
                uint8_t lIdx = readU1();
                int8_t c = readI1();
                frame.locals[lIdx].i += c;
                break;
            }

            case 0x85: frame.push(JavaValue(static_cast<int64_t>(frame.pop().i))); break; // i2l
            case 0x86: frame.push(JavaValue(static_cast<float>(frame.pop().i))); break;   // i2f
            case 0x87: frame.push(JavaValue(static_cast<double>(frame.pop().i))); break;  // i2d
            case 0x88: frame.push(JavaValue(static_cast<int32_t>(frame.pop().l))); break; // l2i
            case 0x8B: frame.push(JavaValue(static_cast<int32_t>(frame.pop().f))); break; // f2i
            case 0x8E: frame.push(JavaValue(static_cast<int32_t>(frame.pop().d))); break; // d2i
            case 0x91: frame.push(JavaValue(static_cast<int32_t>(static_cast<int8_t>(frame.pop().i)))); break; // i2b
            case 0x92: frame.push(JavaValue(static_cast<int32_t>(static_cast<uint16_t>(frame.pop().i)))); break; // i2c
            case 0x93: frame.push(JavaValue(static_cast<int32_t>(static_cast<int16_t>(frame.pop().i)))); break; // i2s

            case 0x99: { // ifeq
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i == 0) pc = branchPc + off;
                break;
            }
            case 0x9A: { // ifne
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i != 0) pc = branchPc + off;
                break;
            }
            case 0x9B: { // iflt
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i < 0) pc = branchPc + off;
                break;
            }
            case 0x9C: { // ifge
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i >= 0) pc = branchPc + off;
                break;
            }
            case 0x9D: { // ifgt
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i > 0) pc = branchPc + off;
                break;
            }
            case 0x9E: { // ifle
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().i <= 0) pc = branchPc + off;
                break;
            }
            case 0x9F: { // if_icmpeq
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 == v2) pc = branchPc + off;
                break;
            }
            case 0xA0: { // if_icmpne
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 != v2) pc = branchPc + off;
                break;
            }
            case 0xA1: { // if_icmplt
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 < v2) pc = branchPc + off;
                break;
            }
            case 0xA2: { // if_icmpge
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 >= v2) pc = branchPc + off;
                break;
            }
            case 0xA3: { // if_icmpgt
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 > v2) pc = branchPc + off;
                break;
            }
            case 0xA4: { // if_icmple
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                int32_t v2 = frame.pop().i;
                int32_t v1 = frame.pop().i;
                if (v1 <= v2) pc = branchPc + off;
                break;
            }
            case 0xA5: { // if_acmpeq
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                auto v2 = frame.pop().ref;
                auto v1 = frame.pop().ref;
                if (v1 == v2) pc = branchPc + off;
                break;
            }
            case 0xA6: { // if_acmpne
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                auto v2 = frame.pop().ref;
                auto v1 = frame.pop().ref;
                if (v1 != v2) pc = branchPc + off;
                break;
            }
            case 0xA7: { // goto
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                pc = branchPc + off;
                break;
            }
            case 0xAC: // ireturn
            case 0xAD: // lreturn
            case 0xAE: // freturn
            case 0xAF: // dreturn
            case 0xB0: // areturn
                return frame.pop();

            case 0xB1: // return
                return JavaValue();

            case 0xB2: { // getstatic
                uint16_t fRef = readU2();
                if (method->clazz) {
                    const auto& cp = method->clazz->constantPool[fRef];
                    std::string cName = method->clazz->getClassNameFromCp(cp.classIndex);
                    auto nat = method->clazz->constantPool[cp.nameAndTypeIndex];
                    std::string fName = method->clazz->getUtf8FromCp(nat.nameIndex);
                    std::string fDesc = method->clazz->getUtf8FromCp(nat.descriptorIndex);

                    auto targetClazz = findClass(cName);
                    if (targetClazz) {
                        auto field = targetClazz->findField(fName, fDesc);
                        if (field && field->slotIndex < targetClazz->staticFieldValues.size()) {
                            frame.push(targetClazz->staticFieldValues[field->slotIndex]);
                            break;
                        }
                    }
                }
                frame.push(JavaValue(0));
                break;
            }
            case 0xB3: { // putstatic
                uint16_t fRef = readU2();
                JavaValue val = frame.pop();
                if (method->clazz) {
                    const auto& cp = method->clazz->constantPool[fRef];
                    std::string cName = method->clazz->getClassNameFromCp(cp.classIndex);
                    auto nat = method->clazz->constantPool[cp.nameAndTypeIndex];
                    std::string fName = method->clazz->getUtf8FromCp(nat.nameIndex);
                    std::string fDesc = method->clazz->getUtf8FromCp(nat.descriptorIndex);

                    auto targetClazz = findClass(cName);
                    if (targetClazz) {
                        auto field = targetClazz->findField(fName, fDesc);
                        if (field && field->slotIndex < targetClazz->staticFieldValues.size()) {
                            targetClazz->staticFieldValues[field->slotIndex] = val;
                        }
                    }
                }
                break;
            }
            case 0xB4: { // getfield
                uint16_t fRef = readU2();
                JavaObject* obj = frame.pop().ref;
                if (!obj) throw std::runtime_error("NullPointerException on getfield");
                if (method->clazz) {
                    const auto& cp = method->clazz->constantPool[fRef];
                    auto nat = method->clazz->constantPool[cp.nameAndTypeIndex];
                    std::string fName = method->clazz->getUtf8FromCp(nat.nameIndex);
                    std::string fDesc = method->clazz->getUtf8FromCp(nat.descriptorIndex);

                    if (obj->clazz) {
                        auto field = obj->clazz->findField(fName, fDesc);
                        if (field && field->slotIndex < obj->fields.size()) {
                            frame.push(obj->fields[field->slotIndex]);
                            break;
                        }
                    }
                }
                frame.push(JavaValue(0));
                break;
            }
            case 0xB5: { // putfield
                uint16_t fRef = readU2();
                JavaValue val = frame.pop();
                JavaObject* obj = frame.pop().ref;
                if (!obj) throw std::runtime_error("NullPointerException on putfield");
                if (method->clazz) {
                    const auto& cp = method->clazz->constantPool[fRef];
                    auto nat = method->clazz->constantPool[cp.nameAndTypeIndex];
                    std::string fName = method->clazz->getUtf8FromCp(nat.nameIndex);
                    std::string fDesc = method->clazz->getUtf8FromCp(nat.descriptorIndex);

                    if (obj->clazz) {
                        auto field = obj->clazz->findField(fName, fDesc);
                        if (field && field->slotIndex < obj->fields.size()) {
                            obj->fields[field->slotIndex] = val;
                        }
                    }
                }
                break;
            }
            case 0xB6: // invokevirtual
            case 0xB7: // invokespecial
            case 0xB8: // invokestatic
            case 0xB9: { // invokeinterface
                uint16_t mRef = readU2();
                if (opcode == 0xB9) {
                    readU1(); // count
                    readU1(); // 0
                }
                if (method->clazz) {
                    const auto& cp = method->clazz->constantPool[mRef];
                    std::string cName = method->clazz->getClassNameFromCp(cp.classIndex);
                    auto nat = method->clazz->constantPool[cp.nameAndTypeIndex];
                    std::string mName = method->clazz->getUtf8FromCp(nat.nameIndex);
                    std::string mDesc = method->clazz->getUtf8FromCp(nat.descriptorIndex);

                    bool isStaticCall = (opcode == 0xB8);
                    size_t argCount = parseMethodArgCount(mDesc, isStaticCall);

                    std::vector<JavaValue> callArgs(argCount);
                    for (int k = static_cast<int>(argCount) - 1; k >= 0; --k) {
                        callArgs[k] = frame.pop();
                    }

                    JavaValue ret = executeMethodByName(cName, mName, mDesc, callArgs);
                    if (mDesc.find(")V") == std::string::npos) {
                        frame.push(ret);
                    }
                }
                break;
            }
            case 0xBB: { // new
                uint16_t cIdx = readU2();
                if (method->clazz) {
                    std::string cName = method->clazz->getClassNameFromCp(cIdx);
                    auto targetClazz = findClass(cName);
                    JavaObject* obj = allocateObject(targetClazz.get());
                    frame.push(JavaValue(obj));
                }
                break;
            }
            case 0xBC: { // newarray
                uint8_t aType = readU1();
                int32_t len = frame.pop().i;
                if (len < 0) throw std::runtime_error("NegativeArraySizeException");
                char codeChar = 'I';
                if (aType == 4) codeChar = 'Z';
                else if (aType == 5) codeChar = 'C';
                else if (aType == 6) codeChar = 'F';
                else if (aType == 7) codeChar = 'D';
                else if (aType == 8) codeChar = 'B';
                else if (aType == 9) codeChar = 'S';
                else if (aType == 10) codeChar = 'I';
                else if (aType == 11) codeChar = 'J';
                JavaArray* arr = allocateArray(codeChar, len);
                frame.push(JavaValue(static_cast<JavaObject*>(arr)));
                break;
            }
            case 0xBD: { // anewarray
                readU2(); // class index
                int32_t len = frame.pop().i;
                if (len < 0) throw std::runtime_error("NegativeArraySizeException");
                JavaArray* arr = allocateArray('L', len);
                frame.push(JavaValue(static_cast<JavaObject*>(arr)));
                break;
            }
            case 0xBE: { // arraylength
                JavaObject* obj = frame.pop().ref;
                auto* arr = dynamic_cast<JavaArray*>(obj);
                if (!arr) throw std::runtime_error("NullPointerException on arraylength");
                frame.push(JavaValue(arr->length));
                break;
            }
            case 0xBF: { // athrow
                JavaObject* exObj = frame.pop().ref;
                bool caught = false;
                for (const auto& ex : method->exceptionTable) {
                    if (currentInstrPc >= ex.startPc && currentInstrPc < ex.endPc) {
                        frame.sp = 0;
                        frame.push(JavaValue(exObj));
                        pc = ex.handlerPc;
                        caught = true;
                        break;
                    }
                }
                if (!caught) {
                    throw std::runtime_error("Uncaught Java Exception");
                }
                break;
            }
            case 0xC6: { // ifnull
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().ref == nullptr) pc = branchPc + off;
                break;
            }
            case 0xC7: { // ifnonnull
                size_t branchPc = pc - 1;
                int16_t off = readI2();
                if (frame.pop().ref != nullptr) pc = branchPc + off;
                break;
            }
            default:
                break;
        }
    }

    return JavaValue();
}

void CldcVirtualMachine::registerStandardNatives() {
    // java/lang/System
    registerNative("java/lang/System", "currentTimeMillis", "()J", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return JavaValue(ms);
    });

    registerNative("java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() < 5) return JavaValue();
        auto srcArr = dynamic_cast<JavaArray*>(args[0].ref);
        int32_t srcPos = args[1].i;
        auto dstArr = dynamic_cast<JavaArray*>(args[2].ref);
        int32_t dstPos = args[3].i;
        int32_t length = args[4].i;

        if (srcArr && dstArr && length > 0) {
            if (srcPos >= 0 && srcPos + length <= srcArr->length &&
                dstPos >= 0 && dstPos + length <= dstArr->length) {
                for (int32_t k = 0; k < length; ++k) {
                    dstArr->elements[dstPos + k] = srcArr->elements[srcPos + k];
                }
            }
        }
        return JavaValue();
    });

    registerNative("java/lang/System", "gc", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        vm->gc();
        return JavaValue();
    });

    registerNative("java/lang/System", "getProperty", "(Ljava/lang/String;)Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.empty() || !args[0].ref) return JavaValue();
        auto* strObj = dynamic_cast<JavaString*>(args[0].ref);
        if (!strObj) return JavaValue();
        const std::string& key = strObj->value;
        if (!j2me::SystemPropertiesManager::instance().hasProperty(key)) {
            return JavaValue();
        }
        std::string val = j2me::SystemPropertiesManager::instance().getProperty(key);
        JavaString* retStr = vm->allocateString(val);
        return JavaValue(static_cast<JavaObject*>(retStr));
    });

    // java/lang/Math
    registerNative("java/lang/Math", "abs", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int32_t v = args.empty() ? 0 : args[0].i;
        return JavaValue(std::abs(v));
    });

    registerNative("java/lang/Math", "abs", "(F)F", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        float v = args.empty() ? 0.0f : args[0].f;
        return JavaValue(std::abs(v));
    });

    registerNative("java/lang/Math", "sin", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        double v = args.empty() ? 0.0 : args[0].d;
        return JavaValue(std::sin(v));
    });

    registerNative("java/lang/Math", "cos", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        double v = args.empty() ? 0.0 : args[0].d;
        return JavaValue(std::cos(v));
    });

    registerNative("java/lang/Math", "sqrt", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        double v = args.empty() ? 0.0 : args[0].d;
        return JavaValue(std::sqrt(v));
    });

    // java/lang/Thread
    registerNative("java/lang/Thread", "sleep", "(J)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int64_t ms = args.empty() ? 0 : args[0].l;
        if (ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return JavaValue();
    });

    // javax/microedition/rms/RecordStore
    registerNative("javax/microedition/rms/RecordStore", "openRecordStore", "(Ljava/lang/String;Z)Ljavax/microedition/rms/RecordStore;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        std::string storeName = "default";
        if (!args.empty() && args[0].ref && args[0].ref->isString()) {
            storeName = static_cast<JavaString*>(args[0].ref)->value;
        }
        bool createIf = args.size() > 1 ? (args[1].i != 0) : true;
        auto* store = j2me::RmsManager::instance().openRecordStore("MIDP_App", storeName, createIf);

        JavaObject* obj = vm->allocateObject(nullptr);
        obj->nativeHandle = store;
        return JavaValue(obj);
    });

    registerNative("javax/microedition/rms/RecordStore", "getNumRecords", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            return JavaValue(store->getNumRecords());
        }
        return JavaValue(0);
    });

    // javax/microedition/lcdui/Canvas
    registerNative("javax/microedition/lcdui/Canvas", "getWidth", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(240);
    });

    registerNative("javax/microedition/lcdui/Canvas", "getHeight", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(320);
    });

    registerNative("javax/microedition/lcdui/Canvas", "repaint", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        // Trigger repaint / publish frame
        return JavaValue();
    });

    // OEM DeviceControl
    registerNative("com/nokia/mid/ui/DeviceControl", "startVibra", "(II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int freq = args.size() > 0 ? args[0].i : 100;
        int dur = args.size() > 1 ? args[1].i : 100;
        universal_loader::oem::DeviceControlManager::instance().startVibra(freq, dur);
        return JavaValue();
    });

    registerNative("com/nokia/mid/ui/DeviceControl", "stopVibra", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        universal_loader::oem::DeviceControlManager::instance().stopVibra();
        return JavaValue();
    });
}

} // namespace universal_loader::jvm
