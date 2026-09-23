#ifndef J2ME_FULL_APIS_H
#define J2ME_FULL_APIS_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_set>

class LcduiDisplay;
struct JavaValue;

// Full J2ME API dispatcher (MIDP2.0/CLDC1.1 + JSRs + vendor APIs).
// Called as fallback from JvmBytecodeEngine::dispatchNativeMethod.
// Returns true if handled (outResult valid when desc != (...)V).
class FullApis {
public:
    static bool dispatch(const std::string& className,
                         const std::string& methodName,
                         const std::string& desc,
                         const std::vector<JavaValue>& args,
                         JavaValue& outResult,
                         LcduiDisplay* display);
    static void reset();
    // Called from JvmInterpreter key path for high-level screens (Form/List softkeys)
    static void onKey(int keyCode, bool isDown, LcduiDisplay* display);
    static uint32_t currentScreen();
    static void renderCurrentScreen(LcduiDisplay* display);
    // Online games: reconnect a closed socket stream once, returns new fd or -1
    static int reconnectSocket(uint32_t streamRef);
    // Garbage Collector integration: mark, reachability traverse & sweep internal collections
    static void markRoots(std::function<void(uint32_t)> addRoot);
    static void traverseReachable(uint32_t curr, std::function<void(uint32_t)> addRoot);
    static void sweep(const std::unordered_set<uint32_t>& marked);
};

#endif
