#ifndef J2ME_CLDC_VM_H
#define J2ME_CLDC_VM_H

#include "class_file.h"
#include "jvm_types.h"
#include "../../include/j2me_core.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace universal_loader::jvm {

class CldcVirtualMachine;

using NativeMethodHandler = std::function<JavaValue(CldcVirtualMachine* vm, const std::vector<JavaValue>& args)>;

class J2ME_API CldcVirtualMachine {
public:
    CldcVirtualMachine();
    ~CldcVirtualMachine();

    CldcVirtualMachine(const CldcVirtualMachine&) = delete;
    CldcVirtualMachine& operator=(const CldcVirtualMachine&) = delete;

    bool loadClass(const uint8_t* data, size_t size);
    bool loadClass(std::shared_ptr<JavaClass> clazz);
    std::shared_ptr<JavaClass> findClass(const std::string& className);

    void registerNative(const std::string& className, const std::string& methodName, const std::string& descriptor, NativeMethodHandler handler);
    void registerStandardNatives();

    JavaValue executeMethod(JavaMethod* method, const std::vector<JavaValue>& args);
    JavaValue executeMethodByName(const std::string& className, const std::string& methodName, const std::string& descriptor, const std::vector<JavaValue>& args);

    JavaObject* allocateObject(JavaClass* clazz);
    JavaArray* allocateArray(char typeCode, int32_t length);
    JavaString* allocateString(const std::string& str);

    void gc();

private:
    std::unordered_map<std::string, std::shared_ptr<JavaClass>> m_classes;
    std::unordered_map<std::string, NativeMethodHandler> m_nativeMethods;
    std::vector<std::unique_ptr<JavaObject>> m_heap;

    static std::string makeNativeKey(const std::string& className, const std::string& methodName, const std::string& descriptor);
};

} // namespace universal_loader::jvm

#endif // J2ME_CLDC_VM_H
