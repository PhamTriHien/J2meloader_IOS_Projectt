#include "jvm_bytecode.h"
#include <iostream>

JvmBytecodeEngine::JvmBytecodeEngine() {}

JvmBytecodeEngine& JvmBytecodeEngine::getInstance() {
    static JvmBytecodeEngine instance;
    return instance;
}

int32_t JvmBytecodeEngine::execute(const uint8_t* code, size_t codeLength, StackFrame& frame) {
    if (!code || codeLength == 0) return 0;

    while (frame.pc >= 0 && (size_t)frame.pc < codeLength) {
        uint8_t op = code[frame.pc++];

        switch (op) {
        case OP_NOP: break;
        case OP_ACONST_NULL: frame.push(0); break;
        case OP_ICONST_M1: frame.push(-1); break;
        case OP_ICONST_0: frame.push(0); break;
        case OP_ICONST_1: frame.push(1); break;
        case OP_ICONST_2: frame.push(2); break;
        case OP_ICONST_3: frame.push(3); break;
        case OP_ICONST_4: frame.push(4); break;
        case OP_ICONST_5: frame.push(5); break;

        case OP_BIPUSH: {
            int8_t b = (int8_t)code[frame.pc++];
            frame.push(b);
            break;
        }
        case OP_SIPUSH: {
            int16_t s = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
            frame.pc += 2;
            frame.push(s);
            break;
        }
        case OP_LDC: {
            uint8_t idx = code[frame.pc++];
            frame.push(idx); // Reference to constant pool
            break;
        }

        case OP_ILOAD:
        case OP_ALOAD: {
            uint8_t idx = code[frame.pc++];
            frame.push(idx < frame.locals.size() ? frame.locals[idx] : 0);
            break;
        }
        case OP_ILOAD_0: case OP_ALOAD_0: frame.push(frame.locals.size() > 0 ? frame.locals[0] : 0); break;
        case OP_ILOAD_1: case OP_ALOAD_1: frame.push(frame.locals.size() > 1 ? frame.locals[1] : 0); break;
        case OP_ILOAD_2: case OP_ALOAD_2: frame.push(frame.locals.size() > 2 ? frame.locals[2] : 0); break;
        case OP_ILOAD_3: case OP_ALOAD_3: frame.push(frame.locals.size() > 3 ? frame.locals[3] : 0); break;

        case OP_ISTORE:
        case OP_ASTORE: {
            uint8_t idx = code[frame.pc++];
            if (idx >= frame.locals.size()) frame.locals.resize(idx + 1, 0);
            frame.locals[idx] = frame.pop();
            break;
        }
        case OP_ISTORE_0: case OP_ASTORE_0: if (frame.locals.size() <= 0) frame.locals.resize(1); frame.locals[0] = frame.pop(); break;
        case OP_ISTORE_1: case OP_ASTORE_1: if (frame.locals.size() <= 1) frame.locals.resize(2); frame.locals[1] = frame.pop(); break;
        case OP_ISTORE_2: case OP_ASTORE_2: if (frame.locals.size() <= 2) frame.locals.resize(3); frame.locals[2] = frame.pop(); break;
        case OP_ISTORE_3: case OP_ASTORE_3: if (frame.locals.size() <= 3) frame.locals.resize(4); frame.locals[3] = frame.pop(); break;

        case OP_POP: frame.pop(); break;
        case OP_POP2: frame.pop(); frame.pop(); break;
        case OP_DUP: {
            int32_t val = frame.peek();
            frame.push(val);
            break;
        }
        case OP_DUP_X1: {
            int32_t v1 = frame.pop();
            int32_t v2 = frame.pop();
            frame.push(v1);
            frame.push(v2);
            frame.push(v1);
            break;
        }
        case OP_SWAP: {
            int32_t v1 = frame.pop();
            int32_t v2 = frame.pop();
            frame.push(v1);
            frame.push(v2);
            break;
        }

        case OP_IADD: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a + b); break; }
        case OP_ISUB: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a - b); break; }
        case OP_IMUL: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a * b); break; }
        case OP_IDIV: { int32_t b = frame.pop(), a = frame.pop(); frame.push(b != 0 ? a / b : 0); break; }
        case OP_IREM: { int32_t b = frame.pop(), a = frame.pop(); frame.push(b != 0 ? a % b : 0); break; }
        case OP_INEG: { int32_t a = frame.pop(); frame.push(-a); break; }
        case OP_ISHL: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a << (b & 0x1F)); break; }
        case OP_ISHR: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a >> (b & 0x1F)); break; }
        case OP_IUSHR: { uint32_t b = (uint32_t)frame.pop(), a = (uint32_t)frame.pop(); frame.push((int32_t)(a >> (b & 0x1F))); break; }
        case OP_IAND: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a & b); break; }
        case OP_IOR: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a | b); break; }
        case OP_IXOR: { int32_t b = frame.pop(), a = frame.pop(); frame.push(a ^ b); break; }
        case OP_IINC: {
            uint8_t idx = code[frame.pc++];
            int8_t val = (int8_t)code[frame.pc++];
            if (idx < frame.locals.size()) frame.locals[idx] += val;
            break;
        }

        case OP_IFEQ: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() == 0) frame.pc += off - 3; break; }
        case OP_IFNE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() != 0) frame.pc += off - 3; break; }
        case OP_IFLT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() < 0) frame.pc += off - 3; break; }
        case OP_IFGE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() >= 0) frame.pc += off - 3; break; }
        case OP_IFGT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() > 0) frame.pc += off - 3; break; }
        case OP_IFLE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; if (frame.pop() <= 0) frame.pc += off - 3; break; }

        case OP_IF_ICMPEQ: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a == b) frame.pc += off - 3; break; }
        case OP_IF_ICMPNE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a != b) frame.pc += off - 3; break; }
        case OP_IF_ICMPLT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a < b) frame.pc += off - 3; break; }
        case OP_IF_ICMPGE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a >= b) frame.pc += off - 3; break; }
        case OP_IF_ICMPGT: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a > b) frame.pc += off - 3; break; }
        case OP_IF_ICMPLE: { int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]); frame.pc += 2; int32_t b = frame.pop(), a = frame.pop(); if (a <= b) frame.pc += off - 3; break; }

        case OP_GOTO: {
            int16_t off = (int16_t)((code[frame.pc] << 8) | code[frame.pc + 1]);
            frame.pc += off - 1;
            break;
        }

        case OP_IRETURN:
        case OP_ARETURN:
            return frame.pop();

        case OP_RETURN:
            return 0;

        default:
            // Standard opcodes skipping
            break;
        }
    }
    return 0;
}