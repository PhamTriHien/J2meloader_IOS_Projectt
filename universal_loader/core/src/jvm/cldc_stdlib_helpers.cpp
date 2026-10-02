#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {

std::mutex g_utf16CacheMutex;

std::u16string utf8ToUtf16(const std::string& in) {
    std::u16string out;
    out.reserve(in.size());
    size_t i = 0;
    const size_t n = in.size();
    while (i < n) {
        uint8_t c = static_cast<uint8_t>(in[i]);
        uint32_t cp;
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<uint8_t>(in[i + 1]) & 0x3Fu);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<uint8_t>(in[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<uint8_t>(in[i + 2]) & 0x3Fu);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
            cp = ((c & 0x07u) << 18) | ((static_cast<uint8_t>(in[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<uint8_t>(in[i + 2]) & 0x3Fu) << 6) | (static_cast<uint8_t>(in[i + 3]) & 0x3Fu);
            i += 4;
        } else {
            cp = c; // Invalid sequence: map byte as Latin-1
            i += 1;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
    }
    return out;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string utf16ToUtf8(const std::u16string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        uint32_t c = in[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < in.size() && in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (in[i + 1] - 0xDC00);
            ++i;
        }
        appendUtf8(out, c);
    }
    return out;
}

std::vector<uint8_t> toModifiedUtf8(const std::u16string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size());
    for (char16_t ch : s) {
        uint32_t c = ch;
        if (c >= 0x01 && c <= 0x7F) {
            out.push_back(static_cast<uint8_t>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<uint8_t>(0xC0 | (c >> 6)));
            out.push_back(static_cast<uint8_t>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<uint8_t>(0xE0 | (c >> 12)));
            out.push_back(static_cast<uint8_t>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<uint8_t>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

std::u16string fromModifiedUtf8(const uint8_t* p, size_t n) {
    std::u16string out;
    out.reserve(n);
    size_t i = 0;
    while (i < n) {
        uint8_t c = p[i];
        if (c < 0x80) {
            out.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            out.push_back(static_cast<char16_t>(((c & 0x1F) << 6) | (p[i + 1] & 0x3F)));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            out.push_back(static_cast<char16_t>(((c & 0x0F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F)));
            i += 3;
        } else {
            out.push_back(c);
            i += 1;
        }
    }
    return out;
}

std::u16string u16(JavaString* s) {
    if (!s) return u"";
    std::lock_guard<std::mutex> lock(g_utf16CacheMutex);
    if (!s->utf16Valid) {
        s->utf16Cache = utf8ToUtf16(s->value);
        s->utf16Valid = true;
    }
    return s->utf16Cache;
}

JavaString* asString(JavaObject* o) {
    return dynamic_cast<JavaString*>(o);
}

JavaValue newStr(CldcVirtualMachine* vm, const std::u16string& s) {
    JavaString* js = vm->allocateString(utf16ToUtf8(s));
    js->utf16Cache = s;
    js->utf16Valid = true;
    return refV(js);
}

JavaValue newStrUtf8(CldcVirtualMachine* vm, const std::string& s) {
    return refV(vm->allocateString(s));
}

std::u16string strArg(CldcVirtualMachine* vm, const Args& a, size_t i) {
    JavaString* s = asString(arg(a, i).ref);
    if (!s) vm->throwJava("java/lang/NullPointerException");
    return u16(s);
}

std::u16string optStr(const Args& a, size_t i) {
    JavaString* s = asString(arg(a, i).ref);
    return s ? u16(s) : std::u16string();
}

JavaArray* arrayArg(CldcVirtualMachine* vm, const Args& a, size_t i) {
    auto* arr = dynamic_cast<JavaArray*>(arg(a, i).ref);
    if (!arr) vm->throwJava("java/lang/NullPointerException");
    return arr;
}

void checkRange(CldcVirtualMachine* vm, int64_t off, int64_t len, int64_t size, const char* exClass) {
    if (off < 0 || len < 0 || off + len > size) {
        vm->throwJava(exClass, "offset " + std::to_string(off) + ", length " + std::to_string(len) + ", size " + std::to_string(size));
    }
}

JavaArray* newByteArray(CldcVirtualMachine* vm, const uint8_t* data, size_t n) {
    JavaArray* arr = vm->allocateArray('B', static_cast<int32_t>(n));
    for (size_t i = 0; i < n; ++i) arr->elements[i] = JavaValue(static_cast<int32_t>(static_cast<int8_t>(data[i])));
    return arr;
}

std::vector<uint8_t> bytesOf(JavaArray* arr, int32_t off, int32_t len) {
    std::vector<uint8_t> out(static_cast<size_t>(len));
    for (int32_t i = 0; i < len; ++i) out[i] = static_cast<uint8_t>(arr->elements[off + i].i & 0xFF);
    return out;
}

char16_t toLowerChar(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c + 32);
    if (c < 0x80) return c;
    if ((c >= 0xC0 && c <= 0xDE && c != 0xD7)) return static_cast<char16_t>(c + 32);
    if (c >= 0x100 && c <= 0x17F) {
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? static_cast<char16_t>(c + 1) : c;
        if (c == 0x130) return u'i';
        return (c & 1) ? c : static_cast<char16_t>(c + 1);
    }
    if (c == 0x1A0 || c == 0x1AF) return static_cast<char16_t>(c + 1);
    if (c >= 0x1EA0 && c <= 0x1EF9) return (c & 1) ? c : static_cast<char16_t>(c + 1);
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return static_cast<char16_t>(c + 32);
    if (c >= 0x410 && c <= 0x42F) return static_cast<char16_t>(c + 32);
    if (c >= 0x400 && c <= 0x40F) return static_cast<char16_t>(c + 80);
    return c;
}

char16_t toUpperChar(char16_t c) {
    if (c >= u'a' && c <= u'z') return static_cast<char16_t>(c - 32);
    if (c < 0x80) return c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return static_cast<char16_t>(c - 32);
    if (c == 0xFF) return 0x178;
    if (c >= 0x100 && c <= 0x17F) {
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c : static_cast<char16_t>(c - 1);
        if (c == 0x131) return u'I';
        return (c & 1) ? static_cast<char16_t>(c - 1) : c;
    }
    if (c == 0x1A1 || c == 0x1B0) return static_cast<char16_t>(c - 1);
    if (c >= 0x1EA0 && c <= 0x1EF9) return (c & 1) ? static_cast<char16_t>(c - 1) : c;
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return static_cast<char16_t>(c - 32);
    if (c >= 0x430 && c <= 0x44F) return static_cast<char16_t>(c - 32);
    if (c >= 0x450 && c <= 0x45F) return static_cast<char16_t>(c - 80);
    return c;
}

bool isUpperChar(char16_t c) { return toLowerChar(c) != c; }
bool isLowerChar(char16_t c) { return toUpperChar(c) != c || c == 0xDF; }
bool isDigitChar(char16_t c) { return c >= u'0' && c <= u'9'; }
bool isLetterChar(char16_t c) {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || isUpperChar(c) || isLowerChar(c) ||
           (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7AF);
}
bool isSpaceChar(char16_t c) {
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == 0x0B || c == 0x0C;
}

int32_t digitOf(char16_t c, int32_t radix) {
    int32_t d = -1;
    if (c >= u'0' && c <= u'9') d = c - u'0';
    else if (c >= u'a' && c <= u'z') d = c - u'a' + 10;
    else if (c >= u'A' && c <= u'Z') d = c - u'A' + 10;
    return (d >= 0 && d < radix) ? d : -1;
}

std::u16string ascii16(const std::string& s) {
    return std::u16string(s.begin(), s.end());
}

std::string intToString(int64_t v, int32_t radix) {
    if (radix < 2 || radix > 36) radix = 10;
    if (v == 0) return "0";
    bool neg = v < 0;
    uint64_t u = neg ? (0ull - static_cast<uint64_t>(v)) : static_cast<uint64_t>(v);
    std::string s;
    while (u > 0) {
        int d = static_cast<int>(u % static_cast<uint64_t>(radix));
        s.push_back(static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10));
        u /= static_cast<uint64_t>(radix);
    }
    if (neg) s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
}

std::string unsignedToString(uint64_t u, int shift) {
    if (u == 0) return "0";
    const uint64_t mask = (1ull << shift) - 1;
    std::string s;
    while (u > 0) {
        int d = static_cast<int>(u & mask);
        s.push_back(static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10));
        u >>= shift;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

std::u16string trimmed(const std::u16string& s) {
    size_t b = 0, e = s.size();
    while (b < e && s[b] <= u' ') ++b;
    while (e > b && s[e - 1] <= u' ') --e;
    return s.substr(b, e - b);
}

int64_t parseIntegral(CldcVirtualMachine* vm, JavaString* js, int32_t radix, int64_t minV, int64_t maxV) {
    if (!js) vm->throwJava("java/lang/NumberFormatException", "null");
    std::u16string s = u16(js);
    auto fail = [&]() { vm->throwJava("java/lang/NumberFormatException", "For input string: \"" + js->value + "\""); };
    if (s.empty() || radix < 2 || radix > 36) fail();
    size_t i = 0;
    bool neg = false;
    if (s[0] == u'-' || s[0] == u'+') {
        neg = (s[0] == u'-');
        i = 1;
        if (s.size() == 1) fail();
    }
    int64_t result = 0;
    const int64_t limit = neg ? minV : -maxV;
    for (; i < s.size(); ++i) {
        int32_t d = digitOf(s[i], radix);
        if (d < 0) fail();
        if (result < (limit + d) / radix) fail();
        result = result * radix - d;
        if (result < limit) fail();
    }
    return neg ? result : -result;
}

double parseFloating(CldcVirtualMachine* vm, JavaString* js) {
    if (!js) vm->throwJava("java/lang/NullPointerException");
    std::u16string s = trimmed(u16(js));
    std::string a(s.begin(), s.end());
    if (!a.empty() && (a.back() == 'f' || a.back() == 'F' || a.back() == 'd' || a.back() == 'D')) a.pop_back();
    if (a == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (a == "Infinity" || a == "+Infinity") return std::numeric_limits<double>::infinity();
    if (a == "-Infinity") return -std::numeric_limits<double>::infinity();
    char* end = nullptr;
    double d = std::strtod(a.c_str(), &end);
    bool valid = !a.empty() && end == a.c_str() + a.size();
    for (char c : a) {
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')) valid = false;
    }
    if (!valid) vm->throwJava("java/lang/NumberFormatException", "For input string: \"" + js->value + "\"");
    return d;
}

int32_t floatBits(float f) {
    if (std::isnan(f)) return 0x7fc00000;
    int32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

int64_t doubleBits(double d) {
    if (std::isnan(d)) return 0x7ff8000000000000LL;
    int64_t b;
    std::memcpy(&b, &d, 8);
    return b;
}

int32_t stringHash(const std::u16string& s) {
    uint32_t h = 0;
    for (char16_t c : s) h = 31u * h + c;
    return static_cast<int32_t>(h);
}

bool javaEquals(CldcVirtualMachine* vm, JavaObject* a, JavaObject* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return vm->executeMethodByName(vm->classNameOf(a), "equals", "(Ljava/lang/Object;)Z", {refV(a), refV(b)}).i != 0;
}

int32_t javaHashCode(CldcVirtualMachine* vm, JavaObject* a) {
    if (!a) return 0;
    return vm->executeMethodByName(vm->classNameOf(a), "hashCode", "()I", {refV(a)}).i;
}

std::u16string javaToString(CldcVirtualMachine* vm, JavaObject* a) {
    if (!a) return u"null";
    if (auto* s = asString(a)) return u16(s);
    JavaValue r = vm->executeMethodByName(vm->classNameOf(a), "toString", "()Ljava/lang/String;", {refV(a)});
    if (auto* s = asString(r.ref)) return u16(s);
    return u"null";
}

std::u16string primitiveToString(char type, const JavaValue& v) {
    switch (type) {
        case 'Z': return v.i ? u"true" : u"false";
        case 'C': return std::u16string(1, static_cast<char16_t>(v.i));
        case 'J': return ascii16(intToString(v.l, 10));
        case 'F': return ascii16(floatingToString(v.f));
        case 'D': return ascii16(floatingToString(v.d));
        default: return ascii16(intToString(v.i, 10));
    }
}

int32_t streamRead(CldcVirtualMachine* vm, JavaObject* s) {
    if (!s) vm->throwJava("java/lang/NullPointerException");
    if (auto* ss = getPayload<SocketStreamPayload>(s)) {
        uint8_t b = 0;
        return socketRecv(vm, *ss->socket, &b, 1) < 0 ? -1 : static_cast<int32_t>(b);
    }
    if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) {
        return (jis->pos < jis->data.size()) ? static_cast<int32_t>(jis->data[jis->pos++]) : -1;
    }
    if (auto* p = getPayload<ByteInputPayload>(s)) {
        return (p->pos < p->data.size()) ? static_cast<int32_t>(p->data[p->pos++]) : -1;
    }
    if (auto* f = getPayload<FilterInputPayload>(s)) {
        return streamRead(vm, f->in);
    }
    if (vm->hasBytecodeMethod(s, "read", "()I")) {
        return vm->executeMethodByName(vm->classNameOf(s), "read", "()I", {refV(s)}).i;
    }
    return -1;
}

int32_t streamReadBlock(CldcVirtualMachine* vm, JavaObject* s, JavaArray* arr, int32_t off, int32_t len) {
    if (!s || !arr) vm->throwJava("java/lang/NullPointerException");
    checkRange(vm, off, len, arr->length, "java/lang/IndexOutOfBoundsException");
    if (len == 0) return 0;
    const std::vector<uint8_t>* data = nullptr;
    size_t* pos = nullptr;
    if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) {
        data = &jis->data;
        pos = &jis->pos;
    } else if (auto* p = getPayload<ByteInputPayload>(s)) {
        data = &p->data;
        pos = &p->pos;
    } else if (auto* f = getPayload<FilterInputPayload>(s)) {
        return streamReadBlock(vm, f->in, arr, off, len);
    } else if (auto* ss = getPayload<SocketStreamPayload>(s)) {
        std::vector<uint8_t> buf(static_cast<size_t>(len));
        int32_t n = socketRecv(vm, *ss->socket, buf.data(), buf.size());
        for (int32_t i = 0; i < n; ++i) {
            arr->elements[off + i] = JavaValue(static_cast<int32_t>(static_cast<int8_t>(buf[i])));
        }
        return n;
    }
    if (data) {
        if (*pos >= data->size()) return -1;
        int32_t n = static_cast<int32_t>(std::min<size_t>(static_cast<size_t>(len), data->size() - *pos));
        for (int32_t i = 0; i < n; ++i) {
            arr->elements[off + i] = JavaValue(static_cast<int32_t>(static_cast<int8_t>((*data)[*pos + i])));
        }
        *pos += static_cast<size_t>(n);
        return n;
    }
    int32_t count = 0;
    while (count < len) {
        int32_t b = streamRead(vm, s);
        if (b < 0) break;
        arr->elements[off + count++] = JavaValue(static_cast<int32_t>(static_cast<int8_t>(b)));
    }
    return count == 0 ? -1 : count;
}

int32_t streamAvailable(CldcVirtualMachine* vm, JavaObject* s) {
    if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) {
        return static_cast<int32_t>(jis->data.size() - std::min(jis->pos, jis->data.size()));
    }
    if (auto* p = getPayload<ByteInputPayload>(s)) {
        return static_cast<int32_t>(p->data.size() - std::min(p->pos, p->data.size()));
    }
    if (auto* f = getPayload<FilterInputPayload>(s)) return f->in ? streamAvailable(vm, f->in) : 0;
    if (auto* ss = getPayload<SocketStreamPayload>(s)) return ss->socket->closed ? 0 : static_cast<int32_t>(ss->socket->buffered()) + ss->socket->sock->available();
    if (vm->hasBytecodeMethod(s, "available", "()I")) {
        return vm->executeMethodByName(vm->classNameOf(s), "available", "()I", {refV(s)}).i;
    }
    return 0;
}

int64_t streamSkip(CldcVirtualMachine* vm, JavaObject* s, int64_t n) {
    if (n <= 0) return 0;
    auto skipIn = [n](size_t& pos, size_t size) -> int64_t {
        int64_t k = std::min<int64_t>(n, static_cast<int64_t>(size - std::min(pos, size)));
        pos += static_cast<size_t>(k);
        return k;
    };
    if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) return skipIn(jis->pos, jis->data.size());
    if (auto* p = getPayload<ByteInputPayload>(s)) return skipIn(p->pos, p->data.size());
    if (auto* f = getPayload<FilterInputPayload>(s)) return streamSkip(vm, f->in, n);
    int64_t k = 0;
    while (k < n && streamRead(vm, s) >= 0) ++k;
    return k;
}

void readFully(CldcVirtualMachine* vm, JavaObject* s, JavaArray* arr, int32_t off, int32_t len) {
    int32_t done = 0;
    while (done < len) {
        int32_t n = streamReadBlock(vm, s, arr, off + done, len - done);
        if (n < 0) vm->throwJava("java/io/EOFException");
        done += n;
    }
}

int32_t readByteOrEof(CldcVirtualMachine* vm, JavaObject* s) {
    int32_t b = streamRead(vm, s);
    if (b < 0) vm->throwJava("java/io/EOFException");
    return b;
}

uint64_t readBigEndian(CldcVirtualMachine* vm, JavaObject* s, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; ++i) v = (v << 8) | static_cast<uint64_t>(readByteOrEof(vm, s));
    return v;
}

void streamWrite(CldcVirtualMachine* vm, JavaObject* s, int32_t b) {
    uint8_t byte = static_cast<uint8_t>(b & 0xFF);
    streamWriteBlock(vm, s, &byte, 1);
}

void streamWriteBlock(CldcVirtualMachine* vm, JavaObject* s, const uint8_t* data, size_t n) {
    if (!s) vm->throwJava("java/lang/NullPointerException");
    if (auto* p = getPayload<ByteOutputPayload>(s)) {
        p->data.insert(p->data.end(), data, data + n);
        return;
    }
    if (auto* f = getPayload<FilterOutputPayload>(s)) {
        streamWriteBlock(vm, f->out, data, n);
        f->written += static_cast<int32_t>(n);
        return;
    }
    if (auto* ss = getPayload<SocketStreamPayload>(s)) {
        SocketPayload& sp = *ss->socket;
        if (sp.closed || !sp.sock->isConnected()) {
            if (netLog()) std::cerr << "[Net] send on closed " << sp.host << ":" << sp.port << std::endl;
            vm->throwJava("java/net/SocketException", "Socket is closed");
        }
        CldcVirtualMachine::BlockingRegion region(vm);
        size_t sent = 0;
        while (sent < n) {
            int r = sp.sock->send(data + sent, n - sent);
            if (r <= 0) break;
            sent += static_cast<size_t>(r);
        }
        if (netLog()) std::cerr << "[Net] send " << sp.host << ":" << sp.port << " n=" << n << " sent=" << sent << std::endl;
        if (sent < n) vm->throwJava("java/net/SocketException", "Connection reset");
        return;
    }
    if (vm->hasBytecodeMethod(s, "write", "(I)V")) {
        for (size_t i = 0; i < n; ++i) {
            vm->executeMethodByName(vm->classNameOf(s), "write", "(I)V", {refV(s), JavaValue(static_cast<int32_t>(data[i]))});
        }
    }
}

void writeBigEndian(CldcVirtualMachine* vm, JavaObject* s, uint64_t v, int bytes) {
    uint8_t buf[8];
    for (int i = 0; i < bytes; ++i) buf[i] = static_cast<uint8_t>(v >> (8 * (bytes - 1 - i)));
    streamWriteBlock(vm, s, buf, static_cast<size_t>(bytes));
}

Charset charsetFor(CldcVirtualMachine* vm, JavaObject* encName) {
    JavaString* js = asString(encName);
    if (!js) vm->throwJava("java/lang/NullPointerException");
    std::string e;
    for (char c : js->value) e.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (e == "UTF-8" || e == "UTF8") return Charset::Utf8;
    if (e == "ISO-8859-1" || e == "ISO8859_1" || e == "ISO8859-1" || e == "LATIN1" || e == "US-ASCII" || e == "ASCII") return Charset::Latin1;
    if (e == "UTF-16BE" || e == "UTF-16" || e == "UNICODEBIGUNMARKED") return Charset::Utf16BE;
    vm->throwJava("java/io/UnsupportedEncodingException", js->value);
}

std::u16string decodeBytes(const std::vector<uint8_t>& b, Charset cs) {
    switch (cs) {
        case Charset::Latin1: return std::u16string(b.begin(), b.end());
        case Charset::Utf16BE: {
            std::u16string s;
            for (size_t i = 0; i + 1 < b.size(); i += 2) s.push_back(static_cast<char16_t>((b[i] << 8) | b[i + 1]));
            return s;
        }
        default: return utf8ToUtf16(std::string(b.begin(), b.end()));
    }
}

std::vector<uint8_t> encodeString(const std::u16string& s, Charset cs) {
    switch (cs) {
        case Charset::Latin1: {
            std::vector<uint8_t> out;
            for (char16_t c : s) out.push_back(c <= 0xFF ? static_cast<uint8_t>(c) : '?');
            return out;
        }
        case Charset::Utf16BE: {
            std::vector<uint8_t> out;
            for (char16_t c : s) {
                out.push_back(static_cast<uint8_t>(c >> 8));
                out.push_back(static_cast<uint8_t>(c & 0xFF));
            }
            return out;
        }
        default: {
            std::string u8 = utf16ToUtf8(s);
            return std::vector<uint8_t>(u8.begin(), u8.end());
        }
    }
}

JavaValue newBox(CldcVirtualMachine* vm, const std::string& cls, JavaValue v) {
    JavaObject* o = newNativeObject(vm, cls);
    ensurePayload<BoxPayload>(o).v = v;
    return refV(o);
}

JavaValue boxValue(CldcVirtualMachine* vm, const Args& a) {
    auto* p = getPayload<BoxPayload>(self(vm, a));
    return p ? p->v : JavaValue();
}

int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::tm localTm(int64_t millis) {
    std::time_t t = static_cast<std::time_t>(millis / 1000);
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

int32_t calendarGet(int64_t millis, int32_t field) {
    std::tm t = localTm(millis);
    switch (field) {
        case ERA: return 1;
        case YEAR: return t.tm_year + 1900;
        case MONTH: return t.tm_mon;
        case DATE: return t.tm_mday;
        case DAY_OF_WEEK: return t.tm_wday + 1;
        case AM_PM: return t.tm_hour >= 12 ? 1 : 0;
        case HOUR: return t.tm_hour % 12;
        case HOUR_OF_DAY: return t.tm_hour;
        case MINUTE: return t.tm_min;
        case SECOND: return t.tm_sec;
        case MILLISECOND: return static_cast<int32_t>(((millis % 1000) + 1000) % 1000);
        default: return 0;
    }
}

int64_t calendarSet(int64_t millis, int32_t field, int32_t value) {
    std::tm t = localTm(millis);
    int32_t ms = static_cast<int32_t>(((millis % 1000) + 1000) % 1000);
    switch (field) {
        case YEAR: t.tm_year = value - 1900; break;
        case MONTH: t.tm_mon = value; break;
        case DATE: t.tm_mday = value; break;
        case HOUR:
        case HOUR_OF_DAY: t.tm_hour = (field == HOUR) ? (t.tm_hour >= 12 ? 12 : 0) + value : value; break;
        case MINUTE: t.tm_min = value; break;
        case SECOND: t.tm_sec = value; break;
        case MILLISECOND: ms = value; break;
        default: break;
    }
    t.tm_isdst = -1;
    return static_cast<int64_t>(std::mktime(&t)) * 1000 + ms;
}

bool engineRunning(CldcVirtualMachine* vm) {
    auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
    return !inst || inst->isRunning.load();
}

bool enginePaused(CldcVirtualMachine* vm) {
    auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
    return inst && inst->isPaused.load();
}

void startDetachedJavaThread(std::function<void()> body) {
#if defined(__APPLE__)
    // The recursive interpreter exceeds Apple's small default worker stack.
    auto task = std::make_unique<std::function<void()>>(std::move(body));
    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (error) throw std::system_error(error, std::generic_category());
    error = pthread_attr_setstacksize(&attributes, 8 * 1024 * 1024);
    if (!error) error = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (!error) {
        error = pthread_create(&thread, &attributes, [](void* context) -> void* {
            std::unique_ptr<std::function<void()>> work(static_cast<std::function<void()>*>(context));
            (*work)();
            return nullptr;
        }, task.get());
    }
    pthread_attr_destroy(&attributes);
    if (error) throw std::system_error(error, std::generic_category());
    task.release();
#else
    std::thread(std::move(body)).detach();
#endif
}

void runJavaRunnable(CldcVirtualMachine* vm, JavaObject* target, const char* context) {
    try {
        vm->executeMethodByName(vm->classNameOf(target), "run", "()V", {refV(target)});
    } catch (const VmTerminated&) {
        throw;
    } catch (const std::exception& e) {
        std::cerr << "[J2ME VM] Uncaught exception in " << context << " " << vm->classNameOf(target) << ": " << e.what() << std::endl;
        if (auto* je = dynamic_cast<const JavaException*>(&e)) {
            for (auto& f : je->trace) std::cerr << "    at " << f << std::endl;
        }
    }
}

std::string narrowArg(CldcVirtualMachine* vm, const Args& a, size_t i) {
    std::u16string u = strArg(vm, a, i);
    return std::string(u.begin(), u.end());
}

std::string upperAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string lowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace universal_loader::jvm
