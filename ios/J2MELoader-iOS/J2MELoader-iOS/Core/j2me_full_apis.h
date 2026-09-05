#ifndef J2ME_FULL_APIS_H
#define J2ME_FULL_APIS_H

#include <string>
#include <vector>
#include <cstdint>

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
    // Online games: reconnect a closed socket stream once, returns new fd or -1
    static int reconnectSocket(uint32_t streamRef);
};

#endif
