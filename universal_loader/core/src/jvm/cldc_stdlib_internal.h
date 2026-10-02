#ifndef J2ME_CLDC_STDLIB_INTERNAL_H
#define J2ME_CLDC_STDLIB_INTERNAL_H

#include <map>
#include <sstream>
#include <regex>
#include "cldc_vm.h"
#include "lcdui_screens.h"
#include "../engine_instance.h"
#include "../network/gcf_network.h"
#include "../network/http_bridge.h"
#include "../audio/mmapi_audio.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <type_traits>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <ctime>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace universal_loader::jvm {

using Args = std::vector<JavaValue>;

// ============================================================================
// Payloads
// ============================================================================

struct StringBufferPayload : NativePayload {
    std::u16string s;
};

struct BoxPayload : NativePayload {
    JavaValue v;
    void trace(std::vector<JavaObject*>& o) const override { gcTraceValue(v, o); }
};

struct VectorPayload : NativePayload {
    std::recursive_mutex mtx;
    std::vector<JavaValue> v;
    void trace(std::vector<JavaObject*>& o) const override { for (auto& e : v) gcTraceValue(e, o); }
};

struct HashtablePayload : NativePayload {
    std::recursive_mutex mtx;
    std::unordered_map<int32_t, std::vector<std::pair<JavaValue, JavaValue>>> buckets;
    size_t count{0};
    void trace(std::vector<JavaObject*>& o) const override {
        for (auto& b : buckets) for (auto& e : b.second) { gcTraceValue(e.first, o); gcTraceValue(e.second, o); }
    }
};

struct EntryPayload : NativePayload { // java.util.Map.Entry
    JavaValue key;
    JavaValue value;
    void trace(std::vector<JavaObject*>& o) const override { gcTraceValue(key, o); gcTraceValue(value, o); }
};

struct EnumerationPayload : NativePayload {
    std::vector<JavaValue> items;
    size_t index{0};
    void trace(std::vector<JavaObject*>& o) const override { for (auto& e : items) gcTraceValue(e, o); }
};

struct RandomPayload : NativePayload {
    int64_t seed{0};
};

struct TimePayload : NativePayload { // java.util.Date / java.util.Calendar
    int64_t millis{0};
};

struct ByteInputPayload : NativePayload { // java.io.ByteArrayInputStream
    std::vector<uint8_t> data;
    size_t pos{0};
    size_t mark{0};
};

struct FilterInputPayload : NativePayload { // java.io.DataInputStream
    JavaObject* in{nullptr};
    void trace(std::vector<JavaObject*>& o) const override { if (in) o.push_back(in); }
};

struct ByteOutputPayload : NativePayload { // java.io.ByteArrayOutputStream
    std::vector<uint8_t> data;
};

struct FilterOutputPayload : NativePayload { // java.io.DataOutputStream
    JavaObject* out{nullptr};
    int32_t written{0};
    void trace(std::vector<JavaObject*>& o) const override { if (out) o.push_back(out); }
};

struct SocketAddressPayload : NativePayload { // java.net.InetSocketAddress / InetAddress
    std::string host;
    int32_t port{0};
};

struct SocketPayload : NativePayload { // java.net.Socket
    std::shared_ptr<j2me::NetworkSocket> sock = std::make_shared<j2me::NetworkSocket>();
    std::string host;
    int32_t port{0};
    int32_t soTimeout{0}; // 0 = block forever
    bool closed{false};
    int32_t gcfOpen{-1};
    bool gcfConnClosed{false};
    std::vector<uint8_t> rbuf;
    size_t rpos{0};
    size_t buffered() const { return rbuf.size() - rpos; }
    JavaObject* in{nullptr};
    JavaObject* out{nullptr};
    void trace(std::vector<JavaObject*>& o) const override { if (in) o.push_back(in); if (out) o.push_back(out); }
};

struct SocketStreamPayload : NativePayload { // input / output stream of a Socket
    std::shared_ptr<SocketPayload> socket;
    bool closed{false};
    void trace(std::vector<JavaObject*>& o) const override { if (socket) socket->trace(o); }
};

struct ThreadPayload : NativePayload {
    JavaObject* runnable{nullptr};
    void trace(std::vector<JavaObject*>& o) const override { if (runnable) o.push_back(runnable); }
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(false);
    int32_t priority{5};
    std::string name;
};

struct TimerPayload : NativePayload { // java.util.Timer / java.util.TimerTask
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
};

struct CharsetPayload : NativePayload {
    std::string name;
};

struct DigestPayload : NativePayload {
    std::string alg;
    std::vector<uint8_t> buf;
};

struct KeyPayload : NativePayload { // SecretKeySpec
    std::vector<uint8_t> key;
    std::string alg;
};

struct ParamSpecPayload : NativePayload { // IvParameterSpec / GCMParameterSpec
    std::vector<uint8_t> iv;
    int32_t tagBits{128};
};

struct CipherPayload : NativePayload {
    std::string mode{"ECB"};    // ECB, CBC, GCM
    bool pkcs5{true};
    int32_t opmode{1};   // ENCRYPT_MODE 1, DECRYPT_MODE 2
    std::vector<uint8_t> key, iv, aad, buf;
    int32_t tagBits{128};
};

struct Base64CoderPayload : NativePayload {
    bool url{false};
    bool pad{true};
};

template <class P>
P* getPayload(JavaObject* o) {
    return o ? dynamic_cast<P*>(o->payload.get()) : nullptr;
}

template <class P>
P& ensurePayload(JavaObject* o) {
    if (auto* p = getPayload<P>(o)) return *p;
    auto sp = std::make_shared<P>();
    P* raw = sp.get();
    o->payload = std::move(sp);
    return *raw;
}

// ============================================================================
// Shared Helper Functions (Inline & Declarations)
// ============================================================================

inline JavaValue refV(JavaObject* o) { return JavaValue(o); }
inline JavaValue nullV() { return JavaValue(static_cast<JavaObject*>(nullptr)); }
inline JavaValue boolV(bool b) { return JavaValue(b ? 1 : 0); }

inline JavaObject* self(CldcVirtualMachine* vm, const Args& a) {
    if (a.empty() || !a[0].ref) vm->throwJava("java/lang/NullPointerException");
    return a[0].ref;
}

inline JavaValue arg(const Args& a, size_t i) {
    return i < a.size() ? a[i] : JavaValue();
}

inline JavaObject* newNativeObject(CldcVirtualMachine* vm, const std::string& className) {
    JavaObject* o = vm->allocateObject(nullptr);
    o->nativeClassName = className;
    return o;
}

std::u16string utf8ToUtf16(const std::string& in);
void appendUtf8(std::string& out, uint32_t cp);
std::string utf16ToUtf8(const std::u16string& in);
std::vector<uint8_t> toModifiedUtf8(const std::u16string& s);
std::u16string fromModifiedUtf8(const uint8_t* p, size_t n);

extern std::mutex g_utf16CacheMutex;

std::u16string u16(JavaString* s);
JavaString* asString(JavaObject* o);
JavaValue newStr(CldcVirtualMachine* vm, const std::u16string& s);
JavaValue newStrUtf8(CldcVirtualMachine* vm, const std::string& s);
std::u16string strArg(CldcVirtualMachine* vm, const Args& a, size_t i);
std::u16string optStr(const Args& a, size_t i);
JavaArray* arrayArg(CldcVirtualMachine* vm, const Args& a, size_t i);
void checkRange(CldcVirtualMachine* vm, int64_t off, int64_t len, int64_t size, const char* exClass);
JavaArray* newByteArray(CldcVirtualMachine* vm, const uint8_t* data, size_t n);
std::vector<uint8_t> bytesOf(JavaArray* arr, int32_t off, int32_t len);

char16_t toLowerChar(char16_t c);
char16_t toUpperChar(char16_t c);
bool isUpperChar(char16_t c);
bool isLowerChar(char16_t c);
bool isDigitChar(char16_t c);
bool isLetterChar(char16_t c);
bool isSpaceChar(char16_t c);
int32_t digitOf(char16_t c, int32_t radix);

std::u16string ascii16(const std::string& s);
std::string intToString(int64_t v, int32_t radix);
std::string unsignedToString(uint64_t u, int shift);

template <typename T>
std::string floatingToString(T v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (v == 0) return std::signbit(v) ? "-0.0" : "0.0";

    char buf[64];
    for (int p = 0; p < std::numeric_limits<T>::max_digits10; ++p) {
        std::snprintf(buf, sizeof(buf), "%.*e", p, static_cast<double>(v));
        if constexpr (std::is_same_v<T, float>) {
            if (std::strtof(buf, nullptr) == v) break;
        } else {
            if (std::strtod(buf, nullptr) == v) break;
        }
    }
    std::string sci(buf);
    bool neg = !sci.empty() && sci[0] == '-';
    if (neg) sci.erase(0, 1);
    size_t ePos = sci.find('e');
    std::string mant = sci.substr(0, ePos);
    int exp = std::atoi(sci.c_str() + ePos + 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits.push_back(c);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();

    std::string out = neg ? "-" : "";
    T mag = std::fabs(v);
    if (mag >= static_cast<T>(1e-3) && mag < static_cast<T>(1e7)) {
        if (exp >= 0) {
            std::string intPart = digits.substr(0, std::min<size_t>(digits.size(), exp + 1));
            while (intPart.size() < static_cast<size_t>(exp + 1)) intPart.push_back('0');
            std::string frac = (digits.size() > static_cast<size_t>(exp + 1)) ? digits.substr(exp + 1) : "0";
            out += intPart + "." + frac;
        } else {
            out += "0." + std::string(static_cast<size_t>(-exp - 1), '0') + digits;
        }
    } else {
        out += digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : "0") + "E" + std::to_string(exp);
    }
    return out;
}

std::u16string trimmed(const std::u16string& s);
int64_t parseIntegral(CldcVirtualMachine* vm, JavaString* js, int32_t radix, int64_t minV, int64_t maxV);
double parseFloating(CldcVirtualMachine* vm, JavaString* js);
int32_t floatBits(float f);
int64_t doubleBits(double d);

int32_t stringHash(const std::u16string& s);
bool javaEquals(CldcVirtualMachine* vm, JavaObject* a, JavaObject* b);
int32_t javaHashCode(CldcVirtualMachine* vm, JavaObject* a);
std::u16string javaToString(CldcVirtualMachine* vm, JavaObject* a);
std::u16string primitiveToString(char type, const JavaValue& v);

bool engineRunning(CldcVirtualMachine* vm);
bool enginePaused(CldcVirtualMachine* vm);
int32_t socketRecv(CldcVirtualMachine* vm, SocketPayload& sp, uint8_t* buf, size_t len);
bool netLog();

int32_t streamRead(CldcVirtualMachine* vm, JavaObject* s);
int32_t streamReadBlock(CldcVirtualMachine* vm, JavaObject* s, JavaArray* arr, int32_t off, int32_t len);
int32_t streamAvailable(CldcVirtualMachine* vm, JavaObject* s);
int64_t streamSkip(CldcVirtualMachine* vm, JavaObject* s, int64_t n);
void readFully(CldcVirtualMachine* vm, JavaObject* s, JavaArray* arr, int32_t off, int32_t len);
int32_t readByteOrEof(CldcVirtualMachine* vm, JavaObject* s);
uint64_t readBigEndian(CldcVirtualMachine* vm, JavaObject* s, int bytes);
void streamWrite(CldcVirtualMachine* vm, JavaObject* s, int32_t b);
void streamWriteBlock(CldcVirtualMachine* vm, JavaObject* s, const uint8_t* data, size_t n);
void writeBigEndian(CldcVirtualMachine* vm, JavaObject* s, uint64_t v, int bytes);

enum class Charset { Utf8, Latin1, Utf16BE };
Charset charsetFor(CldcVirtualMachine* vm, JavaObject* encName);
std::u16string decodeBytes(const std::vector<uint8_t>& b, Charset cs);
std::vector<uint8_t> encodeString(const std::u16string& s, Charset cs);

JavaValue newBox(CldcVirtualMachine* vm, const std::string& cls, JavaValue v);
JavaValue boxValue(CldcVirtualMachine* vm, const Args& a);

int64_t nowMillis();
std::tm localTm(int64_t millis);
enum CalField {
    ERA = 0, YEAR = 1, MONTH = 2, DATE = 5, DAY_OF_WEEK = 7, AM_PM = 9,
    HOUR = 10, HOUR_OF_DAY = 11, MINUTE = 12, SECOND = 13, MILLISECOND = 14
};
int32_t calendarGet(int64_t millis, int32_t field);
int64_t calendarSet(int64_t millis, int32_t field, int32_t value);

void runJavaRunnable(CldcVirtualMachine* vm, JavaObject* target, const char* context);

template <typename F>
void runJavaThread(CldcVirtualMachine* vm, F&& body) {
    vm->threadEnter();
    try { body(); } catch (...) {}
    vm->threadExit();
}

inline void registerFor(CldcVirtualMachine* vm, std::initializer_list<const char*> classes, const std::string& name,
                 const std::string& desc, const NativeMethodHandler& h) {
    for (const char* c : classes) vm->registerNative(c, name, desc, h);
}

std::string narrowArg(CldcVirtualMachine* vm, const Args& a, size_t i);
std::string upperAscii(std::string s);
std::string lowerAscii(std::string s);

// Module registration prototypes
void registerString(CldcVirtualMachine* vm);
void registerStringBuffer(CldcVirtualMachine* vm);
void registerBoxes(CldcVirtualMachine* vm);
void registerLangMisc(CldcVirtualMachine* vm);
void registerCollections(CldcVirtualMachine* vm);
void registerIo(CldcVirtualMachine* vm);
void registerSeCollections(CldcVirtualMachine* vm);
void registerNio(CldcVirtualMachine* vm);
void registerSeCrypto(CldcVirtualMachine* vm);
void registerNet(CldcVirtualMachine* vm);
void registerHttpScanner(CldcVirtualMachine* vm);
void registerConnector(CldcVirtualMachine* vm);
void registerLcduiScreens(CldcVirtualMachine* vm);
void registerMedia(CldcVirtualMachine* vm);

} // namespace universal_loader::jvm

#endif // J2ME_CLDC_STDLIB_INTERNAL_H
