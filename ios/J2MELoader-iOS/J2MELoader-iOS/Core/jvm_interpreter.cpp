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

    if (!m_jarLoader->open(jarPath)) {
        std::cerr << "Failed to open JAR archive: " << jarPath << std::endl;
        return false;
    }

    m_display->resize(width, height);

    // Reset JVM bytecode engine and bind active jar
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.reset();
    FullApis::reset();
    jvm.setJarLoader(m_jarLoader.get());

    // Parse Manifest to extract real MIDlet class name
    auto manifest = m_jarLoader->parseManifest();
    std::string midletEntry = manifest["MIDlet-1"];
    std::string targetClass = mainClass;

    if (!midletEntry.empty()) {
        // Format: "Name, /icon.png, com.package.MainClass"
        std::stringstream ss(midletEntry);
        std::string item;
        std::vector<std::string> tokens;
        while (std::getline(ss, item, ',')) {
            size_t first = item.find_first_not_of(' ');
            size_t last = item.find_last_not_of(' ');
            if (first != std::string::npos && last != std::string::npos) {
                tokens.push_back(item.substr(first, last - first + 1));
            }
        }
        if (tokens.size() >= 3) {
            targetClass = tokens[2];
        }
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

void JvmInterpreter::registerRunnable(uint32_t ref, std::shared_ptr<ClassFile> cls) {
    m_runnableRef = ref;
    m_runnableClass = cls;
    startRunnableThread();
}

void JvmInterpreter::startRunnableThread() {
    if (m_runnableRunning) return;
    if (m_runnableClass && m_runnableRef != 0) {
        m_runnableRunning = true;
        if (m_gameThread.joinable()) {
            if (std::this_thread::get_id() != m_gameThread.get_id()) {
                m_gameThread.join();
            } else {
                m_gameThread.detach();
            }
        }
        m_gameThread = std::thread([this]() {
            auto& jvm = JvmBytecodeEngine::getInstance();
            jvm.executeMethod(m_runnableClass, "run", "()V", { JavaValue(m_runnableRef, true) }, m_display.get());
            m_runnableRunning = false;
        });
    }
}

void JvmInterpreter::findAndBindCanvas() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

    if (!m_graphicsRef) {
        m_graphicsRef = jvm.allocObject("javax/microedition/lcdui/Graphics");
    }

    if (m_canvasClass && m_canvasRef != 0) return;

    // Scan all classes in JAR archive to locate Canvas / GameCanvas subclass
    auto entries = m_jarLoader->listEntries();
    for (const auto& entry : entries) {
        if (entry.size() > 6 && entry.substr(entry.size() - 6) == ".class") {
            std::string className = entry.substr(0, entry.size() - 6);
            auto cls = jvm.findOrLoadClass(className, m_jarLoader.get());
            if (cls) {
                // Check if class defines paint(Ljavax/microedition/lcdui/Graphics;)V
                std::string paintKey = "paint:(Ljavax/microedition/lcdui/Graphics;)V";
                if (cls->methods.find(paintKey) != cls->methods.end()) {
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
                    break;
                }
            }
        }
    }
}

void JvmInterpreter::executionLoop() {
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.setJarLoader(m_jarLoader.get());

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

            // Execute real game bytecode paint(Graphics g) method
            if (m_canvasClass && m_canvasRef != 0 && m_graphicsRef != 0) {
                jvm.executeMethod(
                    m_canvasClass,
                    "paint",
                    "(Ljavax/microedition/lcdui/Graphics;)V",
                    { JavaValue(m_canvasRef, true), JavaValue(m_graphicsRef, true) },
                    m_display.get()
                );
            } else {
                // Retro LCD loading splash screen with spinner animation
                tickCount++;
                int w = m_display->getWidth(), h = m_display->getHeight();
                m_display->clear(0xFF0A0F1D);
                
                std::string loadingText = "Dang tai game Java";
                int dots = (tickCount / 15) % 4;
                for (int d = 0; d < dots; ++d) loadingText += ".";
                
                m_display->drawString(loadingText, w / 2, h / 2 - 10, 1 | 2, 0xFF38BDF8);
                m_display->drawString("J2HienLoader", w / 2, h / 2 + 15, 1 | 2, 0xFF94A3B8);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}