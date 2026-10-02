#include <map>
#include <set>
#include "cldc_vm.h"
#include "lcdui_screens.h"
#include "../engine_instance.h"
#include "../lcdui/frame_buffer.h"
#include "../lcdui/lcdui_graphics.h"
#include "../lcdui/font.h"
#include "../storage/rms_storage.h"
#include "../audio/mmapi_audio.h"
#include "../oem/device_control.h"
#include "../system/system_properties.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>

namespace universal_loader::jvm {

CldcVirtualMachine::CldcVirtualMachine() {
    registerStandardNatives();
}

CldcVirtualMachine::~CldcVirtualMachine() {
    {
        std::lock_guard<std::mutex> lock(m_freeMutex);
        m_freeStop = true;
    }
    m_freeCv.notify_all();
    if (m_freeThread.joinable()) m_freeThread.join();
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

void CldcVirtualMachine::registerNativeStatic(const std::string& className, const std::string& fieldName, NativeMethodHandler handler) {
    m_nativeStatics[className + "." + fieldName] = std::move(handler);
}

bool CldcVirtualMachine::hasBytecodeMethod(JavaObject* obj, const std::string& methodName, const std::string& descriptor) {
    if (!obj || !obj->clazz) return false;
    for (JavaClass* c = obj->clazz; c; ) {
        if (auto* m = c->findMethod(methodName, descriptor)) {
            if (!m->isAbstract() && !m->isNative()) return true;
        }
        auto super = findClass(c->superClassName);
        c = (super && super.get() != c) ? super.get() : nullptr;
    }
    return false;
}

JavaObject* CldcVirtualMachine::classObjectFor(const std::string& className) {
    std::lock_guard<std::mutex> lock(m_classObjectsMutex);
    auto it = m_classObjects.find(className);
    if (it != m_classObjects.end()) return it->second;
    JavaObject* obj = allocateObject(nullptr);
    obj->nativeClassName = "java/lang/Class";
    auto payload = std::make_shared<JavaClassPayload>();
    payload->name = className;
    obj->payload = payload;
    m_classObjects[className] = obj;
    return obj;
}

JavaString* CldcVirtualMachine::intern(const std::string& value) {
    std::lock_guard<std::mutex> lock(m_internMutex);
    auto it = m_internPool.find(value);
    if (it != m_internPool.end()) return it->second;
    JavaString* s = allocateString(value);
    m_internPool.emplace(value, s);
    return s;
}

bool CldcVirtualMachine::hasNativeClass(const std::string& className) const {
    const std::string prefix = className + ".";
    for (const auto& kv : m_nativeMethods) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
}

void CldcVirtualMachine::registerNative(const std::string& className, const std::string& methodName, const std::string& descriptor, NativeMethodHandler handler) {
    m_nativeMethods[makeNativeKey(className, methodName, descriptor)] = handler;
    if (!descriptor.empty()) {
        m_nativeMethods[makeNativeKey(className, methodName, "")] = handler;
    }
}

static JavaValue defaultValueForDescriptor(const std::string& desc) {
    char c = desc.empty() ? 'I' : desc[0];
    switch (c) {
        case 'J': return JavaValue(int64_t(0));
        case 'D': return JavaValue(0.0);
        case 'F': return JavaValue(0.0f);
        case 'L':
        case '[': return JavaValue(static_cast<JavaObject*>(nullptr));
        default: return JavaValue(0);
    }
}

// J2ME_HEAPLOG: periodic census of live heap objects by type (diagnostics only)
static void heapCensus(const std::vector<std::unique_ptr<JavaObject>>& heap) {
    static const bool on = std::getenv("J2ME_HEAPLOG") != nullptr;
    if (!on || heap.size() % 500000 != 0) return;
    std::unordered_map<std::string, std::pair<size_t, size_t>> byType; // count, approx bytes
    for (const auto& o : heap) {
        std::string k;
        size_t bytes = sizeof(JavaObject) + o->fields.size() * sizeof(JavaValue);
        if (o->isString()) { k = "String"; bytes += static_cast<const JavaString*>(o.get())->value.size(); }
        else if (o->isArray()) {
            auto* a = static_cast<const JavaArray*>(o.get());
            k = std::string("[") + a->elementTypeCode;
            bytes += a->elements.size() * sizeof(JavaValue);
        } else k = o->clazz ? o->clazz->thisClassName : (o->nativeClassName.empty() ? "?" : o->nativeClassName);
        auto& e = byType[k];
        e.first++;
        e.second += bytes;
    }
    std::vector<std::pair<std::string, std::pair<size_t, size_t>>> v(byType.begin(), byType.end());
    std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second.second > b.second.second; });
    fprintf(stderr, "===== heap census: %zu objects =====\n", heap.size());
    for (size_t i = 0; i < v.size() && i < 20; ++i)
        fprintf(stderr, "  %-50s %9zu objs %8zu KB\n", v[i].first.c_str(), v[i].second.first, v[i].second.second / 1024);
}

JavaObject* CldcVirtualMachine::allocateObject(JavaClass* clazz) {
    if (clazz) {
        ensureInitialized(clazz);
        resolveLayout(clazz);
    }
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaObject>(clazz);
    if (clazz) {
        size_t total = clazz->instanceFieldBase + clazz->instanceFieldCount;
        obj->fields.resize(total);
        // Type-correct defaults (long/double/ref) for every class in the hierarchy
        for (JavaClass* c = clazz; c; ) {
            for (const auto& f : c->fields) {
                if (!f.isStatic() && c->instanceFieldBase + f.slotIndex < total) {
                    obj->fields[c->instanceFieldBase + f.slotIndex] = defaultValueForDescriptor(f.descriptor);
                }
            }
            auto it = m_classes.find(c->superClassName);
            c = (it != m_classes.end()) ? it->second.get() : nullptr;
        }
    }
    JavaObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

JavaArray* CldcVirtualMachine::allocateArray(char typeCode, int32_t length) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto arr = std::make_unique<JavaArray>(typeCode, length);
    JavaArray* ptr = arr.get();
    m_heap.push_back(std::move(arr));
    heapCensus(m_heap);
    return ptr;
}

JavaString* CldcVirtualMachine::allocateString(const std::string& str) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto s = std::make_unique<JavaString>(str);
    JavaString* ptr = s.get();
    m_heap.push_back(std::move(s));
    heapCensus(m_heap);
    return ptr;
}

JavaImageObject* CldcVirtualMachine::allocateImage(std::shared_ptr<j2me::LcduiImage> img) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaImageObject>(std::move(img));
    JavaImageObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

JavaGraphicsObject* CldcVirtualMachine::allocateGraphics(std::shared_ptr<j2me::LcduiGraphics> g) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaGraphicsObject>(std::move(g));
    JavaGraphicsObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

JavaGraphicsObject* CldcVirtualMachine::allocateGraphics(j2me::LcduiGraphics* g) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaGraphicsObject>(g);
    JavaGraphicsObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

JavaFontObject* CldcVirtualMachine::allocateFont(std::shared_ptr<j2me::LcduiFont> f) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaFontObject>(std::move(f));
    JavaFontObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

JavaInputStreamObject* CldcVirtualMachine::allocateInputStream(std::vector<uint8_t> data) {
    std::lock_guard<std::mutex> lock(m_heapMutex);
    auto obj = std::make_unique<JavaInputStreamObject>(std::move(data));
    JavaInputStreamObject* ptr = obj.get();
    m_heap.push_back(std::move(obj));
    heapCensus(m_heap);
    return ptr;
}

void CldcVirtualMachine::gc() {
    // Collection runs from interpreter safepoints (maybeGc); System.gc() is only a hint
}

void CldcVirtualMachine::pin(JavaObject* o) {
    if (!o) return;
    std::lock_guard<std::mutex> lock(m_pinMutex);
    ++m_pinned[o];
}

void CldcVirtualMachine::unpin(JavaObject* o) {
    if (!o) return;
    std::lock_guard<std::mutex> lock(m_pinMutex);
    auto it = m_pinned.find(o);
    if (it != m_pinned.end() && --it->second <= 0) m_pinned.erase(it);
}

JavaObject* CldcVirtualMachine::nativeSingleton(const std::string& key, const std::function<JavaObject*()>& create) {
    std::lock_guard<std::mutex> lock(m_singletonMutex);
    auto& slot = m_nativeSingletons[key];
    if (!slot) {
        slot = create();
        pin(slot);
    }
    return slot;
}

CldcVirtualMachine::GcThread& CldcVirtualMachine::gcThread() {
    thread_local uint64_t tlSerial = 0;
    thread_local GcThread* tlThread = nullptr;
    if (tlSerial == m_vmSerial && tlThread) return *tlThread;
    thread_local std::unordered_map<uint64_t, GcThread*> perVm;
    GcThread*& slot = perVm[m_vmSerial];
    if (!slot) {
        std::lock_guard<std::mutex> lock(m_gcThreadsMutex);
        m_gcThreads.push_back(std::make_unique<GcThread>());
        slot = m_gcThreads.back().get();
    }
    tlSerial = m_vmSerial;
    tlThread = slot;
    return *slot;
}

CldcVirtualMachine::ArgRoot::ArgRoot(CldcVirtualMachine* vm, const std::vector<JavaValue>* args, bool native)
    : m_t(vm->gcThread()), m_native(native) {
    m_t.args.push_back(args);
    if (m_native) ++m_t.nativeDepth;
}

CldcVirtualMachine::ArgRoot::~ArgRoot() {
    m_t.args.pop_back();
    if (m_native) --m_t.nativeDepth;
}

// Runs a collection when this thread is in pure bytecode and every other Java thread
// is stopped at a point where all its references are visible (frames / rooted args).
void CldcVirtualMachine::maybeGc() {
    GcThread& me = gcThread();
    if (me.nativeDepth != 0) return;
    {
        std::lock_guard<std::mutex> lock(m_gcThreadsMutex);
        for (auto& t : m_gcThreads) {
            if (t.get() == &me) continue;
            if (!t->parked && !(t->frames.empty() && t->nativeDepth == 0)) {
                static const bool gcLog = std::getenv("J2ME_GCLOG") != nullptr;
                static int skips = 0;
                if (gcLog && (++skips % 2000) == 1) {
                    StackFrame* top = t->frames.empty() ? nullptr : t->frames.back();
                    std::fprintf(stderr, "[GC] blocked by thread: frames=%zu depth=%d top=%s.%s\n", t->frames.size(), t->nativeDepth,
                                 top && top->method && top->method->clazz ? top->method->clazz->thisClassName.c_str() : "?",
                                 top && top->method ? top->method->name.c_str() : "?");
                }
                return;
            }
        }
    }
    collectGarbage();
}

void CldcVirtualMachine::queueFree(std::vector<std::unique_ptr<JavaObject>>&& dead) {
    {
        std::lock_guard<std::mutex> lock(m_freeMutex);
        if (m_freeQueue.empty()) m_freeQueue = std::move(dead);
        else for (auto& o : dead) m_freeQueue.push_back(std::move(o));
        if (!m_freeThread.joinable()) {
            m_freeThread = std::thread([this] {
                std::unique_lock<std::mutex> lock(m_freeMutex);
                for (;;) {
                    m_freeCv.wait(lock, [this] { return m_freeStop || !m_freeQueue.empty(); });
                    if (m_freeQueue.empty()) return;
                    auto batch = std::move(m_freeQueue);
                    m_freeQueue.clear();
                    lock.unlock();
                    batch.clear();
                    lock.lock();
                }
            });
        }
    }
    m_freeCv.notify_one();
}

void CldcVirtualMachine::collectGarbage() {
    static const bool gcLog = std::getenv("J2ME_GCLOG") != nullptr;
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<JavaObject*> work;
    work.reserve(1 << 16);
    auto val = [&](const JavaValue& v) { gcTraceValue(v, work); };

    {
        std::lock_guard<std::mutex> lock(m_gcThreadsMutex);
        for (auto& t : m_gcThreads) {
            for (StackFrame* f : t->frames) {
                for (const auto& v : f->locals) val(v);
                for (size_t i = 0; i < f->sp && i < f->operandStack.size(); ++i) val(f->operandStack[i]);
            }
            for (const auto* a : t->args) if (a) for (const auto& v : *a) val(v);
        }
    }
    for (auto& kv : m_classes) {
        if (!kv.second) continue;
        for (const auto& v : kv.second->staticFieldValues) val(v);
        for (const auto& m : kv.second->methods) if (m.trivialKind == 2) val(m.trivialConst);
    }
    {
        std::lock_guard<std::mutex> lock(m_classObjectsMutex);
        for (auto& kv : m_classObjects) if (kv.second) work.push_back(kv.second);
    }
    {
        std::lock_guard<std::mutex> lock(m_internMutex);
        for (auto& kv : m_internPool) if (kv.second) work.push_back(kv.second);
    }
    {
        std::lock_guard<std::mutex> lock(m_pinMutex);
        for (auto& kv : m_pinned) work.push_back(kv.first);
    }
    if (auto* inst = static_cast<J2meEngineInstance*>(m_userContext)) {
        if (inst->currentJavaCanvas) work.push_back(inst->currentJavaCanvas);
        if (inst->currentMidletObject) work.push_back(inst->currentMidletObject);
        if (inst->currentNativeScreen) work.push_back(inst->currentNativeScreen);
    }

    std::lock_guard<std::mutex> heapLock(m_heapMutex);
    while (!work.empty()) {
        JavaObject* o = work.back();
        work.pop_back();
        if (o->gcMark) continue;
        o->gcMark = true;
        for (const auto& v : o->fields) val(v);
        if (o->isArray()) {
            auto* a = static_cast<JavaArray*>(o);
            if (!std::strchr("ZBCSIJFD", a->elementTypeCode)) {
                for (const auto& v : a->elements) val(v);
            }
        }
        if (o->payload) o->payload->trace(work);
    }

    const auto tMark = std::chrono::steady_clock::now();
    const size_t before = m_heap.size();
    size_t live = 0;
    std::vector<std::unique_ptr<JavaObject>> dead;
    dead.reserve(before);
    for (size_t i = 0; i < before; ++i) {
        if (m_heap[i]->gcMark) {
            m_heap[i]->gcMark = false;
            if (live != i) m_heap[live] = std::move(m_heap[i]);
            ++live;
        } else {
            dead.push_back(std::move(m_heap[i]));
        }
    }
    m_heap.resize(live);
    queueFree(std::move(dead));
    m_gcThreshold = std::max<size_t>(400000, live * 2);
    if (gcLog) {
        using ms = std::chrono::duration<double, std::milli>;
        const auto t1 = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[GC] %zu -> %zu objects, pause %.1f ms (mark %.1f), next at %zu\n", before, live,
                     ms(t1 - t0).count(), ms(tMark - t0).count(), m_gcThreshold);
    }
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

// Returns the first descriptor char of each parameter ('L' for objects, '[' for arrays)
static std::vector<char> parseParamTypes(const std::string& desc) {
    std::vector<char> types;
    size_t i = 0;
    if (i < desc.size() && desc[i] == '(') ++i;
    while (i < desc.size() && desc[i] != ')') {
        char c = desc[i];
        types.push_back(c);
        while (i < desc.size() && desc[i] == '[') ++i;
        if (i < desc.size() && desc[i] == 'L') {
            while (i < desc.size() && desc[i] != ';') ++i;
        }
        if (i < desc.size()) ++i;
    }
    return types;
}

// Display.setCurrent: Form / TextBox / List / Alert become a host dialog over the
// current canvas; a Canvas replaces the canvas and closes any dialog.
void lcduiSetCurrent(CldcVirtualMachine* vm, JavaObject* target) {
    auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
    if (!inst || !target) return;
    if (isLcduiScreen(target)) {
        if (target == inst->currentNativeScreen) return;
        inst->currentNativeScreen = target;
        inst->nativeScreenSerial.fetch_add(1);
        return;
    }
    if (inst->currentNativeScreen) {
        inst->currentNativeScreen = nullptr;
        inst->nativeScreenSerial.fetch_add(1);
    }
    const bool changed = inst->currentJavaCanvas != target;
    inst->currentJavaCanvas = target;
    ++g_lcduiCommandsVersion;
    if (!target->clazz) return;
    const std::string& cls = target->clazz->thisClassName;
    try {
        vm->executeMethodByName(cls, "sizeChanged", "(II)V", {JavaValue(target), JavaValue(inst->frameBuffer.getWidth()), JavaValue(inst->frameBuffer.getHeight())});
    } catch (const VmTerminated&) { throw; } catch (...) {}
    if (changed) {
        try {
            vm->executeMethodByName(cls, "showNotify", "()V", {JavaValue(target)});
        } catch (const VmTerminated&) { throw; } catch (...) {}
    }
    try {
        vm->executeMethodByName(cls, "repaint", "()V", {JavaValue(target)});
    } catch (const VmTerminated&) { throw; } catch (...) {}
}

// Alert shown via setCurrent: remember where to return after it is dismissed
static void showAlert(CldcVirtualMachine* vm, JavaObject* alert, JavaObject* next) {
    auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
    auto* s = alert ? dynamic_cast<LcduiScreenPayload*>(alert->payload.get()) : nullptr;
    if (!inst || !s) return;
    if (!next) next = inst->currentNativeScreen && inst->currentNativeScreen != alert ? inst->currentNativeScreen : inst->currentJavaCanvas;
    s->alertNext = next;
    lcduiSetCurrent(vm, alert);
}

static bool isAlert(JavaObject* o) {
    auto* s = o ? dynamic_cast<LcduiScreenPayload*>(o->payload.get()) : nullptr;
    return s && s->kind == LcduiScreenKind::Alert;
}

static std::string getNativeSuperClass(const std::string& className) {
    if (className == "com/nokia/mid/ui/FullCanvas") return "javax/microedition/lcdui/Canvas";
    if (className == "javax/microedition/lcdui/game/GameCanvas") return "javax/microedition/lcdui/Canvas";
    if (className == "javax/microedition/lcdui/Canvas") return "javax/microedition/lcdui/Displayable";
    if (className == "javax/microedition/lcdui/Form" ||
        className == "javax/microedition/lcdui/List" ||
        className == "javax/microedition/lcdui/TextBox" ||
        className == "javax/microedition/lcdui/Alert") return "javax/microedition/lcdui/Screen";
    if (className == "javax/microedition/lcdui/Screen") return "javax/microedition/lcdui/Displayable";
    if (className == "javax/microedition/lcdui/StringItem" ||
        className == "javax/microedition/lcdui/TextField" ||
        className == "javax/microedition/lcdui/ChoiceGroup" ||
        className == "javax/microedition/lcdui/Gauge" ||
        className == "javax/microedition/lcdui/DateField" ||
        className == "javax/microedition/lcdui/Spacer" ||
        className == "javax/microedition/lcdui/ImageItem" ||
        className == "javax/microedition/lcdui/CustomItem") return "javax/microedition/lcdui/Item";
    if (className == "javax/microedition/lcdui/game/Sprite" ||
        className == "javax/microedition/lcdui/game/TiledLayer") return "javax/microedition/lcdui/game/Layer";
    if (className == "java/io/DataInputStream" ||
        className == "java/io/ByteArrayInputStream" ||
        className == "java/net/SocketInputStream") return "java/io/InputStream";
    if (className == "java/io/DataOutputStream" ||
        className == "java/io/ByteArrayOutputStream" ||
        className == "java/io/PrintStream" ||
        className == "java/net/SocketOutputStream") return "java/io/OutputStream";
    if (className == "java/net/InetSocketAddress") return "java/net/SocketAddress";
    if (className == "java/net/HttpURLConnection") return "java/net/URLConnection";
    if (className == "java/util/Stack" ||
        className == "java/util/ArrayList" ||
        className == "java/util/LinkedList") return "java/util/Vector";
    if (className == "java/util/HashMap" ||
        className == "java/util/LinkedHashMap") return "java/util/Hashtable";
    if (className == "javax/microedition/media/control/VolumeControl" ||
        className == "javax/microedition/media/control/ToneControl") return "javax/microedition/media/Control";
    if (className == "javax/microedition/media/Player") return "javax/microedition/media/Controllable";
    // Generic Connection Framework interfaces returned by Connector.open
    if (className == "javax/microedition/io/HttpsConnection") return "javax/microedition/io/HttpConnection";
    if (className == "javax/microedition/io/HttpConnection") return "javax/microedition/io/ContentConnection";
    if (className == "javax/microedition/io/ContentConnection" ||
        className == "javax/microedition/io/SocketConnection") return "javax/microedition/io/StreamConnection";
    if (className == "javax/microedition/io/StreamConnection") return "javax/microedition/io/Connection";

    // CLDC 1.1 / MIDP 2.0 Throwable hierarchy
    static const std::unordered_map<std::string, std::string> throwableParents = {
        {"java/lang/Throwable", "java/lang/Object"},
        {"java/lang/Exception", "java/lang/Throwable"},
        {"java/lang/Error", "java/lang/Throwable"},
        {"java/lang/VirtualMachineError", "java/lang/Error"},
        {"java/lang/OutOfMemoryError", "java/lang/VirtualMachineError"},
        {"java/lang/NoClassDefFoundError", "java/lang/Error"},
        {"java/lang/RuntimeException", "java/lang/Exception"},
        {"java/lang/ClassNotFoundException", "java/lang/Exception"},
        {"java/lang/IllegalAccessException", "java/lang/Exception"},
        {"java/lang/InstantiationException", "java/lang/Exception"},
        {"java/lang/InterruptedException", "java/lang/Exception"},
        {"java/lang/ArithmeticException", "java/lang/RuntimeException"},
        {"java/lang/ArrayStoreException", "java/lang/RuntimeException"},
        {"java/lang/ClassCastException", "java/lang/RuntimeException"},
        {"java/lang/IllegalArgumentException", "java/lang/RuntimeException"},
        {"java/lang/IllegalMonitorStateException", "java/lang/RuntimeException"},
        {"java/lang/IllegalStateException", "java/lang/RuntimeException"},
        {"java/lang/IndexOutOfBoundsException", "java/lang/RuntimeException"},
        {"java/lang/NegativeArraySizeException", "java/lang/RuntimeException"},
        {"java/lang/NullPointerException", "java/lang/RuntimeException"},
        {"java/lang/SecurityException", "java/lang/RuntimeException"},
        {"java/lang/NumberFormatException", "java/lang/IllegalArgumentException"},
        {"java/lang/ArrayIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException"},
        {"java/lang/StringIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException"},
        {"java/util/EmptyStackException", "java/lang/RuntimeException"},
        {"java/util/NoSuchElementException", "java/lang/RuntimeException"},
        {"java/io/IOException", "java/lang/Exception"},
        {"java/net/SocketException", "java/io/IOException"},
        {"java/net/ConnectException", "java/net/SocketException"},
        {"java/net/SocketTimeoutException", "java/io/InterruptedIOException"},
        {"java/net/UnknownHostException", "java/io/IOException"},
        {"java/net/MalformedURLException", "java/io/IOException"},
        {"java/net/ProtocolException", "java/io/IOException"},
        {"java/io/FileNotFoundException", "java/io/IOException"},
        {"java/io/EOFException", "java/io/IOException"},
        {"java/io/InterruptedIOException", "java/io/IOException"},
        {"java/io/UnsupportedEncodingException", "java/io/IOException"},
        {"java/io/UTFDataFormatException", "java/io/IOException"},
        {"javax/microedition/io/ConnectionNotFoundException", "java/io/IOException"},
        {"javax/microedition/rms/RecordStoreException", "java/lang/Exception"},
        {"javax/microedition/rms/InvalidRecordIDException", "javax/microedition/rms/RecordStoreException"},
        {"javax/microedition/rms/RecordStoreFullException", "javax/microedition/rms/RecordStoreException"},
        {"javax/microedition/rms/RecordStoreNotFoundException", "javax/microedition/rms/RecordStoreException"},
        {"javax/microedition/rms/RecordStoreNotOpenException", "javax/microedition/rms/RecordStoreException"},
        {"javax/microedition/media/MediaException", "java/lang/Exception"},
        {"javax/microedition/midlet/MIDletStateChangeException", "java/lang/Exception"},
    };
    auto it = throwableParents.find(className);
    if (it != throwableParents.end()) return it->second;

    if (className != "java/lang/Object" && !className.empty()) return "java/lang/Object";
    return "";
}

void CldcVirtualMachine::resolveLayout(JavaClass* clazz) {
    if (!clazz || clazz->layoutResolved) return;
    size_t base = 0;
    auto super = findClass(clazz->superClassName);
    if (super && super.get() != clazz) {
        resolveLayout(super.get());
        base = super->instanceFieldBase + super->instanceFieldCount;
    }
    clazz->instanceFieldBase = base;
    clazz->layoutResolved = true;
}

void CldcVirtualMachine::ensureInitialized(JavaClass* clazz) {
    if (!clazz || clazz->staticInitDone) return;
    // Mark first: <clinit> may recursively touch its own class
    clazz->staticInitDone = true;
    resolveLayout(clazz);
    for (auto& f : clazz->fields) {
        if (f.isStatic() && f.slotIndex < clazz->staticFieldValues.size()) {
            clazz->staticFieldValues[f.slotIndex] = defaultValueForDescriptor(f.descriptor);
        }
    }
    auto super = findClass(clazz->superClassName);
    if (super && super.get() != clazz) ensureInitialized(super.get());

    if (auto* clinit = clazz->findMethod("<clinit>", "()V")) {
        try {
            executeMethod(clinit, {});
        } catch (const std::exception& e) {
            std::cerr << "[J2ME VM] Exception in " << clazz->thisClassName << ".<clinit>: " << e.what() << std::endl;
        }
    }
}

long CldcVirtualMachine::findInstanceSlot(JavaClass* start, const std::string& name, const std::string& desc) {
    for (JavaClass* c = start; c; ) {
        resolveLayout(c);
        if (auto* f = c->findField(name, desc)) {
            if (!f->isStatic()) return static_cast<long>(c->instanceFieldBase + f->slotIndex);
        }
        auto super = findClass(c->superClassName);
        c = (super && super.get() != c) ? super.get() : nullptr;
    }
    return -1;
}

JavaClass* CldcVirtualMachine::findStaticFieldOwner(JavaClass* start, const std::string& name, const std::string& desc, JavaField** outField) {
    for (JavaClass* c = start; c; ) {
        if (auto* f = c->findField(name, desc)) {
            if (f->isStatic()) {
                *outField = f;
                return c;
            }
        }
        // Static fields may also be inherited from interfaces
        for (const auto& ifName : c->interfaceNames) {
            auto ifc = findClass(ifName);
            if (ifc && ifc.get() != c) {
                if (JavaClass* owner = findStaticFieldOwner(ifc.get(), name, desc, outField)) return owner;
            }
        }
        auto super = findClass(c->superClassName);
        c = (super && super.get() != c) ? super.get() : nullptr;
    }
    return nullptr;
}

std::string CldcVirtualMachine::classNameOf(JavaObject* obj) const {
    if (!obj) return "";
    if (obj->clazz) return obj->clazz->thisClassName;
    if (!obj->nativeClassName.empty()) return obj->nativeClassName;
    if (obj->isString()) return "java/lang/String";
    if (obj->isArray()) return "[" + std::string(1, static_cast<JavaArray*>(obj)->elementTypeCode);
    if (dynamic_cast<JavaImageObject*>(obj)) return "javax/microedition/lcdui/Image";
    if (dynamic_cast<JavaGraphicsObject*>(obj)) return "javax/microedition/lcdui/Graphics";
    if (dynamic_cast<JavaFontObject*>(obj)) return "javax/microedition/lcdui/Font";
    if (dynamic_cast<JavaInputStreamObject*>(obj)) return "java/io/InputStream";
    return "java/lang/Object";
}

bool CldcVirtualMachine::isSubclassOf(const std::string& className, const std::string& targetClass) {
    if (className.empty()) return false;
    if (className == targetClass || targetClass == "java/lang/Object") return true;
    auto clazz = findClass(className);
    if (clazz) {
        for (const auto& ifName : clazz->interfaceNames) {
            if (ifName != className && isSubclassOf(ifName, targetClass)) return true;
        }
        if (!clazz->superClassName.empty() && clazz->superClassName != className) {
            return isSubclassOf(clazz->superClassName, targetClass);
        }
        return false;
    }
    // StreamConnection has two super-interfaces; the native chain only models one
    if (className == "javax/microedition/io/StreamConnection" &&
        (targetClass == "javax/microedition/io/InputConnection" || targetClass == "javax/microedition/io/OutputConnection")) return true;
    std::string parent = getNativeSuperClass(className);
    return !parent.empty() && parent != className && isSubclassOf(parent, targetClass);
}

bool CldcVirtualMachine::isInstanceOf(JavaObject* obj, const std::string& targetClass) {
    if (!obj) return false;
    if (targetClass == "java/lang/Object") return true;
    if (obj->isArray()) {
        return !targetClass.empty() && targetClass[0] == '[';
    }
    std::string name = classNameOf(obj);
    if (isSubclassOf(name, targetClass)) return true;
    // Unloaded system classes without a modeled hierarchy: be permissive for
    // system targets so checkcast/instanceof on platform types do not fail spuriously.
    if (!obj->clazz && obj->nativeClassName.empty() && !findClass(targetClass)) return true;
    return false;
}

void CldcVirtualMachine::throwJava(const std::string& className, const std::string& message) {
    auto cls = findClass(className);
    JavaObject* ex = allocateObject(cls.get());
    if (!cls) ex->nativeClassName = className;
    ex->throwableMessage = message;
    std::string what = className;
    for (char& c : what) if (c == '/') c = '.';
    if (!message.empty()) what += ": " + message;
    throw JavaException(ex, what);
}

// ---- Global interpreter lock & monitors ----

void CldcVirtualMachine::gilAcquire() {
    const auto me = std::this_thread::get_id();
    if (m_gilOwner.load() == me) {
        ++m_gilDepth;
        return;
    }
    ++m_gilWaiters;
    m_gil.lock();
    --m_gilWaiters;
    ++m_gilHandoffs;
    m_gilOwner.store(me);
    m_gilDepth = 1;
}

void CldcVirtualMachine::gilRelease() {
    if (--m_gilDepth == 0) {
        m_gilOwner.store(std::thread::id{});
        m_gil.unlock();
    }
}

CldcVirtualMachine::GilScope::GilScope(CldcVirtualMachine* vm) : m_vm(vm) { m_vm->gilAcquire(); }
CldcVirtualMachine::GilScope::~GilScope() { m_vm->gilRelease(); }

CldcVirtualMachine::BlockingRegion::BlockingRegion(CldcVirtualMachine* vm) : m_vm(vm) {
    if (m_vm->m_gilOwner.load() == std::this_thread::get_id()) {
        GcThread& t = m_vm->gcThread();
        if (!t.parked && t.nativeDepth <= 1) {
            t.parked = true;
            m_parked = true;
        }
        m_savedDepth = m_vm->m_gilDepth;
        m_vm->m_gilDepth = 0;
        m_vm->m_gilOwner.store(std::thread::id{});
        m_vm->m_gil.unlock();
    }
}

CldcVirtualMachine::BlockingRegion::~BlockingRegion() {
    if (m_savedDepth > 0) {
        m_vm->gilAcquire();
        m_vm->m_gilDepth = m_savedDepth;
        if (m_parked) m_vm->gcThread().parked = false;
    }
}

void CldcVirtualMachine::safepoint() {
    checkTerminate();
    auto* inst = static_cast<J2meEngineInstance*>(getUserContext());
    if (inst && inst->isPaused.load() && std::this_thread::get_id() != inst->hostThreadId) {
        BlockingRegion region(this);
        while (inst->isPaused.load() && !terminating()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    checkTerminate();
    if (m_gilWaiters.load() == 0 || m_gilOwner.load() != std::this_thread::get_id()) return;
    // std::mutex is not fair: wait until a waiter actually took the lock (or a short timeout)
    uint64_t handoffs = m_gilHandoffs.load();
    BlockingRegion region(this);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
    while (m_gilHandoffs.load() == handoffs && std::chrono::steady_clock::now() < deadline) {
        if (terminating()) break;
        std::this_thread::yield();
    }
    checkTerminate();
}

void CldcVirtualMachine::monitorEnter(JavaObject* obj) {
    if (!obj) throwJava("java/lang/NullPointerException");
    checkTerminate();
    const auto me = std::this_thread::get_id();
    while (obj->monitorCount > 0 && obj->monitorOwner != me) {
        if (terminating()) throw VmTerminated{};
        BlockingRegion region(this);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    checkTerminate();
    obj->monitorOwner = me;
    ++obj->monitorCount;
}

void CldcVirtualMachine::monitorExit(JavaObject* obj) {
    if (!obj) throwJava("java/lang/NullPointerException");
    if (obj->monitorCount > 0 && obj->monitorOwner == std::this_thread::get_id()) {
        if (--obj->monitorCount == 0) obj->monitorOwner = std::thread::id{};
    }
}

void CldcVirtualMachine::monitorWait(JavaObject* obj, int64_t timeoutMs) {
    if (!obj) throwJava("java/lang/NullPointerException");
    checkTerminate();
    const auto me = std::this_thread::get_id();
    const bool owned = obj->monitorCount > 0 && obj->monitorOwner == me;
    const int32_t savedCount = owned ? obj->monitorCount : 0;
    if (owned) {
        obj->monitorCount = 0;
        obj->monitorOwner = std::thread::id{};
    }
    const uint32_t gen = obj->notifyGen;
    const auto start = std::chrono::steady_clock::now();
    auto* inst = static_cast<J2meEngineInstance*>(m_userContext);
    while (obj->notifyGen == gen) {
        if (terminating()) break;
        if (timeoutMs > 0 && std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(timeoutMs)) break;
        if (inst && !inst->isRunning.load()) break;
        BlockingRegion region(this);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (owned) {
        while (obj->monitorCount > 0 && obj->monitorOwner != me) {
            if (terminating()) throw VmTerminated{};
            BlockingRegion region(this);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        obj->monitorOwner = me;
        obj->monitorCount = savedCount;
    }
    checkTerminate();
}

void CldcVirtualMachine::monitorNotify(JavaObject* obj) {
    // Waking every waiter is valid for notify() too (spurious wakeups are allowed)
    if (obj) ++obj->notifyGen;
}

// Same lookup as invokeMethod, without executing. Returns false when nothing matches.
bool CldcVirtualMachine::resolveInvoke(const std::string& className, const std::string& methodName, const std::string& descriptor,
                                       JavaObject* receiver, bool virtualDispatch, JavaMethod*& method,
                                       const NativeMethodHandler*& native, JavaClass*& initClass) {
    method = nullptr;
    native = nullptr;
    initClass = nullptr;
    if (!virtualDispatch) receiver = nullptr;
    std::shared_ptr<JavaClass> searchClazz = nullptr;
    if (receiver && receiver->clazz) searchClazz = findClass(receiver->clazz->thisClassName);
    if (!searchClazz && !className.empty()) searchClazz = findClass(className);
    if (searchClazz && !virtualDispatch) initClass = searchClazz.get();

    for (auto current = searchClazz; current;) {
        auto m = current->findMethod(methodName, descriptor);
        if (m && !m->isAbstract()) {
            method = m;
            return true;
        }
        if (current->superClassName.empty() || current->superClassName == "java/lang/Object") break;
        auto next = findClass(current->superClassName);
        if (next == current) break;
        current = next;
    }

    std::vector<std::string> names;
    std::string curName = searchClazz ? searchClazz->thisClassName : (receiver ? classNameOf(receiver) : className);
    while (!curName.empty()) {
        if (std::find(names.begin(), names.end(), curName) != names.end()) break;
        names.push_back(curName);
        auto sc = findClass(curName);
        curName = (sc && !sc->superClassName.empty()) ? sc->superClassName : getNativeSuperClass(curName);
    }
    if (!className.empty() && std::find(names.begin(), names.end(), className) == names.end()) names.push_back(className);
    for (const auto& n : names) {
        auto it = m_nativeMethods.find(makeNativeKey(n, methodName, descriptor));
        if (it != m_nativeMethods.end()) { native = &it->second; return true; }
    }
    for (const auto& n : names) {
        auto it = m_nativeMethods.find(makeNativeKey(n, methodName, ""));
        if (it != m_nativeMethods.end()) { native = &it->second; return true; }
    }
    return false;
}

// Recognises the obfuscator's tiny static helpers so call sites can skip frame setup:
//   pure constant:  ldc int; ldc "???"; invokevirtual hashCode; ixor; ireturn
//   guarded getter: invokestatic <pure>; if<cond> L; getstatic F; store_0; load_0; return; L: const; store_0; goto
void CldcVirtualMachine::classifyTrivial(JavaMethod* m) {
    m->trivialKind = 1;
    JavaClass* k = m->clazz;
    if (!k || !m->isStatic() || m->isNative() || (m->accessFlags & ACC_SYNCHRONIZED) || m->descriptor.rfind("()", 0) != 0) return;
    const std::vector<uint8_t>& c = m->code;
    if (c.empty() || !m->exceptionTable.empty()) return;
    auto u2 = [&](size_t i) { return static_cast<uint16_t>((c[i] << 8) | c[i + 1]); };
    auto refOf = [&](uint16_t idx, std::string& cn, std::string& n, std::string& d) {
        if (idx >= k->constantPool.size()) return false;
        const auto& cp = k->constantPool[idx];
        cn = k->getClassNameFromCp(cp.classIndex);
        if (cp.nameAndTypeIndex >= k->constantPool.size()) return false;
        const auto& nat = k->constantPool[cp.nameAndTypeIndex];
        n = k->getUtf8FromCp(nat.nameIndex);
        d = k->getUtf8FromCp(nat.descriptorIndex);
        return true;
    };

    bool pure = m->descriptor == "()I";
    for (size_t i = 0; pure && i < c.size();) {
        uint8_t op = c[i];
        if (op >= 0x02 && op <= 0x08) i += 1;
        else if (op == 0x10 || op == 0x12) i += 2;
        else if (op == 0x11 || op == 0x13) i += 3;
        else if (op == 0x60 || op == 0x64 || op == 0x68 || op == 0x7E || op == 0x80 || op == 0x82) i += 1;
        else if (op == 0xAC) { pure = i + 1 == c.size(); i += 1; }
        else if (op == 0xB6 && i + 2 < c.size()) {
            std::string cn, n, d;
            pure = refOf(u2(i + 1), cn, n, d) && n == "hashCode" && d == "()I";
            i += 3;
        } else pure = false;
    }
    if (pure) {
        m->trivialConst = executeMethod(m, {});
        m->trivialKind = 2;
        return;
    }

    if (c.size() != 17 || c[0] != 0xB8 || c[3] < 0x99 || c[3] > 0x9E || c[6] != 0xB2 || c[14] != 0xA7) return;
    if (3 + static_cast<int16_t>(u2(4)) != 12) return;
    const bool isInt = c[9] == 0x3B && c[10] == 0x1A && c[11] == 0xAC && c[13] == 0x3B && c[12] >= 0x02 && c[12] <= 0x08;
    const bool isRef = c[9] == 0x4B && c[10] == 0x2A && c[11] == 0xB0 && c[13] == 0x4B && c[12] == 0x01;
    if (!isInt && !isRef) return;
    if (10 != 14 + static_cast<int16_t>(u2(15))) return;
    std::string cn, n, d;
    if (!refOf(u2(1), cn, n, d) || d != "()I") return;
    auto pc = findClass(cn);
    JavaMethod* pred = pc ? pc->findMethod(n, d) : nullptr;
    if (!pred) return;
    if (pred->trivialKind == 0) classifyTrivial(pred);
    if (pred->trivialKind != 2) return;
    const int32_t v = pred->trivialConst.i;
    bool taken = false;
    switch (c[3]) {
        case 0x99: taken = v == 0; break;
        case 0x9A: taken = v != 0; break;
        case 0x9B: taken = v < 0; break;
        case 0x9C: taken = v >= 0; break;
        case 0x9D: taken = v > 0; break;
        case 0x9E: taken = v <= 0; break;
    }
    if (taken) {
        m->trivialConst = isRef ? JavaValue(static_cast<JavaObject*>(nullptr)) : JavaValue(static_cast<int32_t>(c[12]) - 3);
        m->trivialKind = 2;
        return;
    }
    if (!refOf(u2(7), cn, n, d)) return;
    auto target = findClass(cn);
    JavaField* field = nullptr;
    JavaClass* owner = target ? findStaticFieldOwner(target.get(), n, d, &field) : nullptr;
    if (!owner || !field) return;
    ensureInitialized(owner);
    if (field->slotIndex >= owner->staticFieldValues.size()) return;
    m->trivialOwner = owner;
    m->trivialSlot = field->slotIndex;
    m->trivialKind = 3;
}

const NativeMethodHandler* CldcVirtualMachine::nativeFor(JavaMethod* method) {
    if (!method->nativeResolved) {
        std::string cName = method->clazz ? method->clazz->thisClassName : "";
        auto it = m_nativeMethods.find(makeNativeKey(cName, method->name, method->descriptor));
        if (it == m_nativeMethods.end()) it = m_nativeMethods.find(makeNativeKey(cName, method->name, ""));
        method->nativeHandler = it != m_nativeMethods.end() ? &it->second : nullptr;
        method->nativeResolved = true;
    }
    return static_cast<const NativeMethodHandler*>(method->nativeHandler);
}

JavaValue CldcVirtualMachine::executeMethodByName(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args) {
    return invokeMethod(className, methodName, descriptor, args, true);
}

JavaValue CldcVirtualMachine::invokeMethod(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args, bool virtualDispatch) {
    checkTerminate();
    GilScope gil(this);
    JavaObject* receiver = nullptr;
    if (virtualDispatch && !args.empty() && args[0].type == JavaValueType::REF) {
        receiver = args[0].ref;
    }

    // 1. Select the class where method lookup starts:
    //    virtual calls use the receiver's runtime class, invokespecial/invokestatic the symbolic class.
    std::shared_ptr<JavaClass> searchClazz = nullptr;
    if (receiver && receiver->clazz) {
        searchClazz = findClass(receiver->clazz->thisClassName);
    }
    if (!searchClazz && !className.empty()) {
        searchClazz = findClass(className);
    }
    if (searchClazz && !virtualDispatch) {
        ensureInitialized(searchClazz.get());
    }

    // 2. Search bytecode method in class hierarchy
    auto current = searchClazz;
    while (current) {
        auto method = current->findMethod(methodName, descriptor);
        if (method && !method->isAbstract()) {
            return executeMethod(method, args);
        }
        if (current->superClassName.empty() || current->superClassName == "java/lang/Object") {
            break;
        }
        auto next = findClass(current->superClassName);
        if (next == current) break;
        current = next;
    }

    // 3. Search native handler in complete class hierarchy (traversing both bytecode & native superclasses)
    std::vector<std::string> nativeSearchNames;
    std::string curName = searchClazz ? searchClazz->thisClassName
                        : (receiver ? classNameOf(receiver) : className);
    while (!curName.empty()) {
        if (std::find(nativeSearchNames.begin(), nativeSearchNames.end(), curName) != nativeSearchNames.end()) {
            break;
        }
        nativeSearchNames.push_back(curName);
        auto sc = findClass(curName);
        if (sc && !sc->superClassName.empty()) {
            curName = sc->superClassName;
        } else {
            curName = getNativeSuperClass(curName);
        }
    }
    if (!className.empty() && std::find(nativeSearchNames.begin(), nativeSearchNames.end(), className) == nativeSearchNames.end()) {
        nativeSearchNames.push_back(className);
    }

    // Exact descriptor anywhere in the hierarchy wins over a name-only match,
    // which could bind a different overload.
    for (const auto& cName : nativeSearchNames) {
        auto it = m_nativeMethods.find(makeNativeKey(cName, methodName, descriptor));
        if (it != m_nativeMethods.end()) {
            ArgRoot root(this, &args, true);
            return it->second(this, args);
        }
    }
    for (const auto& cName : nativeSearchNames) {
        auto it = m_nativeMethods.find(makeNativeKey(cName, methodName, ""));
        if (it != m_nativeMethods.end()) {
            ArgRoot root(this, &args, true);
            return it->second(this, args);
        }
    }

    // Unresolved: return a type-correct default so the caller's stack stays consistent
    static const bool traceMissing = std::getenv("J2ME_TRACE") != nullptr;
    if (traceMissing) {
        static std::mutex seenMutex;
        static std::unordered_map<std::string, bool> seen;
        std::string key = (nativeSearchNames.empty() ? className : nativeSearchNames.front()) + "." + methodName + descriptor;
        std::lock_guard<std::mutex> lock(seenMutex);
        if (!seen[key]) {
            seen[key] = true;
            std::fprintf(stderr, "[J2ME TRACE] missing method %s\n", key.c_str());
        }
    }
    size_t retPos = descriptor.find(')');
    if (retPos != std::string::npos && retPos + 1 < descriptor.size()) {
        return defaultValueForDescriptor(descriptor.substr(retPos + 1));
    }
    return JavaValue();
}

// Java-semantics float/double -> integral conversions (NaN -> 0, saturating)
template <typename To, typename From>
static To javaFloatToIntegral(From v) {
    if (std::isnan(v)) return 0;
    if (v >= static_cast<From>(std::numeric_limits<To>::max())) return std::numeric_limits<To>::max();
    if (v <= static_cast<From>(std::numeric_limits<To>::min())) return std::numeric_limits<To>::min();
    return static_cast<To>(v);
}

template <typename T>
static int32_t javaFCmp(T a, T b, int32_t nanResult) {
    if (std::isnan(a) || std::isnan(b)) return nanResult;
    return (a > b) ? 1 : ((a < b) ? -1 : 0);
}

// ---- Debug: J2ME_THREADDUMP=1 prints every Java thread's call stack every 10 s ----
namespace {
struct DumpFrame { JavaMethod* m; const size_t* pc; };
struct DumpStack { std::mutex mu; std::vector<DumpFrame> frames; };
std::mutex g_dumpMu;
std::map<std::thread::id, std::shared_ptr<DumpStack>> g_dumpStacks;
bool threadDumpEnabled() {
    static const bool on = std::getenv("J2ME_THREADDUMP") != nullptr || std::getenv("J2ME_PROFILE") != nullptr;
    return on;
}
DumpStack& myDumpStack() {
    thread_local std::shared_ptr<DumpStack> st;
    if (!st) {
        st = std::make_shared<DumpStack>();
        std::lock_guard<std::mutex> l(g_dumpMu);
        g_dumpStacks[std::this_thread::get_id()] = st;
    }
    return *st;
}
struct DumpGuard {
    DumpStack* st{nullptr};
    DumpGuard(JavaMethod* m, const size_t* pc) {
        if (!threadDumpEnabled()) return;
        st = &myDumpStack();
        std::lock_guard<std::mutex> l(st->mu);
        st->frames.push_back({m, pc});
    }
    ~DumpGuard() {
        if (!st) return;
        std::lock_guard<std::mutex> l(st->mu);
        st->frames.pop_back();
    }
};
// Debug: J2ME_CALLLOG=cls.method[,cls.method...] logs each entry into those methods
void callLog(JavaMethod* m, const std::vector<JavaValue>& args) {
    static const std::string spec = [] { const char* e = std::getenv("J2ME_CALLLOG"); return e ? "," + std::string(e) + "," : std::string(); }();
    if (spec.empty() || !m->clazz) return;
    const std::string key = "," + m->clazz->thisClassName + "." + m->name + ",";
    if (spec.find(key) == std::string::npos && spec.find("," + m->clazz->thisClassName + ".*,") == std::string::npos) return;
    std::cerr << "[Call] " << key.substr(1, key.size() - 2) << m->descriptor;
    // Leading int fields of plain object arguments
    for (const auto& v : args) {
        if (v.type != JavaValueType::REF || !v.ref || v.ref->isArray() || v.ref->isString()) continue;
        std::cerr << " {";
        for (size_t i = 0; i < v.ref->fields.size() && i < 4; ++i) {
            if (v.ref->fields[i].type == JavaValueType::INT) std::cerr << v.ref->fields[i].i << ",";
        }
        std::cerr << "}";
    }
    std::cerr << std::endl;
    // With J2ME_THREADDUMP also set, J2ME_CALLSTACK=1 prints the callers
    static const bool withStack = std::getenv("J2ME_CALLSTACK") != nullptr;
    if (withStack && threadDumpEnabled()) {
        auto& st = myDumpStack();
        std::lock_guard<std::mutex> l(st.mu);
        for (size_t i = st.frames.size() - 1; i-- > 0 && st.frames.size() - i < 12;) {
            auto& f = st.frames[i];
            std::cerr << "    at " << (f.m->clazz ? f.m->clazz->thisClassName : "?") << "." << f.m->name << f.m->descriptor << " pc=" << *f.pc << std::endl;
        }
    }
}
void startThreadDumper() {
    static std::once_flag once;
    std::call_once(once, [] {
        // J2ME_PROFILE=1: sample the top frames of every thread, print the hottest every 10 s
        if (std::getenv("J2ME_PROFILE")) {
            std::thread([] {
                std::map<std::string, int> self, incl;
                int samples = 0;
                auto last = std::chrono::steady_clock::now();
                for (;;) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    {
                        std::lock_guard<std::mutex> l(g_dumpMu);
                        for (auto& [id, st] : g_dumpStacks) {
                            std::lock_guard<std::mutex> l2(st->mu);
                            if (st->frames.empty()) continue;
                            ++samples;
                            auto name = [](JavaMethod* m) { return (m->clazz ? m->clazz->thisClassName : "?") + "." + m->name + m->descriptor; };
                            self[name(st->frames.back().m)]++;
                            std::set<JavaMethod*> seen;
                            for (auto& f : st->frames) if (seen.insert(f.m).second) incl[name(f.m)]++;
                        }
                    }
                    if (std::chrono::steady_clock::now() - last < std::chrono::seconds(10)) continue;
                    last = std::chrono::steady_clock::now();
                    auto top = [&](std::map<std::string, int>& m, const char* title) {
                        std::vector<std::pair<int, std::string>> v;
                        for (auto& [k, c] : m) v.push_back({c, k});
                        std::sort(v.rbegin(), v.rend());
                        std::cerr << "===== profile " << title << " (" << samples << " samples) =====" << std::endl;
                        for (size_t i = 0; i < v.size() && i < 25; ++i) std::cerr << "  " << v[i].first * 100 / std::max(samples, 1) << "% " << v[i].second << std::endl;
                    };
                    top(self, "self");
                    top(incl, "inclusive");
                    self.clear();
                    incl.clear();
                    samples = 0;
                }
            }).detach();
            return;
        }
        std::thread([] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                std::lock_guard<std::mutex> l(g_dumpMu);
                std::cerr << "===== Java thread dump =====" << std::endl;
                for (auto& [id, st] : g_dumpStacks) {
                    std::lock_guard<std::mutex> l2(st->mu);
                    if (st->frames.empty()) continue;
                    std::cerr << "-- thread " << id << std::endl;
                    for (auto it = st->frames.rbegin(); it != st->frames.rend(); ++it) {
                        auto* m = it->m;
                        std::cerr << "   " << (m->clazz ? m->clazz->thisClassName : "?") << "." << m->name << m->descriptor
                                  << " pc=" << *it->pc << std::endl;
                    }
                }
            }
        }).detach();
    });
}
} // namespace

JavaValue CldcVirtualMachine::executeMethod(JavaMethod* method, const std::vector<JavaValue>& args) {
    if (!method) return JavaValue();
    checkTerminate();
    GilScope gil(this);

    if (method->isNative()) {
        const NativeMethodHandler* h = nativeFor(method);
        ArgRoot root(this, &args, true);
        return h ? (*h)(this, args) : JavaValue();
    }

    if (method->code.empty()) return JavaValue();

    // Map arguments onto local slots: long/double parameters occupy two slots
    if (!method->paramsParsed) {
        method->paramTypes = parseParamTypes(method->descriptor);
        method->paramsParsed = true;
    }
    const std::vector<char>& paramTypes = method->paramTypes;
    size_t slotsNeeded = method->isStatic() ? 0 : 1;
    for (char c : paramTypes) slotsNeeded += (c == 'J' || c == 'D') ? 2 : 1;

    StackFrame frame(method, std::max<size_t>({static_cast<size_t>(method->maxLocals), slotsNeeded, args.size()}) + 1,
                     std::max<size_t>(method->maxStack, 16));
    GcThread& gcT = gcThread();
    gcT.frames.push_back(&frame);
    struct FramePop { GcThread& t; ~FramePop() { t.frames.pop_back(); } } framePop{gcT};
    {
        size_t argIdx = 0;
        size_t slot = 0;
        if (!method->isStatic() && argIdx < args.size()) {
            frame.locals[slot++] = args[argIdx++];
        }
        for (char c : paramTypes) {
            if (argIdx >= args.size()) break;
            frame.locals[slot] = args[argIdx++];
            slot += (c == 'J' || c == 'D') ? 2 : 1;
        }
    }

    // synchronized methods lock the receiver (or the Class object for static methods)
    JavaObject* syncMonitor = nullptr;
    if (method->accessFlags & ACC_SYNCHRONIZED) {
        syncMonitor = method->isStatic() ? (method->clazz ? classObjectFor(method->clazz->thisClassName) : nullptr)
                                         : (args.empty() ? nullptr : args[0].ref);
        if (syncMonitor) monitorEnter(syncMonitor);
    }
    struct MonitorGuard {
        CldcVirtualMachine* vm;
        JavaObject* obj;
        ~MonitorGuard() { if (obj) vm->monitorExit(obj); }
    } syncGuard{this, syncMonitor};

    if (threadDumpEnabled()) startThreadDumper();
    DumpGuard dumpGuard(method, &frame.pc);
    callLog(method, args);

    JavaClass* const cls = method->clazz;
    const uint8_t* code = method->code.data();
    size_t codeLen = method->code.size();
    size_t& pc = frame.pc;
    pc = 0;
    size_t currentInstrPc = 0;

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
        return static_cast<int16_t>(readU2());
    };
    auto readI4 = [&]() -> int32_t {
        if (pc + 4 > codeLen) throw std::runtime_error("Bytecode out of range");
        uint32_t v = (static_cast<uint32_t>(code[pc]) << 24) |
                     (static_cast<uint32_t>(code[pc + 1]) << 16) |
                     (static_cast<uint32_t>(code[pc + 2]) << 8)  |
                     static_cast<uint32_t>(code[pc + 3]);
        pc += 4;
        return static_cast<int32_t>(v);
    };
    auto local = [&](size_t idx) -> JavaValue& {
        if (idx >= frame.locals.size()) frame.locals.resize(idx + 1);
        return frame.locals[idx];
    };
    auto branch = [&](int32_t off) {
        pc = static_cast<size_t>(static_cast<int64_t>(currentInstrPc) + off);
        // Loops give other Java threads (and the UI thread) a chance to run
        if (off <= 0) {
            checkTerminate();
            if ((++m_backBranchTick & 0x1F) == 0) {
                safepoint();
                if (m_heap.size() >= m_gcThreshold) maybeGc();
            }
        }
    };
    auto popArray = [&](int32_t& idx) -> JavaArray* {
        idx = frame.pop().i;
        JavaObject* obj = frame.pop().ref;
        if (!obj) throwJava("java/lang/NullPointerException");
        auto* arr = dynamic_cast<JavaArray*>(obj);
        if (!arr) throwJava("java/lang/ClassCastException");
        if (idx < 0 || idx >= arr->length) {
            throwJava("java/lang/ArrayIndexOutOfBoundsException", std::to_string(idx));
        }
        return arr;
    };
    auto cpClassName = [&](uint16_t idx) -> std::string {
        return cls ? cls->getClassNameFromCp(idx) : std::string();
    };
    struct MemberRef { std::string cName, name, desc; };
    auto cpMemberRef = [&](uint16_t idx) -> MemberRef {
        MemberRef r;
        if (!cls || idx >= cls->constantPool.size()) return r;
        const auto& cp = cls->constantPool[idx];
        r.cName = cls->getClassNameFromCp(cp.classIndex);
        if (cp.nameAndTypeIndex < cls->constantPool.size()) {
            const auto& nat = cls->constantPool[cp.nameAndTypeIndex];
            r.name = cls->getUtf8FromCp(nat.nameIndex);
            r.desc = cls->getUtf8FromCp(nat.descriptorIndex);
        }
        return r;
    };
    auto fieldSite = [&](uint16_t idx) -> FieldSite& {
        FieldSite& fs = m_fieldSites[(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(cls)) << 16) ^ idx];
        if (fs.name.empty()) {
            MemberRef ref = cpMemberRef(idx);
            fs.cName = std::move(ref.cName);
            fs.name = std::move(ref.name);
            fs.desc = std::move(ref.desc);
        }
        return fs;
    };
    auto staticSite = [&](uint16_t idx) -> FieldSite& {
        FieldSite& fs = fieldSite(idx);
        if (!fs.sResolved) {
            fs.sResolved = true;
            auto target = findClass(fs.cName);
            JavaField* field = nullptr;
            JavaClass* owner = target ? findStaticFieldOwner(target.get(), fs.name, fs.desc, &field) : nullptr;
            if (owner && field) {
                ensureInitialized(owner);
                if (field->slotIndex < owner->staticFieldValues.size()) {
                    fs.sOwner = owner;
                    fs.sSlot = field->slotIndex;
                }
            }
        }
        return fs;
    };
    auto fieldSlot = [&](FieldSite& fs, JavaObject* obj) -> long {
        if (fs.slot >= 0 && obj->clazz && fs.objClass == obj->clazz) return fs.slot;
        auto symClass = findClass(fs.cName);
        long slot = findInstanceSlot(symClass ? symClass.get() : obj->clazz, fs.name, fs.desc);
        if (slot < 0 && obj->clazz) slot = findInstanceSlot(obj->clazz, fs.name, fs.desc);
        fs.objClass = obj->clazz;
        fs.slot = slot;
        return slot;
    };
    std::function<JavaArray*(const std::string&, const std::vector<int32_t>&, size_t)> makeMultiArray;
    auto initMultiArray = [&] {
        if (makeMultiArray) return;
        makeMultiArray = [&](const std::string& desc, const std::vector<int32_t>& dims, size_t level) -> JavaArray* {
        // desc is the array type at this level, e.g. "[[I"
        char elemCode = (desc.size() > 1) ? desc[1] : 'I';
        if (elemCode == '[') elemCode = 'L';
        JavaArray* arr = allocateArray(elemCode, dims[level]);
        if (level + 1 < dims.size()) {
            std::string sub = desc.substr(1);
            for (int32_t k = 0; k < dims[level]; ++k) {
                arr->elements[k] = JavaValue(static_cast<JavaObject*>(makeMultiArray(sub, dims, level + 1)));
            }
        }
        return arr;
    };
    };

    for (;;) {
        try {
            while (pc < codeLen) {
                currentInstrPc = pc;
                uint8_t opcode = readU1();
                bool wide = false;
                if (opcode == 0xC4) { // wide
                    wide = true;
                    opcode = readU1();
                }
                auto readLocalIdx = [&]() -> size_t { return wide ? readU2() : readU1(); };

                switch (opcode) {
                    case 0x00: break; // nop
                    case 0x01: frame.push(JavaValue(static_cast<JavaObject*>(nullptr))); break; // aconst_null
                    case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: // iconst_m1..iconst_5
                        frame.push(JavaValue(static_cast<int32_t>(opcode) - 3));
                        break;
                    case 0x09: frame.push(JavaValue(int64_t(0))); break; // lconst_0
                    case 0x0A: frame.push(JavaValue(int64_t(1))); break; // lconst_1
                    case 0x0B: frame.push(JavaValue(0.0f)); break;       // fconst_0
                    case 0x0C: frame.push(JavaValue(1.0f)); break;       // fconst_1
                    case 0x0D: frame.push(JavaValue(2.0f)); break;       // fconst_2
                    case 0x0E: frame.push(JavaValue(0.0)); break;        // dconst_0
                    case 0x0F: frame.push(JavaValue(1.0)); break;        // dconst_1
                    case 0x10: frame.push(JavaValue(static_cast<int32_t>(readI1()))); break; // bipush
                    case 0x11: frame.push(JavaValue(static_cast<int32_t>(readI2()))); break; // sipush
                    case 0x12:   // ldc
                    case 0x13:   // ldc_w
                    case 0x14: { // ldc2_w
                        uint16_t cpIdx = (opcode == 0x12) ? readU1() : readU2();
                        if (!cls || cpIdx >= cls->constantPool.size()) {
                            frame.push(JavaValue());
                            break;
                        }
                        const auto& cp = cls->constantPool[cpIdx];
                        if (cp.tag == CONSTANT_Integer) frame.push(JavaValue(cp.intVal));
                        else if (cp.tag == CONSTANT_Float) frame.push(JavaValue(cp.floatVal));
                        else if (cp.tag == CONSTANT_Long) frame.push(JavaValue(cp.longVal));
                        else if (cp.tag == CONSTANT_Double) frame.push(JavaValue(cp.doubleVal));
                        else if (cp.tag == CONSTANT_String) {
                            frame.push(JavaValue(static_cast<JavaObject*>(intern(cls->getUtf8FromCp(cp.stringIndex)))));
                        } else if (cp.tag == CONSTANT_Class) {
                            frame.push(JavaValue(classObjectFor(cls->getUtf8FromCp(cp.nameIndex))));
                        } else {
                            frame.push(JavaValue());
                        }
                        break;
                    }

                    // Loads
                    case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: // iload lload fload dload aload
                        frame.push(local(readLocalIdx()));
                        break;
                    case 0x1A: case 0x1B: case 0x1C: case 0x1D: frame.push(local(opcode - 0x1A)); break; // iload_n
                    case 0x1E: case 0x1F: case 0x20: case 0x21: frame.push(local(opcode - 0x1E)); break; // lload_n
                    case 0x22: case 0x23: case 0x24: case 0x25: frame.push(local(opcode - 0x22)); break; // fload_n
                    case 0x26: case 0x27: case 0x28: case 0x29: frame.push(local(opcode - 0x26)); break; // dload_n
                    case 0x2A: case 0x2B: case 0x2C: case 0x2D: frame.push(local(opcode - 0x2A)); break; // aload_n

                    // Array loads
                    case 0x2E: case 0x2F: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: {
                        int32_t idx;
                        JavaArray* arr = popArray(idx);
                        frame.push(arr->elements[idx]);
                        break;
                    }

                    // Stores
                    case 0x36: case 0x37: case 0x38: case 0x39: case 0x3A: { // istore lstore fstore dstore astore
                        size_t idx = readLocalIdx();
                        local(idx) = frame.pop();
                        break;
                    }
                    case 0x3B: case 0x3C: case 0x3D: case 0x3E: local(opcode - 0x3B) = frame.pop(); break; // istore_n
                    case 0x3F: case 0x40: case 0x41: case 0x42: local(opcode - 0x3F) = frame.pop(); break; // lstore_n
                    case 0x43: case 0x44: case 0x45: case 0x46: local(opcode - 0x43) = frame.pop(); break; // fstore_n
                    case 0x47: case 0x48: case 0x49: case 0x4A: local(opcode - 0x47) = frame.pop(); break; // dstore_n
                    case 0x4B: case 0x4C: case 0x4D: case 0x4E: local(opcode - 0x4B) = frame.pop(); break; // astore_n

                    // Array stores
                    case 0x4F: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: {
                        JavaValue val = frame.pop();
                        int32_t idx;
                        JavaArray* arr = popArray(idx);
                        switch (opcode) {
                            case 0x54: // bastore (byte or boolean)
                                val = JavaValue(arr->elementTypeCode == 'Z' ? (val.i & 1) : static_cast<int32_t>(static_cast<int8_t>(val.i)));
                                break;
                            case 0x55: val = JavaValue(static_cast<int32_t>(static_cast<uint16_t>(val.i))); break; // castore
                            case 0x56: val = JavaValue(static_cast<int32_t>(static_cast<int16_t>(val.i))); break;  // sastore
                            default: break;
                        }
                        arr->elements[idx] = val;
                        break;
                    }

                    // Stack manipulation (long/double are a single category-2 entry)
                    case 0x57: frame.pop(); break; // pop
                    case 0x58: { // pop2
                        JavaValue v1 = frame.pop();
                        if (!v1.isWide()) frame.pop();
                        break;
                    }
                    case 0x59: frame.push(frame.peek()); break; // dup
                    case 0x5A: { // dup_x1
                        JavaValue v1 = frame.pop();
                        JavaValue v2 = frame.pop();
                        frame.push(v1); frame.push(v2); frame.push(v1);
                        break;
                    }
                    case 0x5B: { // dup_x2
                        JavaValue v1 = frame.pop();
                        JavaValue v2 = frame.pop();
                        if (v2.isWide()) {
                            frame.push(v1); frame.push(v2); frame.push(v1);
                        } else {
                            JavaValue v3 = frame.pop();
                            frame.push(v1); frame.push(v3); frame.push(v2); frame.push(v1);
                        }
                        break;
                    }
                    case 0x5C: { // dup2
                        JavaValue v1 = frame.pop();
                        if (v1.isWide()) {
                            frame.push(v1); frame.push(v1);
                        } else {
                            JavaValue v2 = frame.pop();
                            frame.push(v2); frame.push(v1); frame.push(v2); frame.push(v1);
                        }
                        break;
                    }
                    case 0x5D: { // dup2_x1
                        JavaValue v1 = frame.pop();
                        if (v1.isWide()) {
                            JavaValue v2 = frame.pop();
                            frame.push(v1); frame.push(v2); frame.push(v1);
                        } else {
                            JavaValue v2 = frame.pop();
                            JavaValue v3 = frame.pop();
                            frame.push(v2); frame.push(v1); frame.push(v3); frame.push(v2); frame.push(v1);
                        }
                        break;
                    }
                    case 0x5E: { // dup2_x2
                        JavaValue v1 = frame.pop();
                        if (v1.isWide()) {
                            JavaValue v2 = frame.pop();
                            if (v2.isWide()) {
                                frame.push(v1); frame.push(v2); frame.push(v1);
                            } else {
                                JavaValue v3 = frame.pop();
                                frame.push(v1); frame.push(v3); frame.push(v2); frame.push(v1);
                            }
                        } else {
                            JavaValue v2 = frame.pop();
                            JavaValue v3 = frame.pop();
                            if (v3.isWide()) {
                                frame.push(v2); frame.push(v1); frame.push(v3); frame.push(v2); frame.push(v1);
                            } else {
                                JavaValue v4 = frame.pop();
                                frame.push(v2); frame.push(v1); frame.push(v4); frame.push(v3); frame.push(v2); frame.push(v1);
                            }
                        }
                        break;
                    }
                    case 0x5F: { // swap
                        JavaValue v1 = frame.pop();
                        JavaValue v2 = frame.pop();
                        frame.push(v1); frame.push(v2);
                        break;
                    }

                    // Integer arithmetic (wrapping, as in Java)
                    case 0x60: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)))); break; } // iadd
                    case 0x64: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)))); break; } // isub
                    case 0x68: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)))); break; } // imul
                    case 0x6C: { // idiv
                        int32_t b = frame.pop().i; int32_t a = frame.pop().i;
                        if (b == 0) throwJava("java/lang/ArithmeticException", "/ by zero");
                        frame.push(JavaValue((a == INT32_MIN && b == -1) ? a : a / b));
                        break;
                    }
                    case 0x70: { // irem
                        int32_t b = frame.pop().i; int32_t a = frame.pop().i;
                        if (b == 0) throwJava("java/lang/ArithmeticException", "/ by zero");
                        frame.push(JavaValue((b == -1) ? 0 : a % b));
                        break;
                    }
                    case 0x74: frame.push(JavaValue(static_cast<int32_t>(0u - static_cast<uint32_t>(frame.pop().i)))); break; // ineg
                    case 0x78: { int32_t s = frame.pop().i & 0x1F; int32_t v = frame.pop().i; frame.push(JavaValue(static_cast<int32_t>(static_cast<uint32_t>(v) << s))); break; } // ishl
                    case 0x7A: { int32_t s = frame.pop().i & 0x1F; int32_t v = frame.pop().i; frame.push(JavaValue(v >> s)); break; } // ishr
                    case 0x7C: { int32_t s = frame.pop().i & 0x1F; uint32_t v = static_cast<uint32_t>(frame.pop().i); frame.push(JavaValue(static_cast<int32_t>(v >> s))); break; } // iushr
                    case 0x7E: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(a & b)); break; } // iand
                    case 0x80: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(a | b)); break; } // ior
                    case 0x82: { int32_t b = frame.pop().i; int32_t a = frame.pop().i; frame.push(JavaValue(a ^ b)); break; } // ixor

                    // Long arithmetic
                    case 0x61: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b)))); break; } // ladd
                    case 0x65: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b)))); break; } // lsub
                    case 0x69: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b)))); break; } // lmul
                    case 0x6D: { // ldiv
                        int64_t b = frame.pop().l; int64_t a = frame.pop().l;
                        if (b == 0) throwJava("java/lang/ArithmeticException", "/ by zero");
                        frame.push(JavaValue((a == INT64_MIN && b == -1) ? a : a / b));
                        break;
                    }
                    case 0x71: { // lrem
                        int64_t b = frame.pop().l; int64_t a = frame.pop().l;
                        if (b == 0) throwJava("java/lang/ArithmeticException", "/ by zero");
                        frame.push(JavaValue((b == -1) ? int64_t(0) : a % b));
                        break;
                    }
                    case 0x75: frame.push(JavaValue(static_cast<int64_t>(0ull - static_cast<uint64_t>(frame.pop().l)))); break; // lneg
                    case 0x79: { int32_t s = frame.pop().i & 0x3F; int64_t v = frame.pop().l; frame.push(JavaValue(static_cast<int64_t>(static_cast<uint64_t>(v) << s))); break; } // lshl
                    case 0x7B: { int32_t s = frame.pop().i & 0x3F; int64_t v = frame.pop().l; frame.push(JavaValue(v >> s)); break; } // lshr
                    case 0x7D: { int32_t s = frame.pop().i & 0x3F; uint64_t v = static_cast<uint64_t>(frame.pop().l); frame.push(JavaValue(static_cast<int64_t>(v >> s))); break; } // lushr
                    case 0x7F: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(a & b)); break; } // land
                    case 0x81: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(a | b)); break; } // lor
                    case 0x83: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(a ^ b)); break; } // lxor

                    // Float arithmetic
                    case 0x62: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(a + b)); break; } // fadd
                    case 0x66: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(a - b)); break; } // fsub
                    case 0x6A: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(a * b)); break; } // fmul
                    case 0x6E: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(a / b)); break; } // fdiv
                    case 0x72: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(std::fmod(a, b))); break; } // frem
                    case 0x76: frame.push(JavaValue(-frame.pop().f)); break; // fneg

                    // Double arithmetic
                    case 0x63: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(a + b)); break; } // dadd
                    case 0x67: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(a - b)); break; } // dsub
                    case 0x6B: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(a * b)); break; } // dmul
                    case 0x6F: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(a / b)); break; } // ddiv
                    case 0x73: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(std::fmod(a, b))); break; } // drem
                    case 0x77: frame.push(JavaValue(-frame.pop().d)); break; // dneg

                    case 0x84: { // iinc
                        size_t lIdx = readLocalIdx();
                        int32_t c = wide ? static_cast<int32_t>(readI2()) : static_cast<int32_t>(readI1());
                        JavaValue& v = local(lIdx);
                        v = JavaValue(static_cast<int32_t>(static_cast<uint32_t>(v.i) + static_cast<uint32_t>(c)));
                        break;
                    }

                    // Conversions
                    case 0x85: frame.push(JavaValue(static_cast<int64_t>(frame.pop().i))); break; // i2l
                    case 0x86: frame.push(JavaValue(static_cast<float>(frame.pop().i))); break;   // i2f
                    case 0x87: frame.push(JavaValue(static_cast<double>(frame.pop().i))); break;  // i2d
                    case 0x88: frame.push(JavaValue(static_cast<int32_t>(frame.pop().l))); break; // l2i
                    case 0x89: frame.push(JavaValue(static_cast<float>(frame.pop().l))); break;   // l2f
                    case 0x8A: frame.push(JavaValue(static_cast<double>(frame.pop().l))); break;  // l2d
                    case 0x8B: frame.push(JavaValue(javaFloatToIntegral<int32_t>(frame.pop().f))); break; // f2i
                    case 0x8C: frame.push(JavaValue(javaFloatToIntegral<int64_t>(frame.pop().f))); break; // f2l
                    case 0x8D: frame.push(JavaValue(static_cast<double>(frame.pop().f))); break;          // f2d
                    case 0x8E: frame.push(JavaValue(javaFloatToIntegral<int32_t>(frame.pop().d))); break; // d2i
                    case 0x8F: frame.push(JavaValue(javaFloatToIntegral<int64_t>(frame.pop().d))); break; // d2l
                    case 0x90: frame.push(JavaValue(static_cast<float>(frame.pop().d))); break;           // d2f
                    case 0x91: frame.push(JavaValue(static_cast<int32_t>(static_cast<int8_t>(frame.pop().i)))); break;   // i2b
                    case 0x92: frame.push(JavaValue(static_cast<int32_t>(static_cast<uint16_t>(frame.pop().i)))); break; // i2c
                    case 0x93: frame.push(JavaValue(static_cast<int32_t>(static_cast<int16_t>(frame.pop().i)))); break;  // i2s

                    // Comparisons
                    case 0x94: { int64_t b = frame.pop().l; int64_t a = frame.pop().l; frame.push(JavaValue(static_cast<int32_t>((a > b) ? 1 : ((a < b) ? -1 : 0)))); break; } // lcmp
                    case 0x95: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(javaFCmp(a, b, -1))); break; }   // fcmpl
                    case 0x96: { float b = frame.pop().f; float a = frame.pop().f; frame.push(JavaValue(javaFCmp(a, b, 1))); break; }    // fcmpg
                    case 0x97: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(javaFCmp(a, b, -1))); break; } // dcmpl
                    case 0x98: { double b = frame.pop().d; double a = frame.pop().d; frame.push(JavaValue(javaFCmp(a, b, 1))); break; }  // dcmpg

                    // Branches
                    case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: { // ifeq..ifle
                        int16_t off = readI2();
                        int32_t v = frame.pop().i;
                        bool take = false;
                        switch (opcode) {
                            case 0x99: take = (v == 0); break;
                            case 0x9A: take = (v != 0); break;
                            case 0x9B: take = (v < 0); break;
                            case 0x9C: take = (v >= 0); break;
                            case 0x9D: take = (v > 0); break;
                            case 0x9E: take = (v <= 0); break;
                        }
                        if (take) branch(off);
                        break;
                    }
                    case 0x9F: case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: { // if_icmpeq..if_icmple
                        int16_t off = readI2();
                        int32_t v2 = frame.pop().i;
                        int32_t v1 = frame.pop().i;
                        bool take = false;
                        switch (opcode) {
                            case 0x9F: take = (v1 == v2); break;
                            case 0xA0: take = (v1 != v2); break;
                            case 0xA1: take = (v1 < v2); break;
                            case 0xA2: take = (v1 >= v2); break;
                            case 0xA3: take = (v1 > v2); break;
                            case 0xA4: take = (v1 <= v2); break;
                        }
                        if (take) branch(off);
                        break;
                    }
                    case 0xA5: case 0xA6: { // if_acmpeq, if_acmpne
                        int16_t off = readI2();
                        JavaObject* v2 = frame.pop().ref;
                        JavaObject* v1 = frame.pop().ref;
                        if ((opcode == 0xA5) == (v1 == v2)) branch(off);
                        break;
                    }
                    case 0xA7: branch(readI2()); break; // goto
                    case 0xC8: branch(readI4()); break; // goto_w
                    case 0xA8: { // jsr
                        int16_t off = readI2();
                        frame.push(JavaValue(static_cast<int32_t>(pc)));
                        branch(off);
                        break;
                    }
                    case 0xC9: { // jsr_w
                        int32_t off = readI4();
                        frame.push(JavaValue(static_cast<int32_t>(pc)));
                        branch(off);
                        break;
                    }
                    case 0xA9: pc = static_cast<size_t>(local(readLocalIdx()).i); break; // ret
                    case 0xAA: { // tableswitch
                        pc = (currentInstrPc + 4) & ~static_cast<size_t>(3);
                        int32_t def = readI4();
                        int32_t low = readI4();
                        int32_t high = readI4();
                        int32_t key = frame.pop().i;
                        if (key < low || key > high) {
                            branch(def);
                        } else {
                            pc += static_cast<size_t>(static_cast<int64_t>(key) - low) * 4;
                            branch(readI4());
                        }
                        break;
                    }
                    case 0xAB: { // lookupswitch
                        pc = (currentInstrPc + 4) & ~static_cast<size_t>(3);
                        int32_t def = readI4();
                        int32_t npairs = readI4();
                        int32_t key = frame.pop().i;
                        int32_t target = def;
                        for (int32_t k = 0; k < npairs; ++k) {
                            int32_t match = readI4();
                            int32_t off = readI4();
                            if (match == key) { target = off; break; }
                        }
                        branch(target);
                        break;
                    }
                    case 0xC6: { // ifnull
                        int16_t off = readI2();
                        if (frame.pop().ref == nullptr) branch(off);
                        break;
                    }
                    case 0xC7: { // ifnonnull
                        int16_t off = readI2();
                        if (frame.pop().ref != nullptr) branch(off);
                        break;
                    }

                    // Returns
                    case 0xAC: case 0xAD: case 0xAE: case 0xAF: case 0xB0: // ireturn..areturn
                        return frame.pop();
                    case 0xB1: // return
                        return JavaValue();

                    // Fields
                    case 0xB2: { // getstatic
                        FieldSite& fs = staticSite(readU2());
                        if (fs.sOwner) {
                            ensureInitialized(fs.sOwner);
                            frame.push(fs.sOwner->staticFieldValues[fs.sSlot]);
                            break;
                        }
                        auto ns = m_nativeStatics.find(fs.cName + "." + fs.name);
                        if (ns != m_nativeStatics.end()) {
                            ArgRoot root(this, nullptr, true);
                            frame.push(ns->second(this, {}));
                        } else {
                            frame.push(defaultValueForDescriptor(fs.desc));
                        }
                        break;
                    }
                    case 0xB3: { // putstatic
                        FieldSite& fs = staticSite(readU2());
                        JavaValue val = frame.pop();
                        if (fs.sOwner) {
                            ensureInitialized(fs.sOwner);
                            fs.sOwner->staticFieldValues[fs.sSlot] = val;
                        }
                        break;
                    }
                    case 0xB4: { // getfield
                        const uint16_t cpIdx = readU2();
                        JavaObject* obj = frame.pop().ref;
                        FieldSite& fs = fieldSite(cpIdx);
                        if (!obj) throwJava("java/lang/NullPointerException", "getfield " + fs.name);
                        const long slot = fieldSlot(fs, obj);
                        if (slot >= 0 && static_cast<size_t>(slot) < obj->fields.size()) {
                            frame.push(obj->fields[slot]);
                        } else {
                            frame.push(defaultValueForDescriptor(fs.desc));
                        }
                        break;
                    }
                    case 0xB5: { // putfield
                        const uint16_t cpIdx = readU2();
                        JavaValue val = frame.pop();
                        JavaObject* obj = frame.pop().ref;
                        FieldSite& fs = fieldSite(cpIdx);
                        if (!obj) throwJava("java/lang/NullPointerException", "putfield " + fs.name);
                        const long slot = fieldSlot(fs, obj);
                        if (slot >= 0) {
                            if (static_cast<size_t>(slot) >= obj->fields.size()) obj->fields.resize(slot + 1);
                            obj->fields[slot] = val;
                        }
                        break;
                    }

                    // Invocation
                    case 0xB6: // invokevirtual
                    case 0xB7: // invokespecial
                    case 0xB8: // invokestatic
                    case 0xB9: { // invokeinterface
                        const uint16_t cpIdx = readU2();
                        if (opcode == 0xB9) {
                            readU1(); // count
                            readU1(); // 0
                        }
                        const bool isVirtual = (opcode == 0xB6 || opcode == 0xB9);
                        CallSite& site = m_callSites[(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(cls)) << 16) ^ (static_cast<uint64_t>(opcode) << 60) ^ cpIdx];
                        if (site.cName.empty() && site.name.empty()) {
                            MemberRef ref = cpMemberRef(cpIdx);
                            site.cName = std::move(ref.cName);
                            site.name = std::move(ref.name);
                            site.desc = std::move(ref.desc);
                            site.argCount = parseMethodArgCount(site.desc, opcode == 0xB8);
                            site.isVoid = site.desc.find(")V") != std::string::npos;
                        }
                        std::vector<JavaValue> callArgs(site.argCount);
                        for (int k = static_cast<int>(site.argCount) - 1; k >= 0; --k) {
                            callArgs[k] = frame.pop();
                        }
                        ArgRoot argRoot(this, &callArgs, false);
                        // Monomorphic cache keyed on the receiver runtime class
                        JavaObject* receiver = (isVirtual && !callArgs.empty() && callArgs[0].type == JavaValueType::REF) ? callArgs[0].ref : nullptr;
                        const void* rKey = receiver ? static_cast<const void*>(receiver->clazz) : reinterpret_cast<const void*>(1);
                        bool hit = site.resolved && site.rKey == rKey;
                        if (hit && !rKey) hit = site.rName == classNameOf(receiver);
                        if (!hit) {
                            site.resolved = resolveInvoke(site.cName, site.name, site.desc, receiver, isVirtual, site.method, site.native, site.initClass);
                            site.rKey = rKey;
                            site.rName = rKey ? std::string() : classNameOf(receiver);
                        }
                        JavaValue ret;
                        if (!site.resolved) {
                            ret = invokeMethod(site.cName, site.name, site.desc, callArgs, isVirtual);
                        } else {
                            if (site.initClass) ensureInitialized(site.initClass);
                            if (site.method) {
                                JavaMethod* m = site.method;
                                if (m->trivialKind == 0 && site.argCount == 0) classifyTrivial(m);
                                if (m->trivialKind == 2) ret = m->trivialConst;
                                else if (m->trivialKind == 3) ret = m->trivialOwner->staticFieldValues[m->trivialSlot];
                                else ret = executeMethod(m, callArgs);
                            } else {
                                checkTerminate();
                                ArgRoot nativeRoot(this, nullptr, true);
                                static const bool natProf = std::getenv("J2ME_NATPROF") != nullptr;
                                if (!natProf) {
                                    ret = (*site.native)(this, callArgs);
                                } else {
                                    // J2ME_NATPROF=1: time spent in each native, printed every 5 s
                                    static std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> stats;
                                    static auto window = std::chrono::steady_clock::now();
                                    const auto t0 = std::chrono::steady_clock::now();
                                    ret = (*site.native)(this, callArgs);
                                    const auto t1 = std::chrono::steady_clock::now();
                                    auto& st = stats[site.cName + "." + site.name];
                                    st.first += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                                    ++st.second;
                                    if (t1 - window >= std::chrono::seconds(5)) {
                                        std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> v(stats.begin(), stats.end());
                                        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second.first > b.second.first; });
                                        std::fprintf(stderr, "===== native time (5 s) =====\n");
                                        for (size_t i = 0; i < v.size() && i < 15; ++i) {
                                            std::fprintf(stderr, "  %8.1f ms %9llu calls  %s\n", v[i].second.first / 1e6,
                                                         static_cast<unsigned long long>(v[i].second.second), v[i].first.c_str());
                                        }
                                        stats.clear();
                                        window = t1;
                                    }
                                }
                            }
                        }
                        if (!site.isVoid) {
                            frame.push(ret);
                        }
                        break;
                    }

                    // Objects & arrays
                    case 0xBB: { // new
                        std::string cName = cpClassName(readU2());
                        auto targetClazz = findClass(cName);
                        JavaObject* obj = (cName == "java/lang/String")
                            ? static_cast<JavaObject*>(allocateString(""))
                            : allocateObject(targetClazz.get());
                        if (!targetClazz && cName != "java/lang/String") obj->nativeClassName = cName;
                        frame.push(JavaValue(obj));
                        break;
                    }
                    case 0xBC: { // newarray
                        uint8_t aType = readU1();
                        int32_t len = frame.pop().i;
                        if (len < 0) throwJava("java/lang/NegativeArraySizeException", std::to_string(len));
                        char codeChar = 'I';
                        switch (aType) {
                            case 4: codeChar = 'Z'; break;
                            case 5: codeChar = 'C'; break;
                            case 6: codeChar = 'F'; break;
                            case 7: codeChar = 'D'; break;
                            case 8: codeChar = 'B'; break;
                            case 9: codeChar = 'S'; break;
                            case 10: codeChar = 'I'; break;
                            case 11: codeChar = 'J'; break;
                        }
                        frame.push(JavaValue(static_cast<JavaObject*>(allocateArray(codeChar, len))));
                        break;
                    }
                    case 0xBD: { // anewarray
                        readU2(); // component class index
                        int32_t len = frame.pop().i;
                        if (len < 0) throwJava("java/lang/NegativeArraySizeException", std::to_string(len));
                        frame.push(JavaValue(static_cast<JavaObject*>(allocateArray('L', len))));
                        break;
                    }
                    case 0xC5: { // multianewarray
                        std::string desc = cpClassName(readU2());
                        uint8_t dimCount = readU1();
                        std::vector<int32_t> dims(dimCount);
                        for (int k = dimCount - 1; k >= 0; --k) {
                            dims[k] = frame.pop().i;
                            if (dims[k] < 0) throwJava("java/lang/NegativeArraySizeException", std::to_string(dims[k]));
                        }
                        initMultiArray(); frame.push(JavaValue(static_cast<JavaObject*>(makeMultiArray(desc, dims, 0))));
                        break;
                    }
                    case 0xBE: { // arraylength
                        JavaObject* obj = frame.pop().ref;
                        if (!obj) throwJava("java/lang/NullPointerException", "arraylength");
                        auto* arr = dynamic_cast<JavaArray*>(obj);
                        frame.push(JavaValue(arr ? arr->length : 0));
                        break;
                    }
                    case 0xBF: { // athrow
                        JavaObject* exObj = frame.pop().ref;
                        if (!exObj) throwJava("java/lang/NullPointerException", "athrow");
                        std::string what = classNameOf(exObj);
                        for (char& c : what) if (c == '/') c = '.';
                        if (!exObj->throwableMessage.empty()) what += ": " + exObj->throwableMessage;
                        throw JavaException(exObj, what);
                    }
                    case 0xC0: { // checkcast
                        std::string target = cpClassName(readU2());
                        JavaObject* obj = frame.peek().ref;
                        // Platform objects cast to platform types we do not model (e.g. HashMap -> Map) pass
                        bool unmodeled = obj && !obj->clazz && !findClass(target);
                        if (obj && !unmodeled && !isInstanceOf(obj, target)) {
                            throwJava("java/lang/ClassCastException", classNameOf(obj) + " -> " + target);
                        }
                        break;
                    }
                    case 0xC1: { // instanceof
                        std::string target = cpClassName(readU2());
                        JavaObject* obj = frame.pop().ref;
                        frame.push(JavaValue(isInstanceOf(obj, target) ? 1 : 0));
                        break;
                    }
                    case 0xC2: // monitorenter
                        monitorEnter(frame.pop().ref);
                        break;
                    case 0xC3: // monitorexit
                        monitorExit(frame.pop().ref);
                        break;

                    default: {
                        char buf[96];
                        std::snprintf(buf, sizeof(buf), "Unsupported opcode 0x%02X at pc %zu", opcode, currentInstrPc);
                        throw std::runtime_error(buf);
                    }
                }
            }
            return JavaValue();
        } catch (const JavaException& je) {
            // Look for a matching handler in this method's exception table
            bool handled = false;
            for (const auto& ex : method->exceptionTable) {
                if (currentInstrPc >= ex.startPc && currentInstrPc < ex.endPc &&
                    (ex.catchType == 0 || isInstanceOf(je.object, ex.catchClassName))) {
                    if (je.object && je.object->throwableTrace.empty()) {
                        for (auto& f : je.trace) je.object->throwableTrace += "    at " + f + "\n";
                        je.object->throwableTrace += "    at " + (cls ? cls->thisClassName : std::string("?")) + "." + method->name +
                                                     method->descriptor + " pc=" + std::to_string(currentInstrPc) + "\n";
                    }
                    static const bool excLog = std::getenv("J2ME_EXCLOG") != nullptr;
                    if (excLog) std::cerr << "[J2ME EXC] " << je.what() << "\n" << (je.object ? je.object->throwableTrace : std::string());
                    frame.sp = 0;
                    frame.push(JavaValue(je.object));
                    pc = ex.handlerPc;
                    handled = true;
                    break;
                }
            }
            if (!handled) {
                if (je.trace.size() < 64) {
                    je.trace.push_back((cls ? cls->thisClassName : std::string("?")) + "." + method->name +
                                       method->descriptor + " pc=" + std::to_string(currentInstrPc));
                }
                throw;
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {
            // Native (C++) failure: surface it as a catchable java.lang.RuntimeException
            bool hasHandler = false;
            for (const auto& ex : method->exceptionTable) {
                if (currentInstrPc >= ex.startPc && currentInstrPc < ex.endPc) { hasHandler = true; break; }
            }
            if (!hasHandler) throw;
            std::string msg = e.what();
            try {
                throwJava("java/lang/RuntimeException", msg);
            } catch (const JavaException& je) {
                bool handled = false;
                for (const auto& ex : method->exceptionTable) {
                    if (currentInstrPc >= ex.startPc && currentInstrPc < ex.endPc &&
                        (ex.catchType == 0 || isInstanceOf(je.object, ex.catchClassName))) {
                        frame.sp = 0;
                        frame.push(JavaValue(je.object));
                        pc = ex.handlerPc;
                        handled = true;
                        break;
                    }
                }
                if (!handled) throw std::runtime_error(msg);
            }
        }
    }
}

// RecordStores live in the owning engine's RmsManager; a bare VM has no storage
static j2me::RmsManager* rmsOf(CldcVirtualMachine* vm) {
    auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
    return inst ? &inst->rms : nullptr;
}

static j2me::LcduiGraphics* getG(const JavaValue& v) {
    if (v.type != JavaValueType::REF || !v.ref) return nullptr;
    auto* go = dynamic_cast<JavaGraphicsObject*>(v.ref);
    if (go) {
        if (go->rawGraphics) return go->rawGraphics;
        if (go->graphics) return go->graphics.get();
    }
    return nullptr;
}

void CldcVirtualMachine::registerStandardNatives() {
    // ----------------------------------------------------
    // java/lang/Object
    // ----------------------------------------------------
    registerNative("java/lang/Object", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    // ----------------------------------------------------
    // java/lang/Throwable
    // ----------------------------------------------------
    registerNative("java/lang/Throwable", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("java/lang/Throwable", "<init>", "(Ljava/lang/String;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref) {
            if (auto* s = dynamic_cast<JavaString*>(args[1].ref)) args[0].ref->throwableMessage = s->value;
        }
        return JavaValue();
    });

    registerNative("java/lang/Throwable", "getMessage", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && !args[0].ref->throwableMessage.empty()) {
            return JavaValue(static_cast<JavaObject*>(vm->allocateString(args[0].ref->throwableMessage)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("java/lang/Throwable", "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        std::string s = args.empty() ? "java/lang/Throwable" : vm->classNameOf(args[0].ref);
        for (char& c : s) if (c == '/') c = '.';
        if (!args.empty() && args[0].ref && !args[0].ref->throwableMessage.empty()) s += ": " + args[0].ref->throwableMessage;
        return JavaValue(static_cast<JavaObject*>(vm->allocateString(s)));
    });

    registerNative("java/lang/Throwable", "printStackTrace", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref) {
            std::cerr << "[J2ME VM] " << vm->classNameOf(args[0].ref) << ": " << args[0].ref->throwableMessage << std::endl;
            std::cerr << args[0].ref->throwableTrace;
        }
        return JavaValue();
    });

    registerNative("java/lang/Object", "getClass", "()Ljava/lang/Class;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->clazz) {
            return JavaValue(static_cast<JavaObject*>(vm->allocateString(args[0].ref->clazz->thisClassName)));
        }
        return JavaValue();
    });

    registerNative("java/lang/Object", "hashCode", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref) {
            return JavaValue(static_cast<int32_t>(reinterpret_cast<uintptr_t>(args[0].ref) & 0x7FFFFFFF));
        }
        return JavaValue(0);
    });

    registerNative("java/lang/Object", "equals", "(Ljava/lang/Object;)Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2) {
            return JavaValue(args[0].ref == args[1].ref ? 1 : 0);
        }
        return JavaValue(0);
    });

    registerNative("java/lang/Object", "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        std::string name = "java.lang.Object";
        if (!args.empty() && args[0].ref && args[0].ref->clazz) {
            name = args[0].ref->clazz->thisClassName;
        }
        return JavaValue(static_cast<JavaObject*>(vm->allocateString(name)));
    });

    // ----------------------------------------------------
    // java/lang/Class & java/io/InputStream
    // ----------------------------------------------------
    registerNative("java/lang/Class", "getResourceAsStream", "(Ljava/lang/String;)Ljava/io/InputStream;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() > 1 && args[1].ref && args[1].ref->isString()) {
            std::string resName = static_cast<JavaString*>(args[1].ref)->value;
            auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
            if (inst) {
                std::vector<uint8_t> data;
                if (!inst->jarReader.extractEntry(resName, data)) {
                    if (!resName.empty() && resName[0] == '/') {
                        inst->jarReader.extractEntry(resName.substr(1), data);
                    } else {
                        inst->jarReader.extractEntry("/" + resName, data);
                    }
                }
                if (!data.empty()) {
                    return JavaValue(static_cast<JavaObject*>(vm->allocateInputStream(std::move(data))));
                }
            }
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("java/lang/Class", "getName", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->clazz) {
            return JavaValue(static_cast<JavaObject*>(vm->allocateString(args[0].ref->clazz->thisClassName)));
        }
        return JavaValue(static_cast<JavaObject*>(vm->allocateString("")));
    });

    registerNative("java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->isString()) {
            std::string cName = static_cast<JavaString*>(args[0].ref)->value;
            for (char& c : cName) if (c == '.') c = '/';
            auto clazz = vm->findClass(cName);
            if (clazz) {
                return JavaValue(static_cast<JavaObject*>(vm->allocateObject(clazz.get())));
            }
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("java/io/InputStream", "read", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref) {
            auto* stream = dynamic_cast<JavaInputStreamObject*>(args[0].ref);
            if (stream && stream->pos < stream->data.size()) {
                return JavaValue(static_cast<int32_t>(stream->data[stream->pos++]));
            }
        }
        return JavaValue(-1);
    });

    registerNative("java/io/InputStream", "read", "([BII)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 4 && args[0].ref && args[1].ref) {
            auto* stream = dynamic_cast<JavaInputStreamObject*>(args[0].ref);
            auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
            int32_t off = args[2].i;
            int32_t len = args[3].i;
            if (stream && arr && len > 0) {
                if (stream->pos >= stream->data.size()) return JavaValue(-1);
                int32_t available = static_cast<int32_t>(stream->data.size() - stream->pos);
                int32_t toRead = std::min(len, available);
                for (int32_t i = 0; i < toRead && off + i < arr->length; ++i) {
                    arr->elements[off + i].i = static_cast<int32_t>(static_cast<int8_t>(stream->data[stream->pos + i]));
                }
                stream->pos += toRead;
                return JavaValue(toRead);
            }
        }
        return JavaValue(-1);
    });

    registerNative("java/io/InputStream", "read", "([B)I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref && args[1].ref) {
            auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
            if (arr) {
                std::vector<JavaValue> subArgs = {args[0], args[1], JavaValue(0), JavaValue(arr->length)};
                return vm->executeMethodByName("java/io/InputStream", "read", "([BII)I", subArgs);
            }
        }
        return JavaValue(-1);
    });

    registerNative("java/io/InputStream", "available", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref) {
            auto* stream = dynamic_cast<JavaInputStreamObject*>(args[0].ref);
            if (stream && stream->pos < stream->data.size()) {
                return JavaValue(static_cast<int32_t>(stream->data.size() - stream->pos));
            }
        }
        return JavaValue(0);
    });

    registerNative("java/io/InputStream", "skip", "(J)J", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref) {
            auto* stream = dynamic_cast<JavaInputStreamObject*>(args[0].ref);
            int64_t n = args[1].l;
            if (stream && n > 0) {
                int64_t available = static_cast<int64_t>(stream->data.size() - stream->pos);
                int64_t toSkip = std::min(n, available);
                stream->pos += static_cast<size_t>(toSkip);
                return JavaValue(toSkip);
            }
        }
        return JavaValue(int64_t(0));
    });

    registerNative("java/io/InputStream", "close", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    // ----------------------------------------------------
    // java/lang/System
    // ----------------------------------------------------
    registerNative("java/lang/System", "currentTimeMillis", "()J", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) return JavaValue(inst->gameMillis());
        return JavaValue(J2meEngineInstance::realMillis());
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

    registerNative("java/lang/System", "identityHashCode", "(Ljava/lang/Object;)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref) {
            return JavaValue(static_cast<int32_t>(reinterpret_cast<uintptr_t>(args[0].ref) & 0x7FFFFFFF));
        }
        return JavaValue(0);
    });

    // ----------------------------------------------------
    // java/lang/Runtime
    // ----------------------------------------------------
    registerNative("java/lang/Runtime", "getRuntime", "()Ljava/lang/Runtime;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateObject(nullptr)));
    });

    registerNative("java/lang/Runtime", "totalMemory", "()J", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(int64_t(33554432)); // 32 MB
    });

    registerNative("java/lang/Runtime", "freeMemory", "()J", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(int64_t(16777216)); // 16 MB
    });

    registerNative("java/lang/Runtime", "gc", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        vm->gc();
        return JavaValue();
    });

    // ----------------------------------------------------
    // java/lang/Thread
    // ----------------------------------------------------
    registerNative("java/lang/Thread", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("java/lang/Thread", "<init>", "(Ljava/lang/Runnable;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref && args[1].ref) {
            args[0].ref->fields.resize(2);
            args[0].ref->fields[0] = args[1];
        }
        return JavaValue();
    });

    registerNative("java/lang/Thread", "start", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.empty() || !args[0].ref) return JavaValue();
        JavaObject* threadObj = args[0].ref;
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst && inst->isRunning.load()) {
            std::thread worker([vm, threadObj]() {
                try {
                    JavaObject* target = threadObj;
                    if (!threadObj->fields.empty() && threadObj->fields[0].ref) {
                        target = threadObj->fields[0].ref;
                    }
                    if (target && target->clazz) {
                        vm->executeMethodByName(target->clazz->thisClassName, "run", "()V", {JavaValue(target)});
                    }
                } catch (...) {}
            });
            worker.detach();
        }
        return JavaValue();
    });

    registerNative("java/lang/Thread", "sleep", "(J)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        int64_t ms = args.empty() ? 0 : args[0].l;
        if (ms > 0) {
            BlockingRegion region(vm);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return JavaValue();
    });

    registerNative("java/lang/Thread", "yield", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        BlockingRegion region(vm);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return JavaValue();
    });

    registerNative("java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateObject(nullptr)));
    });

    registerNative("java/lang/Thread", "activeCount", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(2);
    });

    // ----------------------------------------------------
    // java/lang/Math
    // ----------------------------------------------------
    registerNative("java/lang/Math", "abs", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0 : std::abs(args[0].i));
    });

    registerNative("java/lang/Math", "abs", "(J)J", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? int64_t(0) : std::abs(args[0].l));
    });

    registerNative("java/lang/Math", "abs", "(F)F", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0f : std::abs(args[0].f));
    });

    registerNative("java/lang/Math", "abs", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::abs(args[0].d));
    });

    registerNative("java/lang/Math", "min", "(II)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.size() >= 2 ? std::min(args[0].i, args[1].i) : 0);
    });

    registerNative("java/lang/Math", "max", "(II)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.size() >= 2 ? std::max(args[0].i, args[1].i) : 0);
    });

    registerNative("java/lang/Math", "sin", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::sin(args[0].d));
    });

    registerNative("java/lang/Math", "cos", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::cos(args[0].d));
    });

    registerNative("java/lang/Math", "tan", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::tan(args[0].d));
    });

    registerNative("java/lang/Math", "sqrt", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::sqrt(args[0].d));
    });

    registerNative("java/lang/Math", "ceil", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::ceil(args[0].d));
    });

    registerNative("java/lang/Math", "floor", "(D)D", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.empty() ? 0.0 : std::floor(args[0].d));
    });

    // ----------------------------------------------------
    // javax/microedition/midlet/MIDlet
    // ----------------------------------------------------
    registerNative("javax/microedition/midlet/MIDlet", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/midlet/MIDlet", "getAppProperty", "(Ljava/lang/String;)Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() > 1 && args[1].ref && args[1].ref->isString()) {
            std::string key = static_cast<JavaString*>(args[1].ref)->value;
            auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
            if (inst) {
                std::string val = inst->jarReader.getManifestProperty(key);
                if (!val.empty()) {
                    return JavaValue(static_cast<JavaObject*>(vm->allocateString(val)));
                }
            }
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/midlet/MIDlet", "notifyDestroyed", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst) inst->isRunning.store(false);
        return JavaValue();
    });

    registerNative("javax/microedition/midlet/MIDlet", "notifyPaused", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst) inst->isPaused.store(true);
        return JavaValue();
    });

    registerNative("javax/microedition/midlet/MIDlet", "platformRequest", "(Ljava/lang/String;)Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(0);
    });

    registerNative("javax/microedition/midlet/MIDlet", "checkPermission", "(Ljava/lang/String;)I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/Display
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/Display", "getDisplay", "(Ljavax/microedition/midlet/MIDlet;)Ljavax/microedition/lcdui/Display;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateObject(nullptr)));
    });

    registerNative("javax/microedition/lcdui/Display", "setCurrent", "(Ljavax/microedition/lcdui/Displayable;)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        JavaObject* target = args.size() > 1 ? args[1].ref : nullptr;
        if (isAlert(target)) showAlert(vm, target, nullptr);
        else lcduiSetCurrent(vm, target);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Display", "setCurrent", "(Ljavax/microedition/lcdui/Alert;Ljavax/microedition/lcdui/Displayable;)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        JavaObject* alert = args.size() > 1 ? args[1].ref : nullptr;
        JavaObject* next = args.size() > 2 ? args[2].ref : nullptr;
        if (!alert || !next) vm->throwJava("java/lang/NullPointerException");
        if (isAlert(next)) vm->throwJava("java/lang/IllegalArgumentException");
        showAlert(vm, alert, next);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Display", "setCurrentItem", "(Ljavax/microedition/lcdui/Item;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Display", "getCurrent", "()Ljavax/microedition/lcdui/Displayable;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst && inst->currentNativeScreen) return JavaValue(inst->currentNativeScreen);
        if (inst && inst->currentJavaCanvas) {
            return JavaValue(inst->currentJavaCanvas);
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/Display", "isColor", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Display", "numColors", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(16777216);
    });

    registerNative("javax/microedition/lcdui/Display", "numAlphaLevels", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(256);
    });

    registerNative("javax/microedition/lcdui/Display", "getColor", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return JavaValue(args.size() > 1 ? args[1].i : 0);
    });

    registerNative("javax/microedition/lcdui/Display", "vibrate", "(I)Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int dur = args.size() > 1 ? args[1].i : 100;
        universal_loader::oem::DeviceControlManager::instance().startVibra(100, dur);
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Display", "flashBacklight", "(I)Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Display", "callSerially", "(Ljava/lang/Runnable;)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() > 1 && args[1].ref) {
            JavaObject* runObj = args[1].ref;
            if (runObj->clazz) {
                try {
                    vm->executeMethodByName(runObj->clazz->thisClassName, "run", "()V", {JavaValue(runObj)});
                } catch (const VmTerminated&) { throw; } catch (...) {}
            }
        }
        return JavaValue();
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/Displayable & Canvas
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/Displayable", "getWidth", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getWidth() : 240);
    });

    registerNative("javax/microedition/lcdui/Displayable", "getHeight", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getHeight() : 320);
    });

    registerNative("javax/microedition/lcdui/Displayable", "isShown", "()Z", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        JavaObject* o = args.empty() ? nullptr : args[0].ref;
        if (!inst || !o) return JavaValue(0);
        if (inst->currentNativeScreen) return JavaValue(o == inst->currentNativeScreen ? 1 : 0);
        return JavaValue(o == inst->currentJavaCanvas ? 1 : 0);
    });

    registerNative("javax/microedition/lcdui/Displayable", "setTitle", "(Ljava/lang/String;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Displayable", "getTitle", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateString("Canvas")));
    });

    registerNative("javax/microedition/lcdui/Displayable", "addCommand", "(Ljavax/microedition/lcdui/Command;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Displayable", "removeCommand", "(Ljavax/microedition/lcdui/Command;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Displayable", "setCommandListener", "(Ljavax/microedition/lcdui/CommandListener;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Canvas", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    // Default no-op callbacks, overridden by games that care
    for (const char* cb : {"showNotify", "hideNotify"}) {
        registerNative("javax/microedition/lcdui/Canvas", cb, "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) { return JavaValue(); });
    }
    registerNative("javax/microedition/lcdui/Canvas", "sizeChanged", "(II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) { return JavaValue(); });
    registerNative("javax/microedition/lcdui/Canvas", "getWidth", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getWidth() : 240);
    });

    registerNative("javax/microedition/lcdui/Canvas", "getHeight", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getHeight() : 320);
    });

    registerNative("javax/microedition/lcdui/Canvas", "repaint", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) inst->requestRepaint();
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Canvas", "repaint", "(IIII)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) inst->requestRepaint();
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Canvas", "serviceRepaints", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        // Paints a pending repaint synchronously on the calling (game) thread
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (!inst || inst->painting || !inst->repaintPending.load() || inst->shouldSkipBackgroundPaint()) return JavaValue();
        if (inst->currentJavaCanvas && inst->currentJavaCanvas->clazz) {
            inst->repaintPending.store(false);
            inst->painting = true;
            struct PaintDone { J2meEngineInstance* i; ~PaintDone() { i->painting = false; } } done{inst};
            auto g = std::make_shared<j2me::LcduiGraphics>(inst->frameBuffer.getRawDrawBuffer(), inst->frameBuffer.getWidth(), inst->frameBuffer.getHeight());
            auto* gObj = vm->allocateGraphics(g);
            const int64_t paintStart = J2meEngineInstance::monoMillis();
            try {
                vm->executeMethodByName(inst->currentJavaCanvas->clazz->thisClassName, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", {JavaValue(inst->currentJavaCanvas), JavaValue(static_cast<JavaObject*>(gObj))});
            } catch (const VmTerminated&) { throw; } catch (...) {}
            inst->frameBuffer.publishFrame();
            inst->lastPaintMs.store(J2meEngineInstance::monoMillis());
            inst->notePaint(paintStart);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Canvas", "setFullScreenMode", "(Z)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Canvas", "isDoubleBuffered", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Canvas", "hasPointerEvents", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Canvas", "hasPointerMotionEvents", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Canvas", "hasRepeatEvents", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1);
    });

    registerNative("javax/microedition/lcdui/Canvas", "getKeyName", "(I)Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        std::string name = "KEY_" + std::to_string(args.size() > 1 ? args[1].i : 0);
        return JavaValue(static_cast<JavaObject*>(vm->allocateString(name)));
    });

    registerNative("javax/microedition/lcdui/Canvas", "getGameAction", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int code = args.size() > 1 ? args[1].i : 0;
        switch (code) {
            case -1: case '2': return JavaValue(1); // UP
            case -2: case '8': return JavaValue(6); // DOWN
            case -3: case '4': return JavaValue(2); // LEFT
            case -4: case '6': return JavaValue(5); // RIGHT
            case -5: case '5': return JavaValue(8); // FIRE
            case '1': return JavaValue(9);          // GAME_A
            case '3': return JavaValue(10);         // GAME_B
            case '7': return JavaValue(11);         // GAME_C
            case '9': return JavaValue(12);         // GAME_D
            default: return JavaValue(0);
        }
    });

    registerNative("javax/microedition/lcdui/Canvas", "getKeyCode", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        int action = args.size() > 1 ? args[1].i : 0;
        switch (action) {
            case 1: return JavaValue(-1); // UP
            case 6: return JavaValue(-2); // DOWN
            case 2: return JavaValue(-3); // LEFT
            case 5: return JavaValue(-4); // RIGHT
            case 8: return JavaValue(-5); // FIRE
            case 9: return JavaValue('1');
            case 10: return JavaValue('3');
            case 11: return JavaValue('7');
            case 12: return JavaValue('9');
            default: return JavaValue(0);
        }
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/game/GameCanvas
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/game/GameCanvas", "<init>", "(Z)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/game/GameCanvas", "getGraphics", "()Ljavax/microedition/lcdui/Graphics;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst) {
            auto g = std::make_shared<j2me::LcduiGraphics>(inst->frameBuffer.getRawDrawBuffer(), inst->frameBuffer.getWidth(), inst->frameBuffer.getHeight());
            return JavaValue(static_cast<JavaObject*>(vm->allocateGraphics(g)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/game/GameCanvas", "flushGraphics", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst && !inst->shouldSkipBackgroundPaint()) {
            inst->frameBuffer.publishFrame();
            inst->lastPaintMs.store(J2meEngineInstance::monoMillis());
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/game/GameCanvas", "flushGraphics", "(IIII)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (inst && !inst->shouldSkipBackgroundPaint()) {
            inst->frameBuffer.publishFrame();
            inst->lastPaintMs.store(J2meEngineInstance::monoMillis());
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/game/GameCanvas", "getKeyStates", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        int mask = 0;
        if (inst) {
            if (inst->isKeyPressed(-1) || inst->isKeyPressed('2')) mask |= (1 << 1); // UP
            if (inst->isKeyPressed(-2) || inst->isKeyPressed('8')) mask |= (1 << 6); // DOWN
            if (inst->isKeyPressed(-3) || inst->isKeyPressed('4')) mask |= (1 << 2); // LEFT
            if (inst->isKeyPressed(-4) || inst->isKeyPressed('6')) mask |= (1 << 5); // RIGHT
            if (inst->isKeyPressed(-5) || inst->isKeyPressed('5')) mask |= (1 << 8); // FIRE
            if (inst->isKeyPressed('1')) mask |= (1 << 9);  // GAME_A
            if (inst->isKeyPressed('3')) mask |= (1 << 10); // GAME_B
            if (inst->isKeyPressed('7')) mask |= (1 << 11); // GAME_C
            if (inst->isKeyPressed('9')) mask |= (1 << 12); // GAME_D
        }
        return JavaValue(mask);
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/Graphics
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/Graphics", "setColor", "(I)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->setColor(0xFF000000 | static_cast<uint32_t>(args[1].i));
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "setColor", "(III)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->setColorRGB(args[1].i, args[2].i, args[3].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getColor", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(static_cast<int32_t>(g->getColor() & 0x00FFFFFF));
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "setGrayScale", "(I)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->setGrayScale(args[1].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getGrayScale", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getGrayScale());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getRedComponent", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getRedComponent());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getGreenComponent", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getGreenComponent());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getBlueComponent", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getBlueComponent());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "setStrokeStyle", "(I)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->setStrokeStyle(args[1].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getStrokeStyle", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getStrokeStyle());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "fillRect", "(IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->fillRect(args[1].i, args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawRect", "(IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawRect(args[1].i, args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawLine", "(IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawLine(args[1].i, args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawRoundRect", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawRoundRect(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "fillRoundRect", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->fillRoundRect(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawArc", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawArc(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "fillArc", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->fillArc(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawTriangle", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawTriangle(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "fillTriangle", "(IIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->fillTriangle(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawString", "(Ljava/lang/String;III)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* s = dynamic_cast<JavaString*>(args[1].ref);
            if (s) g->drawString(s->value, args[2].i, args[3].i, args[4].i);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawSubstring", "(Ljava/lang/String;IIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* s = dynamic_cast<JavaString*>(args[1].ref);
            if (s) g->drawSubstring(s->value, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawChar", "(CIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->drawChar(static_cast<char>(args[1].i), args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawChars", "([CIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
            if (arr && args[2].i >= 0 && args[3].i >= 0 && args[2].i + args[3].i <= arr->length) {
                std::string s;
                for (int32_t i = 0; i < args[3].i; ++i) s += static_cast<char>(arr->elements[args[2].i + i].i);
                g->drawSubstring(s, 0, s.size(), args[4].i, args[5].i, args[6].i);
            }
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawImage", "(Ljavax/microedition/lcdui/Image;III)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* io = dynamic_cast<JavaImageObject*>(args[1].ref);
            if (io && io->image) {
                g->drawImage(io->image.get(), args[2].i, args[3].i, args[4].i);
            }
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawRegion", "(Ljavax/microedition/lcdui/Image;IIIIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* io = dynamic_cast<JavaImageObject*>(args[1].ref);
            if (io && io->image) {
                g->drawRegion(io->image.get(), args[2].i, args[3].i, args[4].i, args[5].i, args[6].i, args[7].i, args[8].i, args[9].i);
            }
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "drawRGB", "([IIIIIIIZ)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
            if (arr) {
                std::vector<uint32_t> buf(arr->length);
                for (int32_t i = 0; i < arr->length; ++i) buf[i] = static_cast<uint32_t>(arr->elements[i].i);
                g->drawRGB(buf.data(), args[2].i, args[3].i, args[4].i, args[5].i, args[6].i, args[7].i, args[8].i != 0);
            }
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "setClip", "(IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->setClip(args[1].i, args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "clipRect", "(IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->clipRect(args[1].i, args[2].i, args[3].i, args[4].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getClipX", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getClipX());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getClipY", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getClipY());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getClipWidth", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getClipWidth());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getClipHeight", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getClipHeight());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "translate", "(II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->translate(args[1].i, args[2].i);
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getTranslateX", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getTranslateX());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "getTranslateY", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) return JavaValue(g->getTranslateY());
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Graphics", "setFont", "(Ljavax/microedition/lcdui/Font;)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* fo = dynamic_cast<JavaFontObject*>(args[1].ref);
            if (fo && fo->font) g->setFont(fo->font);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/lcdui/Graphics", "getFont", "()Ljavax/microedition/lcdui/Font;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto f = g->getFont();
            if (f) return JavaValue(static_cast<JavaObject*>(vm->allocateFont(f)));
        }
        return JavaValue(static_cast<JavaObject*>(vm->allocateFont(j2me::LcduiFont::getDefaultFont())));
    });

    registerNative("javax/microedition/lcdui/Graphics", "copyArea", "(IIIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) g->copyArea(args[1].i, args[2].i, args[3].i, args[4].i, args[5].i, args[6].i, args[7].i);
        return JavaValue();
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/Image
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/Image", "createImage", "(Ljava/lang/String;)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() > 0 && args[0].ref && args[0].ref->isString()) {
            std::string path = static_cast<JavaString*>(args[0].ref)->value;
            auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
            if (inst) {
                std::vector<uint8_t> data;
                if (!inst->jarReader.extractEntry(path, data)) {
                    if (!path.empty() && path[0] == '/') {
                        inst->jarReader.extractEntry(path.substr(1), data);
                    } else {
                        inst->jarReader.extractEntry("/" + path, data);
                    }
                }
                if (data.empty()) vm->throwJava("java/io/IOException", path);
                auto img = j2me::LcduiImage::createImage(data.data(), data.size());
                if (!img) vm->throwJava("java/io/IOException", "Cannot decode image " + path);
                return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
            }
        }
        vm->throwJava("java/lang/NullPointerException");
    });

    registerNative("javax/microedition/lcdui/Image", "createImage", "([BII)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() >= 3 && args[0].ref && args[0].ref->isArray()) {
            auto* arr = static_cast<JavaArray*>(args[0].ref);
            int32_t off = args[1].i;
            int32_t len = args[2].i;
            if (off < 0 || len < 0 || off + len > arr->length) vm->throwJava("java/lang/ArrayIndexOutOfBoundsException");
            std::vector<uint8_t> bytes(len);
            for (int32_t i = 0; i < len; ++i) {
                bytes[i] = static_cast<uint8_t>(arr->elements[off + i].i & 0xFF);
            }
            auto img = j2me::LcduiImage::createImage(bytes.data(), bytes.size());
            if (!img) vm->throwJava("java/lang/IllegalArgumentException", "Cannot decode image data");
            return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
        }
        vm->throwJava("java/lang/NullPointerException");
    });

    registerNative("javax/microedition/lcdui/Image", "createImage", "(II)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        int32_t w = args[0].i;
        int32_t h = args[1].i;
        auto img = j2me::LcduiImage::createImage(w, h);
        return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
    });

    registerNative("javax/microedition/lcdui/Image", "createImage", "(Ljavax/microedition/lcdui/Image;)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* src = dynamic_cast<JavaImageObject*>(args[0].ref);
        if (src && src->image) return JavaValue(static_cast<JavaObject*>(vm->allocateImage(j2me::LcduiImage::createImage(*src->image))));
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/Image", "createImage", "(Ljavax/microedition/lcdui/Image;IIIII)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* src = dynamic_cast<JavaImageObject*>(args[0].ref);
        if (src && src->image) {
            auto img = j2me::LcduiImage::createImage(*src->image, args[1].i, args[2].i, args[3].i, args[4].i, args[5].i);
            if (img) return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/Image", "createRGBImage", "([IIIZ)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* arr = dynamic_cast<JavaArray*>(args[0].ref);
        if (arr) {
            std::vector<uint32_t> buf(arr->length);
            for (int32_t i = 0; i < arr->length; ++i) buf[i] = static_cast<uint32_t>(arr->elements[i].i);
            auto img = j2me::LcduiImage::createRGBImage(buf.data(), args[1].i, args[2].i, args[3].i != 0);
            if (img) return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/Image", "getGraphics", "()Ljavax/microedition/lcdui/Graphics;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* io = dynamic_cast<JavaImageObject*>(args[0].ref);
        if (io && io->image) {
            auto g = io->image->getGraphics();
            if (g) return JavaValue(static_cast<JavaObject*>(vm->allocateGraphics(g)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/lcdui/Image", "getWidth", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* io = dynamic_cast<JavaImageObject*>(args[0].ref);
        return JavaValue(io && io->image ? io->image->getWidth() : 0);
    });

    registerNative("javax/microedition/lcdui/Image", "getHeight", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* io = dynamic_cast<JavaImageObject*>(args[0].ref);
        return JavaValue(io && io->image ? io->image->getHeight() : 0);
    });

    registerNative("javax/microedition/lcdui/Image", "isMutable", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* io = dynamic_cast<JavaImageObject*>(args[0].ref);
        return JavaValue(io && io->image ? (io->image->isMutable() ? 1 : 0) : 0);
    });

    registerNative("javax/microedition/lcdui/Image", "getRGB", "([IIIIIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* io = dynamic_cast<JavaImageObject*>(args[0].ref);
        auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
        if (io && io->image && arr) {
            int off = args[2].i, scan = args[3].i, x = args[4].i, y = args[5].i, w = args[6].i, h = args[7].i;
            std::vector<int32_t> temp(w * h);
            io->image->getRGB(temp.data(), 0, scan > 0 ? scan : w, x, y, w, h);
            for (size_t i = 0; i < temp.size() && off + i < static_cast<size_t>(arr->length); ++i) {
                arr->elements[off + i].i = temp[i];
            }
        }
        return JavaValue();
    });

    // ----------------------------------------------------
    // javax/microedition/lcdui/Font
    // ----------------------------------------------------
    registerNative("javax/microedition/lcdui/Font", "getDefaultFont", "()Ljavax/microedition/lcdui/Font;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateFont(j2me::LcduiFont::getDefaultFont())));
    });

    registerNative("javax/microedition/lcdui/Font", "getFont", "(III)Ljavax/microedition/lcdui/Font;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateFont(j2me::LcduiFont::getFont(args[0].i, args[1].i, args[2].i))));
    });

    registerNative("javax/microedition/lcdui/Font", "getFont", "(I)Ljavax/microedition/lcdui/Font;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        return JavaValue(static_cast<JavaObject*>(vm->allocateFont(j2me::LcduiFont::getFont(args[0].i))));
    });

    registerNative("javax/microedition/lcdui/Font", "getHeight", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->getHeight() : 18);
    });

    registerNative("javax/microedition/lcdui/Font", "stringWidth", "(Ljava/lang/String;)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        auto* s = dynamic_cast<JavaString*>(args[1].ref);
        if (fo && fo->font && s) return JavaValue(fo->font->stringWidth(s->value));
        return JavaValue(s ? static_cast<int32_t>(s->value.size() * 8) : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "charWidth", "(C)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->charWidth(static_cast<char>(args[1].i)) : 8);
    });

    registerNative("javax/microedition/lcdui/Font", "charsWidth", "([CII)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
        if (fo && fo->font && arr && args[2].i >= 0 && args[3].i >= 0 && args[2].i + args[3].i <= arr->length) {
            std::string s;
            for (int32_t i = 0; i < args[3].i; ++i) s += static_cast<char>(arr->elements[args[2].i + i].i);
            return JavaValue(fo->font->stringWidth(s));
        }
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Font", "substringWidth", "(Ljava/lang/String;II)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        auto* s = dynamic_cast<JavaString*>(args[1].ref);
        if (fo && fo->font && s) return JavaValue(fo->font->substringWidth(s->value, args[2].i, args[3].i));
        return JavaValue(0);
    });

    registerNative("javax/microedition/lcdui/Font", "getBaselinePosition", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->getBaselinePosition() : 14);
    });

    registerNative("javax/microedition/lcdui/Font", "getFace", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->getFace() : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "getStyle", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->getStyle() : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "getSize", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font ? fo->font->getSize() : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "isBold", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font && fo->font->isBold() ? 1 : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "isItalic", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font && fo->font->isItalic() ? 1 : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "isPlain", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font && fo->font->isPlain() ? 1 : 0);
    });

    registerNative("javax/microedition/lcdui/Font", "isUnderlined", "()Z", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        auto* fo = dynamic_cast<JavaFontObject*>(args[0].ref);
        return JavaValue(fo && fo->font && fo->font->isUnderlined() ? 1 : 0);
    });

    // ----------------------------------------------------
    // javax/microedition/rms/RecordStore
    // ----------------------------------------------------
    registerNative("javax/microedition/rms/RecordStore", "openRecordStore", "(Ljava/lang/String;Z)Ljavax/microedition/rms/RecordStore;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        std::string storeName = "default";
        if (!args.empty() && args[0].ref && args[0].ref->isString()) {
            storeName = static_cast<JavaString*>(args[0].ref)->value;
        }
        bool createIf = args.size() > 1 ? (args[1].i != 0) : true;
        auto* rms = rmsOf(vm);
        auto* store = rms ? rms->openRecordStore("MIDP_App", storeName, createIf) : nullptr;

        JavaObject* obj = vm->allocateObject(nullptr);
        obj->nativeHandle = store;
        return JavaValue(obj);
    });

    registerNative("javax/microedition/rms/RecordStore", "closeRecordStore", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* rms = rmsOf(vm);
        if (rms && !args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            rms->closeRecordStore(store);
            args[0].ref->nativeHandle = nullptr;
        }
        return JavaValue();
    });

    registerNative("javax/microedition/rms/RecordStore", "deleteRecordStore", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        auto* rms = rmsOf(vm);
        if (rms && !args.empty() && args[0].ref && args[0].ref->isString()) {
            rms->deleteRecordStore("MIDP_App", static_cast<JavaString*>(args[0].ref)->value);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/rms/RecordStore", "listRecordStores", "()[Ljava/lang/String;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* rms = rmsOf(vm);
        auto names = rms ? rms->listRecordStores("MIDP_App") : std::vector<std::string>{};
        if (names.empty()) return JavaValue(static_cast<JavaObject*>(nullptr));
        auto* arr = vm->allocateArray('L', static_cast<int32_t>(names.size()));
        for (size_t i = 0; i < names.size(); ++i) {
            arr->elements[i] = JavaValue(static_cast<JavaObject*>(vm->allocateString(names[i])));
        }
        return JavaValue(static_cast<JavaObject*>(arr));
    });

    registerNative("javax/microedition/rms/RecordStore", "getNumRecords", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            return JavaValue(store->getNumRecords());
        }
        return JavaValue(0);
    });

    registerNative("javax/microedition/rms/RecordStore", "getSize", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            return JavaValue(store->getSize());
        }
        return JavaValue(0);
    });

    registerNative("javax/microedition/rms/RecordStore", "getSizeAvailable", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue(1048576);
    });

    registerNative("javax/microedition/rms/RecordStore", "getVersion", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            return JavaValue(store->getVersion());
        }
        return JavaValue(1);
    });

    registerNative("javax/microedition/rms/RecordStore", "getLastModified", "()J", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            return JavaValue(store->getLastModified());
        }
        return JavaValue(int64_t(0));
    });

    registerNative("javax/microedition/rms/RecordStore", "addRecord", "([BII)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 4 && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            auto* arr = dynamic_cast<JavaArray*>(args[1].ref);
            int32_t off = args[2].i;
            int32_t len = args[3].i;
            if (arr && off >= 0 && len >= 0 && off + len <= arr->length) {
                std::vector<uint8_t> data(len);
                for (int32_t i = 0; i < len; ++i) {
                    data[i] = static_cast<uint8_t>(arr->elements[off + i].i & 0xFF);
                }
                return JavaValue(store->addRecord(data.data(), data.size()));
            }
        }
        return JavaValue(0);
    });

    registerNative("javax/microedition/rms/RecordStore", "getRecord", "(I)[B", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            int32_t recId = args[1].i;
            std::vector<uint8_t> recData;
            if (store->getRecord(recId, recData)) {
                auto* arr = vm->allocateArray('B', static_cast<int32_t>(recData.size()));
                for (size_t i = 0; i < recData.size(); ++i) {
                    arr->elements[i] = JavaValue(static_cast<int32_t>(static_cast<int8_t>(recData[i])));
                }
                return JavaValue(static_cast<JavaObject*>(arr));
            }
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("javax/microedition/rms/RecordStore", "setRecord", "(I[BII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 5 && args[0].ref && args[0].ref->nativeHandle) {
            auto* store = static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle);
            int32_t recId = args[1].i;
            auto* arr = dynamic_cast<JavaArray*>(args[2].ref);
            int32_t off = args[3].i;
            int32_t len = args[4].i;
            if (arr && off >= 0 && len >= 0 && off + len <= arr->length) {
                std::vector<uint8_t> data(len);
                for (int32_t i = 0; i < len; ++i) {
                    data[i] = static_cast<uint8_t>(arr->elements[off + i].i & 0xFF);
                }
                store->setRecord(recId, data.data(), data.size());
            }
        }
        return JavaValue();
    });

    registerNative("javax/microedition/rms/RecordStore", "deleteRecord", "(I)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref && args[0].ref->nativeHandle) {
            static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle)->deleteRecord(args[1].i);
        }
        return JavaValue();
    });

    registerNative("javax/microedition/rms/RecordStore", "getRecordSize", "(I)I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (args.size() >= 2 && args[0].ref && args[0].ref->nativeHandle) {
            return JavaValue(static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle)->getRecordSize(args[1].i));
        }
        return JavaValue(0);
    });

    registerNative("javax/microedition/rms/RecordStore", "getNextRecordID", "()I", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (!args.empty() && args[0].ref && args[0].ref->nativeHandle) {
            return JavaValue(static_cast<j2me::RecordStoreInstance*>(args[0].ref->nativeHandle)->getNextRecordId());
        }
        return JavaValue(1);
    });

    // ----------------------------------------------------
    // OEM DeviceControl & DirectUtils
    // ----------------------------------------------------
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

    registerNative("com/nokia/mid/ui/DeviceControl", "setLights", "(II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });

    registerNative("com/nokia/mid/ui/DirectUtils", "getDirectGraphics", "(Ljavax/microedition/lcdui/Graphics;)Lcom/nokia/mid/ui/DirectGraphics;", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        return args.empty() ? JavaValue() : args[0];
    });

    registerNative("com/nokia/mid/ui/DirectUtils", "createImage", "(IIII)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine* vm, const std::vector<JavaValue>& args) {
        if (args.size() >= 3) {
            auto img = j2me::LcduiImage::createImage(args[0].i, args[1].i, static_cast<uint32_t>(args[2].i));
            return JavaValue(static_cast<JavaObject*>(vm->allocateImage(img)));
        }
        return JavaValue(static_cast<JavaObject*>(nullptr));
    });

    registerNative("com/nokia/mid/ui/DirectGraphics", "drawImage", "(Ljavax/microedition/lcdui/Image;IIII)V", [](CldcVirtualMachine*, const std::vector<JavaValue>& args) {
        if (auto* g = getG(args[0])) {
            auto* io = dynamic_cast<JavaImageObject*>(args[1].ref);
            if (io && io->image) {
                g->drawRegion(io->image.get(), 0, 0, io->image->getWidth(), io->image->getHeight(), args[4].i, args[2].i, args[3].i, 0);
            }
        }
        return JavaValue();
    });

    registerNative("com/nokia/mid/ui/FullCanvas", "<init>", "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) {
        return JavaValue();
    });
    for (const char* cb : {"showNotify", "hideNotify"}) {
        registerNative("com/nokia/mid/ui/FullCanvas", cb, "()V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) { return JavaValue(); });
    }
    registerNative("com/nokia/mid/ui/FullCanvas", "sizeChanged", "(II)V", [](CldcVirtualMachine*, const std::vector<JavaValue>&) { return JavaValue(); });
    registerNative("com/nokia/mid/ui/FullCanvas", "getWidth", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getWidth() : 240);
    });
    registerNative("com/nokia/mid/ui/FullCanvas", "getHeight", "()I", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        return JavaValue(inst ? inst->frameBuffer.getHeight() : 320);
    });
    registerNative("com/nokia/mid/ui/FullCanvas", "repaint", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) inst->requestRepaint();
        return JavaValue();
    });
    registerNative("com/nokia/mid/ui/FullCanvas", "repaint", "(IIII)V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) inst->requestRepaint();
        return JavaValue();
    });
    registerNative("com/nokia/mid/ui/FullCanvas", "serviceRepaints", "()V", [](CldcVirtualMachine* vm, const std::vector<JavaValue>&) {
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (!inst || !inst->currentJavaCanvas || !inst->currentJavaCanvas->clazz) return JavaValue();
        if (inst->painting || inst->shouldSkipBackgroundPaint()) return JavaValue();
        inst->painting = true;
        auto g = std::make_shared<j2me::LcduiGraphics>(inst->frameBuffer.getRawDrawBuffer(), inst->frameBuffer.getWidth(), inst->frameBuffer.getHeight());
        auto* gObj = vm->allocateGraphics(g);
        try {
            vm->executeMethodByName(inst->currentJavaCanvas->clazz->thisClassName, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", {JavaValue(inst->currentJavaCanvas), JavaValue(static_cast<JavaObject*>(gObj))});
        } catch (...) {}
        inst->painting = false;
        inst->frameBuffer.publishFrame();
        inst->lastPaintMs.store(J2meEngineInstance::monoMillis());
        return JavaValue();
    });

    // CLDC core library (registered last: supersedes the minimal Thread/Class/InputStream stubs above)
    registerCldcStdlib(this);
}

} // namespace universal_loader::jvm

