#ifndef JVM_INTERPRETER_H
#define JVM_INTERPRETER_H

#include "jar_loader.h"
#include "lcdui_display.h"
#include "rms_storage.h"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>

enum class TouchAction {
    Down = 0,
    Move = 1,
    Up = 2
};

struct InputEvent {
    enum Type { Key, Touch } type;
    int32_t codeOrX;
    int32_t extraOrY;
    bool isDownOrAction;
};

class JvmInterpreter {
public:
    static JvmInterpreter& getInstance();

    bool init(const std::string& jarPath, const std::string& mainClass, int width, int height, bool soundEnabled);
    void shutdown();

    void pause();
    void resume();

    void postKeyEvent(int32_t keyCode, bool isDown);
    void postTouchEvent(int32_t x, int32_t y, int32_t action);

    LcduiDisplay* getDisplay() { return m_display.get(); }
    bool isRunning() const { return m_running; }

    void setAudioCallback(std::function<void(const uint8_t*, size_t)> playMidiCb,
                          std::function<void(int, int)> playToneCb) {
        m_playMidiCallback = playMidiCb;
        m_playToneCallback = playToneCb;
    }

private:
    JvmInterpreter();
    ~JvmInterpreter();

    std::string m_jarPath;
    std::string m_mainClass;
    bool m_soundEnabled;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;

    std::unique_ptr<JarLoader> m_jarLoader;
    std::unique_ptr<LcduiDisplay> m_display;
    std::thread m_workerThread;

    std::mutex m_eventMutex;
    std::queue<InputEvent> m_eventQueue;

    std::function<void(const uint8_t*, size_t)> m_playMidiCallback;
    std::function<void(int, int)> m_playToneCallback;

    void executionLoop();
    void processEvents();
    void renderMockSplashScreen(const std::string& title);
};

#endif // JVM_INTERPRETER_H