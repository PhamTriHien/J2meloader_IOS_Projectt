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

    // Fallback: If targetClass is empty, scan JAR for any class defining startApp:()V
    if (targetClass.empty()) {
        auto entries = m_jarLoader->listEntries();
        for (const auto& entry : entries) {
            if (entry.size() > 6 && entry.substr(entry.size() - 6) == ".class") {
                std::string cname = entry.substr(0, entry.size() - 6);
                auto cls = jvm.findOrLoadClass(cname, m_jarLoader.get());
                if (cls && cls->methods.find("startApp:()V") != cls->methods.end()) {
                    targetClass = cname;
                    break;
                }
            }
        }
    }

    if (targetClass.empty()) {
        std::lock_guard<std::mutex> lk(m_bootMutex);
        m_bootError = "Khong tim thay MIDlet";
    }

    m_runnableRunning = false;
    m_running = true;
    m_paused = false;

    if (!targetClass.empty()) {
        std::replace(targetClass.begin(), targetClass.end(), '.', '/');
    }
    m_targetClass = targetClass;

    // Start background emulation thread
    m_workerThread = std::thread(&JvmInterpreter::executionLoop, this);
    return true;
}

void JvmInterpreter::shutdown() {
    if (m_running) {
        if (m_midletClass && m_midletRef != 0) {
            auto& jvm = JvmBytecodeEngine::getInstance();
            jvm.executeMethod(m_midletClass, "destroyApp", "(Z)V", { JavaValue(m_midletRef, true), JavaValue(1) }, m_display.get());
        }
        m_running = false;
        m_runnableRunning = false;
        // Cooperative stop: running bytecode sees cancel and returns promptly,
        // so the game thread can be joined (no detach onto a reset heap).
        JvmBytecodeEngine::getInstance().requestCancel();
        if (m_gameThread.joinable()) {
            if (std::this_thread::get_id() != m_gameThread.get_id()) {
                m_gameThread.join();
            } else {
                m_gameThread.detach();
            }
        }
        if (m_workerThread.joinable()) {
            if (std::this_thread::get_id() != m_workerThread.get_id()) {
                m_workerThread.join();
            } else {
                m_workerThread.detach();
            }
        }
    }
    m_jarLoader->close();
    m_midletClass = nullptr;
    m_canvasClass = nullptr;
    m_runnableClass = nullptr;
    m_midletRef = 0;
    m_canvasRef = 0;
    m_graphicsRef = 0;
    m_runnableRef = 0;
    m_targetClass.clear();
}

void JvmInterpreter::pause() {
    m_paused = true;
    if (m_midletClass && m_midletRef != 0) {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(m_midletClass, "pauseApp", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
    }
}

void JvmInterpreter::resume() {
    m_paused = false;
    if (m_midletClass && m_midletRef != 0) {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(m_midletClass, "startApp", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
    }
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
            // Dispatch directly to active MIDP Canvas bytecode
            if (m_canvasClass && m_canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "keyPressed" : "keyReleased";
                jvm.executeMethod(m_canvasClass, method, "(I)V", { JavaValue(m_canvasRef, true), JavaValue(ev.codeOrX) }, m_display.get());
            }
        }
        else if (ev.type == InputEvent::Touch) {
            // Dispatch directly to active MIDP Canvas touch bytecode
            if (m_canvasClass && m_canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "pointerPressed" : "pointerReleased";
                jvm.executeMethod(m_canvasClass, method, "(II)V", { JavaValue(m_canvasRef, true), JavaValue(ev.codeOrX), JavaValue(ev.extraOrY) }, m_display.get());
            }
        }
    }
}

void JvmInterpreter::setCurrentCanvas(uint32_t ref, std::shared_ptr<ClassFile> cls) {
    m_canvasRef = ref;
    m_canvasClass = cls;
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
    m_runnableRef = ref;
    m_runnableClass = cls;
    std::thread([this, ref, cls]() {
        auto& jvm = JvmBytecodeEngine::getInstance();
        jvm.executeMethod(cls, "run", "()V", { JavaValue(ref, true) }, m_display.get());
    }).detach();
}

void JvmInterpreter::startRunnableThread() {
    if (m_runnableClass && m_runnableRef != 0) {
        registerRunnable(m_runnableRef, m_runnableClass);
    }
}

void JvmInterpreter::findAndBindCanvas() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    if (!m_graphicsRef) {
        m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
    }

    if (m_canvasClass && m_canvasRef != 0) return;

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
        m_canvasClass = cls;
        m_canvasRef = jvm.allocObject(className);
        jvm.executeMethod(m_canvasClass, "<init>", "()V", { JavaValue(m_canvasRef, true) }, m_display.get());

        // Call showNotify() if defined
        if (cls->methods.find("showNotify:()V") != cls->methods.end()) {
            jvm.executeMethod(cls, "showNotify", "()V", { JavaValue(m_canvasRef, true) }, m_display.get());
        }

        // Check if class implements Runnable
        if (cls->methods.find("run:()V") != cls->methods.end()) {
            m_runnableClass = cls;
            m_runnableRef = m_canvasRef;
            startRunnableThread();
        }
        return;
    }
    if (fallback && !m_canvasClass) {
        // Last resort: instantiate via no-arg even if not declared (may NPE inside,
        // still better than black screen without any splash).
        m_canvasClass = fallback;
        m_canvasRef = jvm.allocObject(fallbackName);
        jvm.executeMethod(m_canvasClass, "<init>", "()V", { JavaValue(m_canvasRef, true) }, m_display.get());
        if (fallback->methods.find("run:()V") != fallback->methods.end()) {
            m_runnableClass = fallback;
            m_runnableRef = m_canvasRef;
            startRunnableThread();
        }
    }
}

void JvmInterpreter::executionLoop() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    if (!m_graphicsRef) {
        m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
    }

    if (!m_targetClass.empty()) {
        m_midletClass = jvm.findOrLoadClass(m_targetClass, m_jarLoader.get());
        if (m_midletClass) {
            m_midletRef = jvm.allocObject(m_targetClass);
            // Call <init>()
            jvm.executeMethod(m_midletClass, "<init>", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
            // Call startApp()
            jvm.executeMethod(m_midletClass, "startApp", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
        }
    }

    findAndBindCanvas();
    startRunnableThread();

    int tickCount = 0;
    while (m_running) {
        if (!m_paused) {
            processEvents();

            if (!m_canvasClass) {
                findAndBindCanvas();
                startRunnableThread();
            }

            if (!m_graphicsRef) {
                m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
            }

            // Execute real game bytecode paint(Graphics g) method
            // (resolved through superclass chain: paint is often inherited)
            if (m_canvasClass && m_canvasRef != 0 && m_graphicsRef != 0) {
                auto paintCls = jvm.resolveMethodClass(
                    m_canvasClass, "paint:(Ljavax/microedition/lcdui/Graphics;)V");
                if (paintCls) {
                    jvm.executeMethod(
                        paintCls,
                        "paint",
                        "(Ljavax/microedition/lcdui/Graphics;)V",
                        { JavaValue(m_canvasRef, true), JavaValue(m_graphicsRef, true) },
                        m_display.get()
                    );
                } else {
                    // Bound class lost its paint (stale bind): drop it so the
                    // loading splash shows instead of a frozen black frame.
                    m_canvasClass = nullptr;
                    m_canvasRef = 0;
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