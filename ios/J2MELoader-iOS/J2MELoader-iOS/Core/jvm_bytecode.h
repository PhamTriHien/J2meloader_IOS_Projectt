#ifndef JVM_BYTECODE_H
#define JVM_BYTECODE_H

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cstdint>

enum JavaOpcode {
    OP_NOP = 0x00,
    OP_ACONST_NULL = 0x01,
    OP_ICONST_M1 = 0x02,
    OP_ICONST_0 = 0x03,
    OP_ICONST_1 = 0x04,
    OP_ICONST_2 = 0x05,
    OP_ICONST_3 = 0x06,
    OP_ICONST_4 = 0x07,
    OP_ICONST_5 = 0x08,
    OP_BIPUSH = 0x10,
    OP_SIPUSH = 0x11,
    OP_LDC = 0x12,
    OP_ILOAD = 0x15,
    OP_ALOAD = 0x19,
    OP_ILOAD_0 = 0x1A,
    OP_ILOAD_1 = 0x1B,
    OP_ILOAD_2 = 0x1C,
    OP_ILOAD_3 = 0x1D,
    OP_ALOAD_0 = 0x2A,
    OP_ALOAD_1 = 0x2B,
    OP_ALOAD_2 = 0x2C,
    OP_ALOAD_3 = 0x2D,
    OP_IALOAD = 0x2E,
    OP_AALOAD = 0x32,
    OP_BALOAD = 0x33,
    OP_CALOAD = 0x34,
    OP_SALOAD = 0x35,
    OP_ISTORE = 0x36,
    OP_ASTORE = 0x3A,
    OP_ISTORE_0 = 0x3B,
    OP_ISTORE_1 = 0x3C,
    OP_ISTORE_2 = 0x3D,
    OP_ISTORE_3 = 0x3E,
    OP_ASTORE_0 = 0x4B,
    OP_ASTORE_1 = 0x4C,
    OP_ASTORE_2 = 0x4D,
    OP_ASTORE_3 = 0x4E,
    OP_IASTORE = 0x4F,
    OP_AASTORE = 0x53,
    OP_BASTORE = 0x54,
    OP_CASTORE = 0x55,
    OP_SASTORE = 0x56,
    OP_POP = 0x57,
    OP_POP2 = 0x58,
    OP_DUP = 0x59,
    OP_DUP_X1 = 0x5A,
    OP_DUP_X2 = 0x5B,
    OP_DUP2 = 0x5C,
    OP_SWAP = 0x5F,
    OP_IADD = 0x60,
    OP_ISUB = 0x64,
    OP_IMUL = 0x68,
    OP_IDIV = 0x6C,
    OP_IREM = 0x70,
    OP_INEG = 0x74,
    OP_ISHL = 0x78,
    OP_ISHR = 0x7A,
    OP_IUSHR = 0x7C,
    OP_IAND = 0x7E,
    OP_IOR = 0x80,
    OP_IXOR = 0x82,
    OP_IINC = 0x84,
    OP_IFEQ = 0x99,
    OP_IFNE = 0x9A,
    OP_IFLT = 0x9B,
    OP_IFGE = 0x9C,
    OP_IFGT = 0x9D,
    OP_IFLE = 0x9E,
    OP_IF_ICMPEQ = 0x9F,
    OP_IF_ICMPNE = 0xA0,
    OP_IF_ICMPLT = 0xA1,
    OP_IF_ICMPGE = 0xA2,
    OP_IF_ICMPGT = 0xA3,
    OP_IF_ICMPLE = 0xA4,
    OP_IF_ACMPEQ = 0xA5,
    OP_IF_ACMPNE = 0xA6,
    OP_GOTO = 0xA7,
    OP_TABLESWITCH = 0xAA,
    OP_LOOKUPSWITCH = 0xAB,
    OP_IRETURN = 0xAC,
    OP_ARETURN = 0xB0,
    OP_RETURN = 0xB1,
    OP_GETSTATIC = 0xB2,
    OP_PUTSTATIC = 0xB3,
    OP_GETFIELD = 0xB4,
    OP_PUTFIELD = 0xB5,
    OP_INVOKEVIRTUAL = 0xB6,
    OP_INVOKESPECIAL = 0xB7,
    OP_INVOKESTATIC = 0xB8,
    OP_INVOKEINTERFACE = 0xB9,
    OP_NEW = 0xBB,
    OP_NEWARRAY = 0xBC,
    OP_ANEWARRAY = 0xBD,
    OP_ARRAYLENGTH = 0xBE,
    OP_ATHROW = 0xBF,
    OP_CHECKCAST = 0xC0,
    OP_INSTANCEOF = 0xC1,
    OP_MONITORENTER = 0xC2,
    OP_MONITOREXIT = 0xC3
};

struct StackFrame {
    std::vector<int32_t> locals;
    std::vector<int32_t> stack;
    int pc = 0;

    void push(int32_t val) { stack.push_back(val); }
    int32_t pop() {
        if (stack.empty()) return 0;
        int32_t v = stack.back();
        stack.pop_back();
        return v;
    }
    int32_t peek() const { return stack.empty() ? 0 : stack.back(); }
};

class JvmBytecodeEngine {
public:
    static JvmBytecodeEngine& getInstance();

    int32_t execute(const uint8_t* code, size_t codeLength, StackFrame& frame);

private:
    JvmBytecodeEngine();
};

#endif // JVM_BYTECODE_H