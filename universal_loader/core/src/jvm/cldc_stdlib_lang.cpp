#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {

// ============================================================================
// java.lang.String
// ============================================================================

void registerString(CldcVirtualMachine* vm) {
    const char* S = "java/lang/String";

    auto setStr = [](JavaObject* o, const std::u16string& s) {
        if (auto* js = asString(o)) {
            std::lock_guard<std::mutex> lock(g_utf16CacheMutex);
            js->value = utf16ToUtf8(s);
            js->utf16Cache = s;
            js->utf16Valid = true;
        }
    };

    vm->registerNative(S, "<init>", "()V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        setStr(self(vm, a), u"");
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "(Ljava/lang/String;)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        setStr(self(vm, a), strArg(vm, a, 1));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([C)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        std::u16string s;
        for (auto& e : arr->elements) s.push_back(static_cast<char16_t>(e.i));
        setStr(self(vm, a), s);
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([CII)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        int32_t off = a[2].i, len = a[3].i;
        checkRange(vm, off, len, arr->length, "java/lang/StringIndexOutOfBoundsException");
        std::u16string s;
        for (int32_t i = 0; i < len; ++i) s.push_back(static_cast<char16_t>(arr->elements[off + i].i));
        setStr(self(vm, a), s);
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([B)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        setStr(self(vm, a), decodeBytes(bytesOf(arr, 0, arr->length), Charset::Utf8));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([BII)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        checkRange(vm, a[2].i, a[3].i, arr->length, "java/lang/StringIndexOutOfBoundsException");
        setStr(self(vm, a), decodeBytes(bytesOf(arr, a[2].i, a[3].i), Charset::Utf8));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([BLjava/lang/String;)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        setStr(self(vm, a), decodeBytes(bytesOf(arr, 0, arr->length), charsetFor(vm, a[2].ref)));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "([BIILjava/lang/String;)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        checkRange(vm, a[2].i, a[3].i, arr->length, "java/lang/StringIndexOutOfBoundsException");
        setStr(self(vm, a), decodeBytes(bytesOf(arr, a[2].i, a[3].i), charsetFor(vm, a[4].ref)));
        return JavaValue();
    });
    for (const char* sb : {"java/lang/StringBuffer", "java/lang/StringBuilder"}) {
        vm->registerNative(S, "<init>", std::string("(L") + sb + ";)V", [setStr](CldcVirtualMachine* vm, const Args& a) {
            auto* p = getPayload<StringBufferPayload>(arg(a, 1).ref);
            setStr(self(vm, a), p ? p->s : u"");
            return JavaValue();
        });
    }

    vm->registerNative(S, "length", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(strArg(vm, a, 0).size()));
    });
    vm->registerNative(S, "isEmpty", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        return boolV(asString(self(vm, a))->value.empty());
    });
    vm->registerNative(S, "charAt", "(I)C", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        int32_t i = a[1].i;
        if (i < 0 || i >= static_cast<int32_t>(s.size())) {
            vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(i));
        }
        return JavaValue(static_cast<int32_t>(s[i]));
    });
    vm->registerNative(S, "getChars", "(II[CI)V", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        int32_t b = a[1].i, e = a[2].i, dstBegin = a[4].i;
        JavaArray* dst = arrayArg(vm, a, 3);
        if (b < 0 || e > static_cast<int32_t>(s.size()) || b > e) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
        checkRange(vm, dstBegin, e - b, dst->length, "java/lang/ArrayIndexOutOfBoundsException");
        for (int32_t i = b; i < e; ++i) dst->elements[dstBegin + i - b] = JavaValue(static_cast<int32_t>(s[i]));
        return JavaValue();
    });
    vm->registerNative(S, "toCharArray", "()[C", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        JavaArray* arr = vm->allocateArray('C', static_cast<int32_t>(s.size()));
        for (size_t i = 0; i < s.size(); ++i) arr->elements[i] = JavaValue(static_cast<int32_t>(s[i]));
        return refV(arr);
    });
    vm->registerNative(S, "getBytes", "()[B", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* js = asString(self(vm, a));
        return refV(newByteArray(vm, reinterpret_cast<const uint8_t*>(js->value.data()), js->value.size()));
    });
    vm->registerNative(S, "getBytes", "(Ljava/lang/String;)[B", [](CldcVirtualMachine* vm, const Args& a) {
        std::vector<uint8_t> b = encodeString(strArg(vm, a, 0), charsetFor(vm, arg(a, 1).ref));
        return refV(newByteArray(vm, b.data(), b.size()));
    });
    vm->registerNative(S, "equals", "(Ljava/lang/Object;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* x = asString(self(vm, a));
        JavaString* y = asString(arg(a, 1).ref);
        return boolV(y && x->value == y->value);
    });
    vm->registerNative(S, "equalsIgnoreCase", "(Ljava/lang/String;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* y = asString(arg(a, 1).ref);
        if (!y) return boolV(false);
        std::u16string s1 = strArg(vm, a, 0), s2 = u16(y);
        if (s1.size() != s2.size()) return boolV(false);
        for (size_t i = 0; i < s1.size(); ++i) {
            if (s1[i] != s2[i] && toUpperChar(s1[i]) != toUpperChar(s2[i]) && toLowerChar(s1[i]) != toLowerChar(s2[i])) {
                return boolV(false);
            }
        }
        return boolV(true);
    });
    vm->registerNative(S, "compareTo", "(Ljava/lang/String;)I", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s1 = strArg(vm, a, 0), s2 = strArg(vm, a, 1);
        size_t n = std::min(s1.size(), s2.size());
        for (size_t i = 0; i < n; ++i) {
            if (s1[i] != s2[i]) return JavaValue(static_cast<int32_t>(s1[i]) - static_cast<int32_t>(s2[i]));
        }
        return JavaValue(static_cast<int32_t>(s1.size()) - static_cast<int32_t>(s2.size()));
    });
    vm->registerNative(S, "hashCode", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(stringHash(strArg(vm, a, 0)));
    });
    vm->registerNative(S, "regionMatches", "(ZILjava/lang/String;II)Z", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0), other = strArg(vm, a, 3);
        bool ignoreCase = a[1].i != 0;
        int32_t to = a[2].i, po = a[4].i, len = a[5].i;
        if (po < 0 || to < 0 || to + static_cast<int64_t>(len) > static_cast<int64_t>(s.size()) ||
            po + static_cast<int64_t>(len) > static_cast<int64_t>(other.size())) {
            return boolV(false);
        }
        for (int32_t i = 0; i < len; ++i) {
            char16_t c1 = s[to + i], c2 = other[po + i];
            if (c1 == c2) continue;
            if (ignoreCase && (toUpperChar(c1) == toUpperChar(c2) || toLowerChar(c1) == toLowerChar(c2))) continue;
            return boolV(false);
        }
        return boolV(true);
    });
    vm->registerNative(S, "startsWith", "(Ljava/lang/String;I)Z", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0), p = strArg(vm, a, 1);
        int32_t off = a[2].i;
        if (off < 0 || static_cast<size_t>(off) + p.size() > s.size()) return boolV(false);
        return boolV(s.compare(static_cast<size_t>(off), p.size(), p) == 0);
    });
    vm->registerNative(S, "startsWith", "(Ljava/lang/String;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0), p = strArg(vm, a, 1);
        return boolV(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
    });
    vm->registerNative(S, "endsWith", "(Ljava/lang/String;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0), p = strArg(vm, a, 1);
        return boolV(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    });
    vm->registerNative(S, "contains", "(Ljava/lang/CharSequence;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        return boolV(strArg(vm, a, 0).find(javaToString(vm, arg(a, 1).ref)) != std::u16string::npos);
    });

    auto indexOfChar = [](CldcVirtualMachine* vm, const Args& a, int32_t from) {
        std::u16string s = strArg(vm, a, 0);
        if (from < 0) from = 0;
        for (size_t i = static_cast<size_t>(from); i < s.size(); ++i) {
            if (s[i] == static_cast<char16_t>(a[1].i)) return JavaValue(static_cast<int32_t>(i));
        }
        return JavaValue(-1);
    };
    vm->registerNative(S, "indexOf", "(I)I", [indexOfChar](CldcVirtualMachine* vm, const Args& a) { return indexOfChar(vm, a, 0); });
    vm->registerNative(S, "indexOf", "(II)I", [indexOfChar](CldcVirtualMachine* vm, const Args& a) { return indexOfChar(vm, a, a[2].i); });
    auto indexOfStr = [](CldcVirtualMachine* vm, const Args& a, int32_t from) {
        std::u16string s = strArg(vm, a, 0), p = strArg(vm, a, 1);
        if (from < 0) from = 0;
        if (static_cast<size_t>(from) > s.size()) return JavaValue(p.empty() ? static_cast<int32_t>(s.size()) : -1);
        size_t r = s.find(p, static_cast<size_t>(from));
        return JavaValue(r == std::u16string::npos ? -1 : static_cast<int32_t>(r));
    };
    vm->registerNative(S, "indexOf", "(Ljava/lang/String;)I", [indexOfStr](CldcVirtualMachine* vm, const Args& a) { return indexOfStr(vm, a, 0); });
    vm->registerNative(S, "indexOf", "(Ljava/lang/String;I)I", [indexOfStr](CldcVirtualMachine* vm, const Args& a) { return indexOfStr(vm, a, a[2].i); });
    auto lastIndexOfChar = [](CldcVirtualMachine* vm, const Args& a, int32_t from) {
        std::u16string s = strArg(vm, a, 0);
        if (from >= static_cast<int32_t>(s.size())) from = static_cast<int32_t>(s.size()) - 1;
        for (int32_t i = from; i >= 0; --i) {
            if (s[i] == static_cast<char16_t>(a[1].i)) return JavaValue(i);
        }
        return JavaValue(-1);
    };
    vm->registerNative(S, "lastIndexOf", "(I)I", [lastIndexOfChar](CldcVirtualMachine* vm, const Args& a) {
        return lastIndexOfChar(vm, a, std::numeric_limits<int32_t>::max());
    });
    vm->registerNative(S, "lastIndexOf", "(II)I", [lastIndexOfChar](CldcVirtualMachine* vm, const Args& a) { return lastIndexOfChar(vm, a, a[2].i); });
    vm->registerNative(S, "lastIndexOf", "(Ljava/lang/String;)I", [](CldcVirtualMachine* vm, const Args& a) {
        size_t r = strArg(vm, a, 0).rfind(strArg(vm, a, 1));
        return JavaValue(r == std::u16string::npos ? -1 : static_cast<int32_t>(r));
    });
    vm->registerNative(S, "lastIndexOf", "(Ljava/lang/String;I)I", [](CldcVirtualMachine* vm, const Args& a) {
        if (a[2].i < 0) return JavaValue(-1);
        size_t r = strArg(vm, a, 0).rfind(strArg(vm, a, 1), static_cast<size_t>(a[2].i));
        return JavaValue(r == std::u16string::npos ? -1 : static_cast<int32_t>(r));
    });

    auto substring = [](CldcVirtualMachine* vm, const Args& a, int32_t b, int32_t e) {
        std::u16string s = strArg(vm, a, 0);
        if (b < 0 || e > static_cast<int32_t>(s.size()) || b > e) {
            vm->throwJava("java/lang/StringIndexOutOfBoundsException", "begin " + std::to_string(b) + ", end " + std::to_string(e) + ", length " + std::to_string(s.size()));
        }
        if (b == 0 && e == static_cast<int32_t>(s.size())) return a[0];
        return newStr(vm, s.substr(static_cast<size_t>(b), static_cast<size_t>(e - b)));
    };
    vm->registerNative(S, "substring", "(I)Ljava/lang/String;", [substring](CldcVirtualMachine* vm, const Args& a) {
        return substring(vm, a, a[1].i, static_cast<int32_t>(strArg(vm, a, 0).size()));
    });
    vm->registerNative(S, "substring", "(II)Ljava/lang/String;", [substring](CldcVirtualMachine* vm, const Args& a) {
        return substring(vm, a, a[1].i, a[2].i);
    });
    vm->registerNative(S, "concat", "(Ljava/lang/String;)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string other = strArg(vm, a, 1);
        if (other.empty()) return a[0];
        return newStr(vm, strArg(vm, a, 0) + other);
    });
    vm->registerNative(S, "replace", "(CC)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        std::replace(s.begin(), s.end(), static_cast<char16_t>(a[1].i), static_cast<char16_t>(a[2].i));
        return newStr(vm, s);
    });
    vm->registerNative(S, "replace", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        std::u16string from = javaToString(vm, arg(a, 1).ref), to = javaToString(vm, arg(a, 2).ref);
        if (from.empty()) return a[0];
        std::u16string out;
        size_t pos = 0, hit;
        while ((hit = s.find(from, pos)) != std::u16string::npos) {
            out += s.substr(pos, hit - pos) + to;
            pos = hit + from.size();
        }
        out += s.substr(pos);
        return newStr(vm, out);
    });
    vm->registerNative(S, "toLowerCase", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        for (auto& c : s) c = toLowerChar(c);
        return newStr(vm, s);
    });
    // Regex methods (java.util.regex subset via std::regex on UTF-8)
    auto splitImpl = [](CldcVirtualMachine* vm, const Args& a, int32_t limit) {
        std::string str = utf16ToUtf8(strArg(vm, a, 0));
        std::string pat = utf16ToUtf8(strArg(vm, a, 1));
        std::vector<std::string> parts;
        try {
            std::regex re(pat);
            size_t pos = 0;
            for (auto it = std::sregex_iterator(str.begin(), str.end(), re); it != std::sregex_iterator(); ++it) {
                if (limit > 0 && static_cast<int32_t>(parts.size()) == limit - 1) break;
                size_t mpos = static_cast<size_t>(it->position());
                size_t mlen = static_cast<size_t>(it->length());
                if (mlen == 0 && mpos == 0) continue; // no leading empty string for a zero-width match
                if (mlen == 0 && mpos >= str.size()) break;
                parts.push_back(str.substr(pos, mpos - pos));
                pos = mpos + mlen;
            }
            parts.push_back(str.substr(pos));
        } catch (const std::regex_error& e) {
            vm->throwJava("java/util/regex/PatternSyntaxException", e.what());
        }
        if (limit == 0 && parts.size() > 1) {
            while (!parts.empty() && parts.back().empty()) parts.pop_back();
        }
        auto* arr = vm->allocateArray('L', static_cast<int32_t>(parts.size()));
        for (size_t i = 0; i < parts.size(); ++i) arr->elements[i] = newStrUtf8(vm, parts[i]);
        return JavaValue(static_cast<JavaObject*>(arr));
    };
    vm->registerNative(S, "split", "(Ljava/lang/String;)[Ljava/lang/String;", [splitImpl](CldcVirtualMachine* vm, const Args& a) {
        return splitImpl(vm, a, 0);
    });
    vm->registerNative(S, "split", "(Ljava/lang/String;I)[Ljava/lang/String;", [splitImpl](CldcVirtualMachine* vm, const Args& a) {
        return splitImpl(vm, a, arg(a, 2).i);
    });
    // Java replacement strings use $1, the same syntax as std::regex
    auto regexReplace = [](CldcVirtualMachine* vm, const Args& a, bool all) {
        std::string str = utf16ToUtf8(strArg(vm, a, 0));
        try {
            std::regex re(utf16ToUtf8(strArg(vm, a, 1)));
            auto flags = all ? std::regex_constants::format_default : std::regex_constants::format_first_only;
            return newStrUtf8(vm, std::regex_replace(str, re, utf16ToUtf8(strArg(vm, a, 2)), flags));
        } catch (const std::regex_error& e) {
            vm->throwJava("java/util/regex/PatternSyntaxException", e.what());
        }
        return JavaValue();
    };
    vm->registerNative(S, "replaceAll", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", [regexReplace](CldcVirtualMachine* vm, const Args& a) {
        return regexReplace(vm, a, true);
    });
    vm->registerNative(S, "replaceFirst", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", [regexReplace](CldcVirtualMachine* vm, const Args& a) {
        return regexReplace(vm, a, false);
    });
    vm->registerNative(S, "matches", "(Ljava/lang/String;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        try {
            return boolV(std::regex_match(utf16ToUtf8(strArg(vm, a, 0)), std::regex(utf16ToUtf8(strArg(vm, a, 1)))));
        } catch (const std::regex_error& e) {
            vm->throwJava("java/util/regex/PatternSyntaxException", e.what());
        }
        return boolV(false);
    });
    vm->registerNative(S, "toUpperCase", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        for (auto& c : s) c = toUpperChar(c);
        return newStr(vm, s);
    });
    vm->registerNative(S, "trim", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 0);
        std::u16string t = trimmed(s);
        return t.size() == s.size() ? a[0] : newStr(vm, t);
    });
    vm->registerNative(S, "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        self(vm, a);
        return a[0];
    });
    vm->registerNative(S, "intern", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return refV(vm->intern(asString(self(vm, a))->value));
    });

    // Java SE String.format (used by some modded MIDlets)
    auto format = [](CldcVirtualMachine* vm, const std::u16string& fmt, JavaArray* args) {
        std::u16string out;
        int32_t next = 0;
        auto argAt = [&](int32_t idx) -> JavaObject* {
            if (!args || idx < 0 || idx >= args->length) vm->throwJava("java/util/MissingFormatArgumentException", "Format specifier missing argument");
            return args->elements[idx].ref;
        };
        auto widen = [](const std::string& s) { return std::u16string(s.begin(), s.end()); };
        for (size_t i = 0; i < fmt.size(); ++i) {
            char16_t c = fmt[i];
            if (c != u'%') {
                out.push_back(c);
                continue;
            }
            size_t j = i + 1;
            // %[index$][flags][width][.precision]conversion
            std::string spec = "%";
            int32_t index = -1;
            size_t k = j;
            while (k < fmt.size() && fmt[k] >= u'0' && fmt[k] <= u'9') ++k;
            if (k < fmt.size() && k > j && fmt[k] == u'$') {
                index = 0;
                for (size_t m = j; m < k; ++m) index = index * 10 + (fmt[m] - u'0');
                index -= 1;
                j = k + 1;
            }
            bool leftAlign = false;
            while (j < fmt.size() && std::u16string(u"-#+ 0,(").find(fmt[j]) != std::u16string::npos) {
                if (fmt[j] == u'-') leftAlign = true;
                if (fmt[j] != u',' && fmt[j] != u'(') spec.push_back(static_cast<char>(fmt[j]));
                ++j;
            }
            int32_t width = 0;
            while (j < fmt.size() && fmt[j] >= u'0' && fmt[j] <= u'9') width = width * 10 + (fmt[j++] - u'0');
            int32_t precision = -1;
            if (j < fmt.size() && fmt[j] == u'.') {
                precision = 0;
                ++j;
                while (j < fmt.size() && fmt[j] >= u'0' && fmt[j] <= u'9') precision = precision * 10 + (fmt[j++] - u'0');
            }
            if (j >= fmt.size()) vm->throwJava("java/util/UnknownFormatConversionException", "%");
            char16_t conv = fmt[j];
            i = j;
            if (conv == u'%') { out.push_back(u'%'); continue; }
            if (conv == u'n') { out.push_back(u'\n'); continue; }
            JavaObject* o = argAt(index >= 0 ? index : next++);
            if (width > 0) spec += std::to_string(width);
            if (precision >= 0) spec += "." + std::to_string(precision);
            auto* box = getPayload<BoxPayload>(o);
            std::string cls = o ? vm->classNameOf(o) : std::string();
            char buf[512];
            std::u16string piece;
            switch (conv) {
                case u'd': case u'x': case u'X': case u'o': {
                    if (!box) { piece = u"null"; break; }
                    int64_t v = cls == "java/lang/Long" ? box->v.l : static_cast<int64_t>(box->v.i);
                    if (conv != u'd' && cls != "java/lang/Long") v = static_cast<uint32_t>(v);
                    std::string s = spec;
                    s.erase(std::remove(s.begin(), s.end(), '.'), s.end());
                    snprintf(buf, sizeof buf, (s + "ll" + static_cast<char>(conv)).c_str(), static_cast<long long>(v));
                    piece = widen(buf);
                    break;
                }
                case u'f': case u'e': case u'E': case u'g': case u'G': {
                    if (!box) { piece = u"null"; break; }
                    double v = cls == "java/lang/Double" ? box->v.d : cls == "java/lang/Float" ? box->v.f
                             : cls == "java/lang/Long" ? static_cast<double>(box->v.l) : box->v.i;
                    snprintf(buf, sizeof buf, (spec + static_cast<char>(conv)).c_str(), v);
                    piece = widen(buf);
                    break;
                }
                case u'c': case u'C': {
                    piece = box ? std::u16string(1, static_cast<char16_t>(box->v.i)) : u"null";
                    break;
                }
                case u'b': case u'B': {
                    bool b = cls == "java/lang/Boolean" ? (box && box->v.i) : o != nullptr;
                    piece = b ? u"true" : u"false";
                    break;
                }
                default: { // s, S and anything else: toString()
                    piece = o ? javaToString(vm, o) : u"null";
                    if (precision >= 0 && static_cast<size_t>(precision) < piece.size()) piece.resize(precision);
                    if (conv == u'S') for (auto& ch : piece) if (ch >= u'a' && ch <= u'z') ch -= 32;
                    break;
                }
            }
            if (conv == u'X' || conv == u'E' || conv == u'G' || conv == u'B') for (auto& ch : piece) if (ch >= u'a' && ch <= u'z') ch -= 32;
            if (static_cast<int32_t>(piece.size()) < width) {
                std::u16string pad(width - piece.size(), u' ');
                piece = leftAlign ? piece + pad : pad + piece;
            }
            out += piece;
        }
        return out;
    };
    vm->registerNative(S, "format", "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;", [format](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, format(vm, strArg(vm, a, 0), dynamic_cast<JavaArray*>(arg(a, 1).ref)));
    });
    vm->registerNative(S, "format", "(Ljava/util/Locale;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;", [format](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, format(vm, strArg(vm, a, 1), dynamic_cast<JavaArray*>(arg(a, 2).ref)));
    });

    // Static valueOf / copyValueOf
    vm->registerNative(S, "valueOf", "(Ljava/lang/Object;)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, javaToString(vm, arg(a, 0).ref));
    });
    for (char t : std::string("ZCIJFD")) {
        vm->registerNative(S, "valueOf", std::string("(") + t + ")Ljava/lang/String;", [t](CldcVirtualMachine* vm, const Args& a) {
            return newStr(vm, primitiveToString(t, arg(a, 0)));
        });
    }
    auto fromChars = [](CldcVirtualMachine* vm, JavaArray* arr, int32_t off, int32_t len) {
        checkRange(vm, off, len, arr->length, "java/lang/StringIndexOutOfBoundsException");
        std::u16string s;
        for (int32_t i = 0; i < len; ++i) s.push_back(static_cast<char16_t>(arr->elements[off + i].i));
        return newStr(vm, s);
    };
    vm->registerNative(S, "valueOf", "([C)Ljava/lang/String;", [fromChars](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 0);
        return fromChars(vm, arr, 0, arr->length);
    });
    vm->registerNative(S, "valueOf", "([CII)Ljava/lang/String;", [fromChars](CldcVirtualMachine* vm, const Args& a) {
        return fromChars(vm, arrayArg(vm, a, 0), a[1].i, a[2].i);
    });
    vm->registerNative(S, "copyValueOf", "([C)Ljava/lang/String;", [fromChars](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 0);
        return fromChars(vm, arr, 0, arr->length);
    });
}

// ============================================================================
// java.lang.StringBuffer / StringBuilder
// ============================================================================

void registerStringBuffer(CldcVirtualMachine* vm) {
    const auto SB = {"java/lang/StringBuffer", "java/lang/StringBuilder"};
    auto buf = [](CldcVirtualMachine* vm, const Args& a) -> std::u16string& {
        return ensurePayload<StringBufferPayload>(self(vm, a)).s;
    };

    registerFor(vm, SB, "<init>", "()V", [buf](CldcVirtualMachine* vm, const Args& a) { buf(vm, a).clear(); return JavaValue(); });
    registerFor(vm, SB, "<init>", "(I)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        if (a[1].i < 0) vm->throwJava("java/lang/NegativeArraySizeException");
        buf(vm, a).reserve(static_cast<size_t>(a[1].i));
        return JavaValue();
    });
    registerFor(vm, SB, "<init>", "(Ljava/lang/String;)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        buf(vm, a) = strArg(vm, a, 1);
        return JavaValue();
    });
    registerFor(vm, SB, "<init>", "(Ljava/lang/CharSequence;)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        buf(vm, a) = javaToString(vm, arg(a, 1).ref);
        return JavaValue();
    });

    // append(x) for every argument type; returns this
    auto appendHandler = [buf](char type) {
        return [buf, type](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            if (type == 'L') b += javaToString(vm, arg(a, 1).ref);
            else b += primitiveToString(type, arg(a, 1));
            return a[0];
        };
    };
    for (const char* cls : SB) {
        const std::string ret = std::string(")L") + cls + ";";
        vm->registerNative(cls, "append", "(Ljava/lang/String;" + ret, appendHandler('L'));
        vm->registerNative(cls, "append", "(Ljava/lang/Object;" + ret, appendHandler('L'));
        vm->registerNative(cls, "append", "(Ljava/lang/CharSequence;" + ret, appendHandler('L'));
        vm->registerNative(cls, "append", "(Ljava/lang/StringBuffer;" + ret, appendHandler('L'));
        for (char t : std::string("ZCIJFD")) {
            vm->registerNative(cls, "append", std::string("(") + t + ret, appendHandler(t));
        }
        vm->registerNative(cls, "append", "([C" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            JavaArray* arr = arrayArg(vm, a, 1);
            std::u16string& b = buf(vm, a);
            for (auto& e : arr->elements) b.push_back(static_cast<char16_t>(e.i));
            return a[0];
        });
        vm->registerNative(cls, "append", "([CII" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            JavaArray* arr = arrayArg(vm, a, 1);
            checkRange(vm, a[2].i, a[3].i, arr->length, "java/lang/IndexOutOfBoundsException");
            std::u16string& b = buf(vm, a);
            for (int32_t i = 0; i < a[3].i; ++i) b.push_back(static_cast<char16_t>(arr->elements[a[2].i + i].i));
            return a[0];
        });

        // insert(offset, x); returns this
        auto insertHandler = [buf](char type) {
            return [buf, type](CldcVirtualMachine* vm, const Args& a) {
                std::u16string& b = buf(vm, a);
                int32_t off = a[1].i;
                if (off < 0 || off > static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(off));
                std::u16string ins = (type == 'L') ? javaToString(vm, arg(a, 2).ref) : primitiveToString(type, arg(a, 2));
                b.insert(static_cast<size_t>(off), ins);
                return a[0];
            };
        };
        vm->registerNative(cls, "insert", "(ILjava/lang/String;" + ret, insertHandler('L'));
        vm->registerNative(cls, "insert", "(ILjava/lang/Object;" + ret, insertHandler('L'));
        for (char t : std::string("ZCIJFD")) {
            vm->registerNative(cls, "insert", std::string("(I") + t + ret, insertHandler(t));
        }
        vm->registerNative(cls, "insert", "(I[C" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            int32_t off = a[1].i;
            if (off < 0 || off > static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(off));
            JavaArray* arr = arrayArg(vm, a, 2);
            std::u16string ins;
            for (auto& e : arr->elements) ins.push_back(static_cast<char16_t>(e.i));
            b.insert(static_cast<size_t>(off), ins);
            return a[0];
        });
        vm->registerNative(cls, "delete", "(II" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            int32_t start = a[1].i;
            int32_t end = std::min(a[2].i, static_cast<int32_t>(b.size()));
            if (start < 0 || start > end) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
            b.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
            return a[0];
        });
        vm->registerNative(cls, "deleteCharAt", "(I" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            if (a[1].i < 0 || a[1].i >= static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(a[1].i));
            b.erase(static_cast<size_t>(a[1].i), 1);
            return a[0];
        });
        vm->registerNative(cls, "replace", "(IILjava/lang/String;" + ret, [buf](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            int32_t start = a[1].i;
            int32_t end = std::min(a[2].i, static_cast<int32_t>(b.size()));
            if (start < 0 || start > static_cast<int32_t>(b.size()) || start > end) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
            b.replace(static_cast<size_t>(start), static_cast<size_t>(end - start), strArg(vm, a, 3));
            return a[0];
        });
        vm->registerNative(cls, "reverse", "()" + ret.substr(1), [buf](CldcVirtualMachine* vm, const Args& a) {
            std::u16string& b = buf(vm, a);
            std::reverse(b.begin(), b.end());
            return a[0];
        });
    }

    registerFor(vm, SB, "toString", "()Ljava/lang/String;", [buf](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, buf(vm, a));
    });
    registerFor(vm, SB, "length", "()I", [buf](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(buf(vm, a).size()));
    });
    registerFor(vm, SB, "capacity", "()I", [buf](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(std::max<size_t>(buf(vm, a).capacity(), 16)));
    });
    registerFor(vm, SB, "ensureCapacity", "(I)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        if (a[1].i > 0) buf(vm, a).reserve(static_cast<size_t>(a[1].i));
        return JavaValue();
    });
    registerFor(vm, SB, "setLength", "(I)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        if (a[1].i < 0) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(a[1].i));
        buf(vm, a).resize(static_cast<size_t>(a[1].i), u'\0');
        return JavaValue();
    });
    registerFor(vm, SB, "charAt", "(I)C", [buf](CldcVirtualMachine* vm, const Args& a) {
        std::u16string& b = buf(vm, a);
        if (a[1].i < 0 || a[1].i >= static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(a[1].i));
        return JavaValue(static_cast<int32_t>(b[a[1].i]));
    });
    registerFor(vm, SB, "setCharAt", "(IC)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        std::u16string& b = buf(vm, a);
        if (a[1].i < 0 || a[1].i >= static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException", std::to_string(a[1].i));
        b[a[1].i] = static_cast<char16_t>(a[2].i);
        return JavaValue();
    });
    registerFor(vm, SB, "getChars", "(II[CI)V", [buf](CldcVirtualMachine* vm, const Args& a) {
        std::u16string& b = buf(vm, a);
        int32_t s = a[1].i, e = a[2].i, d = a[4].i;
        JavaArray* dst = arrayArg(vm, a, 3);
        if (s < 0 || e > static_cast<int32_t>(b.size()) || s > e) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
        checkRange(vm, d, e - s, dst->length, "java/lang/ArrayIndexOutOfBoundsException");
        for (int32_t i = s; i < e; ++i) dst->elements[d + i - s] = JavaValue(static_cast<int32_t>(b[i]));
        return JavaValue();
    });
    registerFor(vm, SB, "indexOf", "(Ljava/lang/String;)I", [buf](CldcVirtualMachine* vm, const Args& a) {
        size_t r = buf(vm, a).find(strArg(vm, a, 1));
        return JavaValue(r == std::u16string::npos ? -1 : static_cast<int32_t>(r));
    });
    registerFor(vm, SB, "substring", "(I)Ljava/lang/String;", [buf](CldcVirtualMachine* vm, const Args& a) {
        std::u16string& b = buf(vm, a);
        if (a[1].i < 0 || a[1].i > static_cast<int32_t>(b.size())) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
        return newStr(vm, b.substr(static_cast<size_t>(a[1].i)));
    });
    registerFor(vm, SB, "substring", "(II)Ljava/lang/String;", [buf](CldcVirtualMachine* vm, const Args& a) {
        std::u16string& b = buf(vm, a);
        if (a[1].i < 0 || a[2].i > static_cast<int32_t>(b.size()) || a[1].i > a[2].i) vm->throwJava("java/lang/StringIndexOutOfBoundsException");
        return newStr(vm, b.substr(static_cast<size_t>(a[1].i), static_cast<size_t>(a[2].i - a[1].i)));
    });
}

// ============================================================================
// Boxed primitives: Integer, Long, Short, Byte, Character, Boolean, Float, Double
// ============================================================================

void registerBoxes(CldcVirtualMachine* vm) {
    // Common instance protocol
    struct BoxSpec { const char* cls; char type; };
    const BoxSpec specs[] = {
        {"java/lang/Integer", 'I'}, {"java/lang/Long", 'J'}, {"java/lang/Short", 'S'}, {"java/lang/Byte", 'B'},
        {"java/lang/Character", 'C'}, {"java/lang/Boolean", 'Z'}, {"java/lang/Float", 'F'}, {"java/lang/Double", 'D'},
    };
    for (const auto& spec : specs) {
        const std::string cls = spec.cls;
        const char type = spec.type;
        vm->registerNative(cls, "<init>", std::string("(") + type + ")V", [type](CldcVirtualMachine* vm, const Args& a) {
            JavaValue v = arg(a, 1);
            if (type == 'S') v = JavaValue(static_cast<int32_t>(static_cast<int16_t>(v.i)));
            else if (type == 'B') v = JavaValue(static_cast<int32_t>(static_cast<int8_t>(v.i)));
            ensurePayload<BoxPayload>(self(vm, a)).v = v;
            return JavaValue();
        });
        vm->registerNative(cls, "equals", "(Ljava/lang/Object;)Z", [type](CldcVirtualMachine* vm, const Args& a) {
            JavaObject* me = self(vm, a);
            JavaObject* other = arg(a, 1).ref;
            auto* p1 = getPayload<BoxPayload>(me);
            auto* p2 = getPayload<BoxPayload>(other);
            if (!p1 || !p2 || vm->classNameOf(me) != vm->classNameOf(other)) return boolV(false);
            switch (type) {
                case 'J': return boolV(p1->v.l == p2->v.l);
                case 'F': return boolV(floatBits(p1->v.f) == floatBits(p2->v.f));
                case 'D': return boolV(doubleBits(p1->v.d) == doubleBits(p2->v.d));
                default: return boolV(p1->v.i == p2->v.i);
            }
        });
        vm->registerNative(cls, "hashCode", "()I", [type](CldcVirtualMachine* vm, const Args& a) {
            JavaValue v = boxValue(vm, a);
            switch (type) {
                case 'J': return JavaValue(static_cast<int32_t>(v.l ^ static_cast<int64_t>(static_cast<uint64_t>(v.l) >> 32)));
                case 'F': return JavaValue(floatBits(v.f));
                case 'D': {
                    int64_t b = doubleBits(v.d);
                    return JavaValue(static_cast<int32_t>(b ^ static_cast<int64_t>(static_cast<uint64_t>(b) >> 32)));
                }
                case 'Z': return JavaValue(v.i ? 1231 : 1237);
                default: return JavaValue(v.i);
            }
        });
        vm->registerNative(cls, "toString", "()Ljava/lang/String;", [type](CldcVirtualMachine* vm, const Args& a) {
            return newStr(vm, primitiveToString(type == 'S' || type == 'B' ? 'I' : type, boxValue(vm, a)));
        });
    }

    // Numeric value accessors for Integer/Long/Short/Byte/Float/Double
    const char* numeric[] = {"java/lang/Integer", "java/lang/Long", "java/lang/Short", "java/lang/Byte", "java/lang/Float", "java/lang/Double"};
    const char numericType[] = {'I', 'J', 'S', 'B', 'F', 'D'};
    for (int k = 0; k < 6; ++k) {
        const char type = numericType[k];
        auto asDouble = [type](const JavaValue& v) -> double {
            if (type == 'J') return static_cast<double>(v.l);
            if (type == 'F') return v.f;
            if (type == 'D') return v.d;
            return v.i;
        };
        auto asLong = [type](const JavaValue& v) -> int64_t {
            if (type == 'J') return v.l;
            if (type == 'F') return std::isnan(v.f) ? 0 : static_cast<int64_t>(std::clamp<double>(v.f, -9.2233720368547758e18, 9.2233720368547748e18));
            if (type == 'D') return std::isnan(v.d) ? 0 : static_cast<int64_t>(std::clamp<double>(v.d, -9.2233720368547758e18, 9.2233720368547748e18));
            return v.i;
        };
        vm->registerNative(numeric[k], "intValue", "()I", [asLong](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<int32_t>(asLong(boxValue(vm, a))));
        });
        vm->registerNative(numeric[k], "longValue", "()J", [asLong](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(asLong(boxValue(vm, a)));
        });
        vm->registerNative(numeric[k], "shortValue", "()S", [asLong](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<int32_t>(static_cast<int16_t>(asLong(boxValue(vm, a)))));
        });
        vm->registerNative(numeric[k], "byteValue", "()B", [asLong](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<int32_t>(static_cast<int8_t>(asLong(boxValue(vm, a)))));
        });
        vm->registerNative(numeric[k], "floatValue", "()F", [asDouble](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(static_cast<float>(asDouble(boxValue(vm, a))));
        });
        vm->registerNative(numeric[k], "doubleValue", "()D", [asDouble](CldcVirtualMachine* vm, const Args& a) {
            return JavaValue(asDouble(boxValue(vm, a)));
        });
    }

    // --- Integer ---
    const char* I = "java/lang/Integer";
    vm->registerNative(I, "parseInt", "(Ljava/lang/String;)I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT32_MIN, INT32_MAX)));
    });
    vm->registerNative(I, "parseInt", "(Ljava/lang/String;I)I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), a[1].i, INT32_MIN, INT32_MAX)));
    });
    vm->registerNative(I, "valueOf", "(Ljava/lang/String;)Ljava/lang/Integer;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Integer", JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT32_MIN, INT32_MAX))));
    });
    vm->registerNative(I, "valueOf", "(Ljava/lang/String;I)Ljava/lang/Integer;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Integer", JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), a[1].i, INT32_MIN, INT32_MAX))));
    });
    vm->registerNative(I, "valueOf", "(I)Ljava/lang/Integer;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Integer", JavaValue(arg(a, 0).i));
    });
    vm->registerNative(I, "toString", "(I)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(arg(a, 0).i, 10));
    });
    vm->registerNative(I, "toString", "(II)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(a[0].i, a[1].i));
    });
    vm->registerNative(I, "toHexString", "(I)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, unsignedToString(static_cast<uint32_t>(arg(a, 0).i), 4));
    });
    vm->registerNative(I, "toOctalString", "(I)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, unsignedToString(static_cast<uint32_t>(arg(a, 0).i), 3));
    });
    vm->registerNative(I, "toBinaryString", "(I)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, unsignedToString(static_cast<uint32_t>(arg(a, 0).i), 1));
    });

    // --- Long ---
    const char* L = "java/lang/Long";
    vm->registerNative(L, "parseLong", "(Ljava/lang/String;)J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT64_MIN, INT64_MAX));
    });
    vm->registerNative(L, "parseLong", "(Ljava/lang/String;I)J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(parseIntegral(vm, asString(arg(a, 0).ref), a[1].i, INT64_MIN, INT64_MAX));
    });
    vm->registerNative(L, "valueOf", "(J)Ljava/lang/Long;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Long", JavaValue(arg(a, 0).l));
    });
    vm->registerNative(L, "valueOf", "(Ljava/lang/String;)Ljava/lang/Long;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Long", JavaValue(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT64_MIN, INT64_MAX)));
    });
    vm->registerNative(L, "toString", "(J)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(arg(a, 0).l, 10));
    });
    vm->registerNative(L, "toString", "(JI)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(a[0].l, a[1].i));
    });
    vm->registerNative(L, "toHexString", "(J)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, unsignedToString(static_cast<uint64_t>(arg(a, 0).l), 4));
    });

    // --- Java SE bit twiddling (Integer / Long / Short / Character), used by ported code ---
    auto u32 = [](const Args& a) { return static_cast<uint32_t>(a[0].i); };
    auto u64 = [](const Args& a) { return static_cast<uint64_t>(a[0].l); };
    auto i32 = [](uint32_t v) { return JavaValue(static_cast<int32_t>(v)); };
    auto bswap32 = [](uint32_t v) { return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24); };
    auto clz64 = [](uint64_t v) { int n = 0; if (!v) return 64; while (!(v & (1ULL << 63))) { v <<= 1; n++; } return n; };
    auto ctz64 = [](uint64_t v) { int n = 0; if (!v) return 64; while (!(v & 1)) { v >>= 1; n++; } return n; };
    auto pop64 = [](uint64_t v) { int n = 0; for (; v; v &= v - 1) n++; return n; };
    vm->registerNative(I, "reverseBytes", "(I)I", [=](CldcVirtualMachine*, const Args& a) { return i32(bswap32(u32(a))); });
    vm->registerNative(I, "reverse", "(I)I", [=](CldcVirtualMachine*, const Args& a) {
        uint32_t v = u32(a), r = 0;
        for (int k = 0; k < 32; k++, v >>= 1) r = (r << 1) | (v & 1);
        return i32(r);
    });
    vm->registerNative(I, "bitCount", "(I)I", [=](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(pop64(u32(a)))); });
    vm->registerNative(I, "numberOfLeadingZeros", "(I)I", [=](CldcVirtualMachine*, const Args& a) {
        return JavaValue(static_cast<int32_t>(u32(a) ? clz64(u32(a)) - 32 : 32));
    });
    vm->registerNative(I, "numberOfTrailingZeros", "(I)I", [=](CldcVirtualMachine*, const Args& a) {
        return JavaValue(static_cast<int32_t>(u32(a) ? ctz64(u32(a)) : 32));
    });
    vm->registerNative(I, "highestOneBit", "(I)I", [=](CldcVirtualMachine*, const Args& a) { return i32(u32(a) ? 0x80000000u >> (clz64(u32(a)) - 32) : 0); });
    vm->registerNative(I, "lowestOneBit", "(I)I", [=](CldcVirtualMachine*, const Args& a) { return i32(u32(a) & (0u - u32(a))); });
    vm->registerNative(I, "rotateLeft", "(II)I", [=](CldcVirtualMachine*, const Args& a) {
        uint32_t d = static_cast<uint32_t>(a[1].i) & 31;
        return i32(d ? (u32(a) << d) | (u32(a) >> (32 - d)) : u32(a));
    });
    vm->registerNative(I, "rotateRight", "(II)I", [=](CldcVirtualMachine*, const Args& a) {
        uint32_t d = static_cast<uint32_t>(a[1].i) & 31;
        return i32(d ? (u32(a) >> d) | (u32(a) << (32 - d)) : u32(a));
    });
    vm->registerNative(I, "signum", "(I)I", [](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>((a[0].i > 0) - (a[0].i < 0))); });
    vm->registerNative(L, "reverseBytes", "(J)J", [=](CldcVirtualMachine*, const Args& a) {
        uint64_t v = u64(a);
        return JavaValue(static_cast<int64_t>((static_cast<uint64_t>(bswap32(static_cast<uint32_t>(v))) << 32) | bswap32(static_cast<uint32_t>(v >> 32))));
    });
    vm->registerNative(L, "bitCount", "(J)I", [=](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(pop64(u64(a)))); });
    vm->registerNative(L, "numberOfLeadingZeros", "(J)I", [=](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(clz64(u64(a)))); });
    vm->registerNative(L, "numberOfTrailingZeros", "(J)I", [=](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(ctz64(u64(a)))); });
    vm->registerNative(L, "signum", "(J)I", [](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>((a[0].l > 0) - (a[0].l < 0))); });
    vm->registerNative("java/lang/Short", "reverseBytes", "(S)S", [](CldcVirtualMachine*, const Args& a) {
        uint16_t v = static_cast<uint16_t>(a[0].i);
        return JavaValue(static_cast<int32_t>(static_cast<int16_t>((v >> 8) | (v << 8))));
    });
    vm->registerNative("java/lang/Character", "reverseBytes", "(C)C", [](CldcVirtualMachine*, const Args& a) {
        uint16_t v = static_cast<uint16_t>(a[0].i);
        return JavaValue(static_cast<int32_t>(static_cast<uint16_t>((v >> 8) | (v << 8))));
    });

    // --- Short / Byte ---
    vm->registerNative("java/lang/Short", "parseShort", "(Ljava/lang/String;)S", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT16_MIN, INT16_MAX)));
    });
    vm->registerNative("java/lang/Short", "parseShort", "(Ljava/lang/String;I)S", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), a[1].i, INT16_MIN, INT16_MAX)));
    });
    vm->registerNative("java/lang/Short", "valueOf", "(S)Ljava/lang/Short;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Short", JavaValue(static_cast<int32_t>(static_cast<int16_t>(arg(a, 0).i))));
    });
    vm->registerNative("java/lang/Short", "toString", "(S)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(arg(a, 0).i, 10));
    });
    vm->registerNative("java/lang/Byte", "parseByte", "(Ljava/lang/String;)B", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), 10, INT8_MIN, INT8_MAX)));
    });
    vm->registerNative("java/lang/Byte", "parseByte", "(Ljava/lang/String;I)B", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(parseIntegral(vm, asString(arg(a, 0).ref), a[1].i, INT8_MIN, INT8_MAX)));
    });
    vm->registerNative("java/lang/Byte", "valueOf", "(B)Ljava/lang/Byte;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Byte", JavaValue(static_cast<int32_t>(static_cast<int8_t>(arg(a, 0).i))));
    });
    vm->registerNative("java/lang/Byte", "toString", "(B)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, intToString(arg(a, 0).i, 10));
    });

    // --- Character ---
    const char* C = "java/lang/Character";
    vm->registerNative(C, "charValue", "()C", [](CldcVirtualMachine* vm, const Args& a) { return boxValue(vm, a); });
    vm->registerNative(C, "valueOf", "(C)Ljava/lang/Character;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Character", JavaValue(arg(a, 0).i & 0xFFFF));
    });
    vm->registerNative(C, "toString", "(C)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, std::u16string(1, static_cast<char16_t>(arg(a, 0).i)));
    });
    vm->registerNative(C, "isDigit", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isDigitChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "isLetter", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isLetterChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "isLetterOrDigit", "(C)Z", [](CldcVirtualMachine*, const Args& a) {
        char16_t c = static_cast<char16_t>(arg(a, 0).i);
        return boolV(isLetterChar(c) || isDigitChar(c));
    });
    vm->registerNative(C, "isUpperCase", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isUpperChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "isLowerCase", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isLowerChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "isWhitespace", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isSpaceChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "isSpace", "(C)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(isSpaceChar(static_cast<char16_t>(arg(a, 0).i))); });
    vm->registerNative(C, "toUpperCase", "(C)C", [](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(toUpperChar(static_cast<char16_t>(arg(a, 0).i)))); });
    vm->registerNative(C, "toLowerCase", "(C)C", [](CldcVirtualMachine*, const Args& a) { return JavaValue(static_cast<int32_t>(toLowerChar(static_cast<char16_t>(arg(a, 0).i)))); });
    vm->registerNative(C, "digit", "(CI)I", [](CldcVirtualMachine*, const Args& a) { return JavaValue(digitOf(static_cast<char16_t>(a[0].i), a[1].i)); });
    vm->registerNative(C, "forDigit", "(II)C", [](CldcVirtualMachine*, const Args& a) {
        int32_t d = a[0].i, r = a[1].i;
        if (r < 2 || r > 36 || d < 0 || d >= r) return JavaValue(0);
        return JavaValue(static_cast<int32_t>(d < 10 ? '0' + d : 'a' + d - 10));
    });

    // --- Boolean ---
    const char* Z = "java/lang/Boolean";
    vm->registerNative(Z, "booleanValue", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boxValue(vm, a); });
    auto boolSingleton = [](bool value) {
        auto cache = std::make_shared<std::atomic<JavaObject*>>(nullptr);
        return [cache, value](CldcVirtualMachine* vm, const Args&) {
            JavaObject* o = cache->load();
            if (!o) {
                o = newBox(vm, "java/lang/Boolean", JavaValue(value ? 1 : 0)).ref;
                vm->pin(o);
                cache->store(o);
            }
            return refV(o);
        };
    };
    auto trueObj = boolSingleton(true);
    auto falseObj = boolSingleton(false);
    vm->registerNativeStatic(Z, "TRUE", trueObj);
    vm->registerNativeStatic(Z, "FALSE", falseObj);
    vm->registerNative(Z, "valueOf", "(Z)Ljava/lang/Boolean;", [trueObj, falseObj](CldcVirtualMachine* vm, const Args& a) {
        return arg(a, 0).i ? trueObj(vm, a) : falseObj(vm, a);
    });
    vm->registerNative(Z, "parseBoolean", "(Ljava/lang/String;)Z", [](CldcVirtualMachine*, const Args& a) {
        JavaString* s = asString(arg(a, 0).ref);
        if (!s) return boolV(false);
        std::string v = s->value;
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return boolV(v == "true");
    });
    vm->registerNative(Z, "toString", "(Z)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, arg(a, 0).i ? "true" : "false");
    });

    // --- Float ---
    const char* F = "java/lang/Float";
    vm->registerNative(F, "<init>", "(D)V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<BoxPayload>(self(vm, a)).v = JavaValue(static_cast<float>(arg(a, 1).d));
        return JavaValue();
    });
    vm->registerNative(F, "parseFloat", "(Ljava/lang/String;)F", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<float>(parseFloating(vm, asString(arg(a, 0).ref))));
    });
    vm->registerNative(F, "valueOf", "(Ljava/lang/String;)Ljava/lang/Float;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Float", JavaValue(static_cast<float>(parseFloating(vm, asString(arg(a, 0).ref)))));
    });
    vm->registerNative(F, "valueOf", "(F)Ljava/lang/Float;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Float", JavaValue(arg(a, 0).f));
    });
    vm->registerNative(F, "toString", "(F)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, floatingToString(arg(a, 0).f));
    });
    vm->registerNative(F, "floatToIntBits", "(F)I", [](CldcVirtualMachine*, const Args& a) { return JavaValue(floatBits(arg(a, 0).f)); });
    vm->registerNative(F, "intBitsToFloat", "(I)F", [](CldcVirtualMachine*, const Args& a) {
        int32_t b = arg(a, 0).i;
        float f;
        std::memcpy(&f, &b, 4);
        return JavaValue(f);
    });
    vm->registerNative(F, "isNaN", "(F)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(std::isnan(arg(a, 0).f)); });
    vm->registerNative(F, "isInfinite", "(F)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(std::isinf(arg(a, 0).f)); });
    vm->registerNative(F, "isNaN", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boolV(std::isnan(boxValue(vm, a).f)); });
    vm->registerNative(F, "isInfinite", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boolV(std::isinf(boxValue(vm, a).f)); });

    // --- Double ---
    const char* D = "java/lang/Double";
    vm->registerNative(D, "parseDouble", "(Ljava/lang/String;)D", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(parseFloating(vm, asString(arg(a, 0).ref)));
    });
    vm->registerNative(D, "valueOf", "(Ljava/lang/String;)Ljava/lang/Double;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Double", JavaValue(parseFloating(vm, asString(arg(a, 0).ref))));
    });
    vm->registerNative(D, "valueOf", "(D)Ljava/lang/Double;", [](CldcVirtualMachine* vm, const Args& a) {
        return newBox(vm, "java/lang/Double", JavaValue(arg(a, 0).d));
    });
    vm->registerNative(D, "toString", "(D)Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, floatingToString(arg(a, 0).d));
    });
    vm->registerNative(D, "doubleToLongBits", "(D)J", [](CldcVirtualMachine*, const Args& a) { return JavaValue(doubleBits(arg(a, 0).d)); });
    vm->registerNative(D, "longBitsToDouble", "(J)D", [](CldcVirtualMachine*, const Args& a) {
        int64_t b = arg(a, 0).l;
        double d;
        std::memcpy(&d, &b, 8);
        return JavaValue(d);
    });
    vm->registerNative(D, "isNaN", "(D)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(std::isnan(arg(a, 0).d)); });
    vm->registerNative(D, "isInfinite", "(D)Z", [](CldcVirtualMachine*, const Args& a) { return boolV(std::isinf(arg(a, 0).d)); });
    vm->registerNative(D, "isNaN", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boolV(std::isnan(boxValue(vm, a).d)); });
    vm->registerNative(D, "isInfinite", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boolV(std::isinf(boxValue(vm, a).d)); });
}

// ============================================================================
// java.lang.Object / Class / Thread / System / Math extras
// ============================================================================

void registerLangMisc(CldcVirtualMachine* vm) {
    const char* O = "java/lang/Object";
    vm->registerNative(O, "getClass", "()Ljava/lang/Class;", [](CldcVirtualMachine* vm, const Args& a) {
        return refV(vm->classObjectFor(vm->classNameOf(self(vm, a))));
    });
    vm->registerNative(O, "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        std::string name = vm->classNameOf(o);
        for (char& c : name) if (c == '/') c = '.';
        int32_t h = javaHashCode(vm, o);
        return newStrUtf8(vm, name + "@" + unsignedToString(static_cast<uint32_t>(h), 4));
    });
    vm->registerNative(O, "wait", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        vm->monitorWait(self(vm, a), 0);
        return JavaValue();
    });
    vm->registerNative(O, "wait", "(J)V", [](CldcVirtualMachine* vm, const Args& a) {
        int64_t ms = arg(a, 1).l;
        if (ms < 0) vm->throwJava("java/lang/IllegalArgumentException", "timeout value is negative");
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) {
            ms = inst->realSleepMs(ms);
        }
        vm->monitorWait(self(vm, a), ms);
        return JavaValue();
    });
    vm->registerNative(O, "wait", "(JI)V", [](CldcVirtualMachine* vm, const Args& a) {
        int64_t ms = arg(a, 1).l;
        if (ms < 0) vm->throwJava("java/lang/IllegalArgumentException", "timeout value is negative");
        int64_t finalMs = ms == 0 && arg(a, 2).i > 0 ? 1 : ms;
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) {
            finalMs = inst->realSleepMs(finalMs);
        }
        vm->monitorWait(self(vm, a), finalMs);
        return JavaValue();
    });
    vm->registerNative(O, "notify", "()V", [](CldcVirtualMachine* vm, const Args& a) { vm->monitorNotify(self(vm, a)); return JavaValue(); });
    vm->registerNative(O, "notifyAll", "()V", [](CldcVirtualMachine* vm, const Args& a) { vm->monitorNotify(self(vm, a)); return JavaValue(); });

    // --- java.lang.Class ---
    const char* K = "java/lang/Class";
    auto classNameArg = [](CldcVirtualMachine* vm, const Args& a) -> std::string {
        JavaObject* o = self(vm, a);
        if (auto* p = getPayload<JavaClassPayload>(o)) return p->name;
        if (auto* s = asString(o)) return s->value; // legacy: Class represented by its name
        return vm->classNameOf(o);
    };
    vm->registerNative(K, "getName", "()Ljava/lang/String;", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        std::string n = classNameArg(vm, a);
        for (char& c : n) if (c == '/') c = '.';
        return newStrUtf8(vm, n);
    });
    vm->registerNative(K, "toString", "()Ljava/lang/String;", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        std::string n = classNameArg(vm, a);
        for (char& c : n) if (c == '/') c = '.';
        return newStrUtf8(vm, "class " + n);
    });
    vm->registerNative(K, "forName", "(Ljava/lang/String;)Ljava/lang/Class;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* s = asString(arg(a, 0).ref);
        if (!s) vm->throwJava("java/lang/NullPointerException");
        std::string n = s->value;
        for (char& c : n) if (c == '.') c = '/';
        if (!vm->findClass(n) && !vm->hasNativeClass(n)) {
            vm->throwJava("java/lang/ClassNotFoundException", s->value);
        }
        return refV(vm->classObjectFor(n));
    });
    vm->registerNative(K, "newInstance", "()Ljava/lang/Object;", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        std::string n = classNameArg(vm, a);
        auto cls = vm->findClass(n);
        JavaObject* o = vm->allocateObject(cls.get());
        if (!cls) o->nativeClassName = n;
        vm->executeMethodByName(n, "<init>", "()V", {refV(o)});
        return refV(o);
    });
    vm->registerNative(K, "isInstance", "(Ljava/lang/Object;)Z", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        return boolV(vm->isInstanceOf(arg(a, 1).ref, classNameArg(vm, a)));
    });
    vm->registerNative(K, "isArray", "()Z", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        std::string n = classNameArg(vm, a);
        return boolV(!n.empty() && n[0] == '[');
    });
    vm->registerNative(K, "getResourceAsStream", "(Ljava/lang/String;)Ljava/io/InputStream;", [classNameArg](CldcVirtualMachine* vm, const Args& a) {
        JavaString* s = asString(arg(a, 1).ref);
        auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext());
        if (!s || !inst) return nullV();
        std::string res = s->value;
        std::vector<std::string> candidates;
        if (!res.empty() && res[0] == '/') {
            candidates.push_back(res.substr(1));
        } else {
            // Relative names resolve against the class's package
            std::string cls = classNameArg(vm, a);
            size_t slash = cls.rfind('/');
            if (slash != std::string::npos) candidates.push_back(cls.substr(0, slash + 1) + res);
            candidates.push_back(res);
        }
        for (const auto& c : candidates) {
            std::vector<uint8_t> data;
            if (inst->jarReader.extractEntry(c, data)) {
                return refV(vm->allocateInputStream(std::move(data)));
            }
        }
        return nullV();
    });

    // --- java.lang.reflect.Array (emitted by dex2jar for multi-dimensional object arrays) ---
    auto componentCode = [](CldcVirtualMachine* vm, JavaObject* clsObj) -> char {
        auto* p = getPayload<JavaClassPayload>(clsObj);
        if (!p) vm->throwJava("java/lang/NullPointerException");
        return (p->name.size() == 1) ? p->name[0] : 'L'; // primitive TYPE classes use their descriptor char
    };
    struct MultiArray {
        static JavaArray* make(CldcVirtualMachine* vm, char leaf, const std::vector<int32_t>& dims, size_t level) {
            bool last = (level + 1 == dims.size());
            JavaArray* arr = vm->allocateArray(last ? leaf : 'L', dims[level]);
            if (!last) {
                for (int32_t i = 0; i < dims[level]; ++i) arr->elements[i] = refV(make(vm, leaf, dims, level + 1));
            }
            return arr;
        }
    };
    vm->registerNative("java/lang/reflect/Array", "newInstance", "(Ljava/lang/Class;I)Ljava/lang/Object;", [componentCode](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/NegativeArraySizeException");
        return refV(vm->allocateArray(componentCode(vm, arg(a, 0).ref), arg(a, 1).i));
    });
    vm->registerNative("java/lang/reflect/Array", "newInstance", "(Ljava/lang/Class;[I)Ljava/lang/Object;", [componentCode](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* dimsArr = arrayArg(vm, a, 1);
        std::vector<int32_t> dims;
        for (auto& e : dimsArr->elements) {
            if (e.i < 0) vm->throwJava("java/lang/NegativeArraySizeException");
            dims.push_back(e.i);
        }
        if (dims.empty()) vm->throwJava("java/lang/IllegalArgumentException");
        return refV(MultiArray::make(vm, componentCode(vm, arg(a, 0).ref), dims, 0));
    });
    const std::pair<const char*, const char*> primitiveTypes[] = {
        {"java/lang/Integer", "I"}, {"java/lang/Long", "J"}, {"java/lang/Short", "S"}, {"java/lang/Byte", "B"},
        {"java/lang/Character", "C"}, {"java/lang/Boolean", "Z"}, {"java/lang/Float", "F"}, {"java/lang/Double", "D"},
    };
    for (const auto& [boxCls, code] : primitiveTypes) {
        std::string c = code;
        vm->registerNativeStatic(boxCls, "TYPE", [c](CldcVirtualMachine* vm, const Args&) { return refV(vm->classObjectFor(c)); });
    }

    // --- java.lang.Thread ---
    const char* T = "java/lang/Thread";
    vm->registerNative(T, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<ThreadPayload>(self(vm, a));
        return JavaValue();
    });
    vm->registerNative(T, "<init>", "(Ljava/lang/Runnable;)V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<ThreadPayload>(self(vm, a)).runnable = arg(a, 1).ref;
        return JavaValue();
    });
    vm->registerNative(T, "<init>", "(Ljava/lang/Runnable;Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ThreadPayload>(self(vm, a));
        p.runnable = arg(a, 1).ref;
        if (auto* s = asString(arg(a, 2).ref)) p.name = s->value;
        return JavaValue();
    });
    vm->registerNative(T, "<init>", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ThreadPayload>(self(vm, a));
        if (auto* s = asString(arg(a, 1).ref)) p.name = s->value;
        return JavaValue();
    });
    vm->registerNative(T, "run", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ThreadPayload>(self(vm, a));
        if (p && p->runnable) {
            CldcVirtualMachine::NativeCallout callout(vm);
            vm->executeMethodByName(vm->classNameOf(p->runnable), "run", "()V", {refV(p->runnable)});
        }
        return JavaValue();
    });
    vm->registerNative(T, "start", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* threadObj = self(vm, a);
        auto& p = ensurePayload<ThreadPayload>(threadObj);
        if (!engineRunning(vm) || p.alive->load()) return JavaValue();
        p.alive->store(true);
        auto alive = p.alive;
        vm->pin(threadObj);
        try { startDetachedJavaThread(vm, [vm, threadObj, alive]() {
            // Thread.run() either is overridden in bytecode or delegates to the Runnable
            runJavaThread(vm, [&] { runJavaRunnable(vm, threadObj, "thread"); });
            alive->store(false);
            vm->unpin(threadObj);
        }); } catch (...) {
            alive->store(false);
            vm->unpin(threadObj);
            throw;
        }
        return JavaValue();
    });
    vm->registerNative(T, "isAlive", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ThreadPayload>(self(vm, a));
        return boolV(p && p->alive->load());
    });
    vm->registerNative(T, "join", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ThreadPayload>(self(vm, a));
        CldcVirtualMachine::BlockingRegion region(vm);
        while (p && p->alive->load() && engineRunning(vm) && !vm->terminating()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (vm->terminating()) throw VmTerminated{};
        return JavaValue();
    });
    vm->registerNative(T, "interrupt", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(T, "setPriority", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<ThreadPayload>(self(vm, a)).priority = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(T, "getPriority", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ThreadPayload>(self(vm, a));
        return JavaValue(p ? p->priority : 5);
    });
    vm->registerNative(T, "getName", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<ThreadPayload>(self(vm, a));
        return newStrUtf8(vm, p && !p->name.empty() ? p->name : "Thread-0");
    });
    vm->registerNative(T, "currentThread", "()Ljava/lang/Thread;", [](CldcVirtualMachine* vm, const Args&) {
        JavaObject* t = newNativeObject(vm, "java/lang/Thread");
        ensurePayload<ThreadPayload>(t).alive->store(true);
        return refV(t);
    });
    vm->registerNative(T, "sleep", "(J)V", [](CldcVirtualMachine* vm, const Args& a) {
        int64_t ms = arg(a, 0).l;
        if (ms < 0) vm->throwJava("java/lang/IllegalArgumentException", "timeout value is negative");
        if (vm->terminating()) throw VmTerminated{};
        // Apply speed multiplier
        if (auto* inst = static_cast<J2meEngineInstance*>(vm->getUserContext())) {
            ms = inst->realSleepMs(ms);
        }
        // Sleep in slices so a stopped engine releases game threads promptly
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        CldcVirtualMachine::BlockingRegion region(vm);
        while (std::chrono::steady_clock::now() < until && engineRunning(vm) && !vm->terminating()) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - std::chrono::steady_clock::now());
            std::this_thread::sleep_for(std::min(left, std::chrono::milliseconds(20)));
        }
        if (!engineRunning(vm) || vm->terminating()) throw VmTerminated{};
        return JavaValue();
    });

    // --- java.lang.System.out / err (java.io.PrintStream) ---
    auto printStream = [](bool isErr) {
        auto cache = std::make_shared<std::atomic<JavaObject*>>(nullptr);
        return [cache, isErr](CldcVirtualMachine* vm, const Args&) {
            JavaObject* o = cache->load();
            if (!o) {
                o = newNativeObject(vm, "java/io/PrintStream");
                o->nativeHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(isErr ? 2 : 1));
                vm->pin(o);
                cache->store(o);
            }
            return refV(o);
        };
    };
    vm->registerNativeStatic("java/lang/System", "out", printStream(false));
    vm->registerNativeStatic("java/lang/System", "err", printStream(true));
    const char* PS = "java/io/PrintStream";
    auto printHandler = [](char type, bool newline) {
        return [type, newline](CldcVirtualMachine* vm, const Args& a) {
            std::u16string text;
            if (type == 'L') text = javaToString(vm, arg(a, 1).ref);
            else if (type == '[') {
                if (auto* arr = dynamic_cast<JavaArray*>(arg(a, 1).ref)) {
                    for (auto& e : arr->elements) text.push_back(static_cast<char16_t>(e.i));
                }
            } else if (type != 'V') text = primitiveToString(type, arg(a, 1));
            std::string out = utf16ToUtf8(text);
            if (newline) out.push_back('\n');
            (a.empty() || !a[0].ref || a[0].ref->nativeHandle != reinterpret_cast<void*>(uintptr_t(2)) ? std::cout : std::cerr) << out << std::flush;
            return JavaValue();
        };
    };
    vm->registerNative(PS, "println", "()V", printHandler('V', true));
    for (const char* name : {"print", "println"}) {
        bool nl = std::string(name) == "println";
        vm->registerNative(PS, name, "(Ljava/lang/String;)V", printHandler('L', nl));
        vm->registerNative(PS, name, "(Ljava/lang/Object;)V", printHandler('L', nl));
        vm->registerNative(PS, name, "([C)V", printHandler('[', nl));
        for (char t : std::string("ZCIJFD")) {
            vm->registerNative(PS, name, std::string("(") + t + ")V", printHandler(t, nl));
        }
    }
    vm->registerNative(PS, "flush", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });

    // --- java.lang.Math (CLDC 1.1 members not covered by the core registrations) ---
    const char* M = "java/lang/Math";
    vm->registerNative(M, "min", "(JJ)J", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::min(a[0].l, a[1].l)); });
    vm->registerNative(M, "max", "(JJ)J", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::max(a[0].l, a[1].l)); });
    vm->registerNative(M, "min", "(FF)F", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue((std::isnan(a[0].f) || std::isnan(a[1].f)) ? std::numeric_limits<float>::quiet_NaN() : std::min(a[0].f, a[1].f));
    });
    vm->registerNative(M, "max", "(FF)F", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue((std::isnan(a[0].f) || std::isnan(a[1].f)) ? std::numeric_limits<float>::quiet_NaN() : std::max(a[0].f, a[1].f));
    });
    vm->registerNative(M, "min", "(DD)D", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue((std::isnan(a[0].d) || std::isnan(a[1].d)) ? std::numeric_limits<double>::quiet_NaN() : std::min(a[0].d, a[1].d));
    });
    vm->registerNative(M, "max", "(DD)D", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue((std::isnan(a[0].d) || std::isnan(a[1].d)) ? std::numeric_limits<double>::quiet_NaN() : std::max(a[0].d, a[1].d));
    });
    vm->registerNative(M, "toRadians", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(a[0].d / 180.0 * 3.14159265358979323846); });
    vm->registerNative(M, "toDegrees", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(a[0].d * 180.0 / 3.14159265358979323846); });
    vm->registerNative(M, "round", "(F)I", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue(std::isnan(a[0].f) ? 0 : static_cast<int32_t>(std::clamp<double>(std::floor(a[0].f + 0.5), INT32_MIN, INT32_MAX)));
    });
    vm->registerNative(M, "round", "(D)J", [](CldcVirtualMachine*, const Args& a) {
        return JavaValue(std::isnan(a[0].d) ? int64_t(0) : static_cast<int64_t>(std::clamp<double>(std::floor(a[0].d + 0.5), -9.2233720368547758e18, 9.2233720368547748e18)));
    });
    vm->registerNative(M, "pow", "(DD)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::pow(a[0].d, a[1].d)); });
    vm->registerNative(M, "atan", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::atan(a[0].d)); });
    vm->registerNative(M, "atan2", "(DD)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::atan2(a[0].d, a[1].d)); });
    vm->registerNative(M, "asin", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::asin(a[0].d)); });
    vm->registerNative(M, "acos", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::acos(a[0].d)); });
    vm->registerNative(M, "exp", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::exp(a[0].d)); });
    vm->registerNative(M, "log", "(D)D", [](CldcVirtualMachine*, const Args& a) { return JavaValue(std::log(a[0].d)); });
    vm->registerNative(M, "random", "()D", [](CldcVirtualMachine*, const Args&) {
        static std::atomic<uint64_t> state{static_cast<uint64_t>(nowMillis()) * 6364136223846793005ull + 1442695040888963407ull};
        uint64_t x = state.fetch_add(0x9E3779B97F4A7C15ull) + 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        x ^= x >> 31;
        return JavaValue(static_cast<double>(x >> 11) * (1.0 / 9007199254740992.0));
    });
}


} // namespace universal_loader::jvm
