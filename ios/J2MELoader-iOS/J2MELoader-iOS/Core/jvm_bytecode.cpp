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
#include <cctype>
#include <cstdlib>
#include <functional>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/socket.h>
#include <unistd.h>
#endif
extern "C" bool native_text_measure(const char *utf8, int px, int *outW, int *outH) __attribute__((weak));
extern "C" bool native_decode_image(const uint8_t *data, int len, uint8_t **out_rgba, int *outW, int *outH) __attribute__((weak));
extern "C" void native_free(void *p) __attribute__((weak));

static std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

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

thread_local uint32_t t_pendingException = 0;

void JvmBytecodeEngine::setPendingException(uint32_t ex) {
    t_pendingException = ex;
}

uint32_t JvmBytecodeEngine::getPendingException() {
    return t_pendingException;
}

void JvmBytecodeEngine::clearPendingException() {
    t_pendingException = 0;
}

bool JvmBytecodeEngine::hasPendingException() {
    return t_pendingException != 0;
}

static inline void appendUtf8Char(std::string& s, uint16_t ch) {
    if (ch < 0x80) {
        s.push_back((char)ch);
    } else if (ch < 0x800) {
        s.push_back((char)(0xC0 | (ch >> 6)));
        s.push_back((char)(0x80 | (ch & 0x3F)));
    } else {
        s.push_back((char)(0xE0 | (ch >> 12)));
        s.push_back((char)(0x80 | ((ch >> 6) & 0x3F)));
        s.push_back((char)(0x80 | (ch & 0x3F)));
    }
}

static inline size_t utf8CharToByteOffset(const std::string& s, int charIndex) {
    if (charIndex <= 0) return 0;
    int cur = 0;
    size_t i = 0;
    while (i < s.size() && cur < charIndex) {
        uint8_t b = (uint8_t)s[i++];
        if ((b & 0x80) == 0) { /* 1 byte */ }
        else if ((b & 0xE0) == 0xC0 && i < s.size()) { i += 1; }
        else if ((b & 0xF0) == 0xE0 && i + 1 < s.size()) { i += 2; }
        else if ((b & 0xF8) == 0xF0 && i + 2 < s.size()) { i += 3; }
        cur++;
    }
    return i;
}

JvmBytecodeEngine::JvmBytecodeEngine() {}

JvmBytecodeEngine& JvmBytecodeEngine::getInstance() {
    static JvmBytecodeEngine instance;
    return instance;
}

void JvmBytecodeEngine::reset() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_loadedClasses.clear();
    m_failedClasses.clear();
    m_heapObjects.clear();
    m_heapArrays.clear();
    m_nativeImages.clear();
    m_staticFields.clear();
    m_offscreens.clear();
    m_graphicsTarget.clear();
    m_activeJar = nullptr;
    m_nextRef = 1;
    m_cancel.store(false);
    clearPendingException();
}

uint32_t JvmBytecodeEngine::graphicsForImage(uint32_t imgRef) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    NativeImage* ni = getNativeImage(imgRef);
    if (!ni || ni->width <= 0 || ni->height <= 0) {
        uint32_t g = allocObject("javax/microedition/lcdui/Graphics");
        m_graphicsTarget[g] = 0;
        return g;
    }
    auto it = m_offscreens.find(imgRef);
    if (it == m_offscreens.end()) {
        auto disp = std::make_shared<LcduiDisplay>(ni->width, ni->height);
        disp->clear(0x00000000); // Transparent initial background for offscreen buffer
        if (!ni->pixels.empty()) {
            disp->drawRGB((const int32_t*)ni->pixels.data(), 0, ni->width,
                          0, 0, ni->width, ni->height, true);
        }
        m_offscreens[imgRef] = disp;
    }
    uint32_t g = allocObject("javax/microedition/lcdui/Graphics");
    m_graphicsTarget[g] = imgRef;
    return g;
}

LcduiDisplay* JvmBytecodeEngine::resolveGraphics(uint32_t graphicsRef, LcduiDisplay* screen) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_graphicsTarget.find(graphicsRef);
    if (it == m_graphicsTarget.end() || it->second == 0) return screen;
    auto oi = m_offscreens.find(it->second);
    if (oi != m_offscreens.end() && oi->second) return oi->second.get();
    return screen;
}

void JvmBytecodeEngine::syncImageFromDisplay(uint32_t imgRef) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto oi = m_offscreens.find(imgRef);
    // NOTE: getNativeImage takes m_mutex (recursive) — safe.
    NativeImage* ni = getNativeImage(imgRef);
    if (oi == m_offscreens.end() || !oi->second || !ni) return;
    LcduiDisplay* d = oi->second.get();
    int w = d->getWidth(), h = d->getHeight();
    if (w != ni->width || h != ni->height || !d->getBuffer()) return;
    std::lock_guard<std::mutex> dlock(d->getMutex());
    ni->pixels.assign(d->getBuffer(), d->getBuffer() + w * h);
}

const uint32_t* JvmBytecodeEngine::readableImagePixels(uint32_t imgRef) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto oi = m_offscreens.find(imgRef);
    if (oi != m_offscreens.end() && oi->second && oi->second->getBuffer())
        return oi->second->getBuffer();
    NativeImage* ni = getNativeImage(imgRef);
    if (ni && !ni->pixels.empty()) return ni->pixels.data();
    return nullptr;
}

uint32_t JvmBytecodeEngine::allocObject(const std::string& className) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32_t ref = m_nextRef++;
    JavaObject obj;
    obj.id = ref;
    obj.className = className;
    m_heapObjects[ref] = std::move(obj);
    return ref;
}

uint32_t JvmBytecodeEngine::createString(const std::string& str) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32_t ref = allocObject("java/lang/String");
    JavaObject* obj = getObject(ref);
    if (obj) obj->stringVal = str;
    return ref;
}

std::string JvmBytecodeEngine::getString(uint32_t ref) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    JavaObject* obj = getObject(ref);
    return obj ? obj->stringVal : "";
}

uint32_t JvmBytecodeEngine::allocArray(uint8_t type, int length) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_heapObjects.find(ref);
    return (it != m_heapObjects.end()) ? &it->second : nullptr;
}

JavaArray* JvmBytecodeEngine::getArray(uint32_t ref) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_heapArrays.find(ref);
    return (it != m_heapArrays.end()) ? &it->second : nullptr;
}

uint32_t JvmBytecodeEngine::allocateNativeImage(int w, int h, bool isMutable) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32_t ref = allocObject("javax/microedition/lcdui/Image");
    NativeImage img;
    img.width = w > 0 ? w : 16;
    img.height = h > 0 ? h : 16;
    img.isMutable = isMutable;
    img.pixels.resize(img.width * img.height, 0x00000000); // Fully transparent default
    m_nativeImages[ref] = std::move(img);
    return ref;
}

NativeImage* JvmBytecodeEngine::getNativeImage(uint32_t ref) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_nativeImages.find(ref);
    return (it != m_nativeImages.end()) ? &it->second : nullptr;
}

uint32_t JvmBytecodeEngine::loadNativeImageFromBytes(const uint8_t* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!data || size == 0) return allocateNativeImage(16, 16, false);

    // Auto-detect & decrypt obfuscated PNG files used by Teamobi / DragonBoy / Avatar / NinjaSchool:
    // Signature starts with 'cRBC' (0x63, 0x52, 0x42, 0x43) encrypted with key: [-22, 2, 12, 4, 5, 2, -10]
    std::vector<uint8_t> decryptedBuf;
    const uint8_t* decData = data;
    size_t decSize = size;
    static const int8_t kTeamobiKey[] = { -22, 2, 12, 4, 5, 2, -10 };
    if (size >= 8 && data[0] == 0x63 && data[1] == 0x52 && data[2] == 0x42 && data[3] == 0x43) {
        decryptedBuf.resize(size);
        for (size_t i = 0; i < size; ++i) {
            uint8_t b = data[i];
            uint8_t k = (uint8_t)kTeamobiKey[i % 7];
            decryptedBuf[i] = b ^ k;
        }
        decData = decryptedBuf.data();
        decSize = decryptedBuf.size();
    }

    int w = 0, h = 0;
    std::vector<uint32_t> pixels;
    if (!PngDecoder::decode(decData, decSize, w, h, pixels)) {
        // Fallback: UIImage decodes JPEG/GIF/BMP and odd PNGs game artists used.
        if (native_decode_image && decSize > 0 && decSize <= (8 << 20)) {
            uint8_t* rgba = nullptr;
            int dw = 0, dh = 0;
            if (native_decode_image(decData, (int)decSize, &rgba, &dw, &dh) && rgba && dw > 0 && dh > 0) {
                uint32_t ref = allocObject("javax/microedition/lcdui/Image");
                NativeImage img;
                img.width = dw;
                img.height = dh;
                img.isMutable = false;
                img.pixels.assign((uint32_t*)rgba, (uint32_t*)rgba + (size_t)dw * dh);
                m_nativeImages[ref] = std::move(img);
                if (native_free) native_free(rgba);
                else free(rgba);
                return ref;
            }
            if (rgba) { if (native_free) native_free(rgba); else free(rgba); }
        }
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_activeJar) return allocateNativeImage(16, 16, false);

    std::string entryName = path;
    std::string origEntry = entryName;
    if (!entryName.empty() && entryName[0] == '/') entryName.erase(0, 1);

    std::vector<uint8_t> bytes;
    if (m_activeJar->extractEntry(entryName, bytes) || (origEntry != entryName && m_activeJar->extractEntry(origEntry, bytes))) {
        return loadNativeImageFromBytes(bytes.data(), bytes.size());
    }
    // Prefix fallback: many Vietnamese J2ME games (Teamobi/DragonBoy/Avatar/NinjaSchool)
    // store images inside density folders like 'x1/', 'x2/', 'res/' but request '/myfont/...' or '/bg/...'
    static const char* kPrefixes[] = { "x1/", "x2/", "res/", "data/" };
    for (const char* pfx : kPrefixes) {
        std::string pfxPath = std::string(pfx) + entryName;
        if (m_activeJar->extractEntry(pfxPath, bytes)) {
            return loadNativeImageFromBytes(bytes.data(), bytes.size());
        }
    }
    // Basename fallback (e.g. game requests "/gamelogo.png" or "/mainImage/..." while in JAR it is "x1/gamelogo.png"):
    std::string baseName = entryName;
    size_t lastSlash = baseName.find_last_of('/');
    if (lastSlash != std::string::npos) baseName = baseName.substr(lastSlash + 1);

    auto entries = m_activeJar->listEntries();
    for (const auto& ent : entries) {
        std::string ce = ent;
        if (!ce.empty() && ce[0] == '/') ce.erase(0, 1);
        if (ce == entryName || toLowerStr(ce) == toLowerStr(entryName)) {
            if (m_activeJar->extractEntry(ent, bytes)) {
                return loadNativeImageFromBytes(bytes.data(), bytes.size());
            }
        }
    }
    // Second pass matching by basename:
    if (!baseName.empty()) {
        for (const auto& ent : entries) {
            std::string ce = ent;
            size_t s = ce.find_last_of('/');
            std::string eb = (s != std::string::npos) ? ce.substr(s + 1) : ce;
            if (eb == baseName || toLowerStr(eb) == toLowerStr(baseName)) {
                if (m_activeJar->extractEntry(ent, bytes)) {
                    return loadNativeImageFromBytes(bytes.data(), bytes.size());
                }
            }
        }
    }
    return allocateNativeImage(16, 16, false);
}

// ----------------------------------------------------
// Complete Java Classfile Parser (0xCAFEBABE)
// ----------------------------------------------------
std::shared_ptr<ClassFile> JvmBytecodeEngine::loadClass(const std::vector<uint8_t>& classBytes) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string normName = className;
    std::replace(normName.begin(), normName.end(), '.', '/');

    auto it = m_loadedClasses.find(normName);
    if (it != m_loadedClasses.end()) return it->second;

    // Fast-path: if this class already failed to load before, skip scanning JAR!
    if (m_failedClasses.find(normName) != m_failedClasses.end()) return nullptr;

    if (!jar) {
        m_failedClasses.insert(normName);
        return nullptr;
    }

    std::string entryName = normName + ".class";
    std::vector<uint8_t> bytes;
    if (jar->extractEntry(entryName, bytes)) {
        return loadClass(bytes);
    }
    m_failedClasses.insert(normName);
    return nullptr;
}

static inline void ensureFieldCached(std::shared_ptr<ClassFile>& cls, uint16_t fIdx) {
    if (!cls || fIdx >= cls->constantPool.size()) return;
    auto& cp = cls->constantPool[fIdx];
    if (cp.fieldCached) return;

    std::string cname = "";
    if (cp.classIndex < cls->constantPool.size() && cls->constantPool[cp.classIndex].nameIndex < cls->constantPool.size()) {
        cname = cls->constantPool[cls->constantPool[cp.classIndex].nameIndex].strVal;
    }
    std::string fname = "";
    if (cp.nameAndTypeIndex < cls->constantPool.size()) {
        const auto& nat = cls->constantPool[cp.nameAndTypeIndex];
        if (nat.nameIndex < cls->constantPool.size()) fname = cls->constantPool[nat.nameIndex].strVal;
    }
    cp.cachedShortName = fname;
    cp.cachedFieldKey = cname.empty() ? fname : (cname + ":" + fname);
    cp.fieldCached = true;
}

static const std::string& getFieldKey(std::shared_ptr<ClassFile>& cls, uint16_t fIdx) {
    static const std::string empty;
    if (!cls || fIdx >= cls->constantPool.size()) return empty;
    ensureFieldCached(cls, fIdx);
    return cls->constantPool[fIdx].cachedFieldKey;
}

static const std::string& getFieldName(std::shared_ptr<ClassFile>& cls, uint16_t fIdx) {
    static const std::string empty;
    if (!cls || fIdx >= cls->constantPool.size()) return empty;
    ensureFieldCached(cls, fIdx);
    return cls->constantPool[fIdx].cachedFieldKey;
}

void JvmBytecodeEngine::ensureClinit(std::shared_ptr<ClassFile> cls, LcduiDisplay* display) {
    if (!cls || cls->clinitDone) return;
    cls->clinitDone = true;
    if (!cls->superClassName.empty() && cls->superClassName != "java/lang/Object") {
        auto superCls = findOrLoadClass(cls->superClassName, m_activeJar);
        if (superCls && superCls != cls) {
            ensureClinit(superCls, display);
        }
    }
    auto clinitIt = cls->methods.find("<clinit>:()V");
    if (clinitIt != cls->methods.end()) {
        executeMethod(cls, "<clinit>", "()V", {}, display);
    }
}

bool JvmBytecodeEngine::isInstanceOf(const std::string& className, const std::string& targetType) {
    if (className.empty() || targetType.empty()) return false;
    if (className == targetType || targetType == "java/lang/Object") return true;
    std::string cur = className;
    for (int d = 0; d < 10 && !cur.empty() && cur != "java/lang/Object"; ++d) {
        if (cur == targetType) return true;
        auto c = findOrLoadClass(cur, m_activeJar);
        if (!c) break;
        for (const auto& iface : c->interfaces) {
            if (iface == targetType) return true;
        }
        cur = c->superClassName;
    }
    return false;
}

// ----------------------------------------------------
// Native Dispatcher for Standard CLDC 1.1 / MIDP 2.0
// ----------------------------------------------------
bool JvmBytecodeEngine::dispatchNativeMethod(const std::string& className, const std::string& methodName, const std::string& desc, const std::vector<JavaValue>& args, JavaValue& outResult, LcduiDisplay* display) {
    // dex2jar hex string decoders ($decode_S, $decode_B, $decode_I, $decode_J)
    if (methodName.find("decode_S") != std::string::npos && !args.empty()) {
        std::string hex = getString(args[0].asRef());
        int count = (int)hex.size() / 4;
        uint32_t arrRef = allocArray(9, count); // T_SHORT = 9
        JavaArray* arr = getArray(arrRef);
        if (arr) {
            auto h = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
                return 0;
            };
            for (int i = 0; i < count; ++i) {
                uint8_t b0 = (h(hex[4 * i]) << 4) | h(hex[4 * i + 1]);
                uint8_t b1 = (h(hex[4 * i + 2]) << 4) | h(hex[4 * i + 3]);
                arr->shortData[i] = (int16_t)((uint16_t)b0 | ((uint16_t)b1 << 8));
            }
        }
        outResult = JavaValue(arrRef, true);
        return true;
    }
    if (methodName.find("decode_B") != std::string::npos && !args.empty()) {
        std::string hex = getString(args[0].asRef());
        int count = (int)hex.size() / 2;
        uint32_t arrRef = allocArray(8, count); // T_BYTE = 8
        JavaArray* arr = getArray(arrRef);
        if (arr) {
            auto h = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
                return 0;
            };
            for (int i = 0; i < count; ++i) {
                arr->byteData[i] = (h(hex[2 * i]) << 4) | h(hex[2 * i + 1]);
            }
        }
        outResult = JavaValue(arrRef, true);
        return true;
    }
    if (methodName.find("decode_I") != std::string::npos && !args.empty()) {
        std::string hex = getString(args[0].asRef());
        int count = (int)hex.size() / 8;
        uint32_t arrRef = allocArray(10, count); // T_INT = 10
        JavaArray* arr = getArray(arrRef);
        if (arr) {
            auto h = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
                return 0;
            };
            for (int i = 0; i < count; ++i) {
                uint8_t b0 = (h(hex[8 * i]) << 4) | h(hex[8 * i + 1]);
                uint8_t b1 = (h(hex[8 * i + 2]) << 4) | h(hex[8 * i + 3]);
                uint8_t b2 = (h(hex[8 * i + 4]) << 4) | h(hex[8 * i + 5]);
                uint8_t b3 = (h(hex[8 * i + 6]) << 4) | h(hex[8 * i + 7]);
                arr->intData[i] = (int32_t)((uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24));
            }
        }
        outResult = JavaValue(arrRef, true);
        return true;
    }
    if (methodName.find("decode_J") != std::string::npos && !args.empty()) {
        std::string hex = getString(args[0].asRef());
        int count = (int)hex.size() / 16;
        uint32_t arrRef = allocArray(11, count); // T_LONG = 11
        JavaArray* arr = getArray(arrRef);
        if (arr) {
            auto h = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
                return 0;
            };
            for (int i = 0; i < count; ++i) {
                uint64_t v = 0;
                for (int b = 0; b < 8; ++b) {
                    uint8_t byteVal = (h(hex[16 * i + 2 * b]) << 4) | h(hex[16 * i + 2 * b + 1]);
                    v |= ((uint64_t)byteVal << (8 * b));
                }
                arr->longData[i] = (int64_t)v;
            }
        }
        outResult = JavaValue(arrRef, true);
        return true;
    }

    // Java NIO support
    if (className == "java/nio/ByteOrder") {
        if (methodName == "nativeOrder") {
            uint32_t boRef = allocObject("java/nio/ByteOrder");
            JavaObject* bo = getObject(boRef);
            if (bo) bo->fields["isLittle"] = JavaValue(1);
            outResult = JavaValue(boRef, true);
            return true;
        }
    }
    if (className == "java/nio/ByteBuffer") {
        if (methodName == "wrap" && !args.empty()) {
            uint32_t bbRef = allocObject("java/nio/ByteBuffer");
            JavaObject* bb = getObject(bbRef);
            if (bb) {
                bb->fields["buf"] = args[0];
                int off = args.size() >= 3 ? args[1].asInt() : 0;
                bb->fields["pos"] = JavaValue(off);
                bb->fields["order"] = JavaValue(0);
            }
            outResult = JavaValue(bbRef, true);
            return true;
        }
        if (methodName == "allocate" && !args.empty()) {
            uint32_t bbRef = allocObject("java/nio/ByteBuffer");
            int cap = args[0].asInt();
            uint32_t arrRef = allocArray(8, std::max(0, cap));
            JavaObject* bb = getObject(bbRef);
            if (bb) {
                bb->fields["buf"] = JavaValue(arrRef, true);
                bb->fields["pos"] = JavaValue(0);
                bb->fields["order"] = JavaValue(0);
            }
            outResult = JavaValue(bbRef, true);
            return true;
        }
        if (methodName == "order" && !args.empty()) {
            JavaObject* bb = getObject(args[0].asRef());
            if (bb && args.size() >= 2) {
                JavaObject* bo = getObject(args[1].asRef());
                int isLit = (bo && bo->fields.find("isLittle") != bo->fields.end()) ? bo->fields["isLittle"].asInt() : 1;
                bb->fields["order"] = JavaValue(isLit);
            }
            outResult = args[0];
            return true;
        }
        if (methodName == "asShortBuffer" && !args.empty()) {
            uint32_t sbRef = allocObject("java/nio/ShortBuffer");
            JavaObject* sb = getObject(sbRef);
            JavaObject* bb = getObject(args[0].asRef());
            if (sb && bb) {
                sb->fields["buf"] = bb->fields["buf"];
                sb->fields["pos"] = bb->fields["pos"];
                sb->fields["order"] = bb->fields["order"];
            }
            outResult = JavaValue(sbRef, true);
            return true;
        }
        if (methodName == "asIntBuffer" && !args.empty()) {
            uint32_t ibRef = allocObject("java/nio/IntBuffer");
            JavaObject* ib = getObject(ibRef);
            JavaObject* bb = getObject(args[0].asRef());
            if (ib && bb) {
                ib->fields["buf"] = bb->fields["buf"];
                ib->fields["pos"] = bb->fields["pos"];
                ib->fields["order"] = bb->fields["order"];
            }
            outResult = JavaValue(ibRef, true);
            return true;
        }
        if (methodName == "asLongBuffer" && !args.empty()) {
            uint32_t lbRef = allocObject("java/nio/LongBuffer");
            JavaObject* lb = getObject(lbRef);
            JavaObject* bb = getObject(args[0].asRef());
            if (lb && bb) {
                lb->fields["buf"] = bb->fields["buf"];
                lb->fields["pos"] = bb->fields["pos"];
                lb->fields["order"] = bb->fields["order"];
            }
            outResult = JavaValue(lbRef, true);
            return true;
        }
    }
    if (className == "java/nio/ShortBuffer") {
        if (methodName == "get" && args.size() >= 2) {
            JavaObject* sb = getObject(args[0].asRef());
            JavaArray* dst = getArray(args[1].asRef());
            if (sb && dst) {
                JavaArray* srcBytes = getArray(sb->fields["buf"].asRef());
                int pos = sb->fields["pos"].asInt();
                bool littleEndian = (sb->fields["order"].asInt() == 1);
                int off = args.size() >= 4 ? args[2].asInt() : 0;
                int len = args.size() >= 4 ? args[3].asInt() : (int)dst->shortData.size();
                if (srcBytes) {
                    for (int i = 0; i < len; ++i) {
                        int bPos = pos + i * 2;
                        if (bPos + 1 < (int)srcBytes->byteData.size() && off + i < (int)dst->shortData.size()) {
                            uint8_t b0 = srcBytes->byteData[bPos];
                            uint8_t b1 = srcBytes->byteData[bPos + 1];
                            dst->shortData[off + i] = littleEndian
                                ? (int16_t)((uint16_t)b0 | ((uint16_t)b1 << 8))
                                : (int16_t)(((uint16_t)b0 << 8) | (uint16_t)b1);
                        }
                    }
                    sb->fields["pos"] = JavaValue(pos + len * 2);
                }
            }
            outResult = args[0];
            return true;
        }
    }
    if (className == "java/nio/IntBuffer") {
        if (methodName == "get" && args.size() >= 2) {
            JavaObject* ib = getObject(args[0].asRef());
            JavaArray* dst = getArray(args[1].asRef());
            if (ib && dst) {
                JavaArray* srcBytes = getArray(ib->fields["buf"].asRef());
                int pos = ib->fields["pos"].asInt();
                bool littleEndian = (ib->fields["order"].asInt() == 1);
                int off = args.size() >= 4 ? args[2].asInt() : 0;
                int len = args.size() >= 4 ? args[3].asInt() : (int)dst->intData.size();
                if (srcBytes) {
                    for (int i = 0; i < len; ++i) {
                        int bPos = pos + i * 4;
                        if (bPos + 3 < (int)srcBytes->byteData.size() && off + i < (int)dst->intData.size()) {
                            uint8_t b0 = srcBytes->byteData[bPos];
                            uint8_t b1 = srcBytes->byteData[bPos + 1];
                            uint8_t b2 = srcBytes->byteData[bPos + 2];
                            uint8_t b3 = srcBytes->byteData[bPos + 3];
                            dst->intData[off + i] = littleEndian
                                ? (int32_t)((uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24))
                                : (int32_t)(((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3);
                        }
                    }
                    ib->fields["pos"] = JavaValue(pos + len * 4);
                }
            }
            outResult = args[0];
            return true;
        }
    }
    if (className == "java/nio/LongBuffer") {
        if (methodName == "get" && args.size() >= 2) {
            JavaObject* lb = getObject(args[0].asRef());
            JavaArray* dst = getArray(args[1].asRef());
            if (lb && dst) {
                JavaArray* srcBytes = getArray(lb->fields["buf"].asRef());
                int pos = lb->fields["pos"].asInt();
                bool littleEndian = (lb->fields["order"].asInt() == 1);
                int off = args.size() >= 4 ? args[2].asInt() : 0;
                int len = args.size() >= 4 ? args[3].asInt() : (int)dst->longData.size();
                if (srcBytes) {
                    for (int i = 0; i < len; ++i) {
                        int bPos = pos + i * 8;
                        if (bPos + 7 < (int)srcBytes->byteData.size() && off + i < (int)dst->longData.size()) {
                            uint64_t v = 0;
                            if (littleEndian) {
                                for (int b = 0; b < 8; ++b) v |= ((uint64_t)srcBytes->byteData[bPos + b] << (8 * b));
                            } else {
                                for (int b = 0; b < 8; ++b) v = (v << 8) | srcBytes->byteData[bPos + b];
                            }
                            dst->longData[off + i] = (int64_t)v;
                        }
                    }
                    lb->fields["pos"] = JavaValue(pos + len * 8);
                }
            }
            outResult = args[0];
            return true;
        }
    }

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
            else if (prop == "microedition.hostname") outResult = JavaValue(createString("localhost"), true);
            else if (prop == "microedition.media.version") outResult = JavaValue(createString("1.1"), true);
            else if (prop == "microedition.jtwi.version") outResult = JavaValue(createString("1.0"), true);
            else if (prop == "file.separator") outResult = JavaValue(createString("/"), true);
            else if (prop == "line.separator") outResult = JavaValue(createString("\n"), true);
            else if (prop == "path.separator") outResult = JavaValue(createString(":"), true);
            else if (prop == "fileconn.dir.photos") outResult = JavaValue(createString("file:///root/photos/"), true);
            else if (prop == "fileconn.dir.music") outResult = JavaValue(createString("file:///root/music/"), true);
            else if (prop == "fileconn.dir.memorycard") outResult = JavaValue(createString("file:///SDCard/"), true);
            else if (prop == "user.name") outResult = JavaValue(createString("J2HienLoader"), true);
            else if (prop == "user.home" || prop == "user.dir") outResult = JavaValue(createString("/"), true);
            else if (prop == "os.name") outResult = JavaValue(createString("iOS"), true);
            else if (prop == "os.version") outResult = JavaValue(createString("17.0"), true);
            else if (prop == "java.version") outResult = JavaValue(createString("1.4.2"), true);
            else if (prop == "java.vendor") outResult = JavaValue(createString("J2HienLoader"), true);
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
        if (methodName == "<init>") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) {
                if (args.size() == 1) {
                    obj->stringVal = "";
                } else if (args.size() >= 2 && desc.find("([C") != std::string::npos) {
                    JavaArray* arr = getArray(args[1].asRef());
                    int off = args.size() >= 4 ? args[2].asInt() : 0;
                    int len = args.size() >= 4 ? args[3].asInt() : (arr ? (int)arr->charData.size() : 0);
                    if (arr && off >= 0 && off + len <= (int)arr->charData.size()) {
                        std::string s = "";
                        for (int i = 0; i < len; ++i) {
                            appendUtf8Char(s, arr->charData[off + i]);
                        }
                        obj->stringVal = std::move(s);
                    }
                } else if (args.size() >= 2 && desc.find("([B") != std::string::npos) {
                    JavaArray* arr = getArray(args[1].asRef());
                    int off = args.size() >= 4 ? args[2].asInt() : 0;
                    int len = args.size() >= 4 ? args[3].asInt() : (arr ? (int)arr->byteData.size() : 0);
                    if (arr && off >= 0 && off + len <= (int)arr->byteData.size()) {
                        obj->stringVal = std::string((char*)(arr->byteData.data() + off), len);
                    }
                } else if (args.size() >= 2 && args[1].type == JavaValue::OBJ_REF) {
                    obj->stringVal = getString(args[1].asRef());
                }
            }
            return true;
        }
        if (methodName == "valueOf") {
            if (args.size() >= 1) {
                if (desc.find("(Z)") != std::string::npos) {
                    outResult = JavaValue(createString(args[0].asInt() ? "true" : "false"), true);
                } else if (desc.find("(C)") != std::string::npos) {
                    uint16_t ch = (uint16_t)args[0].asInt();
                    std::string s = "";
                    if (ch < 0x80) {
                        s += (char)ch;
                    } else if (ch < 0x800) {
                        s += (char)(0xC0 | (ch >> 6));
                        s += (char)(0x80 | (ch & 0x3F));
                    } else {
                        s += (char)(0xE0 | (ch >> 12));
                        s += (char)(0x80 | ((ch >> 6) & 0x3F));
                        s += (char)(0x80 | (ch & 0x3F));
                    }
                    outResult = JavaValue(createString(s), true);
                    return true;
                } else if (desc.find("(J)") != std::string::npos) {
                    outResult = JavaValue(createString(std::to_string(args[0].asLong())), true);
                } else if (desc.find("(F)") != std::string::npos) {
                    outResult = JavaValue(createString(std::to_string(args[0].asFloat())), true);
                } else if (desc.find("(D)") != std::string::npos) {
                    outResult = JavaValue(createString(std::to_string(args[0].asDouble())), true);
                } else if (desc.find("(Ljava/lang/Object;)") != std::string::npos) {
                    uint32_t oRef = args[0].asRef();
                    if (oRef == 0) {
                        outResult = JavaValue(createString("null"), true);
                    } else {
                        JavaObject* o = getObject(oRef);
                        if (o && (o->className == "java/lang/String" || o->className == "java/lang/StringBuffer" || o->className == "java/lang/StringBuilder")) {
                            outResult = JavaValue(createString(o->stringVal), true);
                        } else if (o && o->fields.find("value") != o->fields.end()) {
                            auto v = o->fields["value"];
                            if (v.type == JavaValue::INT) outResult = JavaValue(createString(std::to_string(v.asInt())), true);
                            else if (v.type == JavaValue::LONG) outResult = JavaValue(createString(std::to_string(v.asLong())), true);
                            else if (v.type == JavaValue::FLOAT) outResult = JavaValue(createString(std::to_string(v.asFloat())), true);
                            else if (v.type == JavaValue::DOUBLE) outResult = JavaValue(createString(std::to_string(v.asDouble())), true);
                            else outResult = JavaValue(createString(v.asInt() ? "true" : "false"), true);
                        } else {
                            outResult = JavaValue(createString(""), true);
                        }
                    }
                } else {
                    outResult = JavaValue(createString(std::to_string(args[0].asInt())), true);
                }
            } else {
                outResult = JavaValue(createString(""), true);
            }
            return true;
        }
        if (methodName == "length") {
            std::string s = getString(args[0].asRef());
            int charCount = 0;
            for (size_t i = 0; i < s.size();) {
                uint8_t b0 = (uint8_t)s[i++];
                if ((b0 & 0xE0) == 0xC0 && i < s.size()) i += 1;
                else if ((b0 & 0xF0) == 0xE0 && i + 1 < s.size()) i += 2;
                charCount++;
            }
            outResult = JavaValue((int32_t)charCount);
            return true;
        }
        if (methodName == "charAt" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int targetIdx = args[1].asInt();
            if (targetIdx < 0) { outResult = JavaValue(0); return true; }
            int curCharIdx = 0;
            uint32_t foundCp = 0;
            for (size_t i = 0; i < s.size();) {
                uint32_t cp = 0;
                uint8_t b0 = (uint8_t)s[i++];
                if (b0 < 0x80) cp = b0;
                else if ((b0 & 0xE0) == 0xC0 && i < s.size()) {
                    cp = ((b0 & 0x1F) << 6) | ((uint8_t)s[i++] & 0x3F);
                } else if ((b0 & 0xF0) == 0xE0 && i + 1 < s.size()) {
                    cp = ((b0 & 0x0F) << 12) | (((uint8_t)s[i] & 0x3F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
                    i += 2;
                } else cp = b0;
                if (curCharIdx == targetIdx) {
                    foundCp = cp;
                    break;
                }
                curCharIdx++;
            }
            outResult = JavaValue((int32_t)foundCp);
            return true;
        }
        if (methodName == "substring" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int begin = std::max(0, args[1].asInt());
            size_t bStart = utf8CharToByteOffset(s, begin);
            if (args.size() >= 3) {
                int end = std::max(begin, args[2].asInt());
                size_t bEnd = utf8CharToByteOffset(s, end);
                outResult = JavaValue(createString(s.substr(bStart, bEnd - bStart)), true);
            } else {
                outResult = JavaValue(createString(s.substr(bStart)), true);
            }
            return true;
        }
        if (methodName == "indexOf" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int fromIndex = (args.size() >= 3) ? std::max(0, args[2].asInt()) : 0;
            if (fromIndex > (int)s.length()) {
                outResult = JavaValue(-1);
                return true;
            }
            if (desc.find("(Ljava/lang/String;") != std::string::npos || (args[1].type == JavaValue::OBJ_REF && getObject(args[1].asRef()) && getObject(args[1].asRef())->className == "java/lang/String")) {
                std::string target = getString(args[1].asRef());
                size_t pos = s.find(target, fromIndex);
                outResult = JavaValue(pos != std::string::npos ? (int32_t)pos : -1);
            } else {
                uint32_t targetCp = (uint32_t)args[1].asInt();
                int curCharIdx = 0;
                int foundIdx = -1;
                for (size_t i = 0; i < s.size();) {
                    uint32_t cp = 0;
                    uint8_t b0 = (uint8_t)s[i++];
                    if (b0 < 0x80) cp = b0;
                    else if ((b0 & 0xE0) == 0xC0 && i < s.size()) {
                        cp = ((b0 & 0x1F) << 6) | ((uint8_t)s[i++] & 0x3F);
                    } else if ((b0 & 0xF0) == 0xE0 && i + 1 < s.size()) {
                        cp = ((b0 & 0x0F) << 12) | (((uint8_t)s[i] & 0x3F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
                        i += 2;
                    } else cp = b0;
                    if (curCharIdx >= fromIndex && cp == targetCp) {
                        foundIdx = curCharIdx;
                        break;
                    }
                    curCharIdx++;
                }
                outResult = JavaValue(foundIdx);
            }
            return true;
        }
        if (methodName == "lastIndexOf" && args.size() >= 2) {
            std::string s = getString(args[0].asRef());
            int fromIndex = (args.size() >= 3) ? std::min((int)s.length() - 1, args[2].asInt()) : (int)s.length() - 1;
            if (fromIndex < 0) {
                outResult = JavaValue(-1);
                return true;
            }
            if (desc.find("(Ljava/lang/String;") != std::string::npos || (args[1].type == JavaValue::OBJ_REF && getObject(args[1].asRef()) && getObject(args[1].asRef())->className == "java/lang/String")) {
                std::string target = getString(args[1].asRef());
                size_t pos = s.rfind(target, fromIndex);
                outResult = JavaValue(pos != std::string::npos ? (int32_t)pos : -1);
            } else {
                char c = (char)args[1].asInt();
                size_t pos = s.rfind(c, fromIndex);
                outResult = JavaValue(pos != std::string::npos ? (int32_t)pos : -1);
            }
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
                if (desc.find("(C)") != std::string::npos) {
                    appendUtf8Char(obj->stringVal, (uint16_t)args[1].asInt());
                } else if (desc.find("(Z)") != std::string::npos) {
                    obj->stringVal += args[1].asInt() ? "true" : "false";
                } else if (desc.find("(J)") != std::string::npos) {
                    obj->stringVal += std::to_string(args[1].asLong());
                } else if (desc.find("(F)") != std::string::npos) {
                    obj->stringVal += std::to_string(args[1].asFloat());
                } else if (desc.find("(D)") != std::string::npos) {
                    obj->stringVal += std::to_string(args[1].asDouble());
                } else if (desc.find("([CII)") != std::string::npos && args.size() >= 4) {
                    JavaArray* ca = getArray(args[1].asRef());
                    int off = args[2].asInt(), len = args[3].asInt();
                    if (ca) {
                        for (int i = 0; i < len && off + i < (int)ca->charData.size(); ++i) {
                            appendUtf8Char(obj->stringVal, ca->charData[off + i]);
                        }
                    }
                } else if (desc.find("([C)") != std::string::npos) {
                    JavaArray* ca = getArray(args[1].asRef());
                    if (ca) {
                        for (size_t i = 0; i < ca->charData.size(); ++i) {
                            appendUtf8Char(obj->stringVal, ca->charData[i]);
                        }
                    }
                } else if (args[1].type == JavaValue::OBJ_REF) {
                    uint32_t r = args[1].asRef();
                    if (r == 0) {
                        obj->stringVal += "null";
                    } else {
                        JavaObject* argObj = getObject(r);
                        if (argObj && (argObj->className == "java/lang/String" || argObj->className == "java/lang/StringBuffer" || argObj->className == "java/lang/StringBuilder")) {
                            obj->stringVal += argObj->stringVal;
                        } else if (argObj && argObj->fields.find("value") != argObj->fields.end()) {
                            auto v = argObj->fields["value"];
                            if (v.type == JavaValue::INT) obj->stringVal += std::to_string(v.asInt());
                            else if (v.type == JavaValue::LONG) obj->stringVal += std::to_string(v.asLong());
                            else if (v.type == JavaValue::FLOAT) obj->stringVal += std::to_string(v.asFloat());
                            else if (v.type == JavaValue::DOUBLE) obj->stringVal += std::to_string(v.asDouble());
                            else obj->stringVal += (v.asInt() ? "true" : "false");
                        } else if (argObj) {
                            obj->stringVal += argObj->stringVal;
                        }
                    }
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
                int newLen = std::max(0, args[1].asInt());
                if ((size_t)newLen < obj->stringVal.length()) {
                    obj->stringVal.resize(newLen);
                } else if ((size_t)newLen > obj->stringVal.length()) {
                    obj->stringVal.resize(newLen, '\0');
                }
            }
            return true;
        }
        if (methodName == "charAt" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            int idx = args[1].asInt();
            outResult = JavaValue((obj && idx >= 0 && idx < (int)obj->stringVal.length()) ? (int32_t)(uint8_t)obj->stringVal[idx] : 0);
            return true;
        }
        if (methodName == "setCharAt" && args.size() >= 3) {
            JavaObject* obj = getObject(args[0].asRef());
            int idx = args[1].asInt();
            char ch = (char)args[2].asInt();
            if (obj && idx >= 0 && idx < (int)obj->stringVal.length()) {
                obj->stringVal[idx] = ch;
            }
            return true;
        }
        if (methodName == "delete" && args.size() >= 3) {
            JavaObject* obj = getObject(args[0].asRef());
            int start = std::max(0, args[1].asInt());
            int end = obj ? std::min((int)obj->stringVal.length(), args[2].asInt()) : 0;
            if (obj && start < end && start < (int)obj->stringVal.length()) {
                obj->stringVal.erase(start, end - start);
            }
            outResult = JavaValue(args[0].asRef(), true);
            return true;
        }
        if (methodName == "deleteCharAt" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            int idx = args[1].asInt();
            if (obj && idx >= 0 && idx < (int)obj->stringVal.length()) {
                obj->stringVal.erase(idx, 1);
            }
            outResult = JavaValue(args[0].asRef(), true);
            return true;
        }
        if (methodName == "reverse") {
            JavaObject* obj = getObject(args[0].asRef());
            if (obj) {
                std::reverse(obj->stringVal.begin(), obj->stringVal.end());
            }
            outResult = JavaValue(args[0].asRef(), true);
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
            std::cout << "[JVM] Display.setCurrent called with nextRef=" << nextRef << std::endl;
            JavaObject* canvasObj = getObject(nextRef);
            if (canvasObj && !canvasObj->className.empty()) {
                std::cout << "[JVM] Display.setCurrent target className: " << canvasObj->className << std::endl;
                auto cls = findOrLoadClass(canvasObj->className, m_activeJar);
                // Only Canvas subclasses become the paint target. High-level
                // screens (Form/List/TextBox/Alert) fall through to FullApis,
                // which renders them and tracks CommandListener. Binding them
                // here would paint black and kill their commands.
                bool isCanvas = false;
                auto c = cls;
                for (int d = 0; c && d < 16; ++d) {
                    if (c->thisClassName == "javax/microedition/lcdui/Canvas" ||
                        c->thisClassName == "javax/microedition/lcdui/game/GameCanvas" ||
                        c->thisClassName == "javax/microedition/lcdui/FullCanvas" ||
                        c->thisClassName.find("Canvas") != std::string::npos ||
                        c->superClassName == "javax/microedition/lcdui/Canvas" ||
                        c->superClassName == "javax/microedition/lcdui/game/GameCanvas" ||
                        c->superClassName == "javax/microedition/lcdui/FullCanvas" ||
                        c->superClassName.find("Canvas") != std::string::npos ||
                        c->methods.find("paint:(Ljavax/microedition/lcdui/Graphics;)V") != c->methods.end()) {
                        isCanvas = true;
                        break;
                    }
                    if (c->superClassName.empty()) break;
                    c = findOrLoadClass(c->superClassName, m_activeJar);
                }
                if (isCanvas) {
                    JvmInterpreter::getInstance().setCurrentCanvas(nextRef, cls);
                    return true;
                }
                return false; // let FullApis handle high-level screens
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
            // Region variant createImage(Image,int,int,int,int,int) MUST come first:
            // its descriptor contains "(L...Image;" as a prefix substring.
            if (desc.find("(Ljavax/microedition/lcdui/Image;IIIII)") != std::string::npos && args.size() >= 6) {
                syncImageFromDisplay(args[0].asRef());
                NativeImage* src = getNativeImage(args[0].asRef());
                int x = args[1].asInt(), y = args[2].asInt(), w = args[3].asInt(), h = args[4].asInt();
                int t = args[5].asInt();
                bool swapDims = (t == 4 || t == 5 || t == 6 || t == 7);
                int dw = swapDims ? h : w, dh = swapDims ? w : h;
                uint32_t resRef = allocateNativeImage(dw > 0 ? dw : 1, dh > 0 ? dh : 1, false);
                NativeImage* dst = getNativeImage(resRef);
                if (src && dst && w > 0 && h > 0) {
                    auto at = [&](int c, int r) -> uint32_t {
                        int sx = x + c, sy = y + r;
                        if (sx >= 0 && sx < src->width && sy >= 0 && sy < src->height)
                            return src->pixels[(size_t)sy * src->width + sx];
                        return 0;
                    };
                    for (int r = 0; r < h; ++r) {
                        for (int c = 0; c < w; ++c) {
                            uint32_t px = at(c, r);
                            int dx = c, dy = r;
                            if (t == 1) dx = w - 1 - c;
                            else if (t == 2) dy = h - 1 - r;
                            else if (t == 3) { dx = w - 1 - c; dy = h - 1 - r; }
                            else if (t == 4) { dx = r; dy = c; }
                            else if (t == 5) { dx = h - 1 - r; dy = c; }
                            else if (t == 6) { dx = r; dy = w - 1 - c; }
                            else if (t == 7) { dx = h - 1 - r; dy = w - 1 - c; }
                            if (dx >= 0 && dx < dw && dy >= 0 && dy < dh) {
                                dst->pixels[(size_t)dy * dw + dx] = px;
                            }
                        }
                    }
                }
                outResult = JavaValue(resRef, true);
                return true;
            }
            if (desc.find("(Ljavax/microedition/lcdui/Image;)") != std::string::npos && args.size() >= 1) {
                // Immutable copy of existing image
                syncImageFromDisplay(args[0].asRef());
                NativeImage* src = getNativeImage(args[0].asRef());
                int w = src ? src->width : 16, h = src ? src->height : 16;
                uint32_t resRef = allocateNativeImage(w, h, false);
                NativeImage* dst = getNativeImage(resRef);
                if (src && dst) dst->pixels = src->pixels;
                outResult = JavaValue(resRef, true);
                return true;
            }
            if (desc.find("(Ljava/lang/String;)") != std::string::npos && args.size() >= 1) {
                std::string path = getString(args[0].asRef());
                outResult = JavaValue(loadNativeImageFromJar(path), true);
                return true;
            }
            if (desc.find("(Ljava/io/InputStream;)") != std::string::npos && args.size() >= 1) {
                JavaObject* isObj = getObject(args[0].asRef());
                if (isObj && isObj->fields.find("buf") != isObj->fields.end()) {
                    JavaArray* arr = getArray(isObj->fields["buf"].asRef());
                    int pos = isObj->fields.find("pos") != isObj->fields.end() ? isObj->fields["pos"].asInt() : 0;
                    if (arr && pos >= 0 && pos < (int)arr->byteData.size()) {
                        size_t len = arr->byteData.size() - pos;
                        outResult = JavaValue(loadNativeImageFromBytes(arr->byteData.data() + pos, len), true);
                        return true;
                    }
                }
                outResult = JavaValue(allocateNativeImage(16, 16, false), true);
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
            // Bind the Graphics to this Image so offscreen drawing persists
            // (double-buffered games); falls back to screen if image unknown.
            uint32_t imgRef = args.empty() ? 0 : args[0].asRef();
            outResult = JavaValue(graphicsForImage(imgRef), true);
            return true;
        }
        if (methodName == "isMutable") {
            NativeImage* img = getNativeImage(args[0].asRef());
            outResult = JavaValue(img && img->isMutable ? 1 : 0);
            return true;
        }
        if (methodName == "getRGB") {
            syncImageFromDisplay(args[0].asRef());
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
        // Route to the offscreen Image display when this Graphics came from
        // Image.getGraphics() (double-buffered games); else the screen.
        LcduiDisplay* tgt = args.empty() ? display : resolveGraphics(args[0].asRef(), display);
        if (!tgt) return true;
        if (methodName == "setColor") {
            if (args.size() == 2) {
                tgt->setColor((uint32_t)(args[1].asInt() | 0xFF000000));
            } else if (args.size() >= 4) {
                uint32_t r = (args[1].asInt() & 0xFF), g = (args[2].asInt() & 0xFF), b = (args[3].asInt() & 0xFF);
                tgt->setColor(0xFF000000 | (r << 16) | (g << 8) | b);
            }
            return true;
        }
        if (methodName == "getColor") {
            outResult = JavaValue((int32_t)(tgt->getColor() & 0x00FFFFFF));
            return true;
        }
        if (methodName == "fillRect" && args.size() >= 5) {
            tgt->fillRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawRect" && args.size() >= 5) {
            tgt->drawRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawLine" && args.size() >= 5) {
            tgt->drawLine(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawRoundRect" && args.size() >= 7) {
            tgt->drawRoundRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "fillRoundRect" && args.size() >= 7) {
            tgt->fillRoundRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawArc" && args.size() >= 7) {
            tgt->drawArc(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "fillArc" && args.size() >= 7) {
            tgt->fillArc(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawString" && args.size() >= 5) {
            std::string text = getString(args[1].asRef());
            tgt->drawString(text, args[2].asInt(), args[3].asInt(), args[4].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawSubstring" && args.size() >= 7) {
            std::string text = getString(args[1].asRef());
            int off = args[2].asInt(), len = args[3].asInt();
            if (off >= 0 && off + len <= (int)text.length()) {
                tgt->drawString(text.substr(off, len), args[4].asInt(), args[5].asInt(), args[6].asInt(), tgt->getColor());
            }
            return true;
        }
        if (methodName == "drawChar" && args.size() >= 5) {
            tgt->drawChar((char)args[1].asInt(), args[2].asInt(), args[3].asInt(), tgt->getColor());
            return true;
        }
        if (methodName == "drawImage" && args.size() >= 5) {
            NativeImage* img = getNativeImage(args[1].asRef());
            const uint32_t* px = readableImagePixels(args[1].asRef());
            if (img && px) {
                tgt->drawRegion(px, img->width, img->height, 0, 0, img->width, img->height, 0, args[2].asInt(), args[3].asInt(), args[4].asInt());
            }
            return true;
        }
        if (methodName == "drawRegion" && args.size() >= 10) {
            NativeImage* img = getNativeImage(args[1].asRef());
            const uint32_t* px = readableImagePixels(args[1].asRef());
            if (img && px) {
                tgt->drawRegion(px, img->width, img->height, args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), args[7].asInt(), args[8].asInt(), args[9].asInt());
            }
            return true;
        }
        if (methodName == "drawRGB" && args.size() >= 9) {
            JavaArray* arr = getArray(args[1].asRef());
            if (arr && !arr->intData.empty()) {
                tgt->drawRGB(arr->intData.data(), args[2].asInt(), args[3].asInt(), args[4].asInt(), args[5].asInt(), args[6].asInt(), args[7].asInt(), args[8].asInt() != 0);
            }
            return true;
        }
        if (methodName == "setClip" && args.size() >= 5) {
            tgt->setClip(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt());
            return true;
        }
        if (methodName == "clipRect" && args.size() >= 5) {
            tgt->clipRect(args[1].asInt(), args[2].asInt(), args[3].asInt(), args[4].asInt());
            return true;
        }
        if (methodName == "getClipX") { outResult = JavaValue(tgt->getClipX()); return true; }
        if (methodName == "getClipY") { outResult = JavaValue(tgt->getClipY()); return true; }
        if (methodName == "getClipWidth") { outResult = JavaValue(tgt->getClipWidth()); return true; }
        if (methodName == "getClipHeight") { outResult = JavaValue(tgt->getClipHeight()); return true; }
        if (methodName == "translate" && args.size() >= 3) {
            tgt->translate(args[1].asInt(), args[2].asInt());
            return true;
        }
        if (methodName == "getTranslateX") { outResult = JavaValue(tgt->getTranslateX()); return true; }
        if (methodName == "getTranslateY") { outResult = JavaValue(tgt->getTranslateY()); return true; }
        if (methodName == "setFont") return true;
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
            // Display is continuously refreshed at 60 FPS by the execution loop.
            // Do not synchronously invoke paint() here to prevent infinite recursion / stack overflow.
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
            outResult = JavaValue(JvmInterpreter::getInstance().getKeyStates());
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
                std::string origPath = path;
                if (path[0] == '/') path.erase(0, 1);
                std::vector<uint8_t> bytes;
                bool ok = m_activeJar->extractEntry(path, bytes);
                if (!ok && path != origPath) {
                    ok = m_activeJar->extractEntry(origPath, bytes);
                }
                if (!ok) {
                    static const char* kPrefixes[] = { "x1/", "x2/", "res/", "data/" };
                    for (const char* pfx : kPrefixes) {
                        std::string pfxPath = std::string(pfx) + path;
                        if (m_activeJar->extractEntry(pfxPath, bytes)) {
                            ok = true;
                            break;
                        }
                    }
                }
                if (!ok) {
                    auto entries = m_activeJar->listEntries();
                    for (const auto& ent : entries) {
                        std::string cleanEnt = ent;
                        if (!cleanEnt.empty() && cleanEnt[0] == '/') cleanEnt.erase(0, 1);
                        if (cleanEnt == path || toLowerStr(cleanEnt) == toLowerStr(path)) {
                            ok = m_activeJar->extractEntry(ent, bytes);
                            break;
                        }
                    }
                }
                if (ok) {
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
                    int off = (args.size() >= 4) ? args[2].asInt() : 0;
                    int len = (args.size() >= 4) ? args[3].asInt() : (int)arr->byteData.size();
                    obj->fields["pos"] = JavaValue(std::max(0, off));
                    obj->fields["count"] = JavaValue(std::min((int)arr->byteData.size(), std::max(0, off) + std::max(0, len)));
                } else {
                    JavaObject* innerStream = getObject(args[1].asRef());
                    if (innerStream) {
                        obj->fields["buf"] = innerStream->fields["buf"];
                        obj->fields["pos"] = innerStream->fields["pos"];
                        if (innerStream->fields.count("count")) {
                            obj->fields["count"] = innerStream->fields["count"];
                        }
                    }
                }
            }
            return true;
        }
        auto ensureSocketBuffer = [&](JavaObject* obj, int needed) {
            if (!obj) return;
            auto sfi = obj->fields.find("sockFd");
            if (sfi == obj->fields.end() || sfi->second.asInt() < 0) return;
            JavaArray* ba = getArray(obj->fields["buf"].asRef());
            int pp = obj->fields["pos"].asInt();
            int avail = ba ? ((int)ba->byteData.size() - pp) : 0;
            if (avail >= needed) return;

#if !defined(_WIN32) && !defined(_WIN64)
            int fd = sfi->second.asInt();
            int retries = 0;
            while (avail < needed && retries < 150) { // wait up to 3 seconds for requested bytes
                fd_set rs; FD_ZERO(&rs); FD_SET(fd, &rs);
                struct timeval tv{0, 20000}; // wait up to 20ms
                int r = select(fd + 1, &rs, nullptr, nullptr, &tv);
                if (r < 0) break;
                if (r == 0) {
                    retries++;
                    continue;
                }
                uint8_t tmp[4096];
                ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
                if (n <= 0) break;

                if (!ba || pp >= (int)ba->byteData.size()) {
                    uint32_t na = allocArray(8, (int)n);
                    JavaArray* naa = getArray(na);
                    if (naa) naa->byteData.assign(tmp, tmp + n);
                    obj->fields["buf"] = JavaValue(na, true);
                    obj->fields["pos"] = JavaValue(0);
                    ba = naa;
                    pp = 0;
                    avail = (int)n;
                } else {
                    ba->byteData.insert(ba->byteData.end(), tmp, tmp + n);
                    avail = (int)ba->byteData.size() - pp;
                }
            }
#endif
        };

        if (methodName == "read") {
            JavaObject* obj = getObject(args[0].asRef());
            if (!obj) { outResult = JavaValue(-1); return true; }
            int need = (args.size() >= 4) ? args[3].asInt() : ((args.size() == 2 && getArray(args[1].asRef())) ? (int)getArray(args[1].asRef())->byteData.size() : 1);
            ensureSocketBuffer(obj, std::max(1, need));

            JavaArray* arr = getArray(obj->fields["buf"].asRef());
            int pos = obj->fields["pos"].asInt();
            int limit = arr ? (int)arr->byteData.size() : 0;
            auto cit = obj->fields.find("count");
            if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());

            if (arr && pos >= 0 && pos < limit) {
                if (args.size() == 1) {
                    outResult = JavaValue((int32_t)(uint8_t)arr->byteData[pos]);
                    obj->fields["pos"] = JavaValue(pos + 1);
                } else if (args.size() == 2) {
                    JavaArray* dst = getArray(args[1].asRef());
                    int len = dst ? (int)dst->byteData.size() : 0;
                    int available = limit - pos;
                    int count = std::min(len, available);
                    if (count <= 0) { outResult = JavaValue(-1); return true; }
                    for (int i = 0; i < count; ++i) dst->byteData[i] = arr->byteData[pos + i];
                    obj->fields["pos"] = JavaValue(pos + count);
                    outResult = JavaValue(count);
                } else if (args.size() >= 4) {
                    JavaArray* dst = getArray(args[1].asRef());
                    int off = args[2].asInt(), len = args[3].asInt();
                    int available = limit - pos;
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
            ensureSocketBuffer(obj, 1);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos >= 0 && pos < limit) {
                uint8_t b = arr->byteData[pos];
                obj->fields["pos"] = JavaValue(pos + 1);
                outResult = JavaValue(methodName == "readByte" ? (int32_t)(int8_t)b : (int32_t)b);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readBoolean") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 1);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos >= 0 && pos < limit) {
                uint8_t b = arr->byteData[pos];
                obj->fields["pos"] = JavaValue(pos + 1);
                outResult = JavaValue(b != 0 ? 1 : 0);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readShort" || methodName == "readUnsignedShort") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 2);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 1 < limit) {
                uint16_t s = ((uint16_t)arr->byteData[pos] << 8) | arr->byteData[pos + 1];
                obj->fields["pos"] = JavaValue(pos + 2);
                outResult = JavaValue(methodName == "readShort" ? (int32_t)(int16_t)s : (int32_t)s);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readChar") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 2);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 1 < limit) {
                uint16_t s = ((uint16_t)arr->byteData[pos] << 8) | arr->byteData[pos + 1];
                obj->fields["pos"] = JavaValue(pos + 2);
                outResult = JavaValue((int32_t)s);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readFloat") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 4);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 3 < limit) {
                uint32_t val = ((uint32_t)arr->byteData[pos] << 24) |
                               ((uint32_t)arr->byteData[pos + 1] << 16) |
                               ((uint32_t)arr->byteData[pos + 2] << 8) |
                               ((uint32_t)arr->byteData[pos + 3]);
                obj->fields["pos"] = JavaValue(pos + 4);
                float f;
                std::memcpy(&f, &val, 4);
                outResult = JavaValue(f);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0.0f);
            }
            return true;
        }
        if (methodName == "readDouble") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 8);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 7 < limit) {
                uint64_t val = 0;
                for (int i = 0; i < 8; ++i) val = (val << 8) | arr->byteData[pos + i];
                obj->fields["pos"] = JavaValue(pos + 8);
                double d;
                std::memcpy(&d, &val, 8);
                outResult = JavaValue(d);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0.0);
            }
            return true;
        }
        if (methodName == "readInt") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 4);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 3 < limit) {
                int32_t val = ((int32_t)arr->byteData[pos] << 24) |
                              ((int32_t)arr->byteData[pos + 1] << 16) |
                              ((int32_t)arr->byteData[pos + 2] << 8) |
                              ((int32_t)arr->byteData[pos + 3]);
                obj->fields["pos"] = JavaValue(pos + 4);
                outResult = JavaValue(val);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(0);
            }
            return true;
        }
        if (methodName == "readLong") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 8);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 7 < limit) {
                int64_t val = 0;
                for (int i = 0; i < 8; ++i) val = (val << 8) | arr->byteData[pos + i];
                obj->fields["pos"] = JavaValue(pos + 8);
                outResult = JavaValue(val);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue((int64_t)0);
            }
            return true;
        }
        if (methodName == "readUTF") {
            JavaObject* obj = getObject(args[0].asRef());
            ensureSocketBuffer(obj, 2);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && pos + 1 < limit) {
                uint16_t len = ((uint16_t)arr->byteData[pos] << 8) | arr->byteData[pos + 1];
                pos += 2;
                ensureSocketBuffer(obj, len);
                arr = getArray(obj->fields["buf"].asRef());
                limit = arr ? (int)arr->byteData.size() : 0;
                if (obj) {
                    auto cit2 = obj->fields.find("count");
                    if (cit2 != obj->fields.end()) limit = std::min(limit, cit2->second.asInt());
                }
                std::string s = "";
                if (arr && pos + len <= limit) {
                    s = std::string((char*)(arr->byteData.data() + pos), len);
                    pos += len;
                }
                obj->fields["pos"] = JavaValue(pos);
                outResult = JavaValue(createString(s), true);
            } else {
                setPendingException(allocObject("java/io/EOFException"));
                outResult = JavaValue(createString(""), true);
            }
            return true;
        }
        if (methodName == "readFully" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* dst = getArray(args[1].asRef());
            int off = args.size() >= 4 ? args[2].asInt() : 0;
            int len = args.size() >= 4 ? args[3].asInt() : (dst ? (int)dst->byteData.size() : 0);
            ensureSocketBuffer(obj, len);
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            if (arr && dst && len > 0) {
                int count = std::min(len, limit - pos);
                for (int i = 0; i < count; ++i) {
                    if (off + i < (int)dst->byteData.size() && pos + i < limit) {
                        dst->byteData[off + i] = arr->byteData[pos + i];
                    }
                }
                obj->fields["pos"] = JavaValue(pos + count);
                if (count < len) {
                    setPendingException(allocObject("java/io/EOFException"));
                }
            }
            return true;
        }
        if (methodName == "skip" || methodName == "skipBytes") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int64_t n = args.size() >= 2 ? args[1].asLong() : 0;
            int skipped = 0;
            if (obj && arr && n > 0) {
                int pos = obj->fields["pos"].asInt();
                int limit = (int)arr->byteData.size();
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
                int maxSkip = limit - pos;
                skipped = std::max(0, std::min((int)n, maxSkip));
                obj->fields["pos"] = JavaValue(pos + skipped);
            }
            outResult = JavaValue((int32_t)skipped);
            return true;
        }
        if (methodName == "available") {
            JavaObject* obj = getObject(args[0].asRef());
            JavaArray* arr = obj ? getArray(obj->fields["buf"].asRef()) : nullptr;
            int pos = obj ? obj->fields["pos"].asInt() : 0;
            int limit = arr ? (int)arr->byteData.size() : 0;
            if (obj) {
                auto cit = obj->fields.find("count");
                if (cit != obj->fields.end()) limit = std::min(limit, cit->second.asInt());
            }
            int avail = std::max(0, limit - pos);
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
                JavaObject* targetObj = getObject(args[1].asRef());
                // Only assign target if it is not a String (String argument is Thread name)
                if (tObj && targetObj && targetObj->className != "java/lang/String") {
                    tObj->fields["target"] = args[1];
                }
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
                if (tObj && tObj->fields.find("target") != tObj->fields.end()) {
                    uint32_t candidate = tObj->fields["target"].asRef();
                    JavaObject* candObj = getObject(candidate);
                    if (candObj && candObj->className != "java/lang/String" && !candObj->className.empty()) {
                        rRef = candidate;
                    }
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
        if (methodName == "deleteRecordStore") {
            std::string name = (args.size() > 0 && args[0].asRef() != 0) ? getString(args[0].asRef()) : "";
            if (!name.empty()) RmsStorage::getInstance().deleteRecordStore("J2MEApp", name);
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
        if (methodName == "setRecord" && args.size() >= 5) {
            JavaObject* obj = getObject(args[0].asRef());
            int recId = args[1].asInt();
            JavaArray* arr = getArray(args[2].asRef());
            int off = args[3].asInt(), len = args[4].asInt();
            if (obj && arr && off >= 0 && off + len <= (int)arr->byteData.size()) {
                RmsStorage::getInstance().setRecord(obj->stringVal, recId, arr->byteData.data() + off, len);
            }
            return true;
        }
        if (methodName == "deleteRecord" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            int recId = args[1].asInt();
            if (obj) {
                RmsStorage::getInstance().deleteRecord(obj->stringVal, recId);
            }
            return true;
        }
        if (methodName == "getRecord") {
            JavaObject* obj = getObject(args[0].asRef());
            int recId = args[1].asInt();
            std::vector<uint8_t> data;
            if (obj && RmsStorage::getInstance().getRecord(obj->stringVal, recId, data)) {
                if (args.size() >= 4) {
                    JavaArray* dst = getArray(args[2].asRef());
                    int off = args[3].asInt();
                    if (dst && off >= 0) {
                        int copyLen = std::min((int)data.size(), (int)dst->byteData.size() - off);
                        for (int i = 0; i < copyLen; ++i) dst->byteData[off + i] = data[i];
                        outResult = JavaValue((int32_t)data.size());
                    } else {
                        outResult = JavaValue(0);
                    }
                } else {
                    uint32_t arrRef = allocArray(8, (int)data.size());
                    JavaArray* arr = getArray(arrRef);
                    if (arr) arr->byteData = std::move(data);
                    outResult = JavaValue(arrRef, true);
                }
            } else {
                outResult = (args.size() >= 4) ? JavaValue(0) : JavaValue(0, true);
            }
            return true;
        }
        if (methodName == "getNumRecords") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(obj ? RmsStorage::getInstance().getNumRecords(obj->stringVal) : 0);
            return true;
        }
        if (methodName == "getNextRecordID") {
            JavaObject* obj = getObject(args[0].asRef());
            outResult = JavaValue(obj ? RmsStorage::getInstance().getNextRecordID(obj->stringVal) : 1);
            return true;
        }
        if (methodName == "getRecordSize" && args.size() >= 2) {
            JavaObject* obj = getObject(args[0].asRef());
            int recId = args[1].asInt();
            outResult = JavaValue(obj ? RmsStorage::getInstance().getRecordSize(obj->stringVal, recId) : 0);
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

std::shared_ptr<ClassFile> JvmBytecodeEngine::resolveMethodClass(std::shared_ptr<ClassFile> cls, const std::string& key) {
    for (int d = 0; d < 16 && cls; ++d) {
        auto it = cls->methods.find(key);
        if (it != cls->methods.end() && !it->second.code.empty()) return cls;
        // Native MIDP methods (no Code) still count as defined.
        if (it != cls->methods.end()) return cls;
        if (cls->superClassName.empty()) break;
        cls = findOrLoadClass(cls->superClassName, m_activeJar);
    }
    return nullptr;
}

// ----------------------------------------------------
// Complete JVM Opcode Execution Loop
// ----------------------------------------------------
JavaValue JvmBytecodeEngine::executeMethod(std::shared_ptr<ClassFile> cls, const std::string& methodName, const std::string& desc, const std::vector<JavaValue>& args, LcduiDisplay* display) {
    if (!cls || m_cancel.load()) return JavaValue(0);

    thread_local int t_callDepth = 0;
    if (t_callDepth == 0) {
        clearPendingException();
    }
    if (t_callDepth > 128) {
        return JavaValue(0);
    }
    struct DepthGuard {
        int& d;
        DepthGuard(int& depth) : d(depth) { ++d; }
        ~DepthGuard() {
            --d;
            if (d == 0 && JvmBytecodeEngine::hasPendingException()) {
                JvmBytecodeEngine::clearPendingException();
            }
        }
    } depthGuard(t_callDepth);

    // Run static class initializer (<clinit>) once when class is first accessed
    ensureClinit(cls, display);

    std::string key = methodName + ":" + desc;
    std::shared_ptr<ClassFile> curCls = cls;
    auto it = curCls->methods.find(key);
    while (it == curCls->methods.end() && !curCls->superClassName.empty() && curCls->superClassName != "java/lang/Object") {
        auto superCls = findOrLoadClass(curCls->superClassName, m_activeJar);
        if (!superCls || superCls == curCls) break;
        curCls = superCls;
        it = curCls->methods.find(key);
    }

    if (it == curCls->methods.end()) {
        // Try native dispatch on target class and its superclass hierarchy
        JavaValue res;
        std::string checkClass = cls->thisClassName;
        while (!checkClass.empty()) {
            if (dispatchNativeMethod(checkClass, methodName, desc, args, res, display)) {
                return res;
            }
            auto checkCls = findOrLoadClass(checkClass, m_activeJar);
            if (checkCls && !checkCls->superClassName.empty() && checkCls->superClassName != checkClass && checkCls->superClassName != "java/lang/Object") {
                checkClass = checkCls->superClassName;
            } else {
                break;
            }
        }
        return JavaValue(0);
    }

    const MethodInfo& method = it->second;
    if (method.code.empty()) return JavaValue(0);

    cls = curCls; // Crucial: constant pool indexes in method bytecode belong to curCls!
    StackFrame frame;
    frame.classRef = curCls;
    frame.method = &method;
    size_t localSlot = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        localSlot += (args[i].type == JavaValue::LONG || args[i].type == JavaValue::DOUBLE) ? 2 : 1;
    }
    frame.locals.resize(std::max<size_t>(method.maxLocals, localSlot), JavaValue(0));
    localSlot = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        if (localSlot < frame.locals.size()) {
            frame.locals[localSlot] = args[i];
        }
        localSlot += (args[i].type == JavaValue::LONG || args[i].type == JavaValue::DOUBLE) ? 2 : 1;
    }

    const uint8_t* code = method.code.data();
    size_t codeLen = method.code.size();

    auto throwEngineException = [&](uint32_t exRef, int throwPc) -> bool {
        bool handled = false;
        JavaObject* exObj = getObject(exRef);
        std::string exCls = exObj ? exObj->className : "";
        for (auto &e : method.exTable) {
            if (throwPc >= e.startPc && throwPc < e.endPc) {
                bool match = (e.catchType == 0);
                if (!match && e.catchType < cls->constantPool.size()) {
                    const auto& cp = cls->constantPool[e.catchType];
                    std::string cn;
                    if (cp.tag == 7 && cp.nameIndex < cls->constantPool.size()) cn = cls->constantPool[cp.nameIndex].strVal;
                    else if (!cp.strVal.empty()) cn = cp.strVal;
                    if (!cn.empty() && (cn == exCls || exCls.find(cn) != std::string::npos || cn.find("Throwable") != std::string::npos || cn.find("Exception") != std::string::npos)) match = true;
                    if (!match && exObj) {
                        auto ec = findOrLoadClass(exCls, m_activeJar);
                        std::string sup = ec ? ec->superClassName : "";
                        for (int d = 0; d < 4 && !sup.empty(); d++) {
                            if (sup == cn) { match = true; break; }
                            auto sc = findOrLoadClass(sup, m_activeJar);
                            sup = sc ? sc->superClassName : "";
                        }
                    }
                }
                if (match) {
                    frame.stack.clear();
                    frame.push(JavaValue(exRef, true));
                    frame.pc = e.handlerPc;
                    handled = true;
                    break;
                }
            }
        }
        if (!handled) {
            setPendingException(exRef);
        }
        return handled;
    };

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
                else if (cp.tag == CONSTANT_Class) {
                    std::string cname = (cp.nameIndex < cls->constantPool.size()) ? cls->constantPool[cp.nameIndex].strVal : cp.strVal;
                    uint32_t cRef = allocObject("java/lang/Class");
                    JavaObject* cObj = getObject(cRef);
                    if (cObj) cObj->stringVal = cname;
                    frame.push(JavaValue(cRef, true));
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
            uint32_t aRef = frame.pop().asRef();
            JavaArray* arr = getArray(aRef);
            if (!arr && aRef == 0) {
                bool hasHandler = false;
                int throwPc = frame.pc - 1;
                for (auto &e : method.exTable) {
                    if (throwPc >= e.startPc && throwPc < e.endPc) { hasHandler = true; break; }
                }
                if (hasHandler) {
                    uint32_t ex = allocObject("java/lang/NullPointerException");
                    if (!throwEngineException(ex, throwPc)) return JavaValue(0);
                    break;
                }
            }
            frame.push(JavaValue(arr ? arr->length() : 0));
            break;
        }

        // Stack Ops
        case OP_POP: frame.pop(); break;
        case OP_POP2: {
            if (!frame.stack.empty()) {
                JavaValue v = frame.pop();
                if (v.type != JavaValue::LONG && v.type != JavaValue::DOUBLE) {
                    if (!frame.stack.empty()) frame.pop();
                }
            }
            break;
        }
        case OP_DUP: { JavaValue v = frame.peek(); frame.push(v); break; }
        case OP_DUP_X1: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); frame.push(v1); frame.push(v2); frame.push(v1); break; }
        case OP_DUP_X2: {
            if (frame.stack.size() >= 2) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                if (v2.type == JavaValue::LONG || v2.type == JavaValue::DOUBLE) {
                    // Form 2: v1 is Category 1, v2 is Category 2
                    frame.push(v1);
                    frame.push(v2);
                    frame.push(v1);
                } else if (!frame.stack.empty()) {
                    // Form 1: v1, v2, v3 are Category 1
                    JavaValue v3 = frame.pop();
                    frame.push(v1);
                    frame.push(v3);
                    frame.push(v2);
                    frame.push(v1);
                } else {
                    frame.push(v2);
                    frame.push(v1);
                }
            }
            break;
        }
        case OP_DUP2: {
            if (!frame.stack.empty()) {
                JavaValue v1 = frame.stack.back();
                if (v1.type == JavaValue::LONG || v1.type == JavaValue::DOUBLE) {
                    // Form 2: Category 2 type (duplicate single 64-bit value)
                    frame.push(v1);
                } else if (frame.stack.size() >= 2) {
                    // Form 1: Category 1 types (duplicate two 32-bit values)
                    JavaValue v2 = frame.stack[frame.stack.size() - 2];
                    frame.push(v2);
                    frame.push(v1);
                }
            }
            break;
        }
        case OP_SWAP: { JavaValue v1 = frame.pop(); JavaValue v2 = frame.pop(); frame.push(v1); frame.push(v2); break; }

        // Integer Math
        case OP_IADD: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a + b)); break; }
        case OP_ISUB: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a - b)); break; }
        case OP_IMUL: { int32_t b = frame.pop().asInt(), a = frame.pop().asInt(); frame.push(JavaValue(a * b)); break; }
        case OP_IDIV: {
            int32_t b = frame.pop().asInt(), a = frame.pop().asInt();
            if (b == 0) {
                uint32_t ex = allocObject("java/lang/ArithmeticException");
                if (!throwEngineException(ex, frame.pc - 1)) return JavaValue(0);
            } else if (a == INT32_MIN && b == -1) {
                frame.push(JavaValue((int32_t)INT32_MIN));
            } else {
                frame.push(JavaValue(a / b));
            }
            break;
        }
        case OP_IREM: {
            int32_t b = frame.pop().asInt(), a = frame.pop().asInt();
            if (b == 0) {
                uint32_t ex = allocObject("java/lang/ArithmeticException");
                if (!throwEngineException(ex, frame.pc - 1)) return JavaValue(0);
            } else if (a == INT32_MIN && b == -1) {
                frame.push(JavaValue((int32_t)0));
            } else {
                frame.push(JavaValue(a % b));
            }
            break;
        }
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
            if (idx >= frame.locals.size()) frame.locals.resize(idx + 1, JavaValue(0));
            frame.locals[idx] = JavaValue(frame.locals[idx].asInt() + val);
            break;
        }

        // Long Math
        case OP_LADD: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a + b)); break; }
        case OP_LSUB: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a - b)); break; }
        case OP_LMUL: { int64_t b = frame.pop().asLong(), a = frame.pop().asLong(); frame.push(JavaValue(a * b)); break; }
        case OP_LDIV: {
            int64_t b = frame.pop().asLong(), a = frame.pop().asLong();
            if (b == 0) {
                uint32_t ex = allocObject("java/lang/ArithmeticException");
                if (!throwEngineException(ex, frame.pc - 1)) return JavaValue(0);
            } else if (a == INT64_MIN && b == -1) {
                frame.push(JavaValue((int64_t)INT64_MIN));
            } else {
                frame.push(JavaValue(a / b));
            }
            break;
        }
        case OP_LREM: {
            int64_t b = frame.pop().asLong(), a = frame.pop().asLong();
            if (b == 0) {
                uint32_t ex = allocObject("java/lang/ArithmeticException");
                if (!throwEngineException(ex, frame.pc - 1)) return JavaValue(0);
            } else if (a == INT64_MIN && b == -1) {
                frame.push(JavaValue((int64_t)0));
            } else {
                frame.push(JavaValue(a % b));
            }
            break;
        }
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
        case OP_FCMPG: {
            float b = frame.pop().asFloat(), a = frame.pop().asFloat();
            if (std::isnan(a) || std::isnan(b)) {
                frame.push(JavaValue(op == OP_FCMPG ? 1 : -1));
            } else {
                frame.push(JavaValue(a > b ? 1 : (a < b ? -1 : 0)));
            }
            break;
        }
        case OP_DCMPL:
        case OP_DCMPG: {
            double b = frame.pop().asDouble(), a = frame.pop().asDouble();
            if (std::isnan(a) || std::isnan(b)) {
                frame.push(JavaValue(op == OP_DCMPG ? 1 : -1));
            } else {
                frame.push(JavaValue(a > b ? 1 : (a < b ? -1 : 0)));
            }
            break;
        }

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
            const std::string& fKey = getFieldKey(cls, fIdx);
            size_t colon = fKey.find(':');
            if (colon != std::string::npos) {
                std::string fClsName = fKey.substr(0, colon);
                auto fCls = findOrLoadClass(fClsName, m_activeJar);
                if (fCls) ensureClinit(fCls, display);
            }
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
            if (fKey.find("ByteOrder:LITTLE_ENDIAN") != std::string::npos && sv.asRef() == 0) {
                uint32_t r = allocObject("java/nio/ByteOrder");
                JavaObject* bo = getObject(r);
                if (bo) bo->fields["isLittle"] = JavaValue(1);
                sv = JavaValue(r, true);
                setStaticField(fKey, sv);
            } else if (fKey.find("ByteOrder:BIG_ENDIAN") != std::string::npos && sv.asRef() == 0) {
                uint32_t r = allocObject("java/nio/ByteOrder");
                JavaObject* bo = getObject(r);
                if (bo) bo->fields["isLittle"] = JavaValue(0);
                sv = JavaValue(r, true);
                setStaticField(fKey, sv);
            } else if (fKey.find("StandardCharsets:UTF_8") != std::string::npos && sv.asRef() == 0) {
                uint32_t r = allocObject("java/nio/charset/Charset");
                sv = JavaValue(r, true);
                setStaticField(fKey, sv);
            }
            frame.push(sv);
            break;
        }
        case OP_PUTSTATIC: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            const std::string& fKey = getFieldKey(cls, fIdx);
            size_t colon = fKey.find(':');
            if (colon != std::string::npos) {
                std::string fClsName = fKey.substr(0, colon);
                auto fCls = findOrLoadClass(fClsName, m_activeJar);
                if (fCls) ensureClinit(fCls, display);
            }
            setStaticField(fKey, frame.pop());
            break;
        }
        case OP_GETFIELD: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            ensureFieldCached(cls, fIdx);
            const auto& cp = cls->constantPool[fIdx];
            const std::string& fKey = cp.cachedFieldKey;
            const std::string& shortName = cp.cachedShortName;

            uint32_t objRef = frame.pop().asRef();
            JavaObject* obj = getObject(objRef);
            if (obj) {
                auto fit = obj->fields.find(fKey);
                if (fit == obj->fields.end() && !shortName.empty()) fit = obj->fields.find(shortName);
                frame.push(fit != obj->fields.end() ? fit->second : JavaValue(0));
            } else {
                frame.push(JavaValue(0));
            }
            break;
        }
        case OP_PUTFIELD: {
            uint16_t fIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            ensureFieldCached(cls, fIdx);
            const auto& cp = cls->constantPool[fIdx];
            const std::string& fKey = cp.cachedFieldKey;
            const std::string& shortName = cp.cachedShortName;

            JavaValue val = frame.pop();
            uint32_t objRef = frame.pop().asRef();
            JavaObject* obj = getObject(objRef);
            if (obj) {
                obj->fields[fKey] = val;
                if (!shortName.empty() && shortName != fKey) obj->fields[shortName] = val;
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
                if (val == match) {
                    frame.pc = switchStart + off;
                    matched = true;
                    break;
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
            auto targetCls = findOrLoadClass(cname, m_activeJar);
            if (targetCls) ensureClinit(targetCls, display);
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
            uint16_t cpIdx = (code[frame.pc] << 8) | code[frame.pc + 1];
            frame.pc += 2;
            uint32_t ref = frame.pop().asRef();
            if (ref == 0) {
                frame.push(JavaValue(0));
            } else {
                std::string targetClass = "";
                if (cpIdx < cls->constantPool.size() && cls->constantPool[cpIdx].nameIndex < cls->constantPool.size()) {
                    targetClass = cls->constantPool[cls->constantPool[cpIdx].nameIndex].strVal;
                }
                JavaObject* obj = getObject(ref);
                if (obj && !targetClass.empty()) {
                    frame.push(JavaValue(isInstanceOf(obj->className, targetClass) ? 1 : 0));
                } else {
                    JavaArray* arr = getArray(ref);
                    if (arr && !targetClass.empty()) {
                        bool match = false;
                        if (targetClass == "java/lang/Object" || targetClass == "java/lang/Cloneable" || targetClass == "java/io/Serializable") {
                            match = true;
                        } else if (!targetClass.empty() && targetClass[0] == '[') {
                            if ((arr->elemType == 8 || arr->elemType == 4) && (targetClass == "[B" || targetClass == "[Z")) match = true;
                            else if (arr->elemType == 5 && targetClass == "[C") match = true;
                            else if (arr->elemType == 9 && targetClass == "[S") match = true;
                            else if (arr->elemType == 10 && targetClass == "[I") match = true;
                            else if (arr->elemType == 11 && targetClass == "[J") match = true;
                            else if (arr->elemType == 6 && targetClass == "[F") match = true;
                            else if (arr->elemType == 7 && targetClass == "[D") match = true;
                            else if (arr->elemType == 0 && targetClass.rfind("[L", 0) == 0) match = true;
                        }
                        frame.push(JavaValue(match ? 1 : 0));
                    } else {
                        frame.push(JavaValue(0));
                    }
                }
            }
            break;
        }
        case OP_MONITORENTER:
        case OP_MONITOREXIT: {
            frame.pop();
            break;
        }
        case OP_ATHROW: {
            JavaValue ex = frame.pop();
            uint32_t exRef = ex.asRef();
            if (exRef == 0) {
                exRef = allocObject("java/lang/NullPointerException");
            }
            if (!throwEngineException(exRef, frame.pc - 1)) {
                return JavaValue(0);
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
        case OP_DUP2_X1: {
            if (frame.stack.size() >= 3) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                JavaValue v3 = frame.pop();
                frame.push(v2);
                frame.push(v1);
                frame.push(v3);
                frame.push(v2);
                frame.push(v1);
            } else if (frame.stack.size() == 2) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                frame.push(v1);
                frame.push(v2);
                frame.push(v1);
            }
            break;
        }
        case OP_DUP2_X2: {
            if (frame.stack.size() >= 4) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                JavaValue v3 = frame.pop();
                JavaValue v4 = frame.pop();
                frame.push(v2);
                frame.push(v1);
                frame.push(v4);
                frame.push(v3);
                frame.push(v2);
                frame.push(v1);
            } else if (frame.stack.size() == 3) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                JavaValue v3 = frame.pop();
                frame.push(v1);
                frame.push(v3);
                frame.push(v2);
                frame.push(v1);
            } else if (frame.stack.size() == 2) {
                JavaValue v1 = frame.pop();
                JavaValue v2 = frame.pop();
                frame.push(v1);
                frame.push(v2);
                frame.push(v1);
            }
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

            // If this is a virtual call on an object instance, resolve actual object's class.
            // NOTE: OP_INVOKESPECIAL (super.<init>, super.method, private method) MUST use targetClass, NOT actualClass!
            std::string actualClass = targetClass;
            if (op == OP_INVOKEVIRTUAL || op == OP_INVOKEINTERFACE) {
                if (!callArgs.empty() && callArgs[0].asRef() != 0) {
                    JavaObject* thisObj = getObject(callArgs[0].asRef());
                    if (thisObj && !thisObj->className.empty()) {
                        actualClass = thisObj->className;
                    }
                }
            }

            JavaValue retVal;
            if (dispatchNativeMethod(actualClass, targetMethod, targetDesc, callArgs, retVal, display) ||
                (actualClass != targetClass && dispatchNativeMethod(targetClass, targetMethod, targetDesc, callArgs, retVal, display))) {
                if (targetDesc.find(")V") == std::string::npos) frame.push(retVal);
            } else {
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

            // Check if callee threw an uncaught exception -> unwind here
            if (hasPendingException()) {
                uint32_t exRef = getPendingException();
                if (throwEngineException(exRef, frame.pc - 1)) {
                    clearPendingException();
                } else {
                    return JavaValue(0);
                }
                break;
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