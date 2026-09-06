#ifndef JVM_INTERPRETER_H
#define JVM_INTERPRETER_H

#include "jar_loader.h"
#include "lcdui_display.h"
#include "rms_storage.h"
#include "jvm_bytecode.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
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
    // Diagnostics for the loading overlay: "error:<msg>" | "running" | "loading".
    std::string getBootStatus();
    int getPaintTick() const { return m_paintTick.load(); }

    void setAudioCallback(std::function<void(const uint8_t*, size_t)> playMidiCb,
                          std::function<void(int, int)> playToneCb) {
        m_playMidiCallback = playMidiCb;
        m_playToneCallback = playToneCb;
    }

    void setCurrentCanvas(uint32_t ref, std::shared_ptr<ClassFile> cls);

    void registerRunnable(uint32_t ref, std::shared_ptr<ClassFile> cls);
    void startRunnableThread();

    void triggerTone(int freq, int durationMs, int volume) {
        if (m_soundEnabled && m_playToneCallback && freq > 0 && durationMs > 0) {
            m_playToneCallback(freq, durationMs);
        }
    }
    void triggerMidi(const uint8_t* data, size_t size) {
        if (m_soundEnabled && m_playMidiCallback && data && size > 0) {
            m_playMidiCallback(data, size);
        }
    }

private:
    JvmInterpreter();
    ~JvmInterpreter();

    std::string m_jarPath;
    std::string m_mainClass;
    std::string m_targetClass;
    std::string m_bootError;
    std::mutex m_bootMutex;
    bool m_soundEnabled;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_runnableRunning;
    std::atomic<int> m_paintTick{0};

    std::unique_ptr<JarLoader> m_jarLoader;
    std::unique_ptr<LcduiDisplay> m_display;
    std::thread m_workerThread;
    std::thread m_gameThread;
    std::thread m_initThread;
    // Guards m_targetClass/m_midlet*/m_canvas*/m_runnable* across the worker,
    // init and game threads. Never hold while calling into the engine.
    std::mutex m_stateMutex;
    // Bumped on every init/shutdown so a late init thread discards its work
    // instead of binding a canvas into a newer (or dead) session.
    std::atomic<unsigned long> m_generation{0};

    std::mutex m_eventMutex;
    std::queue<InputEvent> m_eventQueue;

    std::function<void(const uint8_t*, size_t)> m_playMidiCallback;
    std::function<void(int, int)> m_playToneCallback;

    uint32_t m_midletRef = 0;
    uint32_t m_canvasRef = 0;
    uint32_t m_graphicsRef = 0;
    uint32_t m_runnableRef = 0;
    std::shared_ptr<ClassFile> m_midletClass;
    std::shared_ptr<ClassFile> m_canvasClass;
    std::shared_ptr<ClassFile> m_runnableClass;

    void executionLoop();
    void processEvents();
    void findAndBindCanvas();
    // Resolves targetClass (incl. startApp fallback scan), runs <init>/startApp.
    // Runs off the worker thread so slow JAR scans / blocking network connects
    // in startApp never freeze the UI or the paint loop (splash stays visible).
    void midletInitRoutine(unsigned long gen);
    void drawBootSplash(const std::string& line1);
};

#endif // JVM_INTERPRETER_H