#ifndef J2ME_ENGINE_INSTANCE_H
#define J2ME_ENGINE_INSTANCE_H

#include "../include/j2me_core.h"
#include "lcdui/frame_buffer.h"
#include "lcdui/lcdui_graphics.h"
#include "lcdui/font.h"
#include "jvm/jar_reader.h"
#include "jvm/cldc_vm.h"
#include "app_profile_config.h"
#include "app/app_repository.h"
#include "app/app_installer.h"
#include "storage/rms_storage.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>

struct J2meEngineInstance {
    std::string storageRoot;
    std::string appTitle{"J2ME Application"};
    std::string appVendor{"Unknown"};
    std::string appVersion{"1.0.0"};
    std::string mainClass;

    universal_loader::config::ProfileModel profile;
    universal_loader::config::ConfigDirs   configDirs;
    // Declared before vm: RecordStore objects in the heap point into it
    j2me::RmsManager rms;
    universal_loader::jvm::CldcVirtualMachine vm;

    std::shared_ptr<universal_loader::app::AppRepository> appRepo;
    std::unique_ptr<universal_loader::app::AppInstaller>  appInstaller;

    j2me::JarReader jarReader;
    j2me::FrameBuffer frameBuffer{240, 320};

    std::atomic<bool> isRunning{false};
    std::atomic<bool> isPaused{false};
    std::atomic<int>  fpsLimit{60};
    std::atomic<int>  speedMultiplier{1};

    // Speedhack clock: game time runs speedMultiplier times faster than wall time.
    // Rebased on every multiplier change so it never jumps backwards.
    std::mutex clockMutex;
    int64_t clockRealBase{0};
    int64_t clockVirtBase{0};

    static int64_t realMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    int64_t gameMillis() {
        int mult = speedMultiplier.load();
        if (mult <= 1 && clockRealBase == 0) return realMillis();
        std::lock_guard<std::mutex> l(clockMutex);
        int64_t now = realMillis();
        if (clockRealBase == 0) { clockRealBase = now; clockVirtBase = now; }
        return clockVirtBase + (now - clockRealBase) * (mult < 1 ? 1 : mult);
    }
    void setSpeed(int mult) {
        std::lock_guard<std::mutex> l(clockMutex);
        int64_t now = realMillis();
        int old = speedMultiplier.load();
        if (clockRealBase == 0) { clockRealBase = now; clockVirtBase = now; }
        clockVirtBase += (now - clockRealBase) * (old < 1 ? 1 : old);
        clockRealBase = now;
        speedMultiplier.store(mult);
    }
    // Wall-clock duration for a game-time sleep; keeps at least 1 ms so loops never spin
    int64_t realSleepMs(int64_t ms) {
        int mult = speedMultiplier.load();
        if (mult <= 1 || ms <= 0) return ms;
        return std::max<int64_t>(1, ms / mult);
    }

    std::thread gameThread;
    std::vector<std::thread> workerThreads;
    std::mutex stateMutex;

    // Key states (bitmask or array)
    bool keyStates[256]{false};
    bool specialKeyStates[64]{false};

    bool isKeyPressed(int code) const {
        if (code >= 0 && code < 256) return keyStates[code];
        if (code < 0 && code >= -63) return specialKeyStates[-code];
        return false;
    }

    uint64_t frameCounter{0};
    std::shared_ptr<j2me::LcduiFont> currentFont;

    universal_loader::jvm::JavaObject* currentJavaCanvas{nullptr};
    universal_loader::jvm::JavaObject* currentMidletObject{nullptr};
    // Form / TextBox being shown as a host input dialog (the canvas stays underneath)
    universal_loader::jvm::JavaObject* currentNativeScreen{nullptr};
    std::atomic<int> nativeScreenSerial{0};

    // Canvas paint scheduling (MIDP model): repaint() requests, serviceRepaints() or the
    // engine loop performs it. Only one paint runs at a time so frames are never interleaved.
    std::atomic<bool> repaintPending{true};
    std::atomic<int64_t> repaintRequestedMs{0};
    std::atomic<int64_t> lastPaintMs{0};
    bool painting{false}; // GIL-protected
    static int64_t monoMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    // J2ME_FPSLOG=1: prints canvas paints per second and the average paint time
    int fpsFrames{0};
    double fpsPaintMs{0};
    int64_t fpsWindowStart{0};
    void notePaint(int64_t startMs) {
        static const bool on = std::getenv("J2ME_FPSLOG") != nullptr;
        if (!on) return;
        const int64_t now = monoMillis();
        ++fpsFrames;
        fpsPaintMs += static_cast<double>(now - startMs);
        if (fpsWindowStart == 0) fpsWindowStart = now;
        if (now - fpsWindowStart >= 2000) {
            std::fprintf(stderr, "[FPS] %.1f fps, paint %.1f ms avg\n", fpsFrames * 1000.0 / (now - fpsWindowStart), fpsPaintMs / fpsFrames);
            fpsFrames = 0;
            fpsPaintMs = 0;
            fpsWindowStart = now;
        }
    }
    void requestRepaint() {
        if (!repaintPending.exchange(true)) repaintRequestedMs.store(monoMillis());
    }
};

#endif // J2ME_ENGINE_INSTANCE_H
