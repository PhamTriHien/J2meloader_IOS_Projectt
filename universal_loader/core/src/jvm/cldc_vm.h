#ifndef J2ME_CLDC_VM_H
#define J2ME_CLDC_VM_H

#include "class_file.h"
#include "jvm_types.h"
#include "../../include/j2me_core.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace j2me {
class LcduiImage;
class LcduiGraphics;
class LcduiFont;
} // namespace j2me

namespace universal_loader::jvm {

class CldcVirtualMachine;

using NativeMethodHandler = std::function<JavaValue(CldcVirtualMachine* vm, const std::vector<JavaValue>& args)>;

class JavaImageObject : public JavaObject {
public:
    std::shared_ptr<j2me::LcduiImage> image;
    explicit JavaImageObject(std::shared_ptr<j2me::LcduiImage> img) : image(std::move(img)) {}
};

class JavaGraphicsObject : public JavaObject {
public:
    std::shared_ptr<j2me::LcduiGraphics> graphics;
    j2me::LcduiGraphics* rawGraphics{nullptr};
    explicit JavaGraphicsObject(std::shared_ptr<j2me::LcduiGraphics> g) : graphics(g), rawGraphics(g.get()) {}
    explicit JavaGraphicsObject(j2me::LcduiGraphics* g) : rawGraphics(g) {}
};

class JavaFontObject : public JavaObject {
public:
    std::shared_ptr<j2me::LcduiFont> font;
    explicit JavaFontObject(std::shared_ptr<j2me::LcduiFont> f) : font(std::move(f)) {}
};

class JavaInputStreamObject : public JavaObject {
public:
    std::vector<uint8_t> data;
    size_t pos{0};
    size_t mark{0};
    explicit JavaInputStreamObject(std::vector<uint8_t> d) : data(std::move(d)), pos(0) {}
};

// State of a java.lang.Class instance
struct JavaClassPayload : NativePayload {
    std::string name; // internal form, e.g. "main/GameMidlet"
};

// A Java exception propagating through native (C++) frames.
class JavaException : public std::runtime_error {
public:
    JavaObject* object;
    // Frames the exception unwound through, innermost first ("pkg/Cls.method(desc) pc=N")
    mutable std::vector<std::string> trace;
    JavaException(JavaObject* obj, const std::string& what) : std::runtime_error(what), object(obj) {}
};

// Thrown to unwind every Java thread when the engine stops. Not a std::exception,
// so neither Java catch handlers nor native catch(std::exception&) swallow it.
struct VmTerminated {};

class J2ME_API CldcVirtualMachine {
public:
    CldcVirtualMachine();
    ~CldcVirtualMachine();

    CldcVirtualMachine(const CldcVirtualMachine&) = delete;
    CldcVirtualMachine& operator=(const CldcVirtualMachine&) = delete;

    bool loadClass(const uint8_t* data, size_t size);
    bool loadClass(std::shared_ptr<JavaClass> clazz);
    std::shared_ptr<JavaClass> findClass(const std::string& className);

    void setUserContext(void* ctx) { m_userContext = ctx; }
    void* getUserContext() const { return m_userContext; }

    void registerNative(const std::string& className, const std::string& methodName, const std::string& descriptor, NativeMethodHandler handler);
    // Value provider for a static field of a system class without bytecode (e.g. System.out)
    void registerNativeStatic(const std::string& className, const std::string& fieldName, NativeMethodHandler handler);
    void registerStandardNatives();

    JavaValue executeMethod(JavaMethod* method, const std::vector<JavaValue>& args);
    JavaValue executeMethodByName(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args);

    JavaObject* allocateObject(JavaClass* clazz);
    JavaArray* allocateArray(char typeCode, int32_t length);
    JavaString* allocateString(const std::string& str);
    JavaImageObject* allocateImage(std::shared_ptr<j2me::LcduiImage> img);
    JavaGraphicsObject* allocateGraphics(std::shared_ptr<j2me::LcduiGraphics> g);
    JavaGraphicsObject* allocateGraphics(j2me::LcduiGraphics* g);
    JavaFontObject* allocateFont(std::shared_ptr<j2me::LcduiFont> f);
    JavaInputStreamObject* allocateInputStream(std::vector<uint8_t> data);

    void gc();
    // Keeps an object alive while referenced only from C++ (caches, native threads)
    void pin(JavaObject* o);
    void unpin(JavaObject* o);
    // Object a native hands out once per VM (e.g. List.SELECT_COMMAND); created on first use and pinned
    JavaObject* nativeSingleton(const std::string& key, const std::function<JavaObject*()>& create);

    // Per-thread GC bookkeeping; frames/args/nativeDepth are only touched with the GIL held
    struct GcThread {
        std::vector<StackFrame*> frames;
        std::vector<const std::vector<JavaValue>*> args;
        int nativeDepth{0};
        bool parked{false};
    };
    GcThread& gcThread();
    // Wraps a native's call back into Java when the native holds no unrooted references,
    // so collection can still run inside the callee (e.g. Thread.run -> Runnable.run)
    class NativeCallout {
    public:
        explicit NativeCallout(CldcVirtualMachine* vm) : m_t(vm->gcThread()), m_dec(m_t.nativeDepth > 0) { if (m_dec) --m_t.nativeDepth; }
        ~NativeCallout() { if (m_dec) ++m_t.nativeDepth; }
        NativeCallout(const NativeCallout&) = delete;
        NativeCallout& operator=(const NativeCallout&) = delete;
    private:
        GcThread& m_t;
        bool m_dec;
    };
    // Roots an argument vector (and optionally marks a native call) for a scope
    class ArgRoot {
    public:
        ArgRoot(CldcVirtualMachine* vm, const std::vector<JavaValue>* args, bool native);
        ~ArgRoot();
        ArgRoot(const ArgRoot&) = delete;
        ArgRoot& operator=(const ArgRoot&) = delete;
    private:
        GcThread& m_t;
        bool m_native;
    };

    // Runs <clinit> (and the superclass's) once, before first active use.
    void ensureInitialized(JavaClass* clazz);
    std::string classNameOf(JavaObject* obj) const;
    bool isInstanceOf(JavaObject* obj, const std::string& targetClass);
    bool isSubclassOf(const std::string& className, const std::string& targetClass);
    [[noreturn]] void throwJava(const std::string& className, const std::string& message = "");
    // True if obj's class hierarchy has a (non-abstract) bytecode implementation of the method
    bool hasBytecodeMethod(JavaObject* obj, const std::string& methodName, const std::string& descriptor);
    // java.lang.Class instance for a class name (in '/' form)
    JavaObject* classObjectFor(const std::string& className);
    // Canonical String instance for a value (string literals and String.intern())
    JavaString* intern(const std::string& value);
    // True if any native method is registered for the class
    bool hasNativeClass(const std::string& className) const;

    // Global interpreter lock: bytecode and natives run on one thread at a time.
    // Re-entrant; held for the duration of every executeMethod/executeMethodByName call.
    class GilScope {
    public:
        explicit GilScope(CldcVirtualMachine* vm);
        ~GilScope();
        GilScope(const GilScope&) = delete;
        GilScope& operator=(const GilScope&) = delete;
    private:
        CldcVirtualMachine* m_vm;
    };
    // Temporarily gives up the GIL around a blocking call (sleep, wait, join).
    class BlockingRegion {
    public:
        explicit BlockingRegion(CldcVirtualMachine* vm);
        ~BlockingRegion();
        BlockingRegion(const BlockingRegion&) = delete;
        BlockingRegion& operator=(const BlockingRegion&) = delete;
    private:
        CldcVirtualMachine* m_vm;
        int m_savedDepth{0};
        bool m_parked{false};
    };

    // Java object monitors (synchronized, Object.wait/notify). Must be called with the GIL held.
    void monitorEnter(JavaObject* obj);
    void monitorExit(JavaObject* obj);
    // timeoutMs <= 0 waits until notified
    void monitorWait(JavaObject* obj, int64_t timeoutMs);
    void monitorNotify(JavaObject* obj);
    // Lets other threads waiting for the GIL run (called on backward branches)
    void safepoint();

    // Engine stop: makes every running Java thread unwind with VmTerminated
    void requestTerminate(bool on) { m_terminate.store(on); }
    bool terminating() const { return m_terminate.load(); }
    void checkTerminate() const { if (m_terminate.load()) throw VmTerminated{}; }
    // Counts native threads running Java code (Thread.start, Timer)
    void threadEnter() { m_liveThreads.fetch_add(1); }
    void threadExit() { m_liveThreads.fetch_sub(1); }
    int liveThreads() const { return m_liveThreads.load(); }

private:
    std::atomic<bool> m_terminate{false};
    std::atomic<int> m_liveThreads{0};
    void gilAcquire();
    void gilRelease();
    void maybeGc();
    void collectGarbage();
    inline static std::atomic<uint64_t> s_vmSerials{1};
    uint64_t m_vmSerial{s_vmSerials.fetch_add(1)};
    std::mutex m_gcThreadsMutex;
    std::vector<std::unique_ptr<GcThread>> m_gcThreads;
    std::mutex m_pinMutex;
    std::unordered_map<JavaObject*, int> m_pinned;
    std::mutex m_singletonMutex;
    std::unordered_map<std::string, JavaObject*> m_nativeSingletons;
    size_t m_gcThreshold{400000};
    // Unreachable objects are destroyed on a helper thread so a collection only pauses for mark + compact
    void queueFree(std::vector<std::unique_ptr<JavaObject>>&& dead);
    std::mutex m_freeMutex;
    std::condition_variable m_freeCv;
    std::vector<std::unique_ptr<JavaObject>> m_freeQueue;
    bool m_freeStop{false};
    std::thread m_freeThread;
    uint32_t m_backBranchTick{0}; // GIL-protected

    std::mutex m_gil;
    std::atomic<std::thread::id> m_gilOwner{};
    int m_gilDepth{0};
    std::atomic<int> m_gilWaiters{0};
    std::atomic<uint64_t> m_gilHandoffs{0};

    // Per call-site inline caches (accessed under the GIL)
    struct CallSite {
        std::string cName, name, desc;
        size_t argCount{0};
        bool isVoid{false};
        bool resolved{false};
        const void* rKey{nullptr};     // receiver JavaClass*, or null for class-less receivers
        std::string rName;             // receiver class name when rKey is null
        JavaMethod* method{nullptr};
        const NativeMethodHandler* native{nullptr};
        JavaClass* initClass{nullptr};
    };
    struct FieldSite {
        std::string cName, name, desc;
        JavaClass* objClass{nullptr};
        long slot{-1};
        bool sResolved{false};         // getstatic / putstatic
        JavaClass* sOwner{nullptr};
        size_t sSlot{0};
    };
    void classifyTrivial(JavaMethod* m);
    std::unordered_map<uint64_t, CallSite> m_callSites;
    std::unordered_map<uint64_t, FieldSite> m_fieldSites;
    bool resolveInvoke(const std::string& className, const std::string& methodName, const std::string& descriptor,
                       JavaObject* receiver, bool virtualDispatch, JavaMethod*& method, const NativeMethodHandler*& native, JavaClass*& initClass);
    const NativeMethodHandler* nativeFor(JavaMethod* m);

    JavaValue invokeMethod(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args, bool virtualDispatch);
    void resolveLayout(JavaClass* clazz);
    // Returns the object field index for an instance field, or -1.
    long findInstanceSlot(JavaClass* start, const std::string& name, const std::string& desc);
    // Finds the class declaring a static field (walking superclasses).
    JavaClass* findStaticFieldOwner(JavaClass* start, const std::string& name, const std::string& desc, JavaField** outField);

    void* m_userContext{nullptr};
    mutable std::mutex m_heapMutex;
    std::mutex m_classObjectsMutex;
    std::unordered_map<std::string, std::shared_ptr<JavaClass>> m_classes;
    std::unordered_map<std::string, NativeMethodHandler> m_nativeMethods;
    std::unordered_map<std::string, NativeMethodHandler> m_nativeStatics;
    std::unordered_map<std::string, JavaObject*> m_classObjects;
    std::mutex m_internMutex;
    std::unordered_map<std::string, JavaString*> m_internPool;
    std::vector<std::unique_ptr<JavaObject>> m_heap;

    static std::string makeNativeKey(const std::string& className, const std::string& methodName, const std::string& descriptor);
};

// Registers the CLDC 1.1 core library (java.lang / java.util / java.io) natives.
void registerCldcStdlib(CldcVirtualMachine* vm);

} // namespace universal_loader::jvm

#endif // J2ME_CLDC_VM_H
