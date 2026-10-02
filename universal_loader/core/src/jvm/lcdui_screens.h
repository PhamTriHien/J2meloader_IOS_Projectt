#pragma once
// Native state of the high-level LCDUI (Form, TextBox, List, Alert, their items and
// Commands). High-level screens are shown by the host UI as a native dialog.

#include "jvm_types.h"
#include <atomic>
#include <string>
#include <vector>

namespace universal_loader::jvm {

class CldcVirtualMachine;

struct LcduiCommandPayload : NativePayload {
    std::u16string label;
    std::u16string longLabel;
    int32_t type{1};
    int32_t priority{0};
};

// Choice (List / ChoiceGroup) content. MIDP types: EXCLUSIVE 1, MULTIPLE 2, IMPLICIT 3, POPUP 4
struct LcduiChoice {
    int32_t type{1};
    std::vector<std::u16string> strings;
    std::vector<bool> selected;
    int32_t fitPolicy{0};

    bool exclusive() const { return type != 2; }
};

enum class LcduiItemKind { Text, String, Choice, Gauge, Date, Spacer, Image };

struct LcduiItemPayload : NativePayload {
    LcduiItemKind kind{LcduiItemKind::String};
    std::u16string label;
    std::u16string text;        // TextField / StringItem text, ImageItem alt text
    int32_t maxSize{256};       // TextField
    int32_t constraints{0};
    int32_t appearance{0};      // StringItem / ImageItem: PLAIN 0, HYPERLINK 1, BUTTON 2
    LcduiChoice choice;         // ChoiceGroup
    bool interactive{false};    // Gauge
    int32_t maxValue{100};
    int32_t value{0};
    int32_t dateMode{3};        // DateField: DATE 1, TIME 2, DATE_TIME 3
    bool hasDate{false};
    int64_t date{0};
    int32_t layout{0};
    std::vector<JavaObject*> commands;
    JavaObject* defaultCommand{nullptr};
    JavaObject* commandListener{nullptr}; // ItemCommandListener
    JavaObject* owner{nullptr};           // Form containing the item
    void trace(std::vector<JavaObject*>& o) const override {
        for (auto* c : commands) if (c) o.push_back(c);
        if (defaultCommand) o.push_back(defaultCommand);
        if (commandListener) o.push_back(commandListener);
        if (owner) o.push_back(owner);
    }
};
using LcduiTextFieldPayload = LcduiItemPayload;

enum class LcduiScreenKind { Canvas, Form, TextBox, List, Alert };

// State of any Displayable. Canvases only use title/commands/listener.
struct LcduiScreenPayload : NativePayload {
    LcduiScreenKind kind{LcduiScreenKind::Canvas};
    std::u16string title;
    std::vector<JavaObject*> commands;
    JavaObject* listener{nullptr};
    // TextBox
    LcduiItemPayload box;
    // Form
    std::vector<JavaObject*> items;
    JavaObject* itemStateListener{nullptr};
    // List
    LcduiChoice list;
    bool customSelectCommand{false};
    JavaObject* selectCommand{nullptr};   // valid when customSelectCommand
    // Alert
    std::u16string alertText;
    int32_t alertTimeout{-2};             // Alert.FOREVER
    int32_t alertType{0};
    JavaObject* alertNext{nullptr};
    void trace(std::vector<JavaObject*>& o) const override {
        for (auto* c : commands) if (c) o.push_back(c);
        if (listener) o.push_back(listener);
        box.trace(o);
        for (auto* c : items) if (c) o.push_back(c);
        if (itemStateListener) o.push_back(itemStateListener);
        if (selectCommand) o.push_back(selectCommand);
        if (alertNext) o.push_back(alertNext);
    }

    bool isTextBox() const { return kind == LcduiScreenKind::TextBox; }
};

struct LcduiAlertTypePayload : NativePayload {
    int32_t type{0};
};

inline bool isLcduiScreen(const JavaObject* o) {
    auto* s = o ? dynamic_cast<LcduiScreenPayload*>(o->payload.get()) : nullptr;
    return s && s->kind != LcduiScreenKind::Canvas;
}

// Bumped whenever a Displayable's commands change (host UI refreshes its command menu)
inline std::atomic<int> g_lcduiCommandsVersion{0};

// Singletons List.SELECT_COMMAND / Alert.DISMISS_COMMAND
JavaObject* lcduiSelectCommand(CldcVirtualMachine* vm);
JavaObject* lcduiDismissCommand(CldcVirtualMachine* vm);

// Display.setCurrent semantics (implemented in cldc_vm.cpp)
void lcduiSetCurrent(CldcVirtualMachine* vm, JavaObject* target);

} // namespace universal_loader::jvm
