#include "jvm_interpreter.h"
#include <chrono>
#include <iostream>
#include <cmath>

struct GameState {
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

static GameState s_state;

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
    s_state = GameState();
    s_state.playerX = width / 2;
    s_state.playerY = height / 2;
    
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
        
        if (ev.type == InputEvent::Key && ev.isDownOrAction) {
            s_state.lastKey = ev.codeOrX;
            s_state.lastKeyTimer = 30; // 30 frames
            
            // Play key tone
            if (m_soundEnabled && m_playToneCallback) {
                m_playToneCallback(520, 40);
            }
            
            // D-Pad / Numbers Navigation
            // UP = -1 or '2' (50)
            if (ev.codeOrX == -1 || ev.codeOrX == 50) {
                if (s_state.inGame) s_state.playerY -= s_state.playerSpeed;
                else s_state.selectedMenu = (s_state.selectedMenu - 1 + 3) % 3;
            }
            // DOWN = -2 or '8' (56)
            else if (ev.codeOrX == -2 || ev.codeOrX == 56) {
                if (s_state.inGame) s_state.playerY += s_state.playerSpeed;
                else s_state.selectedMenu = (s_state.selectedMenu + 1) % 3;
            }
            // LEFT = -3 or '4' (52)
            else if (ev.codeOrX == -3 || ev.codeOrX == 52) {
                s_state.playerX -= s_state.playerSpeed;
            }
            // RIGHT = -4 or '6' (54)
            else if (ev.codeOrX == -4 || ev.codeOrX == 54) {
                s_state.playerX += s_state.playerSpeed;
            }
            // FIRE / OK = -5 or '5' (53)
            else if (ev.codeOrX == -5 || ev.codeOrX == 53) {
                if (!s_state.inGame) {
                    if (s_state.selectedMenu == 0) s_state.inGame = true; // Start Game
                } else {
                    s_state.score += 100;
                    if (m_soundEnabled && m_playToneCallback) {
                        m_playToneCallback(880, 80);
                    }
                }
            }
            // SOFT LEFT = -6
            else if (ev.codeOrX == -6) {
                if (s_state.inGame) s_state.inGame = false; // Return to menu
                else s_state.inGame = true;
            }
        }
        else if (ev.type == InputEvent::Touch) {
            s_state.touchX = ev.codeOrX;
            s_state.touchY = ev.extraOrY;
            s_state.touchTimer = 20;
            if (ev.isDownOrAction && s_state.inGame) {
                s_state.playerX = ev.codeOrX;
                s_state.playerY = ev.extraOrY;
            }
        }
    }
}

void JvmInterpreter::renderMockSplashScreen(const std::string& title) {
    int w = m_display->getWidth();
    int h = m_display->getHeight();
    s_state.animTick++;

    if (!s_state.inGame) {
        // === RETRO TITLE / MENU SCREEN ===
        m_display->clear(0xFF0F172A); // Dark Slate background
        
        // Top Nokia Status Bar
        m_display->fillRect(0, 0, w, 16, 0xFF020617);
        m_display->drawString("J2ME-Loader", 4, 4, 4 | 16, 0xFF38BDF8);
        m_display->drawString("60 FPS", w - 4, 4, 8 | 16, 0xFF94A3B8);
        
        // Animated Title Banner
        int bob = (int)(sinf(s_state.animTick * 0.08f) * 4.0f);
        m_display->drawRect(8, 26 + bob, w - 16, 44, 0xFF334155);
        m_display->fillRect(9, 27 + bob, w - 18, 42, 0xFF1E293B);
        m_display->drawString(title, w / 2, 40 + bob, 1 | 2, 0xFFFACC15);
        m_display->drawString("Java ME MIDP-2.0", w / 2, 56 + bob, 1 | 2, 0xFF64748B);

        // Menu Items
        const char* menuItems[3] = { "1. Start Game", "2. High Scores", "3. Options" };
        for (int i = 0; i < 3; ++i) {
            int my = 110 + i * 36;
            bool isSel = (s_state.selectedMenu == i);
            if (isSel) {
                m_display->fillRect(16, my - 4, w - 32, 28, 0xFF2563EB);
                m_display->drawString(menuItems[i], w / 2, my + 10, 1 | 2, 0xFFFFFFFF);
            } else {
                m_display->drawRect(16, my - 4, w - 32, 28, 0xFF1E293B);
                m_display->drawString(menuItems[i], w / 2, my + 10, 1 | 2, 0xFF94A3B8);
            }
        }
        
        // High Score / Info Box
        m_display->drawRect(16, 230, w - 32, 40, 0xFF1E293B);
        m_display->drawString("HI-SCORE: 12500 PTS", w / 2, 244, 1 | 2, 0xFFF59E0B);
        m_display->drawString("Press OK or [5] to Play", w / 2, 258, 1 | 2, 0xFF64748B);

        // Bottom Softkey bar
        m_display->fillRect(0, h - 20, w, 20, 0xFF020617);
        m_display->drawString("Select", 8, h - 16, 4 | 16, 0xFF38BDF8);
        m_display->drawString("Exit", w - 8, h - 16, 8 | 16, 0xFFEF4444);
    } else {
        // === IN-GAME ACTIVE CANVAS ===
        m_display->clear(0xFF022C22); // Retro Game Green
        
        // Starfield / grid background
        for (int gy = 0; gy < h; gy += 24) {
            m_display->drawLine(0, gy, w, gy, 0xFF064E3B);
        }
        for (int gx = 0; gx < w; gx += 24) {
            m_display->drawLine(gx, 0, gx, h, 0xFF064E3B);
        }

        // Animated Player Sprite (Ship / Character)
        int px = s_state.playerX;
        int py = s_state.playerY;
        
        // Clamp player in screen
        if (px < 16) s_state.playerX = 16;
        if (px > w - 16) s_state.playerX = w - 16;
        if (py < 28) s_state.playerY = 28;
        if (py > h - 36) s_state.playerY = h - 36;
        
        // Draw Player Ship (Triangle Polygon)
        m_display->fillRect(px - 10, py - 4, 20, 8, 0xFF10B981);
        m_display->fillRect(px - 4, py - 12, 8, 16, 0xFF34D399);
        m_display->fillRect(px - 2, py - 16, 4, 6, 0xFFFCD34D);
        
        // Engine Thruster Flame
        if ((s_state.animTick % 4) < 2) {
            m_display->fillRect(px - 3, py + 4, 6, 6, 0xFFEF4444);
        }

        // Top Game HUD
        m_display->fillRect(0, 0, w, 18, 0xFF020617);
        m_display->drawString("SCORE: " + std::to_string(s_state.score), 6, 5, 4 | 16, 0xFFFBBF24);
        m_display->drawString("LIVES: 3", w - 6, 5, 8 | 16, 0xFF34D399);

        // Touch Cursor feedback
        if (s_state.touchTimer > 0) {
            s_state.touchTimer--;
            m_display->drawRect(s_state.touchX - 8, s_state.touchY - 8, 16, 16, 0xFF38BDF8);
        }

        // Bottom In-Game Softkey Bar
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

            // Render interactive gameplay
            renderMockSplashScreen(appName);
            frameCount++;

            // Play startup beep
            if (frameCount == 10 && m_soundEnabled && m_playToneCallback) {
                m_playToneCallback(440, 200); // 440 Hz A4 startup chime
            }
        }

        // Cap at ~60 FPS (16.6 ms per frame)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}