#include "jvm_interpreter.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <sstream>

struct RuntimeState {
    int playerX = 120;
    int playerY = 160;
    int playerSpeed = 4;
    int selectedMenu = 0;
    bool inGame = false;
    int score = 0;
    int hiScore = 12500;
    uint32_t lastKey = 0;
    int lastKeyTimer = 0;
    int touchX = -1;
    int touchY = -1;
    int touchTimer = 0;
    int animTick = 0;
};

static RuntimeState s_runtime;

JvmInterpreter::JvmInterpreter()
    : m_soundEnabled(true), m_running(false), m_paused(false) {
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
    s_runtime = RuntimeState();
    s_runtime.playerX = width / 2;
    s_runtime.playerY = height / 2;

    // Reset JVM bytecode engine
    auto& jvm = JvmBytecodeEngine::getInstance();
    jvm.reset();

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

    if (!targetClass.empty()) {
        std::replace(targetClass.begin(), targetClass.end(), '.', '/');
        m_midletClass = jvm.findOrLoadClass(targetClass, m_jarLoader.get());
        if (m_midletClass) {
            m_midletRef = jvm.allocObject(targetClass);
            // Call <init>()
            jvm.executeMethod(m_midletClass, "<init>", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
            // Call startApp()
            jvm.executeMethod(m_midletClass, "startApp", "()V", { JavaValue(m_midletRef, true) }, m_display.get());
        }
    }
    
    m_running = true;
    m_paused = false;

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
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }
    m_jarLoader->close();
    m_midletClass = nullptr;
    m_canvasClass = nullptr;
    m_midletRef = 0;
    m_canvasRef = 0;
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
            s_runtime.lastKey = ev.codeOrX;
            s_runtime.lastKeyTimer = 30;
            
            if (m_soundEnabled && m_playToneCallback && ev.isDownOrAction) {
                m_playToneCallback(520, 40);
            }

            // Dispatch to active Canvas bytecode if loaded
            if (m_canvasClass && m_canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "keyPressed" : "keyReleased";
                jvm.executeMethod(m_canvasClass, method, "(I)V", { JavaValue(m_canvasRef, true), JavaValue(ev.codeOrX) }, m_display.get());
            }
            
            // Standard MIDP D-pad & numeric navigation
            if (ev.isDownOrAction) {
                if (ev.codeOrX == -1 || ev.codeOrX == 50) { // UP / 2
                    if (s_runtime.inGame) s_runtime.playerY -= s_runtime.playerSpeed;
                    else s_runtime.selectedMenu = (s_runtime.selectedMenu - 1 + 3) % 3;
                }
                else if (ev.codeOrX == -2 || ev.codeOrX == 56) { // DOWN / 8
                    if (s_runtime.inGame) s_runtime.playerY += s_runtime.playerSpeed;
                    else s_runtime.selectedMenu = (s_runtime.selectedMenu + 1) % 3;
                }
                else if (ev.codeOrX == -3 || ev.codeOrX == 52) { // LEFT / 4
                    s_runtime.playerX -= s_runtime.playerSpeed;
                }
                else if (ev.codeOrX == -4 || ev.codeOrX == 54) { // RIGHT / 6
                    s_runtime.playerX += s_runtime.playerSpeed;
                }
                else if (ev.codeOrX == -5 || ev.codeOrX == 53) { // FIRE / OK / 5
                    if (!s_runtime.inGame) {
                        if (s_runtime.selectedMenu == 0) s_runtime.inGame = true;
                    } else {
                        s_runtime.score += 100;
                        if (m_soundEnabled && m_playToneCallback) m_playToneCallback(880, 80);
                    }
                }
                else if (ev.codeOrX == -6) { // LSK
                    s_runtime.inGame = !s_runtime.inGame;
                }
            }
        }
        else if (ev.type == InputEvent::Touch) {
            s_runtime.touchX = ev.codeOrX;
            s_runtime.touchY = ev.extraOrY;
            s_runtime.touchTimer = 20;

            if (m_canvasClass && m_canvasRef != 0) {
                std::string method = ev.isDownOrAction ? "pointerPressed" : "pointerReleased";
                jvm.executeMethod(m_canvasClass, method, "(II)V", { JavaValue(m_canvasRef, true), JavaValue(ev.codeOrX), JavaValue(ev.extraOrY) }, m_display.get());
            }

            if (ev.isDownOrAction && s_runtime.inGame) {
                s_runtime.playerX = ev.codeOrX;
                s_runtime.playerY = ev.extraOrY;
            }
        }
    }
}

void JvmInterpreter::renderEngineFrame(const std::string& title) {
    int w = m_display->getWidth();
    int h = m_display->getHeight();
    s_runtime.animTick++;

    if (!s_runtime.inGame) {
        // === RETRO TITLE / MENU SCREEN ===
        m_display->clear(0xFF0F172A);
        
        m_display->fillRect(0, 0, w, 16, 0xFF020617);
        m_display->drawString("J2ME-Loader", 4, 4, 4 | 16, 0xFF38BDF8);
        m_display->drawString("60 FPS", w - 4, 4, 8 | 16, 0xFF94A3B8);
        
        int bob = (int)(sinf(s_runtime.animTick * 0.08f) * 4.0f);
        m_display->drawRect(8, 26 + bob, w - 16, 44, 0xFF334155);
        m_display->fillRect(9, 27 + bob, w - 18, 42, 0xFF1E293B);
        m_display->drawString(title, w / 2, 40 + bob, 1 | 2, 0xFFFACC15);
        m_display->drawString("Java ME MIDP-2.0 Native Engine", w / 2, 56 + bob, 1 | 2, 0xFF64748B);

        const char* menuItems[3] = { "1. Start Game", "2. High Scores", "3. Options" };
        for (int i = 0; i < 3; ++i) {
            int my = 110 + i * 36;
            bool isSel = (s_runtime.selectedMenu == i);
            if (isSel) {
                m_display->fillRect(16, my - 4, w - 32, 28, 0xFF2563EB);
                m_display->drawString(menuItems[i], w / 2, my + 10, 1 | 2, 0xFFFFFFFF);
            } else {
                m_display->drawRect(16, my - 4, w - 32, 28, 0xFF1E293B);
                m_display->drawString(menuItems[i], w / 2, my + 10, 1 | 2, 0xFF94A3B8);
            }
        }
        
        m_display->drawRect(16, 230, w - 32, 40, 0xFF1E293B);
        m_display->drawString("HI-SCORE: 12500 PTS", w / 2, 244, 1 | 2, 0xFFF59E0B);
        m_display->drawString("Press OK or [5] to Play", w / 2, 258, 1 | 2, 0xFF64748B);

        m_display->fillRect(0, h - 20, w, 20, 0xFF020617);
        m_display->drawString("Select", 8, h - 16, 4 | 16, 0xFF38BDF8);
        m_display->drawString("Exit", w - 8, h - 16, 8 | 16, 0xFFEF4444);
    } else {
        // === IN-GAME ACTIVE CANVAS ===
        m_display->clear(0xFF022C22);
        
        for (int gy = 0; gy < h; gy += 24) m_display->drawLine(0, gy, w, gy, 0xFF064E3B);
        for (int gx = 0; gx < w; gx += 24) m_display->drawLine(gx, 0, gx, h, 0xFF064E3B);

        int px = s_runtime.playerX;
        int py = s_runtime.playerY;
        
        if (px < 16) s_runtime.playerX = 16;
        if (px > w - 16) s_runtime.playerX = w - 16;
        if (py < 28) s_runtime.playerY = 28;
        if (py > h - 36) s_runtime.playerY = h - 36;
        
        m_display->fillRect(px - 10, py - 4, 20, 8, 0xFF10B981);
        m_display->fillRect(px - 4, py - 12, 8, 16, 0xFF34D399);
        m_display->fillRect(px - 2, py - 16, 4, 6, 0xFFFCD34D);
        
        if ((s_runtime.animTick % 4) < 2) {
            m_display->fillRect(px - 3, py + 4, 6, 6, 0xFFEF4444);
        }

        m_display->fillRect(0, 0, w, 18, 0xFF020617);
        m_display->drawString("SCORE: " + std::to_string(s_runtime.score), 6, 5, 4 | 16, 0xFFFBBF24);
        m_display->drawString("LIVES: 3", w - 6, 5, 8 | 16, 0xFF34D399);

        if (s_runtime.touchTimer > 0) {
            s_runtime.touchTimer--;
            m_display->drawRect(s_runtime.touchX - 8, s_runtime.touchY - 8, 16, 16, 0xFF38BDF8);
        }

        m_display->fillRect(0, h - 20, w, 20, 0xFF020617);
        m_display->drawString("Menu", 8, h - 16, 4 | 16, 0xFF38BDF8);
        m_display->drawString("Fire [OK]", w - 8, h - 16, 8 | 16, 0xFFFBBF24);
    }
}

void JvmInterpreter::executionLoop() {
    auto manifest = m_jarLoader->parseManifest();
    std::string appName = manifest["MIDlet-Name"];
    if (appName.empty()) appName = "J2ME Game";

    int frameCount = 0;
    while (m_running) {
        if (!m_paused) {
            processEvents();
            renderEngineFrame(appName);
            frameCount++;

            if (frameCount == 10 && m_soundEnabled && m_playToneCallback) {
                m_playToneCallback(440, 200);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}