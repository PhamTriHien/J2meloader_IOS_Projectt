#include "jvm_interpreter.h"
#include <chrono>
#include <iostream>

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
        std::cerr << "Failed to open JAR: " << jarPath << std::endl;
        return false;
    }

    m_display->resize(width, height);
    m_running = true;
    m_paused = false;

    // Start background emulation thread
    m_workerThread = std::thread(&JvmInterpreter::executionLoop, this);
    return true;
}

void JvmInterpreter::shutdown() {
    if (m_running) {
        m_running = false;
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }
    m_jarLoader->close();
}

void JvmInterpreter::pause() {
    m_paused = true;
}

void JvmInterpreter::resume() {
    m_paused = false;
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
    while (!m_eventQueue.empty()) {
        InputEvent ev = m_eventQueue.front();
        m_eventQueue.pop();
        // Dispatched to MIDlet Canvas keyPressed / keyReleased / pointerPressed / pointerReleased
    }
}

void JvmInterpreter::renderMockSplashScreen(const std::string& title) {
    int w = m_display->getWidth();
    int h = m_display->getHeight();

    m_display->clear(0xFF1E1E2E); // Dark retro background
    
    // Draw phone status bar
    m_display->fillRect(0, 0, w, 16, 0xFF11111B);
    m_display->drawString("J2ME-iOS", 4, 4, 4 | 16, 0xFF89B4FA);
    m_display->drawString("60FPS", w - 4, 4, 8 | 16, 0xFFA6ADC8);

    // Decorative frame
    m_display->drawRect(4, 20, w - 8, h - 40, 0xFF45475A);

    // Title & Info
    m_display->drawString(title, w / 2, h / 2 - 24, 1 | 2, 0xFFF38BA8);
    m_display->drawString("MIDlet Initialized", w / 2, h / 2, 1 | 2, 0xFFA6E3A1);
    m_display->drawString("Ready to Play", w / 2, h / 2 + 20, 1 | 2, 0xFFCDD6F4);

    // Softkey bar
    m_display->fillRect(0, h - 18, w, 18, 0xFF11111B);
    m_display->drawString("Menu", 8, h - 14, 4 | 16, 0xFFF9E2AF);
    m_display->drawString("Exit", w - 8, h - 14, 8 | 16, 0xFFF9E2AF);
}

void JvmInterpreter::executionLoop() {
    auto manifest = m_jarLoader->parseManifest();
    std::string appName = manifest["MIDlet-Name"];
    if (appName.empty()) appName = "J2ME Game";

    int frameCount = 0;
    while (m_running) {
        if (!m_paused) {
            processEvents();

            // Render current frame
            renderMockSplashScreen(appName);
            frameCount++;

            // Play startup beep
            if (frameCount == 10 && m_soundEnabled && m_playToneCallback) {
                m_playToneCallback(440, 200); // 440 Hz A4 tone
            }
        }

        // Cap at ~60 FPS (16.6 ms per frame)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}