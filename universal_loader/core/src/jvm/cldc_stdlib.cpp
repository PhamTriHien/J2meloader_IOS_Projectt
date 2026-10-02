#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {

void registerCldcStdlib(CldcVirtualMachine* vm) {
    registerString(vm);
    registerStringBuffer(vm);
    registerBoxes(vm);
    registerLangMisc(vm);
    registerCollections(vm);
    registerIo(vm);
    registerSeCollections(vm);
    registerNio(vm);
    registerSeCrypto(vm);
    registerNet(vm);
    registerHttpScanner(vm);
    registerConnector(vm);
    registerLcduiScreens(vm);
    registerMedia(vm);
}

} // namespace universal_loader::jvm
