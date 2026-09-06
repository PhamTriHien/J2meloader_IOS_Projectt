#include "jvm_interpreter.h"
#include "j2me_full_apis.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <sstream>

JvmInterpreter::JvmInterpreter()
    : m_soundEnabled(true), m_running(false), m_paused(false), m_runnableRunning(false) {
    m_display = std::make_unique<LcduiDisplay>(240, 320);
    m_jarLoader = std::make_unique<JarLoader>();
}

JvmInterpreter::~JvmInterpreter() {
    shutdown();
}

JvmInterpreter& JvmInterpreter::getInstance() {
    static JvmInterpreter instance;
    return instance;
}

bool JvmInterpreter::init(const std::string& jarPath, const std::string& mainClass, int width, int height, bool soundEnabled) {
    shutdown();

    m_jarPath = jarPath;
    m_mainClass = mainClass;
    m_soundEnabled = soundEnabled;
    { std::lock_guard<std::mutex> lk(m_bootMutex); m_bootError.clear(); }

    m_display->resize(width, height);

    if (!m_jarLoader->open(jarPath)) {
        std::cerr << "Failed to open JAR archive: " << jarPath << std::endl;
        m_display->clear(0xFF0A0F1D);
        m_display->drawString("Khong mo duoc JAR", m_display->getWidth() / 2,
                              m_display->getHeight() / 2, 1 | 2, 0xFFF87171);
        { std::lock_guard<std::mutex> lk(m_bootMutex); m_bootError = "Khong mo duoc JAR"; }
        return false;
    }

    // Reset JVM bytecode engine and bind active jar
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.reset();
    FullApis::reset();
    jvm.setJarLoader(m_jarLoader.get());

    // Parse Manifest to extract real MIDlet class name
    auto manifest = m_jarLoader->parseManifest();
    std::string targetClass = mainClass;

    std::string midletEntry = "";
    for (const auto& kv : manifest) {
        std::string k = kv.first;
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        if (k == "midlet-1" || k == "midlet-1:" || k.find("midlet-1") != std::string::npos) {
            midletEntry = kv.second;
            break;
        }
    }

    if (!midletEntry.empty()) {
        // Format: "Name, /icon.png, com.package.MainClass" or "Name, com.package.MainClass" or "com.package.MainClass"
        std::stringstream ss(midletEntry);
        std::string item;
        std::vector<std::string> tokens;
        while (std::getline(ss, item, ',')) {
            size_t first = item.find_first_not_of(" \t\r\n");
            size_t last = item.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                tokens.push_back(item.substr(first, last - first + 1));
            }
        }
        if (tokens.size() >= 3) {
            targetClass = tokens[2];
        } else if (tokens.size() == 2) {
            targetClass = tokens[1];
        } else if (tokens.size() == 1) {
            targetClass = tokens[0];
        }
    }

    // NOTE: the startApp fallback scan (loads every class in the JAR) moved to
    // midletInitRoutine() so big JARs never freeze the UI thread here.
    {
        // Reap any leftover init thread under lock (same handoff as shutdown).
        std::thread leftover;
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            if (m_initThread.joinable()) leftover = std::move(m_initThread);
        }
        if (leftover.joinable()) leftover.join();
    }

    if (targetClass.empty()) {
        std::lock_guard<std::mutex> lk(m_bootMutex);
        m_bootError = "Dang tim MIDlet...";
    }

    m_runnableRunning = false;
    m_running = true;
    m_paused = false;

    if (!targetClass.empty()) {
        std::replace(targetClass.begin(), targetClass.end(), '.', '/');
    }
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_targetClass = targetClass;
    }
    ++m_generation;

    // Start background emulation thread
    m_workerThread = std::thread(&JvmInterpreter::executionLoop, this);
    return true;
}

void JvmInterpreter::shutdown() {
    // Move threads out under lock so a concurrently starting worker can never
    // race on the std::thread objects (assigning a joinable thread terminates).
    auto takeThread = [this](std::thread& t) -> std::thread {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (t.joinable()) return std::move(t);
        return std::thread();
    };
    auto joinGuarded = [](std::thread& t) {
        if (t.joinable()) {
            if (std::this_thread::get_id() != t.get_id()) {
                t.join();
            } else {
                t.detach();
            }
        }
    };
    if (m_running) {
        std::shared_ptr<ClassFile> midletCls;
        uint32_t midletRef = 0;
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            midletCls = m_midletClass;
            midletRef = m_midletRef;
        }
        if (midletCls && midletRef != 0) {
            auto& jvm = JvmBytecodeEngine::getInstance();
            jvm.executeMethod(midletCls, "destroyApp", "(Z)V", { JavaValue(midletRef, true), JavaValue(1) }, m_display.get());
        }
        m_running = false;
        m_runnableRunning = false;
        // Invalidate any in-flight init thread so its late results are dropped.
        ++m_generation;
        // Cooperative stop: running bytecode sees cancel and returns promptly,
        // so threads can be joined (no detach onto a reset heap).
        JvmBytecodeEngine::getInstance().requestCancel();
        std::thread initT = takeThread(m_initThread);
        std::thread gameT = takeThread(m_gameThread);
        std::thread workerT = takeThread(m_workerThread);
        joinGuarded(initT);
        joinGuarded(gameT);
        joinGuarded(workerT);
    } else {
        // Session never started: still reap a stray init thread if present.
        std::thread initT = takeThread(m_initThread);
        joinGuarded(initT);
    }
    m_jarLoader->close();
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_midletClass = nullptr;
        m_canvasClass = nullptr;
        m_runnableClass = nullptr;
        m_midletRef = 0;
        m_canvasRef = 0;
        m_graphicsRef = 0;
        m_runnableRef = 0;
        m_targetClass.clear();
    }
}

void JvmInterpreter::pause() {
    m_paused = true;
    std::shared_ptr<ClassFile> cls;
    uint32_t ref = 0;
    { std::lock_guard<std::mutex> lk(m_stateMutex); cls = m_midletClass; ref = m_midletRef; }
    if (cls && ref != 0) {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(cls, "pauseApp", "()V", { JavaValue(ref, true) }, m_display.get());
    }
}

void JvmInterpreter::resume() {
    m_paused = false;
    std::shared_ptr<ClassFile> cls;
    uint32_t ref = 0;
    { std::lock_guard<std::mutex> lk(m_stateMutex); cls = m_midletClass; ref = m_midletRef; }
    if (cls && ref != 0) {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(cls, "startApp", "()V", { JavaValue(ref, true) }, m_display.get());
    }
}

std::string JvmInterpreter::getBootStatus() {
    {
        std::lock_guard<std::mutex> lk(m_bootMutex);
        if (!m_bootError.empty()) return "error:" + m_bootError;
    }
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (m_canvasClass && m_canvasRef != 0) return "running";
    }
    return "loading";
}

void JvmInterpreter::postKeyEvent(int32_t keyCode, bool isDown) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    InputEvent ev;
    ev.type = InputEvent::Key;
    ev.codeOrX = keyCode;
    ev.extraOrY = 0;
    ev.isDownOrAction = isDown;
    m_eventQueue.push(ev);
}

void JvmInterpreter::postTouchEvent(int32_t x, int32_t y, int32_t action) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    InputEvent ev;
    ev.type = InputEvent::Touch;
    ev.codeOrX = x;
    ev.extraOrY = y;
    ev.isDownOrAction = (action == 0 || action == 1);
    m_eventQueue.push(ev);
}

void JvmInterpreter::processEvents() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    auto& jvm = JvmBytecodeEngine::getInstance();

    while (!m_eventQueue.empty()) {
        InputEvent ev = m_eventQueue.front();
        m_eventQueue.pop();
        
        if (ev.type == InputEvent::Key) {
            if (m_soundEnabled && m_playToneCallback && ev.isDownOrAction) {
                m_playToneCallback(520, 30);
            }

            // High-level Form/List softkey -> CommandListener
            FullApis::onKey(ev.codeOrX, ev.isDownOrAction, m_display.get());
            // Snapshot canvas under lock; execute outside it (engine calls back
            // into setCurrentCanvas which takes the same mutex).
            std::shared_ptr<ClassFile> canvasCls;
            uint32_t canvasRef = 0;
            { std::lock_guard<std::mutex> slk(m_stateMutex); canvasCls = m_canvasClass; canvasRef = m_canvasRef; }
            // Dispatch directly to active MIDP Canvas bytecode
            if (canvasCls && canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "keyPressed" : "keyReleased";
                jvm.executeMethod(canvasCls, method, "(I)V", { JavaValue(canvasRef, true), JavaValue(ev.codeOrX) }, m_display.get());
            }
        }
        else if (ev.type == InputEvent::Touch) {
            std::shared_ptr<ClassFile> canvasCls;
            uint32_t canvasRef = 0;
            { std::lock_guard<std::mutex> slk(m_stateMutex); canvasCls = m_canvasClass; canvasRef = m_canvasRef; }
            // Dispatch directly to active MIDP Canvas touch bytecode
            if (canvasCls && canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "pointerPressed" : "pointerReleased";
                jvm.executeMethod(canvasCls, method, "(II)V", { JavaValue(canvasRef, true), JavaValue(ev.codeOrX), JavaValue(ev.extraOrY) }, m_display.get());
            }
        }
    }
}

void JvmInterpreter::setCurrentCanvas(uint32_t ref, std::shared_ptr<ClassFile> cls) {
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_canvasRef = ref;
        m_canvasClass = cls;
    }
    if (cls && ref != 0) {
        auto& jvm = JvmBytecodeEngine::getInstance();
        // J2ME spec: showNotify() MUST be called when canvas is made current
        std::string showKey = "showNotify:()V";
        if (cls->methods.find(showKey) != cls->methods.end()) {
            jvm.executeMethod(cls, "showNotify", "()V", { JavaValue(ref, true) }, m_display.get());
        }
        // If the canvas itself implements Runnable, start its game loop thread
        std::string runKey = "run:()V";
        if (cls->methods.find(runKey) != cls->methods.end()) {
            registerRunnable(ref, cls);
        }
    }
}

void JvmInterpreter::registerRunnable(uint32_t ref, std::shared_ptr<ClassFile> cls) {
    if (!cls || ref == 0) return;
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_runnableRef = ref;
        m_runnableClass = cls;
    }
    std::thread([this, ref, cls]() {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(cls, "run", "()V", { JavaValue(ref, true) }, m_display.get());
    }).detach();
}

void JvmInterpreter::startRunnableThread() {
    std::shared_ptr<ClassFile> cls;
    uint32_t ref = 0;
    { std::lock_guard<std::mutex> lk(m_stateMutex); cls = m_runnableClass; ref = m_runnableRef; }
    if (cls && ref != 0) {
        registerRunnable(ref, cls);
    }
}

void JvmInterpreter::findAndBindCanvas() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    if (!m_graphicsRef) {
        m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
    }

    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (m_canvasClass && m_canvasRef != 0) return;
    }

    // Scan all classes in JAR archive to locate Canvas / GameCanvas subclass.
    // Must skip abstract bases/interfaces (their paint has no Code and would
    // render black forever) and prefer classes with a no-arg constructor.
    static const std::string paintKey = "paint:(Ljavax/microedition/lcdui/Graphics;)V";
    // paint may be inherited: walk superclass chain for real Code.
    auto paintWithCode = [&](std::shared_ptr<ClassFile> c) -> bool {
        for (int d = 0; d < 8 && c; ++d) {
            auto pit = c->methods.find(paintKey);
            if (pit != c->methods.end() && !pit->second.code.empty()) return true;
            if (c->superClassName.empty()) break;
            c = jvm.findOrLoadClass(c->superClassName, m_jarLoader.get());
        }
        return false;
    };
    auto entries = m_jarLoader->listEntries();
    std::shared_ptr<ClassFile> fallback;
    std::string fallbackName;
    for (const auto& entry : entries) {
        if (entry.size() <= 6 || entry.substr(entry.size() - 6) != ".class") continue;
        std::string className = entry.substr(0, entry.size() - 6);
        auto cls = jvm.findOrLoadClass(className, m_jarLoader.get());
        if (!cls) continue;
        if (!paintWithCode(cls)) continue;
        if (cls->accessFlags & 0x0400) continue; // ACC_ABSTRACT
        if (cls->accessFlags & 0x0200) continue; // ACC_INTERFACE
        bool hasNoArgInit = cls->methods.find("<init>:()V") != cls->methods.end();
        if (!hasNoArgInit) {
            // Constructor needs args (e.g. Canvas(MIDlet)): only use if nothing better.
            if (!fallback) { fallback = cls; fallbackName = className; }
            continue;
        }
        // Publish under lock only; another thread may have bound meanwhile.
        uint32_t newRef = 0;
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            if (m_canvasClass && m_canvasRef != 0) return;
            m_canvasClass = cls;
            newRef = m_canvasRef = jvm.allocObject(className);
        }
        jvm.executeMethod(cls, "<init>", "()V", { JavaValue(newRef, true) }, m_display.get());

        // Call showNotify() if defined
        if (cls->methods.find("showNotify:()V") != cls->methods.end()) {
            jvm.executeMethod(cls, "showNotify", "()V", { JavaValue(newRef, true) }, m_display.get());
        }

        // Check if class implements Runnable
        if (cls->methods.find("run:()V") != cls->methods.end()) {
            registerRunnable(newRef, cls);
        }
        return;
    }
    if (fallback) {
        // Last resort: instantiate via no-arg even if not declared (may NPE inside,
        // still better than black screen without any splash).
        uint32_t newRef = 0;
        {
            std::lock_guard<std::mutex> lk(m_stateMutex);
            if (m_canvasClass && m_canvasRef != 0) return;
            m_canvasClass = fallback;
            newRef = m_canvasRef = jvm.allocObject(fallbackName);
        }
        jvm.executeMethod(fallback, "<init>", "()V", { JavaValue(newRef, true) }, m_display.get());
        if (fallback->methods.find("run:()V") != fallback->methods.end()) {
            registerRunnable(newRef, fallback);
        }
    }
}

void JvmInterpreter::drawBootSplash(const std::string& line1) {
    int w = m_display->getWidth(), h = m_display->getHeight();
    m_display->clear(0xFF0A0F1D);
    m_display->drawString(line1, w / 2, h / 2 - 10, 1 | 2, 0xFF38BDF8);
    m_display->drawString("J2HienLoader", w / 2, h / 2 + 15, 1 | 2, 0xFF94A3B8);
}

void JvmInterpreter::midletInitRoutine(unsigned long gen) {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    auto alive = [&]() -> bool {
        return gen == m_generation.load() && m_running.load();
    };

    std::string target;
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        target = m_targetClass;
    }
    if (target.empty() && alive()) {
        // Fallback scan (loads every class): was freezing the UI thread before.
        auto entries = m_jarLoader->listEntries();
        for (const auto& entry : entries) {
            if (!alive()) return;
            if (entry.size() <= 6 || entry.substr(entry.size() - 6) != ".class") continue;
            std::string cname = entry.substr(0, entry.size() - 6);
            auto cls = jvm.findOrLoadClass(cname, m_jarLoader.get());
            if (cls && cls->methods.find("startApp:()V") != cls->methods.end()) {
                target = cname;
                break;
            }
        }
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (gen == m_generation.load() && m_targetClass.empty()) m_targetClass = target;
    }

    if (target.empty()) {
        if (alive()) {
            std::lock_guard<std::mutex> lk(m_bootMutex);
            m_bootError = "Khong tim thay MIDlet";
        }
        return;
    }
    if (!alive()) return;
    auto cls = jvm.findOrLoadClass(target, m_jarLoader.get());
    if (!cls) {
        if (alive()) {
            std::lock_guard<std::mutex> lk(m_bootMutex);
            m_bootError = "Khong nap duoc MIDlet";
        }
        return;
    }
    uint32_t ref = jvm.allocObject(target);
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (gen != m_generation.load() || !m_running.load()) return;
        m_midletClass = cls;
        m_midletRef = ref;
    }
    // <init>() then startApp(). May block on network for online games — the
    // paint loop below keeps running the splash meanwhile.
    jvm.executeMethod(cls, "<init>", "()V", { JavaValue(ref, true) }, m_display.get());
    if (gen != m_generation.load() || !m_running.load()) return;
    jvm.executeMethod(cls, "startApp", "()V", { JavaValue(ref, true) }, m_display.get());
}

void JvmInterpreter::executionLoop() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    if (!m_graphicsRef) {
        m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
    }

    // Paint one splash frame immediately: the user never stares at a black
    // screen while startApp() blocks (e.g. online games connecting).
    drawBootSplash("Dang tai game Java");

    // MIDlet lifecycle runs on its own thread; the loop below keeps painting.
    // Published under lock: shutdown() may concurrently move it out to join.
    unsigned long gen = m_generation.load();
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        if (!m_running.load()) return;
        m_initThread = std::thread(&JvmInterpreter::midletInitRoutine, this, gen);
    }

    findAndBindCanvas();
    startRunnableThread();

    int tickCount = 0;
    while (m_running) {
        if (!m_paused) {
            processEvents();

            bool haveCanvas = false;
            {
                std::lock_guard<std::mutex> lk(m_stateMutex);
                haveCanvas = (m_canvasClass && m_canvasRef != 0);
            }
            if (!haveCanvas) {
                findAndBindCanvas();
                startRunnableThread();
            }

            if (!m_graphicsRef) {
                m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
            }

            // Execute real game bytecode paint(Graphics g) method
            // (resolved through superclass chain: paint is often inherited)
            std::shared_ptr<ClassFile> canvasCls;
            uint32_t canvasRef = 0;
            {
                std::lock_guard<std::mutex> lk(m_stateMutex);
                canvasCls = m_canvasClass;
                canvasRef = m_canvasRef;
            }
            if (canvasCls && canvasRef != 0 && m_graphicsRef != 0) {
                auto paintCls = jvm.resolveMethodClass(
                    canvasCls, "paint:(Ljavax/microedition/lcdui/Graphics;)V");
                if (paintCls) {
                    jvm.executeMethod(
                        paintCls,
                        "paint",
                        "(Ljavax/microedition/lcdui/Graphics;)V",
                        { JavaValue(canvasRef, true), JavaValue(m_graphicsRef, true) },
                        m_display.get()
                    );
                    ++m_paintTick;
                } else {
                    // Bound class lost its paint (stale bind): drop it so the
                    // loading splash shows instead of a frozen black frame.
                    std::lock_guard<std::mutex> lk(m_stateMutex);
                    if (m_canvasRef == canvasRef) {
                        m_canvasClass = nullptr;
                        m_canvasRef = 0;
                    }
                }
            } else {
                // Retro LCD loading splash screen with spinner animation.
                // A boot error (JAR/MIDlet) is shown in red instead of hanging black.
                std::string bootErr;
                { std::lock_guard<std::mutex> lk(m_bootMutex); bootErr = m_bootError; }
                tickCount++;
                int w = m_display->getWidth(), h = m_display->getHeight();
                m_display->clear(0xFF0A0F1D);

                if (!bootErr.empty()) {
                    m_display->drawString(bootErr, w / 2, h / 2 - 10, 1 | 2, 0xFFF87171);
                    m_display->drawString("Kiem tra file JAR", w / 2, h / 2 + 15, 1 | 2, 0xFF94A3B8);
                } else {
                    std::string loadingText = "Dang tai game Java";
                    int dots = (tickCount / 15) % 4;
                    for (int d = 0; d < dots; ++d) loadingText += ".";

                    m_display->drawString(loadingText, w / 2, h / 2 - 10, 1 | 2, 0xFF38BDF8);
                    m_display->drawString("J2HienLoader", w / 2, h / 2 + 15, 1 | 2, 0xFF94A3B8);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}