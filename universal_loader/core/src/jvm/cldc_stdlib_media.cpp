#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {


// Displayable state; nullptr for objects that already carry an unrelated payload
LcduiScreenPayload* dispOf(JavaObject* o) {
    if (!o) return nullptr;
    if (o->payload && !getPayload<LcduiScreenPayload>(o)) return nullptr;
    return &ensurePayload<LcduiScreenPayload>(o);
}

LcduiItemPayload& itemOf(JavaObject* o, LcduiItemKind kind) {
    bool fresh = !getPayload<LcduiItemPayload>(o);
    auto& it = ensurePayload<LcduiItemPayload>(o);
    if (fresh) it.kind = kind;
    return it;
}

JavaObject* newCommand(CldcVirtualMachine* vm, const std::u16string& label, int32_t type) {
    JavaObject* c = newNativeObject(vm, "javax/microedition/lcdui/Command");
    auto& p = ensurePayload<LcduiCommandPayload>(c);
    p.label = label;
    p.type = type;
    return c;
}

JavaObject* lcduiSelectCommand(CldcVirtualMachine* vm) {
    return vm->nativeSingleton("javax/microedition/lcdui/List.SELECT_COMMAND", [vm] { return newCommand(vm, u"", 1); });
}

JavaObject* lcduiDismissCommand(CldcVirtualMachine* vm) {
    return vm->nativeSingleton("javax/microedition/lcdui/Alert.DISMISS_COMMAND", [vm] { return newCommand(vm, u"Done", 4); });
}

// ---- Choice (List / ChoiceGroup) ----
void choiceSelect(LcduiChoice& c, int32_t i, bool on) {
    if (i < 0 || i >= static_cast<int32_t>(c.strings.size())) return;
    if (c.exclusive()) {
        if (!on) return;
        std::fill(c.selected.begin(), c.selected.end(), false);
    }
    c.selected[i] = on;
}

void choiceInsert(LcduiChoice& c, int32_t i, const std::u16string& s) {
    c.strings.insert(c.strings.begin() + i, s);
    c.selected.insert(c.selected.begin() + i, false);
    if (c.exclusive() && c.strings.size() == 1) c.selected[0] = true;
}

void choiceDelete(LcduiChoice& c, int32_t i) {
    bool wasSelected = c.selected[i];
    c.strings.erase(c.strings.begin() + i);
    c.selected.erase(c.selected.begin() + i);
    if (c.exclusive() && wasSelected && !c.strings.empty()) c.selected[std::min<size_t>(i, c.strings.size() - 1)] = true;
}

void choiceInit(LcduiChoice& c, int32_t type, JavaObject* strings) {
    c.type = type;
    c.strings.clear();
    c.selected.clear();
    if (auto* arr = dynamic_cast<JavaArray*>(strings)) {
        for (auto& e : arr->elements) choiceInsert(c, static_cast<int32_t>(c.strings.size()), asString(e.ref) ? u16(asString(e.ref)) : std::u16string());
    }
}

void registerChoice(CldcVirtualMachine* vm, const std::string& cls, LcduiChoice& (*choiceOf)(CldcVirtualMachine*, const Args&)) {
    auto checkIndex = [](CldcVirtualMachine* vm, const LcduiChoice& c, int32_t i) {
        if (i < 0 || i >= static_cast<int32_t>(c.strings.size())) vm->throwJava("java/lang/IndexOutOfBoundsException");
    };
    vm->registerNative(cls, "append", "(Ljava/lang/String;Ljavax/microedition/lcdui/Image;)I", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        choiceInsert(c, static_cast<int32_t>(c.strings.size()), optStr(a, 1));
        return JavaValue(static_cast<int32_t>(c.strings.size()) - 1);
    });
    vm->registerNative(cls, "insert", "(ILjava/lang/String;Ljavax/microedition/lcdui/Image;)V", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        int32_t i = arg(a, 1).i;
        if (i < 0 || i > static_cast<int32_t>(c.strings.size())) vm->throwJava("java/lang/IndexOutOfBoundsException");
        choiceInsert(c, i, optStr(a, 2));
        return JavaValue();
    });
    vm->registerNative(cls, "delete", "(I)V", [choiceOf, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        checkIndex(vm, c, arg(a, 1).i);
        choiceDelete(c, arg(a, 1).i);
        return JavaValue();
    });
    vm->registerNative(cls, "deleteAll", "()V", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        c.strings.clear();
        c.selected.clear();
        return JavaValue();
    });
    vm->registerNative(cls, "set", "(ILjava/lang/String;Ljavax/microedition/lcdui/Image;)V", [choiceOf, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        checkIndex(vm, c, arg(a, 1).i);
        c.strings[arg(a, 1).i] = optStr(a, 2);
        return JavaValue();
    });
    vm->registerNative(cls, "getString", "(I)Ljava/lang/String;", [choiceOf, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        checkIndex(vm, c, arg(a, 1).i);
        return newStr(vm, c.strings[arg(a, 1).i]);
    });
    vm->registerNative(cls, "getImage", "(I)Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    vm->registerNative(cls, "size", "()I", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(choiceOf(vm, a).strings.size()));
    });
    vm->registerNative(cls, "isSelected", "(I)Z", [choiceOf, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        checkIndex(vm, c, arg(a, 1).i);
        return boolV(c.selected[arg(a, 1).i]);
    });
    vm->registerNative(cls, "getSelectedIndex", "()I", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        if (!c.exclusive()) return JavaValue(-1);
        for (size_t i = 0; i < c.selected.size(); ++i) {
            if (c.selected[i]) return JavaValue(static_cast<int32_t>(i));
        }
        return JavaValue(-1);
    });
    vm->registerNative(cls, "getSelectedFlags", "([Z)I", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref);
        if (!arr) vm->throwJava("java/lang/NullPointerException");
        if (arr->elements.size() < c.strings.size()) vm->throwJava("java/lang/IllegalArgumentException");
        int32_t n = 0;
        for (size_t i = 0; i < arr->elements.size(); ++i) {
            bool on = i < c.selected.size() && c.selected[i];
            arr->elements[i] = JavaValue(on ? 1 : 0);
            n += on;
        }
        return JavaValue(n);
    });
    vm->registerNative(cls, "setSelectedIndex", "(IZ)V", [choiceOf, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        checkIndex(vm, c, arg(a, 1).i);
        choiceSelect(c, arg(a, 1).i, arg(a, 2).i != 0);
        return JavaValue();
    });
    vm->registerNative(cls, "setSelectedFlags", "([Z)V", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = choiceOf(vm, a);
        auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref);
        if (!arr) vm->throwJava("java/lang/NullPointerException");
        if (arr->elements.size() < c.strings.size()) vm->throwJava("java/lang/IllegalArgumentException");
        if (c.exclusive()) {
            int32_t first = -1;
            for (size_t i = 0; i < c.strings.size() && first < 0; ++i) {
                if (arr->elements[i].i) first = static_cast<int32_t>(i);
            }
            std::fill(c.selected.begin(), c.selected.end(), false);
            if (!c.selected.empty()) c.selected[first < 0 ? 0 : first] = true;
        } else {
            for (size_t i = 0; i < c.strings.size(); ++i) c.selected[i] = arr->elements[i].i != 0;
        }
        return JavaValue();
    });
    vm->registerNative(cls, "setFitPolicy", "(I)V", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        choiceOf(vm, a).fitPolicy = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(cls, "getFitPolicy", "()I", [choiceOf](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(choiceOf(vm, a).fitPolicy);
    });
    vm->registerNative(cls, "setFont", "(ILjavax/microedition/lcdui/Font;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(cls, "getFont", "(I)Ljavax/microedition/lcdui/Font;", [](CldcVirtualMachine* vm, const Args&) {
        return vm->executeMethodByName("javax/microedition/lcdui/Font", "getDefaultFont", "()Ljavax/microedition/lcdui/Font;", {});
    });
}

void registerLcduiScreens(CldcVirtualMachine* vm) {
    const std::string C = "javax/microedition/lcdui/Command";
    vm->registerNative(C, "<init>", "(Ljava/lang/String;II)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& c = ensurePayload<LcduiCommandPayload>(self(vm, a));
        c.label = optStr(a, 1);
        c.type = arg(a, 2).i;
        c.priority = arg(a, 3).i;
        return JavaValue();
    });
    vm->registerNative(C, "<init>", "(Ljava/lang/String;Ljava/lang/String;II)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& c = ensurePayload<LcduiCommandPayload>(self(vm, a));
        c.label = optStr(a, 1);
        c.longLabel = optStr(a, 2);
        c.type = arg(a, 3).i;
        c.priority = arg(a, 4).i;
        return JavaValue();
    });
    vm->registerNative(C, "getLabel", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, ensurePayload<LcduiCommandPayload>(self(vm, a)).label);
    });
    vm->registerNative(C, "getLongLabel", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto& c = ensurePayload<LcduiCommandPayload>(self(vm, a));
        return c.longLabel.empty() ? nullV() : newStr(vm, c.longLabel);
    });
    vm->registerNative(C, "getCommandType", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(ensurePayload<LcduiCommandPayload>(self(vm, a)).type);
    });
    vm->registerNative(C, "getPriority", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(ensurePayload<LcduiCommandPayload>(self(vm, a)).priority);
    });

    // ---- Displayable / Screen: title and commands (Canvas included) ----
    for (const char* cls : {"javax/microedition/lcdui/Displayable", "javax/microedition/lcdui/Screen"}) {
        const std::string D = cls;
        vm->registerNative(D, "setTitle", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
            if (auto* d = dispOf(self(vm, a))) d->title = optStr(a, 1);
            return JavaValue();
        });
        vm->registerNative(D, "getTitle", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
            auto* d = dispOf(self(vm, a));
            return d && !d->title.empty() ? newStr(vm, d->title) : nullV();
        });
        vm->registerNative(D, "addCommand", "(Ljavax/microedition/lcdui/Command;)V", [](CldcVirtualMachine* vm, const Args& a) {
            JavaObject* c = arg(a, 1).ref;
            if (!c) vm->throwJava("java/lang/NullPointerException");
            auto* d = dispOf(self(vm, a));
            if (d && std::find(d->commands.begin(), d->commands.end(), c) == d->commands.end()) {
                d->commands.push_back(c);
                ++g_lcduiCommandsVersion;
            }
            return JavaValue();
        });
        vm->registerNative(D, "removeCommand", "(Ljavax/microedition/lcdui/Command;)V", [](CldcVirtualMachine* vm, const Args& a) {
            if (auto* d = dispOf(self(vm, a))) {
                d->commands.erase(std::remove(d->commands.begin(), d->commands.end(), arg(a, 1).ref), d->commands.end());
                if (d->customSelectCommand && d->selectCommand == arg(a, 1).ref) d->selectCommand = nullptr;
                ++g_lcduiCommandsVersion;
            }
            return JavaValue();
        });
        vm->registerNative(D, "setCommandListener", "(Ljavax/microedition/lcdui/CommandListener;)V", [](CldcVirtualMachine* vm, const Args& a) {
            if (auto* d = dispOf(self(vm, a))) d->listener = arg(a, 1).ref;
            return JavaValue();
        });
        vm->registerNative(D, "setTicker", "(Ljavax/microedition/lcdui/Ticker;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
        vm->registerNative(D, "getTicker", "()Ljavax/microedition/lcdui/Ticker;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    }
    const std::string TK = "javax/microedition/lcdui/Ticker";
    vm->registerNative(TK, "<init>", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        itemOf(self(vm, a), LcduiItemKind::String).text = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(TK, "getString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, itemOf(self(vm, a), LcduiItemKind::String).text);
    });
    vm->registerNative(TK, "setString", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        itemOf(self(vm, a), LcduiItemKind::String).text = optStr(a, 1);
        return JavaValue();
    });

    // ---- TextField / TextBox ----
    auto textOf = [](CldcVirtualMachine* vm, const Args& a) -> LcduiItemPayload& {
        JavaObject* o = self(vm, a);
        if (auto* s = getPayload<LcduiScreenPayload>(o)) return s->box;
        return itemOf(o, LcduiItemKind::Text);
    };
    auto clip = [](LcduiItemPayload& t) {
        if (t.maxSize > 0 && static_cast<int32_t>(t.text.size()) > t.maxSize) t.text.resize(t.maxSize);
    };
    for (const char* cls : {"javax/microedition/lcdui/TextField", "javax/microedition/lcdui/TextBox"}) {
        const std::string T = cls;
        const bool box = T.find("TextBox") != std::string::npos;
        vm->registerNative(T, "<init>", "(Ljava/lang/String;Ljava/lang/String;II)V", [box, clip](CldcVirtualMachine* vm, const Args& a) {
            JavaObject* o = self(vm, a);
            LcduiItemPayload* t;
            if (box) {
                auto& s = ensurePayload<LcduiScreenPayload>(o);
                s.kind = LcduiScreenKind::TextBox;
                s.title = optStr(a, 1);
                t = &s.box;
                t->kind = LcduiItemKind::Text;
            } else {
                t = &itemOf(o, LcduiItemKind::Text);
                t->kind = LcduiItemKind::Text;
                t->label = optStr(a, 1);
            }
            t->text = optStr(a, 2);
            t->maxSize = arg(a, 3).i;
            t->constraints = arg(a, 4).i;
            clip(*t);
            return JavaValue();
        });
        vm->registerNative(T, "getString", "()Ljava/lang/String;", [textOf](CldcVirtualMachine* vm, const Args& a) {
            return newStr(vm, textOf(vm, a).text);
        });
        vm->registerNative(T, "setString", "(Ljava/lang/String;)V", [textOf, clip](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            t.text = optStr(a, 1);
            clip(t);
            return JavaValue();
        });
        vm->registerNative(T, "getChars", "([C)I", [textOf](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref);
            if (!arr) vm->throwJava("java/lang/NullPointerException");
            if (arr->elements.size() < t.text.size()) vm->throwJava("java/lang/ArrayIndexOutOfBoundsException");
            for (size_t i = 0; i < t.text.size(); ++i) arr->elements[i] = JavaValue(static_cast<int32_t>(t.text[i]));
            return JavaValue(static_cast<int32_t>(t.text.size()));
        });
        vm->registerNative(T, "setChars", "([CII)V", [textOf, clip](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            t.text.clear();
            if (auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref)) {
                int32_t off = arg(a, 2).i, len = arg(a, 3).i;
                for (int32_t i = off; i < off + len && i < static_cast<int32_t>(arr->elements.size()); ++i) t.text += static_cast<char16_t>(arr->elements[i].i);
            }
            clip(t);
            return JavaValue();
        });
        vm->registerNative(T, "size", "()I", [textOf](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<int32_t>(textOf(vm, a).text.size()));
        });
        vm->registerNative(T, "getMaxSize", "()I", [textOf](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(textOf(vm, a).maxSize);
        });
        vm->registerNative(T, "setMaxSize", "(I)I", [textOf, clip](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            t.maxSize = arg(a, 1).i;
            clip(t);
            return JavaValue(t.maxSize);
        });
        vm->registerNative(T, "getConstraints", "()I", [textOf](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(textOf(vm, a).constraints);
        });
        vm->registerNative(T, "setConstraints", "(I)V", [textOf](CldcVirtualMachine* vm, const Args& a) {
            textOf(vm, a).constraints = arg(a, 1).i;
            return JavaValue();
        });
        vm->registerNative(T, "getCaretPosition", "()I", [textOf](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<int32_t>(textOf(vm, a).text.size()));
        });
        vm->registerNative(T, "insert", "(Ljava/lang/String;I)V", [textOf, clip](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            size_t pos = static_cast<size_t>(std::clamp<int32_t>(arg(a, 2).i, 0, static_cast<int32_t>(t.text.size())));
            t.text.insert(pos, optStr(a, 1));
            clip(t);
            return JavaValue();
        });
        vm->registerNative(T, "insert", "([CIII)V", [textOf, clip](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            std::u16string s;
            if (auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref)) {
                int32_t off = arg(a, 2).i, len = arg(a, 3).i;
                for (int32_t i = off; i < off + len && i < static_cast<int32_t>(arr->elements.size()); ++i) s += static_cast<char16_t>(arr->elements[i].i);
            }
            size_t pos = static_cast<size_t>(std::clamp<int32_t>(arg(a, 4).i, 0, static_cast<int32_t>(t.text.size())));
            t.text.insert(pos, s);
            clip(t);
            return JavaValue();
        });
        vm->registerNative(T, "delete", "(II)V", [textOf](CldcVirtualMachine* vm, const Args& a) {
            auto& t = textOf(vm, a);
            size_t off = static_cast<size_t>(std::clamp<int32_t>(arg(a, 1).i, 0, static_cast<int32_t>(t.text.size())));
            t.text.erase(off, static_cast<size_t>(std::max<int32_t>(arg(a, 2).i, 0)));
            return JavaValue();
        });
        vm->registerNative(T, "setInitialInputMode", "(Ljava/lang/String;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    }

    // ---- Item (common) ----
    const std::string I = "javax/microedition/lcdui/Item";
    auto anyItem = [](CldcVirtualMachine* vm, const Args& a) -> LcduiItemPayload& {
        return itemOf(self(vm, a), LcduiItemKind::String);
    };
    vm->registerNative(I, "getLabel", "()Ljava/lang/String;", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = anyItem(vm, a);
        return t.label.empty() ? nullV() : newStr(vm, t.label);
    });
    vm->registerNative(I, "setLabel", "(Ljava/lang/String;)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        anyItem(vm, a).label = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(I, "addCommand", "(Ljavax/microedition/lcdui/Command;)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = anyItem(vm, a);
        JavaObject* c = arg(a, 1).ref;
        if (!c) vm->throwJava("java/lang/NullPointerException");
        if (std::find(t.commands.begin(), t.commands.end(), c) == t.commands.end()) t.commands.push_back(c);
        return JavaValue();
    });
    vm->registerNative(I, "removeCommand", "(Ljavax/microedition/lcdui/Command;)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = anyItem(vm, a);
        t.commands.erase(std::remove(t.commands.begin(), t.commands.end(), arg(a, 1).ref), t.commands.end());
        if (t.defaultCommand == arg(a, 1).ref) t.defaultCommand = nullptr;
        return JavaValue();
    });
    vm->registerNative(I, "setDefaultCommand", "(Ljavax/microedition/lcdui/Command;)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = anyItem(vm, a);
        JavaObject* c = arg(a, 1).ref;
        t.defaultCommand = c;
        if (c && std::find(t.commands.begin(), t.commands.end(), c) == t.commands.end()) t.commands.push_back(c);
        return JavaValue();
    });
    vm->registerNative(I, "setItemCommandListener", "(Ljavax/microedition/lcdui/ItemCommandListener;)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        anyItem(vm, a).commandListener = arg(a, 1).ref;
        return JavaValue();
    });
    vm->registerNative(I, "getLayout", "()I", [anyItem](CldcVirtualMachine* vm, const Args& a) { return JavaValue(anyItem(vm, a).layout); });
    vm->registerNative(I, "setLayout", "(I)V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        anyItem(vm, a).layout = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(I, "setPreferredSize", "(II)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    for (const char* m : {"getPreferredWidth", "getPreferredHeight", "getMinimumWidth", "getMinimumHeight"}) {
        vm->registerNative(I, m, "()I", [](CldcVirtualMachine*, const Args&) { return JavaValue(0); });
    }
    vm->registerNative(I, "notifyStateChanged", "()V", [anyItem](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* item = self(vm, a);
        auto& t = anyItem(vm, a);
        auto* form = getPayload<LcduiScreenPayload>(t.owner);
        if (form && form->itemStateListener && form->itemStateListener->clazz) {
            vm->executeMethodByName(form->itemStateListener->clazz->thisClassName, "itemStateChanged",
                                    "(Ljavax/microedition/lcdui/Item;)V", {refV(form->itemStateListener), refV(item)});
        }
        return JavaValue();
    });

    // ---- StringItem ----
    const std::string SI = "javax/microedition/lcdui/StringItem";
    for (const char* d : {"(Ljava/lang/String;Ljava/lang/String;)V", "(Ljava/lang/String;Ljava/lang/String;I)V"}) {
        vm->registerNative(SI, "<init>", d, [](CldcVirtualMachine* vm, const Args& a) {
            auto& t = itemOf(self(vm, a), LcduiItemKind::String);
            t.label = optStr(a, 1);
            t.text = optStr(a, 2);
            t.appearance = a.size() > 3 ? arg(a, 3).i : 0;
            t.maxSize = -1;
            return JavaValue();
        });
    }
    vm->registerNative(SI, "getText", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, itemOf(self(vm, a), LcduiItemKind::String).text);
    });
    vm->registerNative(SI, "setText", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        itemOf(self(vm, a), LcduiItemKind::String).text = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(SI, "getAppearanceMode", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(itemOf(self(vm, a), LcduiItemKind::String).appearance);
    });
    vm->registerNative(SI, "setFont", "(Ljavax/microedition/lcdui/Font;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });

    // ---- ImageItem (shown by its alt text) / Spacer ----
    const std::string II = "javax/microedition/lcdui/ImageItem";
    for (const char* d : {"(Ljava/lang/String;Ljavax/microedition/lcdui/Image;ILjava/lang/String;)V",
                          "(Ljava/lang/String;Ljavax/microedition/lcdui/Image;ILjava/lang/String;I)V"}) {
        vm->registerNative(II, "<init>", d, [](CldcVirtualMachine* vm, const Args& a) {
            auto& t = itemOf(self(vm, a), LcduiItemKind::Image);
            t.label = optStr(a, 1);
            t.layout = arg(a, 3).i;
            t.text = optStr(a, 4);
            t.appearance = a.size() > 5 ? arg(a, 5).i : 0;
            return JavaValue();
        });
    }
    vm->registerNative(II, "getAltText", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, itemOf(self(vm, a), LcduiItemKind::Image).text);
    });
    vm->registerNative(II, "setAltText", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        itemOf(self(vm, a), LcduiItemKind::Image).text = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(II, "setImage", "(Ljavax/microedition/lcdui/Image;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(II, "getImage", "()Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    vm->registerNative("javax/microedition/lcdui/Spacer", "<init>", "(II)V", [](CldcVirtualMachine* vm, const Args& a) {
        itemOf(self(vm, a), LcduiItemKind::Spacer);
        return JavaValue();
    });
    vm->registerNative("javax/microedition/lcdui/Spacer", "setMinimumSize", "(II)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });

    // ---- ChoiceGroup / List ----
    const std::string CG = "javax/microedition/lcdui/ChoiceGroup";
    vm->registerNative(CG, "<init>", "(Ljava/lang/String;I)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& t = itemOf(self(vm, a), LcduiItemKind::Choice);
        t.kind = LcduiItemKind::Choice;
        t.label = optStr(a, 1);
        choiceInit(t.choice, arg(a, 2).i, nullptr);
        return JavaValue();
    });
    vm->registerNative(CG, "<init>", "(Ljava/lang/String;I[Ljava/lang/String;[Ljavax/microedition/lcdui/Image;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& t = itemOf(self(vm, a), LcduiItemKind::Choice);
        t.kind = LcduiItemKind::Choice;
        t.label = optStr(a, 1);
        choiceInit(t.choice, arg(a, 2).i, arg(a, 3).ref);
        return JavaValue();
    });
    registerChoice(vm, CG, [](CldcVirtualMachine* vm, const Args& a) -> LcduiChoice& {
        return itemOf(self(vm, a), LcduiItemKind::Choice).choice;
    });

    const std::string L = "javax/microedition/lcdui/List";
    auto initList = [](CldcVirtualMachine* vm, const Args& a, JavaObject* strings) {
        auto& s = ensurePayload<LcduiScreenPayload>(self(vm, a));
        s.kind = LcduiScreenKind::List;
        s.title = optStr(a, 1);
        choiceInit(s.list, arg(a, 2).i, strings);
    };
    vm->registerNative(L, "<init>", "(Ljava/lang/String;I)V", [initList](CldcVirtualMachine* vm, const Args& a) {
        initList(vm, a, nullptr);
        return JavaValue();
    });
    vm->registerNative(L, "<init>", "(Ljava/lang/String;I[Ljava/lang/String;[Ljavax/microedition/lcdui/Image;)V", [initList](CldcVirtualMachine* vm, const Args& a) {
        initList(vm, a, arg(a, 3).ref);
        return JavaValue();
    });
    registerChoice(vm, L, [](CldcVirtualMachine* vm, const Args& a) -> LcduiChoice& {
        return ensurePayload<LcduiScreenPayload>(self(vm, a)).list;
    });
    vm->registerNative(L, "setSelectCommand", "(Ljavax/microedition/lcdui/Command;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& s = ensurePayload<LcduiScreenPayload>(self(vm, a));
        if (s.list.type != 3) return JavaValue();
        JavaObject* c = arg(a, 1).ref;
        s.customSelectCommand = c != lcduiSelectCommand(vm);
        s.selectCommand = s.customSelectCommand ? c : nullptr;
        if (c && s.customSelectCommand && std::find(s.commands.begin(), s.commands.end(), c) == s.commands.end()) s.commands.push_back(c);
        return JavaValue();
    });
    vm->registerNativeStatic(L, "SELECT_COMMAND", [](CldcVirtualMachine* vm, const Args&) { return refV(lcduiSelectCommand(vm)); });

    // ---- Gauge ----
    const std::string G = "javax/microedition/lcdui/Gauge";
    vm->registerNative(G, "<init>", "(Ljava/lang/String;ZII)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& t = itemOf(self(vm, a), LcduiItemKind::Gauge);
        t.kind = LcduiItemKind::Gauge;
        t.label = optStr(a, 1);
        t.interactive = arg(a, 2).i != 0;
        t.maxValue = arg(a, 3).i;
        t.value = arg(a, 4).i;
        return JavaValue();
    });
    auto gauge = [](CldcVirtualMachine* vm, const Args& a) -> LcduiItemPayload& { return itemOf(self(vm, a), LcduiItemKind::Gauge); };
    vm->registerNative(G, "getValue", "()I", [gauge](CldcVirtualMachine* vm, const Args& a) { return JavaValue(gauge(vm, a).value); });
    vm->registerNative(G, "setValue", "(I)V", [gauge](CldcVirtualMachine* vm, const Args& a) {
        auto& t = gauge(vm, a);
        t.value = t.maxValue > 0 ? std::clamp(arg(a, 1).i, 0, t.maxValue) : arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(G, "getMaxValue", "()I", [gauge](CldcVirtualMachine* vm, const Args& a) { return JavaValue(gauge(vm, a).maxValue); });
    vm->registerNative(G, "setMaxValue", "(I)V", [gauge](CldcVirtualMachine* vm, const Args& a) {
        auto& t = gauge(vm, a);
        t.maxValue = arg(a, 1).i;
        if (t.maxValue > 0) t.value = std::min(t.value, t.maxValue);
        return JavaValue();
    });
    vm->registerNative(G, "isInteractive", "()Z", [gauge](CldcVirtualMachine* vm, const Args& a) { return boolV(gauge(vm, a).interactive); });

    // ---- DateField ----
    const std::string DF = "javax/microedition/lcdui/DateField";
    for (const char* d : {"(Ljava/lang/String;I)V", "(Ljava/lang/String;ILjava/util/TimeZone;)V"}) {
        vm->registerNative(DF, "<init>", d, [](CldcVirtualMachine* vm, const Args& a) {
            auto& t = itemOf(self(vm, a), LcduiItemKind::Date);
            t.kind = LcduiItemKind::Date;
            t.label = optStr(a, 1);
            t.dateMode = arg(a, 2).i;
            return JavaValue();
        });
    }
    auto dateItem = [](CldcVirtualMachine* vm, const Args& a) -> LcduiItemPayload& { return itemOf(self(vm, a), LcduiItemKind::Date); };
    vm->registerNative(DF, "getDate", "()Ljava/util/Date;", [dateItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = dateItem(vm, a);
        if (!t.hasDate) return nullV();
        JavaObject* d = newNativeObject(vm, "java/util/Date");
        ensurePayload<TimePayload>(d).millis = t.date;
        return refV(d);
    });
    vm->registerNative(DF, "setDate", "(Ljava/util/Date;)V", [dateItem](CldcVirtualMachine* vm, const Args& a) {
        auto& t = dateItem(vm, a);
        auto* d = getPayload<TimePayload>(arg(a, 1).ref);
        t.hasDate = d != nullptr;
        t.date = d ? d->millis : 0;
        return JavaValue();
    });
    vm->registerNative(DF, "getInputMode", "()I", [dateItem](CldcVirtualMachine* vm, const Args& a) { return JavaValue(dateItem(vm, a).dateMode); });
    vm->registerNative(DF, "setInputMode", "(I)V", [dateItem](CldcVirtualMachine* vm, const Args& a) {
        dateItem(vm, a).dateMode = arg(a, 1).i;
        return JavaValue();
    });

    // ---- Form ----
    const std::string F = "javax/microedition/lcdui/Form";
    auto form = [](CldcVirtualMachine* vm, const Args& a) -> LcduiScreenPayload& {
        auto& s = ensurePayload<LcduiScreenPayload>(self(vm, a));
        s.kind = LcduiScreenKind::Form;
        return s;
    };
    auto adopt = [](CldcVirtualMachine* vm, JavaObject* formObj, JavaObject* item) {
        if (!item) vm->throwJava("java/lang/NullPointerException");
        auto& t = itemOf(item, LcduiItemKind::String);
        if (t.owner && t.owner != formObj) vm->throwJava("java/lang/IllegalStateException");
        t.owner = formObj;
    };
    auto release = [](JavaObject* item) {
        if (auto* t = getPayload<LcduiItemPayload>(item)) t->owner = nullptr;
    };
    auto checkIndex = [](CldcVirtualMachine* vm, const LcduiScreenPayload& s, int32_t i, bool allowEnd) {
        int32_t n = static_cast<int32_t>(s.items.size());
        if (i < 0 || i > n || (i == n && !allowEnd)) vm->throwJava("java/lang/IndexOutOfBoundsException");
    };
    vm->registerNative(F, "<init>", "(Ljava/lang/String;)V", [form](CldcVirtualMachine* vm, const Args& a) {
        form(vm, a).title = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(F, "<init>", "(Ljava/lang/String;[Ljavax/microedition/lcdui/Item;)V", [form, adopt](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        s.title = optStr(a, 1);
        if (auto* arr = dynamic_cast<JavaArray*>(arg(a, 2).ref)) {
            for (auto& e : arr->elements) {
                adopt(vm, self(vm, a), e.ref);
                s.items.push_back(e.ref);
            }
        }
        return JavaValue();
    });
    vm->registerNative(F, "append", "(Ljavax/microedition/lcdui/Item;)I", [form, adopt](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        adopt(vm, self(vm, a), arg(a, 1).ref);
        s.items.push_back(arg(a, 1).ref);
        return JavaValue(static_cast<int32_t>(s.items.size()) - 1);
    });
    vm->registerNative(F, "append", "(Ljava/lang/String;)I", [form](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        JavaObject* item = newNativeObject(vm, "javax/microedition/lcdui/StringItem");
        auto& t = itemOf(item, LcduiItemKind::String);
        t.text = optStr(a, 1);
        t.maxSize = -1;
        t.owner = self(vm, a);
        s.items.push_back(item);
        return JavaValue(static_cast<int32_t>(s.items.size()) - 1);
    });
    vm->registerNative(F, "append", "(Ljavax/microedition/lcdui/Image;)I", [form](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        JavaObject* item = newNativeObject(vm, "javax/microedition/lcdui/ImageItem");
        itemOf(item, LcduiItemKind::Image).owner = self(vm, a);
        s.items.push_back(item);
        return JavaValue(static_cast<int32_t>(s.items.size()) - 1);
    });
    vm->registerNative(F, "insert", "(ILjavax/microedition/lcdui/Item;)V", [form, adopt, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        checkIndex(vm, s, arg(a, 1).i, true);
        adopt(vm, self(vm, a), arg(a, 2).ref);
        s.items.insert(s.items.begin() + arg(a, 1).i, arg(a, 2).ref);
        return JavaValue();
    });
    vm->registerNative(F, "set", "(ILjavax/microedition/lcdui/Item;)V", [form, adopt, release, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        checkIndex(vm, s, arg(a, 1).i, false);
        adopt(vm, self(vm, a), arg(a, 2).ref);
        release(s.items[arg(a, 1).i]);
        s.items[arg(a, 1).i] = arg(a, 2).ref;
        return JavaValue();
    });
    vm->registerNative(F, "size", "()I", [form](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(form(vm, a).items.size()));
    });
    vm->registerNative(F, "get", "(I)Ljavax/microedition/lcdui/Item;", [form, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        checkIndex(vm, s, arg(a, 1).i, false);
        return refV(s.items[arg(a, 1).i]);
    });
    vm->registerNative(F, "delete", "(I)V", [form, release, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        checkIndex(vm, s, arg(a, 1).i, false);
        release(s.items[arg(a, 1).i]);
        s.items.erase(s.items.begin() + arg(a, 1).i);
        return JavaValue();
    });
    vm->registerNative(F, "deleteAll", "()V", [form, release](CldcVirtualMachine* vm, const Args& a) {
        auto& s = form(vm, a);
        for (JavaObject* it : s.items) release(it);
        s.items.clear();
        return JavaValue();
    });
    vm->registerNative(F, "setItemStateListener", "(Ljavax/microedition/lcdui/ItemStateListener;)V", [form](CldcVirtualMachine* vm, const Args& a) {
        form(vm, a).itemStateListener = arg(a, 1).ref;
        return JavaValue();
    });

    // ---- Alert / AlertType ----
    const std::string A = "javax/microedition/lcdui/Alert";
    auto alertTypeOf = [](JavaObject* t) {
        auto* p = getPayload<LcduiAlertTypePayload>(t);
        return p ? p->type : 0;
    };
    auto alert = [](CldcVirtualMachine* vm, const Args& a) -> LcduiScreenPayload& {
        auto& s = ensurePayload<LcduiScreenPayload>(self(vm, a));
        s.kind = LcduiScreenKind::Alert;
        return s;
    };
    vm->registerNative(A, "<init>", "(Ljava/lang/String;)V", [alert](CldcVirtualMachine* vm, const Args& a) {
        auto& s = alert(vm, a);
        s.title = optStr(a, 1);
        s.alertTimeout = 2000;
        return JavaValue();
    });
    vm->registerNative(A, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljavax/microedition/lcdui/Image;Ljavax/microedition/lcdui/AlertType;)V",
                       [alert, alertTypeOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = alert(vm, a);
        s.title = optStr(a, 1);
        s.alertText = optStr(a, 2);
        s.alertType = alertTypeOf(arg(a, 4).ref);
        s.alertTimeout = 2000;
        return JavaValue();
    });
    vm->registerNative(A, "getString", "()Ljava/lang/String;", [alert](CldcVirtualMachine* vm, const Args& a) {
        auto& s = alert(vm, a);
        return s.alertText.empty() ? nullV() : newStr(vm, s.alertText);
    });
    vm->registerNative(A, "setString", "(Ljava/lang/String;)V", [alert](CldcVirtualMachine* vm, const Args& a) {
        alert(vm, a).alertText = optStr(a, 1);
        return JavaValue();
    });
    vm->registerNative(A, "getTimeout", "()I", [alert](CldcVirtualMachine* vm, const Args& a) { return JavaValue(alert(vm, a).alertTimeout); });
    vm->registerNative(A, "setTimeout", "(I)V", [alert](CldcVirtualMachine* vm, const Args& a) {
        alert(vm, a).alertTimeout = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(A, "getDefaultTimeout", "()I", [](CldcVirtualMachine*, const Args&) { return JavaValue(2000); });
    vm->registerNative(A, "setType", "(Ljavax/microedition/lcdui/AlertType;)V", [alert, alertTypeOf](CldcVirtualMachine* vm, const Args& a) {
        alert(vm, a).alertType = alertTypeOf(arg(a, 1).ref);
        return JavaValue();
    });
    vm->registerNative(A, "setImage", "(Ljavax/microedition/lcdui/Image;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(A, "getImage", "()Ljavax/microedition/lcdui/Image;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    vm->registerNative(A, "setIndicator", "(Ljavax/microedition/lcdui/Gauge;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(A, "getIndicator", "()Ljavax/microedition/lcdui/Gauge;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    vm->registerNativeStatic(A, "DISMISS_COMMAND", [](CldcVirtualMachine* vm, const Args&) { return refV(lcduiDismissCommand(vm)); });

    const std::string AT = "javax/microedition/lcdui/AlertType";
    const char* typeNames[] = {"ALARM", "CONFIRMATION", "ERROR", "INFO", "WARNING"};
    for (int32_t t = 1; t <= 5; ++t) {
        vm->registerNativeStatic(AT, typeNames[t - 1], [t, AT](CldcVirtualMachine* vm, const Args&) {
            return refV(vm->nativeSingleton(AT + "." + std::to_string(t), [vm, t] {
                JavaObject* type = newNativeObject(vm, "javax/microedition/lcdui/AlertType");
                ensurePayload<LcduiAlertTypePayload>(type).type = t;
                return type;
            }));
        });
    }
    vm->registerNative(AT, "playSound", "(Ljavax/microedition/lcdui/Display;)Z", [](CldcVirtualMachine*, const Args&) { return boolV(true); });
}

struct MediaState {
    std::shared_ptr<j2me::Player> player;
    std::vector<JavaObject*> listeners;  // written with the GIL and mtx held
    JavaObject* volumeControl{nullptr};  // GIL-protected
    std::mutex mtx;                      // guards the fields below
    std::deque<std::pair<std::string, int64_t>> events; // (event, Long data or -1 for null)
    bool watching{false};
    bool hostPaused{false};
    uint32_t eomSeen{0};
};

struct MediaPayload : NativePayload { // javax.microedition.media.Player
    std::shared_ptr<MediaState> st = std::make_shared<MediaState>();
    void trace(std::vector<JavaObject*>& o) const override {
        for (auto* l : st->listeners) o.push_back(l);
        if (st->volumeControl) o.push_back(st->volumeControl);
    }
};

struct MediaControlPayload : NativePayload { // VolumeControl / ToneControl of a Player
    std::shared_ptr<MediaState> st;
};

struct NokiaSoundPayload : NativePayload { // com.nokia.mid.sound.Sound
    std::shared_ptr<j2me::Player> player;
    int32_t toneNote{-1};
    int32_t toneMs{0};
    int32_t gain{255};
};

const char* const kPlayerClass = "javax/microedition/media/Player";

std::shared_ptr<MediaState> mediaOf(CldcVirtualMachine* vm, const Args& a) {
    auto* p = getPayload<MediaPayload>(self(vm, a));
    if (!p) vm->throwJava("java/lang/IllegalStateException", "Player");
    return p->st;
}

std::shared_ptr<MediaState> controlOf(CldcVirtualMachine* vm, const Args& a) {
    auto* p = getPayload<MediaControlPayload>(self(vm, a));
    if (!p || !p->st) vm->throwJava("java/lang/IllegalStateException", "Control");
    return p->st;
}

std::vector<uint8_t> readAllBytes(CldcVirtualMachine* vm, JavaObject* s) {
    if (!s) return {};
    std::vector<uint8_t> out;
    if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) {
        if (jis->pos < jis->data.size()) {
            out.assign(jis->data.begin() + jis->pos, jis->data.end());
            jis->pos = jis->data.size();
        }
        return out;
    }
    if (auto* p = getPayload<ByteInputPayload>(s)) {
        if (p->pos < p->data.size()) {
            out.assign(p->data.begin() + p->pos, p->data.end());
            p->pos = p->data.size();
        }
        return out;
    }
    if (auto* f = getPayload<FilterInputPayload>(s)) return readAllBytes(vm, f->in);
    JavaArray* buf = vm->allocateArray('B', 4096);
    while (true) {
        int32_t n = streamReadBlock(vm, s, buf, 0, 4096);
        if (n <= 0) break;
        for (int32_t i = 0; i < n; ++i) out.push_back(static_cast<uint8_t>(buf->elements[i].i & 0xFF));
    }
    return out;
}

JavaObject* newPlayerObject(CldcVirtualMachine* vm, std::shared_ptr<j2me::Player> player) {
    if (!player) return nullptr;
    JavaObject* o = newNativeObject(vm, kPlayerClass);
    auto& p = ensurePayload<MediaPayload>(o);
    p.st->player = std::move(player);
    return o;
}

void deliverMediaEvents(CldcVirtualMachine* vm, JavaObject* playerObj, const std::shared_ptr<MediaState>& st,
                        const std::string& event, int64_t data) {
    if (!st || st->listeners.empty()) return;
    std::vector<JavaObject*> listeners;
    {
        std::lock_guard<std::mutex> lock(st->mtx);
        listeners = st->listeners;
    }
    JavaValue dataVal = data >= 0 ? newBox(vm, "java/lang/Long", JavaValue(data)) : nullV();
    for (auto* l : listeners) {
        if (!l) continue;
        try {
            vm->executeMethodByName(vm->classNameOf(l), "playerUpdate", "(Ljavax/microedition/media/Player;Ljava/lang/String;Ljava/lang/Object;)V",
                                    {refV(l), refV(playerObj), newStrUtf8(vm, event), dataVal});
        } catch (const VmTerminated&) { throw; } catch (...) {}
    }
}

void mediaPost(CldcVirtualMachine* vm, JavaObject* playerObj, const std::shared_ptr<MediaState>& st,
               const std::string& event, int64_t data) {
    deliverMediaEvents(vm, playerObj, st, event, data);
}

JavaObject* newMediaControl(CldcVirtualMachine* vm, const std::string& cls, const std::shared_ptr<MediaState>& st) {
    JavaObject* o = newNativeObject(vm, cls);
    ensurePayload<MediaControlPayload>(o).st = st;
    return o;
}

std::vector<uint8_t> toneSequenceToMidi(JavaArray* seq) {
    if (!seq) return {};
    std::vector<uint8_t> midi;
    const uint8_t header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96};
    midi.insert(midi.end(), std::begin(header), std::end(header));
    std::vector<uint8_t> track;
    track.push_back(0); track.push_back(0xC0); track.push_back(0);
    for (int32_t i = 0; i + 1 < seq->length; i += 2) {
        int note = seq->elements[i].i & 0xFF;
        int dur = seq->elements[i + 1].i & 0xFF;
        if (note == 0) continue;
        track.push_back(0); track.push_back(0x90); track.push_back(static_cast<uint8_t>(note)); track.push_back(64);
        track.push_back(static_cast<uint8_t>(std::min(dur * 4, 127)));
        track.push_back(0x80); track.push_back(static_cast<uint8_t>(note)); track.push_back(0);
    }
    track.push_back(0); track.push_back(0xFF); track.push_back(0x2F); track.push_back(0);
    const uint8_t trkHeader[] = {'M', 'T', 'r', 'k', static_cast<uint8_t>(track.size() >> 24), static_cast<uint8_t>(track.size() >> 16), static_cast<uint8_t>(track.size() >> 8), static_cast<uint8_t>(track.size())};
    midi.insert(midi.end(), std::begin(trkHeader), std::end(trkHeader));
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}

std::vector<uint8_t> toneSequenceToMidi(const std::vector<int8_t>& seq) {
    std::vector<uint8_t> midi;
    const uint8_t header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96};
    midi.insert(midi.end(), std::begin(header), std::end(header));
    std::vector<uint8_t> track;
    track.push_back(0); track.push_back(0xC0); track.push_back(0);
    for (size_t i = 0; i + 1 < seq.size(); i += 2) {
        int note = seq[i] & 0xFF;
        int dur = seq[i + 1] & 0xFF;
        if (note == 0) continue;
        track.push_back(0); track.push_back(0x90); track.push_back(static_cast<uint8_t>(note)); track.push_back(64);
        track.push_back(static_cast<uint8_t>(std::min(dur * 4, 127)));
        track.push_back(0x80); track.push_back(static_cast<uint8_t>(note)); track.push_back(0);
    }
    track.push_back(0); track.push_back(0xFF); track.push_back(0x2F); track.push_back(0);
    const uint8_t trkHeader[] = {'M', 'T', 'r', 'k', static_cast<uint8_t>(track.size() >> 24), static_cast<uint8_t>(track.size() >> 16), static_cast<uint8_t>(track.size() >> 8), static_cast<uint8_t>(track.size())};
    midi.insert(midi.end(), std::begin(trkHeader), std::end(trkHeader));
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}

void registerMedia(CldcVirtualMachine* vm) {
    const char* M = "javax/microedition/media/Manager";
    const char* P = kPlayerClass;
    const char* VC = "javax/microedition/media/control/VolumeControl";
    const char* TC = "javax/microedition/media/control/ToneControl";

    vm->registerNative(M, "playTone", "(III)V", [](CldcVirtualMachine* vm, const Args& a) {
        int32_t note = arg(a, 0).i, duration = arg(a, 1).i, volume = arg(a, 2).i;
        if (note < 0 || note > 127 || duration <= 0) vm->throwJava("java/lang/IllegalArgumentException", "playTone");
        j2me::MmapiManager::playTone(note, duration, volume);
        return JavaValue();
    });
    vm->registerNative(M, "createPlayer", "(Ljava/io/InputStream;Ljava/lang/String;)Ljavax/microedition/media/Player;", [](CldcVirtualMachine* vm, const Args& a) {
        std::vector<uint8_t> data = readAllBytes(vm, arg(a, 0).ref);
        JavaString* type = asString(arg(a, 1).ref);
        auto player = j2me::MmapiManager::createPlayer(data.data(), data.size(), type ? lowerAscii(type->value) : "");
        return refV(newPlayerObject(vm, std::move(player)));
    });
    vm->registerNative(M, "createPlayer", "(Ljava/lang/String;)Ljavax/microedition/media/Player;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* loc = asString(arg(a, 0).ref);
        if (!loc) vm->throwJava("java/lang/IllegalArgumentException", "locator is null");
        std::string path = loc->value;
        if (path.rfind("resource:", 0) == 0) path = path.substr(9);
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        std::vector<uint8_t> data;
        if (inst && !path.empty() && path.find("://") == std::string::npos &&
            inst->jarReader.extractEntry(path[0] == '/' ? path.substr(1) : path, data)) {
            return refV(newPlayerObject(vm, j2me::MmapiManager::createPlayer(data.data(), data.size(), "")));
        }
        // device://tone (ToneControl), device://midi and remote locators: a silent player
        return refV(newPlayerObject(vm, j2me::MmapiManager::createPlayer(loc->value)));
    });
    auto stringArray = [](CldcVirtualMachine* vm, const std::vector<std::string>& items) {
        JavaArray* arr = vm->allocateArray('L', static_cast<int32_t>(items.size()));
        vm->pin(arr);
        for (size_t i = 0; i < items.size(); ++i) arr->elements[i] = newStrUtf8(vm, items[i]);
        vm->unpin(arr);
        return refV(arr);
    };
    vm->registerNative(M, "getSupportedContentTypes", "(Ljava/lang/String;)[Ljava/lang/String;", [stringArray](CldcVirtualMachine* vm, const Args&) {
        return stringArray(vm, j2me::MmapiManager::getSupportedContentTypes());
    });
    vm->registerNative(M, "getSupportedProtocols", "(Ljava/lang/String;)[Ljava/lang/String;", [stringArray](CldcVirtualMachine* vm, const Args&) {
        return stringArray(vm, {"device", "resource"});
    });

    vm->registerNative(P, "realize", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) vm->throwJava("java/lang/IllegalStateException", "Player is closed");
        st->player->realize();
        return JavaValue();
    });
    vm->registerNative(P, "prefetch", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) vm->throwJava("java/lang/IllegalStateException", "Player is closed");
        st->player->realize();
        st->player->prefetch();
        return JavaValue();
    });
    vm->registerNative(P, "start", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) vm->throwJava("java/lang/IllegalStateException", "Player is closed");
        if (st->player->getState() == j2me::PLAYER_STARTED) return JavaValue();
        st->player->realize();
        st->player->prefetch();
        st->player->start();
        mediaPost(vm, o, st, "started", st->player->getMediaTime());
        return JavaValue();
    });
    vm->registerNative(P, "stop", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) vm->throwJava("java/lang/IllegalStateException", "Player is closed");
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->hostPaused = false;
        }
        if (st->player->getState() != j2me::PLAYER_STARTED) return JavaValue();
        st->player->stop();
        mediaPost(vm, o, st, "stopped", st->player->getMediaTime());
        return JavaValue();
    });
    vm->registerNative(P, "deallocate", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) vm->throwJava("java/lang/IllegalStateException", "Player is closed");
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->hostPaused = false;
        }
        st->player->deallocate();
        return JavaValue();
    });
    vm->registerNative(P, "close", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        auto st = mediaOf(vm, a);
        if (st->player->getState() == j2me::PLAYER_CLOSED) return JavaValue();
        {
            std::lock_guard<std::mutex> lock(st->mtx);
            st->hostPaused = false;
        }
        st->player->close();
        mediaPost(vm, o, st, "closed", -1);
        return JavaValue();
    });
    vm->registerNative(P, "getState", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(mediaOf(vm, a)->player->getState()));
    });
    vm->registerNative(P, "setLoopCount", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        int32_t count = arg(a, 1).i;
        if (count == 0) vm->throwJava("java/lang/IllegalArgumentException", "loop count 0");
        if (st->player->getState() == j2me::PLAYER_STARTED) vm->throwJava("java/lang/IllegalStateException", "Player is started");
        st->player->setLoopCount(count);
        return JavaValue();
    });
    vm->registerNative(P, "getDuration", "()J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int64_t>(mediaOf(vm, a)->player->getDuration()));
    });
    vm->registerNative(P, "getMediaTime", "()J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int64_t>(mediaOf(vm, a)->player->getMediaTime()));
    });
    vm->registerNative(P, "setMediaTime", "(J)J", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        st->player->realize();
        return JavaValue(static_cast<int64_t>(st->player->setMediaTime(arg(a, 1).l)));
    });
    vm->registerNative(P, "getContentType", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::string t = mediaOf(vm, a)->player->getContentType();
        return newStrUtf8(vm, t.empty() ? "audio/midi" : t);
    });
    vm->registerNative(P, "addPlayerListener", "(Ljavax/microedition/media/PlayerListener;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        JavaObject* l = arg(a, 1).ref;
        std::lock_guard<std::mutex> lock(st->mtx); // the watcher peeks at listeners.empty()
        if (l && std::find(st->listeners.begin(), st->listeners.end(), l) == st->listeners.end()) st->listeners.push_back(l);
        return JavaValue();
    });
    vm->registerNative(P, "removePlayerListener", "(Ljavax/microedition/media/PlayerListener;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        std::lock_guard<std::mutex> lock(st->mtx);
        st->listeners.erase(std::remove(st->listeners.begin(), st->listeners.end(), arg(a, 1).ref), st->listeners.end());
        return JavaValue();
    });
    auto volumeControl = [VC](CldcVirtualMachine* vm, const std::shared_ptr<MediaState>& st) {
        if (!st->volumeControl) st->volumeControl = newMediaControl(vm, VC, st);
        return st->volumeControl;
    };
    vm->registerNative(P, "getControl", "(Ljava/lang/String;)Ljavax/microedition/media/Control;", [volumeControl, TC](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        JavaString* name = asString(arg(a, 1).ref);
        if (!name) vm->throwJava("java/lang/IllegalArgumentException", "control type is null");
        std::string n = name->value;
        size_t dot = n.rfind('.');
        if (dot != std::string::npos) n = n.substr(dot + 1);
        if (n == "VolumeControl") return refV(volumeControl(vm, st));
        if (n == "ToneControl") return refV(newMediaControl(vm, TC, st));
        return nullV();
    });
    vm->registerNative(P, "getControls", "()[Ljavax/microedition/media/Control;", [volumeControl](CldcVirtualMachine* vm, const Args& a) {
        auto st = mediaOf(vm, a);
        JavaObject* vc = volumeControl(vm, st);
        JavaArray* arr = vm->allocateArray('L', 1);
        arr->elements[0] = refV(vc);
        return refV(arr);
    });
    vm->registerNative(P, "getTimeBase", "()Ljavax/microedition/media/TimeBase;", [](CldcVirtualMachine*, const Args&) { return nullV(); });
    vm->registerNative(P, "setTimeBase", "(Ljavax/microedition/media/TimeBase;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });

    vm->registerNative(VC, "setLevel", "(I)I", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = controlOf(vm, a);
        int32_t level = std::clamp(arg(a, 1).i, 0, 100);
        st->player->getVolumeControl()->setLevel(level);
        return JavaValue(level);
    });
    vm->registerNative(VC, "getLevel", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(controlOf(vm, a)->player->getVolumeControl()->getLevel()));
    });
    vm->registerNative(VC, "setMute", "(Z)V", [](CldcVirtualMachine* vm, const Args& a) {
        controlOf(vm, a)->player->getVolumeControl()->setMute(arg(a, 1).i != 0);
        return JavaValue();
    });
    vm->registerNative(VC, "isMuted", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        return boolV(controlOf(vm, a)->player->getVolumeControl()->isMuted());
    });

    vm->registerNative(TC, "setSequence", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto st = controlOf(vm, a);
        JavaArray* arr = arrayArg(vm, a, 1);
        if (st->player->getState() >= j2me::PLAYER_PREFETCHED) vm->throwJava("java/lang/IllegalStateException", "Player is prefetched");
        std::vector<int8_t> seq(static_cast<size_t>(arr->length));
        for (int32_t i = 0; i < arr->length; ++i) seq[i] = static_cast<int8_t>(arr->elements[i].i);
        std::vector<uint8_t> smf = toneSequenceToMidi(seq);
        auto player = j2me::MmapiManager::createPlayer(smf.data(), smf.size(), "audio/x-tone-seq");
        player->getVolumeControl()->setLevel(st->player->getVolumeControl()->getLevel());
        player->getVolumeControl()->setMute(st->player->getVolumeControl()->isMuted());
        player->realize();
        st->player->close();
        std::lock_guard<std::mutex> lock(st->mtx);
        st->player = std::move(player);
        st->eomSeen = st->player->getEndOfMediaCount();
        return JavaValue();
    });

    // --- com.nokia.mid.sound.Sound ---
    const char* NS = "com/nokia/mid/sound/Sound";
    auto nokiaOf = [](CldcVirtualMachine* vm, const Args& a) -> NokiaSoundPayload& {
        return ensurePayload<NokiaSoundPayload>(self(vm, a));
    };
    auto nokiaInitData = [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        JavaArray* arr = arrayArg(vm, a, 1);
        std::vector<uint8_t> data(static_cast<size_t>(arr->length));
        for (int32_t i = 0; i < arr->length; ++i) data[i] = static_cast<uint8_t>(arr->elements[i].i);
        // FORMAT_TONE (1) is a Nokia Smart Messaging ringtone, played by Sonivox's OTA parser; FORMAT_WAV (5)
        if (s.player) s.player->close();
        s.player = j2me::MmapiManager::createPlayer(data.data(), data.size(), arg(a, 2).i == 5 ? "audio/x-wav" : "audio/x-ota");
        s.toneNote = -1;
        return JavaValue();
    };
    auto nokiaInitTone = [nokiaOf](CldcVirtualMachine* vm, const Args& a, int64_t durationMs) {
        auto& s = nokiaOf(vm, a);
        int32_t freq = arg(a, 1).i;
        if (s.player) s.player->close();
        s.player.reset();
        s.toneNote = freq > 0 ? std::clamp(static_cast<int32_t>(std::lround(69 + 12 * std::log2(freq / 440.0))), 0, 127) : -1;
        s.toneMs = static_cast<int32_t>(std::clamp<int64_t>(durationMs, 0, 60000));
        return JavaValue();
    };
    vm->registerNative(NS, "<init>", "([BI)V", nokiaInitData);
    vm->registerNative(NS, "init", "([BI)V", nokiaInitData);
    vm->registerNative(NS, "<init>", "(IJ)V", [nokiaInitTone](CldcVirtualMachine* vm, const Args& a) { return nokiaInitTone(vm, a, arg(a, 2).l); });
    vm->registerNative(NS, "init", "(IJ)V", [nokiaInitTone](CldcVirtualMachine* vm, const Args& a) { return nokiaInitTone(vm, a, arg(a, 2).l); });
    vm->registerNative(NS, "<init>", "(II)V", [nokiaInitTone](CldcVirtualMachine* vm, const Args& a) { return nokiaInitTone(vm, a, arg(a, 2).i); });
    vm->registerNative(NS, "play", "(I)V", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        int32_t loop = arg(a, 1).i;
        if (s.toneNote >= 0) {
            j2me::MmapiManager::playTone(s.toneNote, s.toneMs, s.gain * 100 / 255);
        } else if (s.player) {
            s.player->stop();
            s.player->setMediaTime(0);
            s.player->setLoopCount(loop <= 0 ? -1 : loop);
            s.player->getVolumeControl()->setLevel(s.gain * 100 / 255);
            s.player->prefetch();
            s.player->start();
        }
        return JavaValue();
    });
    vm->registerNative(NS, "stop", "()V", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        if (s.player) s.player->stop();
        return JavaValue();
    });
    vm->registerNative(NS, "release", "()V", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        if (s.player) s.player->close();
        s.player.reset();
        s.toneNote = -1;
        return JavaValue();
    });
    vm->registerNative(NS, "setGain", "(I)V", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        s.gain = std::clamp(arg(a, 1).i, 0, 255);
        if (s.player) s.player->getVolumeControl()->setLevel(s.gain * 100 / 255);
        return JavaValue();
    });
    vm->registerNative(NS, "getGain", "()I", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(nokiaOf(vm, a).gain);
    });
    vm->registerNative(NS, "getState", "()I", [nokiaOf](CldcVirtualMachine* vm, const Args& a) {
        auto& s = nokiaOf(vm, a);
        // SOUND_PLAYING = 0, SOUND_STOPPED = 1, SOUND_UNINITIALIZED = 3
        if (!s.player && s.toneNote < 0) return JavaValue(3);
        return JavaValue(s.player && s.player->getState() == j2me::PLAYER_STARTED ? 0 : 1);
    });
    vm->registerNative(NS, "getSupportedFormats", "()[I", [](CldcVirtualMachine* vm, const Args&) {
        JavaArray* arr = vm->allocateArray('I', 2);
        arr->elements[0] = JavaValue(1);
        arr->elements[1] = JavaValue(5);
        return refV(arr);
    });
    vm->registerNative(NS, "setSoundListener", "(Lcom/nokia/mid/sound/SoundListener;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
}

// ---------------------------------------------------------------------------
// Java SE security / crypto subset used by Android-era MIDlets:
// MessageDigest (MD5, SHA-1, SHA-256), SecureRandom, Cipher (AES ECB/CBC/GCM),
// SecretKeySpec, IvParameterSpec, GCMParameterSpec, Base64, String <-> Charset
// ---------------------------------------------------------------------------

namespace crypto {

inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::vector<uint8_t> md5(const std::vector<uint8_t>& msg) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    std::vector<uint8_t> m = msg;
    uint64_t bits = static_cast<uint64_t>(msg.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; ++i) w[i] = m[off + 4 * i] | (m[off + 4 * i + 1] << 8) | (m[off + 4 * i + 2] << 16) | (static_cast<uint32_t>(m[off + 4 * i + 3]) << 24);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; ++i) {
            uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t t = d;
            d = c;
            c = b;
            b = b + rotl(a + f + K[i] + w[g], R[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    std::vector<uint8_t> out;
    for (uint32_t v : h) for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    return out;
}

std::vector<uint8_t> sha1(const std::vector<uint8_t>& msg) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<uint8_t> m = msg;
    uint64_t bits = static_cast<uint64_t>(msg.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = (static_cast<uint32_t>(m[off + 4 * i]) << 24) | (m[off + 4 * i + 1] << 16) | (m[off + 4 * i + 2] << 8) | m[off + 4 * i + 3];
        for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::vector<uint8_t> out;
    for (uint32_t v : h) for (int i = 3; i >= 0; --i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    return out;
}

std::vector<uint8_t> sha256(const std::vector<uint8_t>& msg) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<uint8_t> m = msg;
    uint64_t bits = static_cast<uint64_t>(msg.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (static_cast<uint32_t>(m[off + 4 * i]) << 24) | (m[off + 4 * i + 1] << 16) | (m[off + 4 * i + 2] << 8) | m[off + 4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::vector<uint8_t> out;
    for (uint32_t v : h) for (int i = 3; i >= 0; --i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    return out;
}

// AES block cipher (FIPS-197), 128/192/256-bit keys
class Aes {
public:
    explicit Aes(const std::vector<uint8_t>& key) {
        static bool init = false;
        if (!init) { buildTables(); init = true; }
        const int nk = static_cast<int>(key.size() / 4);
        rounds_ = nk + 6;
        const int total = 4 * (rounds_ + 1);
        w_.resize(total);
        for (int i = 0; i < nk; ++i) w_[i] = (static_cast<uint32_t>(key[4 * i]) << 24) | (key[4 * i + 1] << 16) | (key[4 * i + 2] << 8) | key[4 * i + 3];
        uint8_t rcon = 1;
        for (int i = nk; i < total; ++i) {
            uint32_t t = w_[i - 1];
            if (i % nk == 0) {
                t = subWord((t << 8) | (t >> 24)) ^ (static_cast<uint32_t>(rcon) << 24);
                rcon = xtime(rcon);
            } else if (nk > 6 && i % nk == 4) {
                t = subWord(t);
            }
            w_[i] = w_[i - nk] ^ t;
        }
    }
    void encrypt(const uint8_t in[16], uint8_t out[16]) const {
        uint8_t s[16];
        for (int i = 0; i < 16; ++i) s[i] = in[i];
        addRound(s, 0);
        for (int r = 1; r <= rounds_; ++r) {
            for (auto& b : s) b = sbox()[b];
            shiftRows(s);
            if (r != rounds_) mixColumns(s);
            addRound(s, r);
        }
        for (int i = 0; i < 16; ++i) out[i] = s[i];
    }
    void decrypt(const uint8_t in[16], uint8_t out[16]) const {
        uint8_t s[16];
        for (int i = 0; i < 16; ++i) s[i] = in[i];
        addRound(s, rounds_);
        for (int r = rounds_ - 1; r >= 0; --r) {
            invShiftRows(s);
            for (auto& b : s) b = invSbox()[b];
            addRound(s, r);
            if (r != 0) invMixColumns(s);
        }
        for (int i = 0; i < 16; ++i) out[i] = s[i];
    }

private:
    int rounds_{10};
    std::vector<uint32_t> w_;

    static uint8_t* sbox() { static uint8_t t[256]; return t; }
    static uint8_t* invSbox() { static uint8_t t[256]; return t; }
    static uint8_t xtime(uint8_t x) { return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
    static uint8_t mul(uint8_t a, uint8_t b) {
        uint8_t p = 0;
        while (b) {
            if (b & 1) p ^= a;
            a = xtime(a);
            b >>= 1;
        }
        return p;
    }
    static void buildTables() {
        uint8_t p = 1, q = 1;
        do {
            p = static_cast<uint8_t>(p ^ (p << 1) ^ ((p & 0x80) ? 0x1b : 0));
            q ^= q << 1; q ^= q << 2; q ^= q << 4;
            if (q & 0x80) q ^= 0x09;
            uint8_t x = static_cast<uint8_t>(q ^ ((q << 1) | (q >> 7)) ^ ((q << 2) | (q >> 6)) ^ ((q << 3) | (q >> 5)) ^ ((q << 4) | (q >> 4)));
            sbox()[p] = static_cast<uint8_t>(x ^ 0x63);
        } while (p != 1);
        sbox()[0] = 0x63;
        for (int i = 0; i < 256; ++i) invSbox()[sbox()[i]] = static_cast<uint8_t>(i);
    }
    static uint32_t subWord(uint32_t w) {
        return (static_cast<uint32_t>(sbox()[w >> 24]) << 24) | (sbox()[(w >> 16) & 0xff] << 16) | (sbox()[(w >> 8) & 0xff] << 8) | sbox()[w & 0xff];
    }
    void addRound(uint8_t s[16], int r) const {
        for (int c = 0; c < 4; ++c) {
            uint32_t k = w_[4 * r + c];
            s[4 * c] ^= k >> 24; s[4 * c + 1] ^= k >> 16; s[4 * c + 2] ^= k >> 8; s[4 * c + 3] ^= k;
        }
    }
    static void shiftRows(uint8_t s[16]) {
        uint8_t t[16];
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) t[4 * c + r] = s[4 * ((c + r) % 4) + r];
        for (int i = 0; i < 16; ++i) s[i] = t[i];
    }
    static void invShiftRows(uint8_t s[16]) {
        uint8_t t[16];
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) t[4 * ((c + r) % 4) + r] = s[4 * c + r];
        for (int i = 0; i < 16; ++i) s[i] = t[i];
    }
    static void mixColumns(uint8_t s[16]) {
        for (int c = 0; c < 4; ++c) {
            uint8_t* a = s + 4 * c;
            uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
            a[0] = mul(a0, 2) ^ mul(a1, 3) ^ a2 ^ a3;
            a[1] = a0 ^ mul(a1, 2) ^ mul(a2, 3) ^ a3;
            a[2] = a0 ^ a1 ^ mul(a2, 2) ^ mul(a3, 3);
            a[3] = mul(a0, 3) ^ a1 ^ a2 ^ mul(a3, 2);
        }
    }
    static void invMixColumns(uint8_t s[16]) {
        for (int c = 0; c < 4; ++c) {
            uint8_t* a = s + 4 * c;
            uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
            a[0] = mul(a0, 14) ^ mul(a1, 11) ^ mul(a2, 13) ^ mul(a3, 9);
            a[1] = mul(a0, 9) ^ mul(a1, 14) ^ mul(a2, 11) ^ mul(a3, 13);
            a[2] = mul(a0, 13) ^ mul(a1, 9) ^ mul(a2, 14) ^ mul(a3, 11);
            a[3] = mul(a0, 11) ^ mul(a1, 13) ^ mul(a2, 9) ^ mul(a3, 14);
        }
    }
};

// GF(2^128) multiply for GHASH
void gfMul(uint8_t x[16], const uint8_t h[16]) {
    uint8_t z[16] = {0}, v[16];
    for (int i = 0; i < 16; ++i) v[i] = h[i];
    for (int i = 0; i < 128; ++i) {
        if (x[i / 8] & (0x80 >> (i % 8))) for (int j = 0; j < 16; ++j) z[j] ^= v[j];
        bool lsb = v[15] & 1;
        for (int j = 15; j > 0; --j) v[j] = static_cast<uint8_t>((v[j] >> 1) | (v[j - 1] << 7));
        v[0] >>= 1;
        if (lsb) v[0] ^= 0xe1;
    }
    for (int i = 0; i < 16; ++i) x[i] = z[i];
}

void ghashBlocks(uint8_t y[16], const uint8_t h[16], const std::vector<uint8_t>& data) {
    for (size_t off = 0; off < data.size(); off += 16) {
        for (size_t i = 0; i < 16 && off + i < data.size(); ++i) y[i] ^= data[off + i];
        gfMul(y, h);
    }
}

void incr32(uint8_t ctr[16]) {
    for (int i = 15; i >= 12; --i) if (++ctr[i]) break;
}

// AES-GCM. Returns ciphertext||tag on encrypt; on decrypt returns plaintext or false on tag mismatch.
bool gcm(const Aes& aes, bool enc, const std::vector<uint8_t>& iv, const std::vector<uint8_t>& aad,
         const std::vector<uint8_t>& input, size_t tagLen, std::vector<uint8_t>& out) {
    uint8_t h[16] = {0};
    aes.encrypt(h, h);
    uint8_t j0[16] = {0};
    if (iv.size() == 12) {
        for (int i = 0; i < 12; ++i) j0[i] = iv[i];
        j0[15] = 1;
    } else {
        ghashBlocks(j0, h, iv);
        uint8_t len[16] = {0};
        uint64_t bits = static_cast<uint64_t>(iv.size()) * 8;
        for (int i = 0; i < 8; ++i) len[15 - i] = static_cast<uint8_t>(bits >> (8 * i));
        for (int i = 0; i < 16; ++i) j0[i] ^= len[i];
        gfMul(j0, h);
    }
    std::vector<uint8_t> data = input;
    std::vector<uint8_t> tagIn;
    if (!enc) {
        if (data.size() < tagLen) return false;
        tagIn.assign(data.end() - static_cast<long>(tagLen), data.end());
        data.resize(data.size() - tagLen);
    }
    std::vector<uint8_t> result(data.size());
    uint8_t ctr[16];
    for (int i = 0; i < 16; ++i) ctr[i] = j0[i];
    for (size_t off = 0; off < data.size(); off += 16) {
        incr32(ctr);
        uint8_t ks[16];
        aes.encrypt(ctr, ks);
        for (size_t i = 0; i < 16 && off + i < data.size(); ++i) result[off + i] = data[off + i] ^ ks[i];
    }
    const std::vector<uint8_t>& cipherText = enc ? result : data;
    uint8_t s[16] = {0};
    ghashBlocks(s, h, aad);
    ghashBlocks(s, h, cipherText);
    uint8_t len[16] = {0};
    uint64_t aBits = static_cast<uint64_t>(aad.size()) * 8, cBits = static_cast<uint64_t>(cipherText.size()) * 8;
    for (int i = 0; i < 8; ++i) {
        len[7 - i] = static_cast<uint8_t>(aBits >> (8 * i));
        len[15 - i] = static_cast<uint8_t>(cBits >> (8 * i));
    }
    for (int i = 0; i < 16; ++i) s[i] ^= len[i];
    gfMul(s, h);
    uint8_t ekj0[16];
    aes.encrypt(j0, ekj0);
    std::vector<uint8_t> tag(tagLen);
    for (size_t i = 0; i < tagLen; ++i) tag[i] = s[i] ^ ekj0[i];
    if (enc) {
        out = result;
        out.insert(out.end(), tag.begin(), tag.end());
        return true;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < tagLen; ++i) diff |= tag[i] ^ tagIn[i];
    if (diff) return false;
    out = result;
    return true;
}

void randomBytes(uint8_t* p, size_t n) {
    static std::mutex mu;
    static std::mt19937_64 rng = [] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())};
        return std::mt19937_64(seq);
    }();
    std::lock_guard<std::mutex> l(mu);
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(rng() >> 24);
}

std::string base64(const std::vector<uint8_t>& in, bool url, bool pad) {
    const char* tbl = url ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
                          : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += tbl[v & 63];
    }
    if (i + 1 == in.size()) {
        uint32_t v = in[i] << 16;
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63];
        if (pad) out += "==";
    } else if (i + 2 == in.size()) {
        uint32_t v = (in[i] << 16) | (in[i + 1] << 8);
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63];
        if (pad) out += '=';
    }
    return out;
}

bool unbase64(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=' || c == '\r' || c == '\n') continue;
        else return false;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>(acc >> bits));
        }
    }
    return true;
}

} // namespace crypto

Charset charsetOfObject(CldcVirtualMachine* vm, JavaObject* o) {
    if (!o) vm->throwJava("java/lang/NullPointerException");
    if (auto* p = getPayload<CharsetPayload>(o)) {
        std::string n = upperAscii(p->name);
        if (n == "UTF-8" || n == "UTF8") return Charset::Utf8;
        if (n == "UTF-16BE" || n == "UTF-16") return Charset::Utf16BE;
        return Charset::Latin1;
    }
    return charsetFor(vm, o);
}

std::vector<uint8_t> runCipher(CldcVirtualMachine* vm, CipherPayload& c, const std::vector<uint8_t>& input) {
    if (c.key.size() != 16 && c.key.size() != 24 && c.key.size() != 32) vm->throwJava("java/security/InvalidKeyException", "Invalid AES key length");
    crypto::Aes aes(c.key);
    const bool enc = c.opmode == 1;
    std::vector<uint8_t> out;
    if (c.mode == "GCM") {
        if (!crypto::gcm(aes, enc, c.iv, c.aad, input, static_cast<size_t>(c.tagBits / 8), out)) {
            vm->throwJava("javax/crypto/AEADBadTagException", "Tag mismatch");
        }
        c.aad.clear();
        return out;
    }
    std::vector<uint8_t> data = input;
    if (enc && c.pkcs5) {
        uint8_t pad = static_cast<uint8_t>(16 - data.size() % 16);
        data.insert(data.end(), pad, pad);
    }
    if (data.size() % 16) vm->throwJava("javax/crypto/IllegalBlockSizeException", "Input length not multiple of 16 bytes");
    std::vector<uint8_t> prev = c.iv;
    prev.resize(16);
    out.resize(data.size());
    for (size_t off = 0; off < data.size(); off += 16) {
        uint8_t blk[16];
        if (enc) {
            for (int i = 0; i < 16; ++i) blk[i] = data[off + i] ^ (c.mode == "CBC" ? prev[i] : 0);
            aes.encrypt(blk, &out[off]);
            if (c.mode == "CBC") prev.assign(out.begin() + static_cast<long>(off), out.begin() + static_cast<long>(off) + 16);
        } else {
            aes.decrypt(&data[off], blk);
            for (int i = 0; i < 16; ++i) out[off + i] = blk[i] ^ (c.mode == "CBC" ? prev[i] : 0);
            if (c.mode == "CBC") prev.assign(data.begin() + static_cast<long>(off), data.begin() + static_cast<long>(off) + 16);
        }
    }
    if (!enc && c.pkcs5) {
        uint8_t pad = out.empty() ? 0 : out.back();
        if (pad == 0 || pad > 16 || pad > out.size()) vm->throwJava("javax/crypto/BadPaddingException", "Given final block not properly padded");
        out.resize(out.size() - pad);
    }
    return out;
}

void registerSeCrypto(CldcVirtualMachine* vm) {
    auto setStr = [](JavaObject* o, const std::u16string& s) {
        if (auto* js = asString(o)) {
            std::lock_guard<std::mutex> lock(g_utf16CacheMutex);
            js->value = utf16ToUtf8(s);
            js->utf16Cache = s;
            js->utf16Valid = true;
        }
    };
    // String <-> Charset
    const char* S = "java/lang/String";
    vm->registerNative(S, "getBytes", "(Ljava/nio/charset/Charset;)[B", [](CldcVirtualMachine* vm, const Args& a) {
        std::vector<uint8_t> b = encodeString(strArg(vm, a, 0), charsetOfObject(vm, arg(a, 1).ref));
        return refV(newByteArray(vm, b.data(), b.size()));
    });
    vm->registerNative(S, "<init>", "([BLjava/nio/charset/Charset;)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        setStr(self(vm, a), decodeBytes(bytesOf(arr, 0, arr->length), charsetOfObject(vm, a[2].ref)));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([BIILjava/nio/charset/Charset;)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        setStr(self(vm, a), decodeBytes(bytesOf(arr, a[2].i, a[3].i), charsetOfObject(vm, a[4].ref)));
        return JavaValue();
    });

    // MessageDigest
    const char* MD = "java/security/MessageDigest";
    vm->registerNative(MD, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;", [](CldcVirtualMachine* vm, const Args& a) {
        std::string alg = upperAscii(narrowArg(vm, a, 0));
        alg.erase(std::remove(alg.begin(), alg.end(), '-'), alg.end());
        if (alg != "MD5" && alg != "SHA1" && alg != "SHA" && alg != "SHA256") {
            vm->throwJava("java/security/NoSuchAlgorithmException", narrowArg(vm, a, 0));
        }
        JavaObject* o = newNativeObject(vm, "java/security/MessageDigest");
        ensurePayload<DigestPayload>(o).alg = alg == "SHA" ? "SHA1" : alg;
        return refV(o);
    });
    auto digestOf = [](CldcVirtualMachine* vm, JavaObject* o) -> DigestPayload& {
        auto* p = getPayload<DigestPayload>(o);
        if (!p) vm->throwJava("java/lang/IllegalStateException");
        return *p;
    };
    auto finish = [](DigestPayload& d) {
        std::vector<uint8_t> r = d.alg == "MD5" ? crypto::md5(d.buf) : d.alg == "SHA1" ? crypto::sha1(d.buf) : crypto::sha256(d.buf);
        d.buf.clear();
        return r;
    };
    vm->registerNative(MD, "update", "([B)V", [digestOf](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto b = bytesOf(arr, 0, arr->length);
        auto& d = digestOf(vm, self(vm, a));
        d.buf.insert(d.buf.end(), b.begin(), b.end());
        return JavaValue();
    });
    vm->registerNative(MD, "update", "([BII)V", [digestOf](CldcVirtualMachine* vm, const Args& a) {
        auto b = bytesOf(arrayArg(vm, a, 1), a[2].i, a[3].i);
        auto& d = digestOf(vm, self(vm, a));
        d.buf.insert(d.buf.end(), b.begin(), b.end());
        return JavaValue();
    });
    vm->registerNative(MD, "update", "(B)V", [digestOf](CldcVirtualMachine* vm, const Args& a) {
        digestOf(vm, self(vm, a)).buf.push_back(static_cast<uint8_t>(a[1].i));
        return JavaValue();
    });
    vm->registerNative(MD, "digest", "()[B", [digestOf, finish](CldcVirtualMachine* vm, const Args& a) {
        auto r = finish(digestOf(vm, self(vm, a)));
        return refV(newByteArray(vm, r.data(), r.size()));
    });
    vm->registerNative(MD, "digest", "([B)[B", [digestOf, finish](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto b = bytesOf(arr, 0, arr->length);
        auto& d = digestOf(vm, self(vm, a));
        d.buf.insert(d.buf.end(), b.begin(), b.end());
        auto r = finish(d);
        return refV(newByteArray(vm, r.data(), r.size()));
    });
    vm->registerNative(MD, "reset", "()V", [digestOf](CldcVirtualMachine* vm, const Args& a) {
        digestOf(vm, self(vm, a)).buf.clear();
        return JavaValue();
    });
    vm->registerNative(MD, "getDigestLength", "()I", [digestOf](CldcVirtualMachine* vm, const Args& a) {
        auto& d = digestOf(vm, self(vm, a));
        return JavaValue(d.alg == "MD5" ? 16 : d.alg == "SHA1" ? 20 : 32);
    });

    // SecureRandom
    const char* SR = "java/security/SecureRandom";
    vm->registerNative(SR, "<init>", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(SR, "<init>", "([B)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(SR, "nextBytes", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        std::vector<uint8_t> b(static_cast<size_t>(arr->length));
        crypto::randomBytes(b.data(), b.size());
        for (int32_t i = 0; i < arr->length; ++i) arr->elements[i] = JavaValue(static_cast<int32_t>(static_cast<int8_t>(b[i])));
        return JavaValue();
    });
    auto rnd32 = [] {
        uint32_t v;
        crypto::randomBytes(reinterpret_cast<uint8_t*>(&v), sizeof v);
        return v;
    };
    vm->registerNative(SR, "nextInt", "()I", [rnd32](CldcVirtualMachine*, const Args&) { return JavaValue(static_cast<int32_t>(rnd32())); });
    vm->registerNative(SR, "nextInt", "(I)I", [rnd32](CldcVirtualMachine* vm, const Args& a) {
        if (a[1].i <= 0) vm->throwJava("java/lang/IllegalArgumentException", "bound must be positive");
        return JavaValue(static_cast<int32_t>(rnd32() % static_cast<uint32_t>(a[1].i)));
    });
    vm->registerNative(SR, "nextLong", "()J", [rnd32](CldcVirtualMachine*, const Args&) {
        return JavaValue(static_cast<int64_t>((static_cast<uint64_t>(rnd32()) << 32) | rnd32()));
    });
    vm->registerNative(SR, "nextBoolean", "()Z", [rnd32](CldcVirtualMachine*, const Args&) { return boolV(rnd32() & 1); });
    vm->registerNative(SR, "nextDouble", "()D", [rnd32](CldcVirtualMachine*, const Args&) {
        uint64_t v = (static_cast<uint64_t>(rnd32()) << 21) ^ rnd32();
        return JavaValue(static_cast<double>(v & ((1ULL << 53) - 1)) / static_cast<double>(1ULL << 53));
    });

    // Key / parameter specs
    vm->registerNative("javax/crypto/spec/SecretKeySpec", "<init>", "([BLjava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto& k = ensurePayload<KeyPayload>(self(vm, a));
        k.key = bytesOf(arr, 0, arr->length);
        k.alg = narrowArg(vm, a, 2);
        return JavaValue();
    });
    vm->registerNative("javax/crypto/spec/SecretKeySpec", "<init>", "([BIILjava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& k = ensurePayload<KeyPayload>(self(vm, a));
        k.key = bytesOf(arrayArg(vm, a, 1), a[2].i, a[3].i);
        k.alg = narrowArg(vm, a, 4);
        return JavaValue();
    });
    vm->registerNative("javax/crypto/spec/SecretKeySpec", "getEncoded", "()[B", [](CldcVirtualMachine* vm, const Args& a) {
        auto* k = getPayload<KeyPayload>(self(vm, a));
        return refV(k ? newByteArray(vm, k->key.data(), k->key.size()) : nullptr);
    });
    vm->registerNative("javax/crypto/spec/SecretKeySpec", "getAlgorithm", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* k = getPayload<KeyPayload>(self(vm, a));
        return newStrUtf8(vm, k ? k->alg : std::string());
    });
    vm->registerNative("javax/crypto/spec/GCMParameterSpec", "<init>", "(I[B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 2);
        auto& p = ensurePayload<ParamSpecPayload>(self(vm, a));
        p.tagBits = a[1].i;
        p.iv = bytesOf(arr, 0, arr->length);
        return JavaValue();
    });
    vm->registerNative("javax/crypto/spec/GCMParameterSpec", "<init>", "(I[BII)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ParamSpecPayload>(self(vm, a));
        p.tagBits = a[1].i;
        p.iv = bytesOf(arrayArg(vm, a, 2), a[3].i, a[4].i);
        return JavaValue();
    });
    vm->registerNative("javax/crypto/spec/IvParameterSpec", "<init>", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        ensurePayload<ParamSpecPayload>(self(vm, a)).iv = bytesOf(arr, 0, arr->length);
        return JavaValue();
    });
    vm->registerNative("javax/crypto/spec/IvParameterSpec", "getIV", "()[B", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ParamSpecPayload>(self(vm, a));
        return refV(p ? newByteArray(vm, p->iv.data(), p->iv.size()) : nullptr);
    });

    // Cipher
    const char* C = "javax/crypto/Cipher";
    vm->registerNative(C, "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;", [](CldcVirtualMachine* vm, const Args& a) {
        std::string t = upperAscii(narrowArg(vm, a, 0));
        std::vector<std::string> parts;
        for (size_t s = 0, e; s <= t.size(); s = e + 1) {
            e = t.find('/', s);
            if (e == std::string::npos) e = t.size();
            parts.push_back(t.substr(s, e - s));
        }
        if (parts[0] != "AES") vm->throwJava("java/security/NoSuchAlgorithmException", narrowArg(vm, a, 0));
        JavaObject* o = newNativeObject(vm, "javax/crypto/Cipher");
        auto& c = ensurePayload<CipherPayload>(o);
        c.mode = parts.size() > 1 ? parts[1] : "ECB";
        c.pkcs5 = parts.size() > 2 ? parts[2] != "NOPADDING" : true;
        if (c.mode == "GCM") c.pkcs5 = false;
        if (c.mode != "ECB" && c.mode != "CBC" && c.mode != "GCM") vm->throwJava("java/security/NoSuchAlgorithmException", narrowArg(vm, a, 0));
        return refV(o);
    });
    vm->registerNativeStatic(C, "ENCRYPT_MODE", [](CldcVirtualMachine*, const Args&) { return JavaValue(1); });
    vm->registerNativeStatic(C, "DECRYPT_MODE", [](CldcVirtualMachine*, const Args&) { return JavaValue(2); });
    auto cipherOf = [](CldcVirtualMachine* vm, JavaObject* o) -> CipherPayload& {
        auto* c = getPayload<CipherPayload>(o);
        if (!c) vm->throwJava("java/lang/IllegalStateException", "Cipher not initialized");
        return *c;
    };
    auto init = [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = cipherOf(vm, self(vm, a));
        auto* k = getPayload<KeyPayload>(arg(a, 2).ref);
        if (!k) vm->throwJava("java/security/InvalidKeyException", "No key");
        c.opmode = a[1].i;
        c.key = k->key;
        c.iv.clear();
        c.aad.clear();
        c.buf.clear();
        c.tagBits = 128;
        if (a.size() > 3) {
            if (auto* p = getPayload<ParamSpecPayload>(a[3].ref)) {
                c.iv = p->iv;
                c.tagBits = p->tagBits;
            }
        }
        if (c.iv.empty() && c.mode != "ECB") {
            c.iv.resize(c.mode == "GCM" ? 12 : 16);
            crypto::randomBytes(c.iv.data(), c.iv.size());
        }
        return JavaValue();
    };
    vm->registerNative(C, "init", "(ILjava/security/Key;)V", init);
    vm->registerNative(C, "init", "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V", init);
    vm->registerNative(C, "updateAAD", "([B)V", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto b = bytesOf(arr, 0, arr->length);
        auto& c = cipherOf(vm, self(vm, a));
        c.aad.insert(c.aad.end(), b.begin(), b.end());
        return JavaValue();
    });
    vm->registerNative(C, "update", "([B)[B", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto b = bytesOf(arr, 0, arr->length);
        auto& c = cipherOf(vm, self(vm, a));
        c.buf.insert(c.buf.end(), b.begin(), b.end());
        return refV(newByteArray(vm, nullptr, 0));
    });
    vm->registerNative(C, "doFinal", "()[B", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = cipherOf(vm, self(vm, a));
        auto r = runCipher(vm, c, c.buf);
        c.buf.clear();
        return refV(newByteArray(vm, r.data(), r.size()));
    });
    vm->registerNative(C, "doFinal", "([B)[B", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto& c = cipherOf(vm, self(vm, a));
        auto in = c.buf;
        auto b = bytesOf(arr, 0, arr->length);
        in.insert(in.end(), b.begin(), b.end());
        c.buf.clear();
        auto r = runCipher(vm, c, in);
        return refV(newByteArray(vm, r.data(), r.size()));
    });
    vm->registerNative(C, "doFinal", "([BII)[B", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = cipherOf(vm, self(vm, a));
        auto in = c.buf;
        auto b = bytesOf(arrayArg(vm, a, 1), a[2].i, a[3].i);
        in.insert(in.end(), b.begin(), b.end());
        c.buf.clear();
        auto r = runCipher(vm, c, in);
        return refV(newByteArray(vm, r.data(), r.size()));
    });
    vm->registerNative(C, "getIV", "()[B", [cipherOf](CldcVirtualMachine* vm, const Args& a) {
        auto& c = cipherOf(vm, self(vm, a));
        return refV(c.iv.empty() ? nullptr : newByteArray(vm, c.iv.data(), c.iv.size()));
    });
    vm->registerNative(C, "getBlockSize", "()I", [](CldcVirtualMachine*, const Args&) { return JavaValue(16); });

    // java.util.Base64
    const char* B64 = "java/util/Base64";
    auto coder = [](const char* cls, bool url, bool pad) {
        return [cls, url, pad](CldcVirtualMachine* vm, const Args&) {
            JavaObject* o = newNativeObject(vm, cls);
            auto& p = ensurePayload<Base64CoderPayload>(o);
            p.url = url;
            p.pad = pad;
            return refV(o);
        };
    };
    vm->registerNative(B64, "getEncoder", "()Ljava/util/Base64$Encoder;", coder("java/util/Base64$Encoder", false, true));
    vm->registerNative(B64, "getUrlEncoder", "()Ljava/util/Base64$Encoder;", coder("java/util/Base64$Encoder", true, true));
    vm->registerNative(B64, "getMimeEncoder", "()Ljava/util/Base64$Encoder;", coder("java/util/Base64$Encoder", false, true));
    vm->registerNative(B64, "getDecoder", "()Ljava/util/Base64$Decoder;", coder("java/util/Base64$Decoder", false, true));
    vm->registerNative(B64, "getUrlDecoder", "()Ljava/util/Base64$Decoder;", coder("java/util/Base64$Decoder", true, true));
    vm->registerNative(B64, "getMimeDecoder", "()Ljava/util/Base64$Decoder;", coder("java/util/Base64$Decoder", false, true));
    const char* ENC = "java/util/Base64$Encoder";
    auto encOpts = [](JavaObject* o) {
        auto* p = getPayload<Base64CoderPayload>(o);
        return p ? *p : Base64CoderPayload{};
    };
    vm->registerNative(ENC, "withoutPadding", "()Ljava/util/Base64$Encoder;", [encOpts](CldcVirtualMachine* vm, const Args& a) {
        auto opts = encOpts(self(vm, a));
        JavaObject* o = newNativeObject(vm, "java/util/Base64$Encoder");
        auto& p = ensurePayload<Base64CoderPayload>(o);
        p.url = opts.url;
        p.pad = false;
        return refV(o);
    });
    vm->registerNative(ENC, "encodeToString", "([B)Ljava/lang/String;", [encOpts](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto opts = encOpts(self(vm, a));
        return newStrUtf8(vm, crypto::base64(bytesOf(arr, 0, arr->length), opts.url, opts.pad));
    });
    vm->registerNative(ENC, "encode", "([B)[B", [encOpts](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto opts = encOpts(self(vm, a));
        std::string s = crypto::base64(bytesOf(arr, 0, arr->length), opts.url, opts.pad);
        return refV(newByteArray(vm, reinterpret_cast<const uint8_t*>(s.data()), s.size()));
    });
    const char* DEC = "java/util/Base64$Decoder";
    auto decode = [](CldcVirtualMachine* vm, const std::string& s) {
        std::vector<uint8_t> out;
        if (!crypto::unbase64(s, out)) vm->throwJava("java/lang/IllegalArgumentException", "Illegal base64 character");
        return refV(newByteArray(vm, out.data(), out.size()));
    };
    vm->registerNative(DEC, "decode", "(Ljava/lang/String;)[B", [decode](CldcVirtualMachine* vm, const Args& a) {
        return decode(vm, narrowArg(vm, a, 1));
    });
    vm->registerNative(DEC, "decode", "([B)[B", [decode](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        auto b = bytesOf(arr, 0, arr->length);
        return decode(vm, std::string(b.begin(), b.end()));
    });
}


} // namespace universal_loader::jvm
