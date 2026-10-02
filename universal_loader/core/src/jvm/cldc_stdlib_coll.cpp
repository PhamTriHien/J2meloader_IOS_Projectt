#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {

// ============================================================================
// java.util: Vector, Stack, Hashtable, Enumeration, Random, Date, Calendar, Timer
// ============================================================================

void registerCollections(CldcVirtualMachine* vm) {
    const auto VEC = {"java/util/Vector", "java/util/Stack"};
    using Lock = std::lock_guard<std::recursive_mutex>;
    auto vec = [](CldcVirtualMachine* vm, const Args& a) -> VectorPayload& {
        return ensurePayload<VectorPayload>(self(vm, a));
    };
    auto checkIndex = [](CldcVirtualMachine* vm, int32_t i, size_t size, bool allowEnd = false) {
        if (i < 0 || static_cast<size_t>(i) > size || (!allowEnd && static_cast<size_t>(i) == size)) {
            vm->throwJava("java/lang/ArrayIndexOutOfBoundsException", std::to_string(i) + " >= " + std::to_string(size));
        }
    };
    auto indexOf = [](CldcVirtualMachine* vm, VectorPayload& p, JavaObject* o, int32_t from) -> int32_t {
        for (size_t i = static_cast<size_t>(std::max(from, 0)); i < p.v.size(); ++i) {
            if (javaEquals(vm, o, p.v[i].ref)) return static_cast<int32_t>(i);
        }
        return -1;
    };

    registerFor(vm, VEC, "<init>", "()V", [vec](CldcVirtualMachine* vm, const Args& a) { vec(vm, a); return JavaValue(); });
    registerFor(vm, VEC, "<init>", "(I)V", [vec](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Illegal Capacity");
        vec(vm, a).v.reserve(static_cast<size_t>(arg(a, 1).i));
        return JavaValue();
    });
    registerFor(vm, VEC, "<init>", "(II)V", [vec](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Illegal Capacity");
        vec(vm, a).v.reserve(static_cast<size_t>(arg(a, 1).i));
        return JavaValue();
    });
    registerFor(vm, VEC, "size", "()I", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(static_cast<int32_t>(p.v.size()));
    });
    registerFor(vm, VEC, "isEmpty", "()Z", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return boolV(p.v.empty());
    });
    registerFor(vm, VEC, "capacity", "()I", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(static_cast<int32_t>(std::max<size_t>(p.v.capacity(), 10)));
    });
    registerFor(vm, VEC, "ensureCapacity", "(I)V", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (a[1].i > 0) p.v.reserve(static_cast<size_t>(a[1].i));
        return JavaValue();
    });
    registerFor(vm, VEC, "trimToSize", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    registerFor(vm, VEC, "setSize", "(I)V", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (a[1].i < 0) vm->throwJava("java/lang/ArrayIndexOutOfBoundsException", std::to_string(a[1].i));
        p.v.resize(static_cast<size_t>(a[1].i), nullV());
        return JavaValue();
    });
    auto addElement = [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        p.v.push_back(arg(a, 1));
        return JavaValue();
    };
    registerFor(vm, VEC, "addElement", "(Ljava/lang/Object;)V", addElement);
    registerFor(vm, VEC, "add", "(Ljava/lang/Object;)Z", [addElement](CldcVirtualMachine* vm, const Args& a) {
        addElement(vm, a);
        return boolV(true);
    });
    registerFor(vm, VEC, "elementAt", "(I)Ljava/lang/Object;", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size());
        return p.v[a[1].i];
    });
    registerFor(vm, VEC, "get", "(I)Ljava/lang/Object;", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size());
        return p.v[a[1].i];
    });
    registerFor(vm, VEC, "setElementAt", "(Ljava/lang/Object;I)V", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[2].i, p.v.size());
        p.v[a[2].i] = a[1];
        return JavaValue();
    });
    registerFor(vm, VEC, "set", "(ILjava/lang/Object;)Ljava/lang/Object;", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size());
        JavaValue old = p.v[a[1].i];
        p.v[a[1].i] = a[2];
        return old;
    });
    registerFor(vm, VEC, "insertElementAt", "(Ljava/lang/Object;I)V", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[2].i, p.v.size(), true);
        p.v.insert(p.v.begin() + a[2].i, a[1]);
        return JavaValue();
    });
    registerFor(vm, VEC, "add", "(ILjava/lang/Object;)V", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size(), true);
        p.v.insert(p.v.begin() + a[1].i, a[2]);
        return JavaValue();
    });
    registerFor(vm, VEC, "removeElementAt", "(I)V", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size());
        p.v.erase(p.v.begin() + a[1].i);
        return JavaValue();
    });
    registerFor(vm, VEC, "remove", "(I)Ljava/lang/Object;", [vec, checkIndex](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        checkIndex(vm, a[1].i, p.v.size());
        JavaValue old = p.v[a[1].i];
        p.v.erase(p.v.begin() + a[1].i);
        return old;
    });
    auto removeElement = [vec, indexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        int32_t i = indexOf(vm, p, arg(a, 1).ref, 0);
        if (i >= 0) p.v.erase(p.v.begin() + i);
        return boolV(i >= 0);
    };
    registerFor(vm, VEC, "removeElement", "(Ljava/lang/Object;)Z", removeElement);
    registerFor(vm, VEC, "remove", "(Ljava/lang/Object;)Z", removeElement);
    auto clearAll = [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        p.v.clear();
        return JavaValue();
    };
    registerFor(vm, VEC, "removeAllElements", "()V", clearAll);
    registerFor(vm, VEC, "clear", "()V", clearAll);
    registerFor(vm, VEC, "contains", "(Ljava/lang/Object;)Z", [vec, indexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return boolV(indexOf(vm, p, arg(a, 1).ref, 0) >= 0);
    });
    registerFor(vm, VEC, "indexOf", "(Ljava/lang/Object;)I", [vec, indexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(indexOf(vm, p, arg(a, 1).ref, 0));
    });
    registerFor(vm, VEC, "indexOf", "(Ljava/lang/Object;I)I", [vec, indexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(indexOf(vm, p, arg(a, 1).ref, a[2].i));
    });
    auto lastIndexOf = [](CldcVirtualMachine* vm, VectorPayload& p, JavaObject* o, int32_t from) -> int32_t {
        for (int32_t i = std::min(from, static_cast<int32_t>(p.v.size()) - 1); i >= 0; --i) {
            if (javaEquals(vm, o, p.v[i].ref)) return i;
        }
        return -1;
    };
    registerFor(vm, VEC, "lastIndexOf", "(Ljava/lang/Object;)I", [vec, lastIndexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(lastIndexOf(vm, p, arg(a, 1).ref, std::numeric_limits<int32_t>::max()));
    });
    registerFor(vm, VEC, "lastIndexOf", "(Ljava/lang/Object;I)I", [vec, lastIndexOf](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return JavaValue(lastIndexOf(vm, p, arg(a, 1).ref, a[2].i));
    });
    registerFor(vm, VEC, "firstElement", "()Ljava/lang/Object;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (p.v.empty()) vm->throwJava("java/util/NoSuchElementException");
        return p.v.front();
    });
    registerFor(vm, VEC, "lastElement", "()Ljava/lang/Object;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (p.v.empty()) vm->throwJava("java/util/NoSuchElementException");
        return p.v.back();
    });
    registerFor(vm, VEC, "copyInto", "([Ljava/lang/Object;)V", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        JavaArray* arr = arrayArg(vm, a, 1);
        if (static_cast<size_t>(arr->length) < p.v.size()) vm->throwJava("java/lang/ArrayIndexOutOfBoundsException");
        for (size_t i = 0; i < p.v.size(); ++i) arr->elements[i] = p.v[i];
        return JavaValue();
    });
    registerFor(vm, VEC, "elements", "()Ljava/util/Enumeration;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        JavaObject* e = newNativeObject(vm, "java/util/Enumeration");
        ensurePayload<EnumerationPayload>(e).items = p.v;
        return refV(e);
    });
    registerFor(vm, VEC, "toString", "()Ljava/lang/String;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        std::u16string s = u"[";
        for (size_t i = 0; i < p.v.size(); ++i) {
            if (i) s += u", ";
            s += javaToString(vm, p.v[i].ref);
        }
        return newStr(vm, s + u"]");
    });

    // Stack
    const char* ST = "java/util/Stack";
    vm->registerNative(ST, "push", "(Ljava/lang/Object;)Ljava/lang/Object;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        p.v.push_back(arg(a, 1));
        return arg(a, 1);
    });
    vm->registerNative(ST, "pop", "()Ljava/lang/Object;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (p.v.empty()) vm->throwJava("java/util/EmptyStackException");
        JavaValue top = p.v.back();
        p.v.pop_back();
        return top;
    });
    vm->registerNative(ST, "peek", "()Ljava/lang/Object;", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        if (p.v.empty()) vm->throwJava("java/util/EmptyStackException");
        return p.v.back();
    });
    vm->registerNative(ST, "empty", "()Z", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        return boolV(p.v.empty());
    });
    vm->registerNative(ST, "search", "(Ljava/lang/Object;)I", [vec](CldcVirtualMachine* vm, const Args& a) {
        auto& p = vec(vm, a);
        Lock l(p.mtx);
        for (int32_t i = static_cast<int32_t>(p.v.size()) - 1; i >= 0; --i) {
            if (javaEquals(vm, arg(a, 1).ref, p.v[i].ref)) return JavaValue(static_cast<int32_t>(p.v.size()) - i);
        }
        return JavaValue(-1);
    });

    // Enumeration (snapshot)
    const char* EN = "java/util/Enumeration";
    vm->registerNative(EN, "hasMoreElements", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EnumerationPayload>(self(vm, a));
        return boolV(p && p->index < p->items.size());
    });
    vm->registerNative(EN, "nextElement", "()Ljava/lang/Object;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EnumerationPayload>(self(vm, a));
        if (!p || p->index >= p->items.size()) vm->throwJava("java/util/NoSuchElementException");
        return p->items[p->index++];
    });

    // Hashtable
    const char* H = "java/util/Hashtable";
    auto ht = [](CldcVirtualMachine* vm, const Args& a) -> HashtablePayload& {
        return ensurePayload<HashtablePayload>(self(vm, a));
    };
    auto findEntry = [](CldcVirtualMachine* vm, HashtablePayload& p, JavaObject* key, int32_t h) -> std::pair<JavaValue, JavaValue>* {
        auto it = p.buckets.find(h);
        if (it == p.buckets.end()) return nullptr;
        for (auto& e : it->second) {
            if (javaEquals(vm, key, e.first.ref)) return &e;
        }
        return nullptr;
    };
    vm->registerNative(H, "<init>", "()V", [ht](CldcVirtualMachine* vm, const Args& a) { ht(vm, a); return JavaValue(); });
    vm->registerNative(H, "<init>", "(I)V", [ht](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Illegal Capacity");
        ht(vm, a);
        return JavaValue();
    });
    vm->registerNative(H, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", [ht, findEntry](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* key = arg(a, 1).ref;
        // Hashtable rejects nulls; HashMap (sharing this implementation) accepts them
        if ((!key || !arg(a, 2).ref) && vm->classNameOf(self(vm, a)) == "java/util/Hashtable") vm->throwJava("java/lang/NullPointerException");
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        int32_t h = javaHashCode(vm, key);
        if (auto* e = findEntry(vm, p, key, h)) {
            JavaValue old = e->second;
            e->second = a[2];
            return old;
        }
        p.buckets[h].emplace_back(a[1], a[2]);
        p.count++;
        return nullV();
    });
    vm->registerNative(H, "get", "(Ljava/lang/Object;)Ljava/lang/Object;", [ht, findEntry](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* key = arg(a, 1).ref;
        if (!key && vm->classNameOf(self(vm, a)) == "java/util/Hashtable") vm->throwJava("java/lang/NullPointerException");
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        auto* e = findEntry(vm, p, key, javaHashCode(vm, key));
        return e ? e->second : nullV();
    });
    vm->registerNative(H, "containsKey", "(Ljava/lang/Object;)Z", [ht, findEntry](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* key = arg(a, 1).ref;
        if (!key && vm->classNameOf(self(vm, a)) == "java/util/Hashtable") vm->throwJava("java/lang/NullPointerException");
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        return boolV(findEntry(vm, p, key, javaHashCode(vm, key)) != nullptr);
    });
    vm->registerNative(H, "remove", "(Ljava/lang/Object;)Ljava/lang/Object;", [ht](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* key = arg(a, 1).ref;
        if (!key && vm->classNameOf(self(vm, a)) == "java/util/Hashtable") vm->throwJava("java/lang/NullPointerException");
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        auto it = p.buckets.find(javaHashCode(vm, key));
        if (it == p.buckets.end()) return nullV();
        auto& bucket = it->second;
        for (size_t i = 0; i < bucket.size(); ++i) {
            if (javaEquals(vm, key, bucket[i].first.ref)) {
                JavaValue old = bucket[i].second;
                bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(i));
                if (bucket.empty()) p.buckets.erase(it);
                p.count--;
                return old;
            }
        }
        return nullV();
    });
    auto containsValue = [ht](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* v = arg(a, 1).ref;
        if (!v) vm->throwJava("java/lang/NullPointerException");
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        for (auto& [h, bucket] : p.buckets) {
            for (auto& e : bucket) {
                if (javaEquals(vm, v, e.second.ref)) return boolV(true);
            }
        }
        return boolV(false);
    };
    vm->registerNative(H, "contains", "(Ljava/lang/Object;)Z", containsValue);
    vm->registerNative(H, "containsValue", "(Ljava/lang/Object;)Z", containsValue);
    vm->registerNative(H, "size", "()I", [ht](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        return JavaValue(static_cast<int32_t>(p.count));
    });
    vm->registerNative(H, "isEmpty", "()Z", [ht](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        return boolV(p.count == 0);
    });
    vm->registerNative(H, "clear", "()V", [ht](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        p.buckets.clear();
        p.count = 0;
        return JavaValue();
    });
    vm->registerNative(H, "rehash", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    auto enumerate = [ht](bool keys) {
        return [ht, keys](CldcVirtualMachine* vm, const Args& a) {
            auto& p = ht(vm, a);
            Lock l(p.mtx);
            JavaObject* e = newNativeObject(vm, "java/util/Enumeration");
            auto& items = ensurePayload<EnumerationPayload>(e).items;
            for (auto& [h, bucket] : p.buckets) {
                for (auto& entry : bucket) items.push_back(keys ? entry.first : entry.second);
            }
            return refV(e);
        };
    };
    vm->registerNative(H, "keys", "()Ljava/util/Enumeration;", enumerate(true));
    vm->registerNative(H, "elements", "()Ljava/util/Enumeration;", enumerate(false));
    vm->registerNative(H, "toString", "()Ljava/lang/String;", [ht](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ht(vm, a);
        Lock l(p.mtx);
        std::u16string s = u"{";
        bool first = true;
        for (auto& [h, bucket] : p.buckets) {
            for (auto& e : bucket) {
                if (!first) s += u", ";
                first = false;
                s += javaToString(vm, e.first.ref) + u"=" + javaToString(vm, e.second.ref);
            }
        }
        return newStr(vm, s + u"}");
    });

    // Random (java.util.Random's 48-bit LCG, bit-exact)
    const char* R = "java/util/Random";
    constexpr int64_t MULT = 0x5DEECE66DLL;
    constexpr int64_t MASK = (1LL << 48) - 1;
    auto rnd = [](CldcVirtualMachine* vm, const Args& a) -> RandomPayload& { return ensurePayload<RandomPayload>(self(vm, a)); };
    auto next = [](RandomPayload& p, int bits) -> int32_t {
        p.seed = (p.seed * MULT + 0xBLL) & MASK;
        return static_cast<int32_t>(static_cast<uint64_t>(p.seed) >> (48 - bits));
    };
    vm->registerNative(R, "<init>", "()V", [rnd](CldcVirtualMachine* vm, const Args& a) {
        static std::atomic<int64_t> uniquifier{8682522807148012LL};
        int64_t u = uniquifier.fetch_add(1) * 181783497276652981LL;
        rnd(vm, a).seed = ((u ^ (nowMillis() * 1000000)) ^ MULT) & MASK;
        return JavaValue();
    });
    vm->registerNative(R, "<init>", "(J)V", [rnd](CldcVirtualMachine* vm, const Args& a) {
        rnd(vm, a).seed = (arg(a, 1).l ^ MULT) & MASK;
        return JavaValue();
    });
    vm->registerNative(R, "setSeed", "(J)V", [rnd](CldcVirtualMachine* vm, const Args& a) {
        rnd(vm, a).seed = (arg(a, 1).l ^ MULT) & MASK;
        return JavaValue();
    });
    vm->registerNative(R, "next", "(I)I", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(next(rnd(vm, a), a[1].i));
    });
    vm->registerNative(R, "nextInt", "()I", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(next(rnd(vm, a), 32));
    });
    vm->registerNative(R, "nextInt", "(I)I", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        int32_t n = arg(a, 1).i;
        if (n <= 0) vm->throwJava("java/lang/IllegalArgumentException", "n must be positive");
        auto& p = rnd(vm, a);
        if ((n & -n) == n) return JavaValue(static_cast<int32_t>((static_cast<int64_t>(n) * next(p, 31)) >> 31));
        int32_t bits, val;
        do {
            bits = next(p, 31);
            val = bits % n;
        } while (static_cast<int32_t>(static_cast<uint32_t>(bits) - static_cast<uint32_t>(val) + static_cast<uint32_t>(n - 1)) < 0);
        return JavaValue(val);
    });
    vm->registerNative(R, "nextLong", "()J", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        auto& p = rnd(vm, a);
        int64_t hi = next(p, 32);
        int64_t lo = next(p, 32);
        return JavaValue(static_cast<int64_t>(static_cast<uint64_t>(hi) << 32) + lo);
    });
    vm->registerNative(R, "nextBoolean", "()Z", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        return boolV(next(rnd(vm, a), 1) != 0);
    });
    vm->registerNative(R, "nextFloat", "()F", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(next(rnd(vm, a), 24) / static_cast<float>(1 << 24));
    });
    vm->registerNative(R, "nextDouble", "()D", [rnd, next](CldcVirtualMachine* vm, const Args& a) {
        auto& p = rnd(vm, a);
        int64_t hi = next(p, 26);
        int64_t lo = next(p, 27);
        return JavaValue(static_cast<double>((hi << 27) + lo) * (1.0 / static_cast<double>(1LL << 53)));
    });

    // Date
    const char* DT = "java/util/Date";
    auto tp = [](CldcVirtualMachine* vm, const Args& a) -> TimePayload& { return ensurePayload<TimePayload>(self(vm, a)); };
    vm->registerNative(DT, "<init>", "()V", [tp](CldcVirtualMachine* vm, const Args& a) { tp(vm, a).millis = nowMillis(); return JavaValue(); });
    vm->registerNative(DT, "<init>", "(J)V", [tp](CldcVirtualMachine* vm, const Args& a) { tp(vm, a).millis = arg(a, 1).l; return JavaValue(); });
    vm->registerNative(DT, "getTime", "()J", [tp](CldcVirtualMachine* vm, const Args& a) { return JavaValue(tp(vm, a).millis); });
    vm->registerNative(DT, "setTime", "(J)V", [tp](CldcVirtualMachine* vm, const Args& a) { tp(vm, a).millis = arg(a, 1).l; return JavaValue(); });
    vm->registerNative(DT, "equals", "(Ljava/lang/Object;)Z", [tp](CldcVirtualMachine* vm, const Args& a) {
        auto* other = getPayload<TimePayload>(arg(a, 1).ref);
        return boolV(other && vm->classNameOf(arg(a, 1).ref) == "java/util/Date" && other->millis == tp(vm, a).millis);
    });
    vm->registerNative(DT, "hashCode", "()I", [tp](CldcVirtualMachine* vm, const Args& a) {
        int64_t m = tp(vm, a).millis;
        return JavaValue(static_cast<int32_t>(m ^ static_cast<int64_t>(static_cast<uint64_t>(m) >> 32)));
    });
    vm->registerNative(DT, "toString", "()Ljava/lang/String;", [tp](CldcVirtualMachine* vm, const Args& a) {
        std::tm t = localTm(tp(vm, a).millis);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &t);
        return newStrUtf8(vm, buf);
    });

    // Calendar / TimeZone (local time zone)
    const char* CAL = "java/util/Calendar";
    auto newCalendar = [](CldcVirtualMachine* vm, const Args&) {
        JavaObject* c = newNativeObject(vm, "java/util/Calendar");
        ensurePayload<TimePayload>(c).millis = nowMillis();
        return refV(c);
    };
    vm->registerNative(CAL, "getInstance", "()Ljava/util/Calendar;", newCalendar);
    vm->registerNative(CAL, "getInstance", "(Ljava/util/TimeZone;)Ljava/util/Calendar;", newCalendar);
    vm->registerNative(CAL, "get", "(I)I", [tp](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(calendarGet(tp(vm, a).millis, a[1].i));
    });
    vm->registerNative(CAL, "set", "(II)V", [tp](CldcVirtualMachine* vm, const Args& a) {
        auto& p = tp(vm, a);
        p.millis = calendarSet(p.millis, a[1].i, a[2].i);
        return JavaValue();
    });
    vm->registerNative(CAL, "getTime", "()Ljava/util/Date;", [tp](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* d = newNativeObject(vm, "java/util/Date");
        ensurePayload<TimePayload>(d).millis = tp(vm, a).millis;
        return refV(d);
    });
    vm->registerNative(CAL, "setTime", "(Ljava/util/Date;)V", [tp](CldcVirtualMachine* vm, const Args& a) {
        auto* d = getPayload<TimePayload>(arg(a, 1).ref);
        if (!d) vm->throwJava("java/lang/NullPointerException");
        tp(vm, a).millis = d->millis;
        return JavaValue();
    });
    vm->registerNative(CAL, "getTimeInMillis", "()J", [tp](CldcVirtualMachine* vm, const Args& a) { return JavaValue(tp(vm, a).millis); });
    vm->registerNative(CAL, "setTimeInMillis", "(J)V", [tp](CldcVirtualMachine* vm, const Args& a) { tp(vm, a).millis = arg(a, 1).l; return JavaValue(); });
    vm->registerNative(CAL, "setTimeZone", "(Ljava/util/TimeZone;)V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    auto newTimeZone = [](CldcVirtualMachine* vm, const Args&) { return refV(newNativeObject(vm, "java/util/TimeZone")); };
    vm->registerNative("java/util/TimeZone", "getDefault", "()Ljava/util/TimeZone;", newTimeZone);
    vm->registerNative("java/util/TimeZone", "getTimeZone", "(Ljava/lang/String;)Ljava/util/TimeZone;", newTimeZone);
    vm->registerNative("java/util/TimeZone", "getRawOffset", "()I", [](CldcVirtualMachine*, const Args&) {
        std::time_t now = std::time(nullptr);
        std::tm local = localTm(static_cast<int64_t>(now) * 1000);
        std::tm utc{};
#if defined(_WIN32)
        gmtime_s(&utc, &now);
#else
        gmtime_r(&now, &utc);
#endif
        utc.tm_isdst = local.tm_isdst;
        return JavaValue(static_cast<int32_t>(std::difftime(std::mktime(&local), std::mktime(&utc)) * 1000));
    });
    vm->registerNative("java/util/TimeZone", "getID", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args&) {
        return newStrUtf8(vm, "GMT");
    });

    // Timer / TimerTask
    const char* TM = "java/util/Timer";
    const char* TT = "java/util/TimerTask";
    vm->registerNative(TM, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) { ensurePayload<TimerPayload>(self(vm, a)); return JavaValue(); });
    vm->registerNative(TT, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) { ensurePayload<TimerPayload>(self(vm, a)); return JavaValue(); });
    vm->registerNative(TM, "cancel", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<TimerPayload>(self(vm, a)).cancelled->store(true);
        return JavaValue();
    });
    vm->registerNative(TT, "cancel", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<TimerPayload>(self(vm, a));
        return boolV(!p.cancelled->exchange(true));
    });
    auto schedule = [](bool fixedRate, bool periodic) {
        return [fixedRate, periodic](CldcVirtualMachine* vm, const Args& a) {
            JavaObject* timer = self(vm, a);
            JavaObject* task = arg(a, 1).ref;
            if (!task) vm->throwJava("java/lang/NullPointerException");
            int64_t delay = arg(a, 2).l;
            int64_t period = periodic ? arg(a, 3).l : 0;
            if (delay < 0) vm->throwJava("java/lang/IllegalArgumentException", "Negative delay.");
            if (periodic && period <= 0) vm->throwJava("java/lang/IllegalArgumentException", "Non-positive period.");
            auto timerCancelled = ensurePayload<TimerPayload>(timer).cancelled;
            auto taskCancelled = ensurePayload<TimerPayload>(task).cancelled;
            if (timerCancelled->load()) vm->throwJava("java/lang/IllegalStateException", "Timer already cancelled.");
            vm->pin(task);
            try { startDetachedJavaThread(vm, [vm, task, delay, period, fixedRate, timerCancelled, taskCancelled]() { runJavaThread(vm, [&] {
                auto nextRun = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
                for (;;) {
                    while (std::chrono::steady_clock::now() < nextRun) {
                        if (timerCancelled->load() || taskCancelled->load() || !engineRunning(vm)) return;
                        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(nextRun - std::chrono::steady_clock::now());
                        std::this_thread::sleep_for(std::min(left, std::chrono::milliseconds(20)));
                    }
                    if (timerCancelled->load() || taskCancelled->load() || !engineRunning(vm)) return;
                    if (!enginePaused(vm)) runJavaRunnable(vm, task, "TimerTask");
                    if (period <= 0) return;
                    nextRun = fixedRate ? nextRun + std::chrono::milliseconds(period)
                                        : std::chrono::steady_clock::now() + std::chrono::milliseconds(period);
                }
            }); vm->unpin(task); }); } catch (...) {
                vm->unpin(task);
                throw;
            }
            return JavaValue();
        };
    };
    vm->registerNative(TM, "schedule", "(Ljava/util/TimerTask;J)V", schedule(false, false));
    vm->registerNative(TM, "schedule", "(Ljava/util/TimerTask;JJ)V", schedule(false, true));
    vm->registerNative(TM, "scheduleAtFixedRate", "(Ljava/util/TimerTask;JJ)V", schedule(true, true));
    vm->registerNative(TM, "schedule", "(Ljava/util/TimerTask;Ljava/util/Date;)V", [schedule](CldcVirtualMachine* vm, const Args& a) {
        auto* d = getPayload<TimePayload>(arg(a, 2).ref);
        Args b = {a[0], a[1], JavaValue(std::max<int64_t>(0, (d ? d->millis : 0) - nowMillis()))};
        return schedule(false, false)(vm, b);
    });
}

// ============================================================================
// java.io: InputStream, ByteArrayInputStream, DataInputStream,
//          OutputStream, ByteArrayOutputStream, DataOutputStream
// ============================================================================

void registerIo(CldcVirtualMachine* vm) {
    // Generic InputStream protocol (resource streams, byte-array streams, filters)
    const char* IS = "java/io/InputStream";
    vm->registerNative(IS, "<init>", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(IS, "read", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        // A bytecode subclass reaching the native read() has not overridden it
        if (!dynamic_cast<JavaInputStreamObject*>(s) && !getPayload<ByteInputPayload>(s) && !getPayload<FilterInputPayload>(s)) return JavaValue(-1);
        return JavaValue(streamRead(vm, s));
    });
    vm->registerNative(IS, "read", "([B)I", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        return JavaValue(streamReadBlock(vm, self(vm, a), arr, 0, arr->length));
    });
    vm->registerNative(IS, "read", "([BII)I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(streamReadBlock(vm, self(vm, a), arrayArg(vm, a, 1), a[2].i, a[3].i));
    });
    vm->registerNative(IS, "available", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        if (!dynamic_cast<JavaInputStreamObject*>(s) && !getPayload<ByteInputPayload>(s) && !getPayload<FilterInputPayload>(s)) return JavaValue(0);
        return JavaValue(streamAvailable(vm, s));
    });
    vm->registerNative(IS, "skip", "(J)J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(streamSkip(vm, self(vm, a), arg(a, 1).l));
    });
    vm->registerNative(IS, "close", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(IS, "markSupported", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        return boolV(dynamic_cast<JavaInputStreamObject*>(s) || getPayload<ByteInputPayload>(s));
    });
    vm->registerNative(IS, "mark", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) jis->mark = jis->pos;
        else if (auto* p = getPayload<ByteInputPayload>(s)) p->mark = p->pos;
        else if (auto* f = getPayload<FilterInputPayload>(s)) vm->executeMethodByName(vm->classNameOf(f->in), "mark", "(I)V", {refV(f->in), arg(a, 1)});
        return JavaValue();
    });
    vm->registerNative(IS, "reset", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        if (auto* jis = dynamic_cast<JavaInputStreamObject*>(s)) jis->pos = jis->mark;
        else if (auto* p = getPayload<ByteInputPayload>(s)) p->pos = p->mark;
        else if (auto* f = getPayload<FilterInputPayload>(s)) vm->executeMethodByName(vm->classNameOf(f->in), "reset", "()V", {refV(f->in)});
        else vm->throwJava("java/io/IOException", "mark/reset not supported");
        return JavaValue();
    });

    // ByteArrayInputStream
    const char* BAIS = "java/io/ByteArrayInputStream";
    vm->registerNative(BAIS, "<init>", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        ensurePayload<ByteInputPayload>(self(vm, a)).data = bytesOf(arr, 0, arr->length);
        return JavaValue();
    });
    vm->registerNative(BAIS, "<init>", "([BII)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        int32_t off = std::clamp(a[2].i, 0, arr->length);
        int32_t len = std::clamp(a[3].i, 0, arr->length - off);
        auto& p = ensurePayload<ByteInputPayload>(self(vm, a));
        p.data = bytesOf(arr, off, len);
        return JavaValue();
    });

    // DataInputStream
    const char* DIS = "java/io/DataInputStream";
    vm->registerNative(DIS, "<init>", "(Ljava/io/InputStream;)V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<FilterInputPayload>(self(vm, a)).in = arg(a, 1).ref;
        return JavaValue();
    });
    vm->registerNative(DIS, "close", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto* f = getPayload<FilterInputPayload>(self(vm, a));
        if (f && f->in) vm->executeMethodByName(vm->classNameOf(f->in), "close", "()V", {refV(f->in)});
        return JavaValue();
    });
    vm->registerNative(DIS, "readFully", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        readFully(vm, self(vm, a), arr, 0, arr->length);
        return JavaValue();
    });
    vm->registerNative(DIS, "readFully", "([BII)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        checkRange(vm, a[2].i, a[3].i, arr->length, "java/lang/IndexOutOfBoundsException");
        readFully(vm, self(vm, a), arr, a[2].i, a[3].i);
        return JavaValue();
    });
    vm->registerNative(DIS, "skipBytes", "(I)I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(streamSkip(vm, self(vm, a), arg(a, 1).i)));
    });
    vm->registerNative(DIS, "readBoolean", "()Z", [](CldcVirtualMachine* vm, const Args& a) { return boolV(readByteOrEof(vm, self(vm, a)) != 0); });
    vm->registerNative(DIS, "readByte", "()B", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(static_cast<int8_t>(readByteOrEof(vm, self(vm, a)))));
    });
    vm->registerNative(DIS, "readUnsignedByte", "()I", [](CldcVirtualMachine* vm, const Args& a) { return JavaValue(readByteOrEof(vm, self(vm, a))); });
    vm->registerNative(DIS, "readShort", "()S", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(static_cast<int16_t>(readBigEndian(vm, self(vm, a), 2))));
    });
    vm->registerNative(DIS, "readUnsignedShort", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(readBigEndian(vm, self(vm, a), 2)));
    });
    vm->registerNative(DIS, "readChar", "()C", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(readBigEndian(vm, self(vm, a), 2)));
    });
    vm->registerNative(DIS, "readInt", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int32_t>(static_cast<uint32_t>(readBigEndian(vm, self(vm, a), 4))));
    });
    vm->registerNative(DIS, "readLong", "()J", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(static_cast<int64_t>(readBigEndian(vm, self(vm, a), 8)));
    });
    vm->registerNative(DIS, "readFloat", "()F", [](CldcVirtualMachine* vm, const Args& a) {
        uint32_t bits = static_cast<uint32_t>(readBigEndian(vm, self(vm, a), 4));
        float f;
        std::memcpy(&f, &bits, 4);
        return JavaValue(f);
    });
    vm->registerNative(DIS, "readDouble", "()D", [](CldcVirtualMachine* vm, const Args& a) {
        uint64_t bits = readBigEndian(vm, self(vm, a), 8);
        double d;
        std::memcpy(&d, &bits, 8);
        return JavaValue(d);
    });
    auto readUTF = [](CldcVirtualMachine* vm, JavaObject* s) {
        uint32_t len = static_cast<uint32_t>(readBigEndian(vm, s, 2));
        JavaArray* tmp = vm->allocateArray('B', static_cast<int32_t>(len));
        readFully(vm, s, tmp, 0, static_cast<int32_t>(len));
        std::vector<uint8_t> bytes = bytesOf(tmp, 0, static_cast<int32_t>(len));
        return newStr(vm, fromModifiedUtf8(bytes.data(), bytes.size()));
    };
    vm->registerNative(DIS, "readUTF", "()Ljava/lang/String;", [readUTF](CldcVirtualMachine* vm, const Args& a) {
        JavaValue r = readUTF(vm, self(vm, a));
        static const bool dump = std::getenv("J2ME_UTFLOG") != nullptr;
        if (dump && r.ref) {
            std::u16string u = javaToString(vm, r.ref);
            std::string o;
            for (char16_t c : u) {
                if (c < 0x80) o.push_back(static_cast<char>(c));
                else if (c < 0x800) { o.push_back(static_cast<char>(0xC0 | (c >> 6))); o.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
                else { o.push_back(static_cast<char>(0xE0 | (c >> 12))); o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F))); o.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
            }
            std::cerr << "[UTF] " << o << std::endl;
        }
        return r;
    });
    vm->registerNative(DIS, "readUTF", "(Ljava/io/DataInput;)Ljava/lang/String;", [readUTF](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = arg(a, 0).ref;
        if (!s) vm->throwJava("java/lang/NullPointerException");
        return readUTF(vm, s);
    });

    // Generic OutputStream protocol
    const char* OS = "java/io/OutputStream";
    vm->registerNative(OS, "<init>", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(OS, "write", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* s = self(vm, a);
        if (getPayload<ByteOutputPayload>(s) || getPayload<FilterOutputPayload>(s)) streamWrite(vm, s, arg(a, 1).i);
        return JavaValue();
    });
    vm->registerNative(OS, "write", "([B)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        std::vector<uint8_t> b = bytesOf(arr, 0, arr->length);
        streamWriteBlock(vm, self(vm, a), b.data(), b.size());
        return JavaValue();
    });
    vm->registerNative(OS, "write", "([BII)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 1);
        checkRange(vm, a[2].i, a[3].i, arr->length, "java/lang/IndexOutOfBoundsException");
        std::vector<uint8_t> b = bytesOf(arr, a[2].i, a[3].i);
        streamWriteBlock(vm, self(vm, a), b.data(), b.size());
        return JavaValue();
    });
    vm->registerNative(OS, "flush", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
    vm->registerNative(OS, "close", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });

    // ByteArrayOutputStream
    const char* BAOS = "java/io/ByteArrayOutputStream";
    auto bo = [](CldcVirtualMachine* vm, const Args& a) -> ByteOutputPayload& { return ensurePayload<ByteOutputPayload>(self(vm, a)); };
    vm->registerNative(BAOS, "<init>", "()V", [bo](CldcVirtualMachine* vm, const Args& a) { bo(vm, a); return JavaValue(); });
    vm->registerNative(BAOS, "<init>", "(I)V", [bo](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Negative initial size");
        bo(vm, a).data.reserve(static_cast<size_t>(arg(a, 1).i));
        return JavaValue();
    });
    vm->registerNative(BAOS, "toByteArray", "()[B", [bo](CldcVirtualMachine* vm, const Args& a) {
        auto& p = bo(vm, a);
        return refV(newByteArray(vm, p.data.data(), p.data.size()));
    });
    vm->registerNative(BAOS, "size", "()I", [bo](CldcVirtualMachine* vm, const Args& a) { return JavaValue(static_cast<int32_t>(bo(vm, a).data.size())); });
    vm->registerNative(BAOS, "reset", "()V", [bo](CldcVirtualMachine* vm, const Args& a) { bo(vm, a).data.clear(); return JavaValue(); });
    vm->registerNative(BAOS, "toString", "()Ljava/lang/String;", [bo](CldcVirtualMachine* vm, const Args& a) {
        return newStr(vm, decodeBytes(bo(vm, a).data, Charset::Utf8));
    });

    // DataOutputStream
    const char* DOS = "java/io/DataOutputStream";
    auto out = [](CldcVirtualMachine* vm, const Args& a) -> JavaObject* { return self(vm, a); };
    vm->registerNative(DOS, "<init>", "(Ljava/io/OutputStream;)V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<FilterOutputPayload>(self(vm, a)).out = arg(a, 1).ref;
        return JavaValue();
    });
    vm->registerNative(DOS, "flush", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto* f = getPayload<FilterOutputPayload>(self(vm, a));
        if (f && f->out) vm->executeMethodByName(vm->classNameOf(f->out), "flush", "()V", {refV(f->out)});
        return JavaValue();
    });
    vm->registerNative(DOS, "close", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto* f = getPayload<FilterOutputPayload>(self(vm, a));
        if (f && f->out) vm->executeMethodByName(vm->classNameOf(f->out), "close", "()V", {refV(f->out)});
        return JavaValue();
    });
    vm->registerNative(DOS, "size", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        auto* f = getPayload<FilterOutputPayload>(self(vm, a));
        return JavaValue(f ? f->written : 0);
    });
    vm->registerNative(DOS, "writeBoolean", "(Z)V", [out](CldcVirtualMachine* vm, const Args& a) { streamWrite(vm, out(vm, a), a[1].i ? 1 : 0); return JavaValue(); });
    vm->registerNative(DOS, "writeByte", "(I)V", [out](CldcVirtualMachine* vm, const Args& a) { streamWrite(vm, out(vm, a), a[1].i); return JavaValue(); });
    vm->registerNative(DOS, "writeShort", "(I)V", [out](CldcVirtualMachine* vm, const Args& a) { writeBigEndian(vm, out(vm, a), static_cast<uint32_t>(a[1].i), 2); return JavaValue(); });
    vm->registerNative(DOS, "writeChar", "(I)V", [out](CldcVirtualMachine* vm, const Args& a) { writeBigEndian(vm, out(vm, a), static_cast<uint32_t>(a[1].i), 2); return JavaValue(); });
    vm->registerNative(DOS, "writeInt", "(I)V", [out](CldcVirtualMachine* vm, const Args& a) { writeBigEndian(vm, out(vm, a), static_cast<uint32_t>(a[1].i), 4); return JavaValue(); });
    vm->registerNative(DOS, "writeLong", "(J)V", [out](CldcVirtualMachine* vm, const Args& a) { writeBigEndian(vm, out(vm, a), static_cast<uint64_t>(a[1].l), 8); return JavaValue(); });
    vm->registerNative(DOS, "writeFloat", "(F)V", [out](CldcVirtualMachine* vm, const Args& a) {
        writeBigEndian(vm, out(vm, a), static_cast<uint32_t>(floatBits(a[1].f)), 4);
        return JavaValue();
    });
    vm->registerNative(DOS, "writeDouble", "(D)V", [out](CldcVirtualMachine* vm, const Args& a) {
        writeBigEndian(vm, out(vm, a), static_cast<uint64_t>(doubleBits(a[1].d)), 8);
        return JavaValue();
    });
    vm->registerNative(DOS, "writeChars", "(Ljava/lang/String;)V", [out](CldcVirtualMachine* vm, const Args& a) {
        for (char16_t c : strArg(vm, a, 1)) writeBigEndian(vm, out(vm, a), c, 2);
        return JavaValue();
    });
    vm->registerNative(DOS, "writeBytes", "(Ljava/lang/String;)V", [out](CldcVirtualMachine* vm, const Args& a) {
        std::u16string s = strArg(vm, a, 1);
        std::vector<uint8_t> b(s.begin(), s.end()); // low byte of each char
        streamWriteBlock(vm, out(vm, a), b.data(), b.size());
        return JavaValue();
    });
    vm->registerNative(DOS, "writeUTF", "(Ljava/lang/String;)V", [out](CldcVirtualMachine* vm, const Args& a) {
        std::vector<uint8_t> b = toModifiedUtf8(strArg(vm, a, 1));
        if (b.size() > 65535) vm->throwJava("java/io/UTFDataFormatException", "encoded string too long");
        writeBigEndian(vm, out(vm, a), static_cast<uint32_t>(b.size()), 2);
        streamWriteBlock(vm, out(vm, a), b.data(), b.size());
        return JavaValue();
    });
}

// ============================================================================
// Java SE collections used by converted (dex2jar) MIDlets, built on the
// Vector / Hashtable payloads: ArrayList, LinkedList, HashMap, HashSet, Iterator
// ============================================================================

JavaObject* newIterator(CldcVirtualMachine* vm, std::vector<JavaValue> items) {
    JavaObject* it = newNativeObject(vm, "java/util/Iterator");
    ensurePayload<EnumerationPayload>(it).items = std::move(items);
    return it;
}

JavaObject* newListSnapshot(CldcVirtualMachine* vm, std::vector<JavaValue> items) {
    JavaObject* list = newNativeObject(vm, "java/util/ArrayList");
    ensurePayload<VectorPayload>(list).v = std::move(items);
    return list;
}

// Elements of a List/Set (native payload) or of a bytecode Collection via iterator()
std::vector<JavaValue> collectionItems(CldcVirtualMachine* vm, JavaObject* c) {
    if (!c) vm->throwJava("java/lang/NullPointerException");
    if (auto* v = getPayload<VectorPayload>(c)) {
        std::lock_guard<std::recursive_mutex> l(v->mtx);
        return v->v;
    }
    if (auto* h = getPayload<HashtablePayload>(c)) { // HashSet
        std::lock_guard<std::recursive_mutex> l(h->mtx);
        std::vector<JavaValue> out;
        for (auto& [hash, bucket] : h->buckets) {
            for (auto& e : bucket) out.push_back(e.first);
        }
        return out;
    }
    std::vector<JavaValue> out;
    JavaValue it = vm->executeMethodByName(vm->classNameOf(c), "iterator", "()Ljava/util/Iterator;", {refV(c)});
    while (it.ref && vm->executeMethodByName(vm->classNameOf(it.ref), "hasNext", "()Z", {it}).i) {
        out.push_back(vm->executeMethodByName(vm->classNameOf(it.ref), "next", "()Ljava/lang/Object;", {it}));
    }
    return out;
}

void registerSeCollections(CldcVirtualMachine* vm) {
    using Lock = std::lock_guard<std::recursive_mutex>;
    const auto LISTS = {"java/util/Vector", "java/util/ArrayList", "java/util/LinkedList"};

    // Iterator (snapshot)
    const char* IT = "java/util/Iterator";
    vm->registerNative(IT, "hasNext", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EnumerationPayload>(self(vm, a));
        return boolV(p && p->index < p->items.size());
    });
    vm->registerNative(IT, "next", "()Ljava/lang/Object;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EnumerationPayload>(self(vm, a));
        if (!p || p->index >= p->items.size()) vm->throwJava("java/util/NoSuchElementException");
        return p->items[p->index++];
    });
    vm->registerNative(IT, "remove", "()V", [](CldcVirtualMachine* vm, const Args&) -> JavaValue {
        vm->throwJava("java/lang/UnsupportedOperationException", "Iterator.remove");
    });

    // List additions (ArrayList / LinkedList share Vector's implementation)
    registerFor(vm, LISTS, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<VectorPayload>(self(vm, a));
        return JavaValue();
    });
    registerFor(vm, LISTS, "<init>", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Illegal Capacity");
        ensurePayload<VectorPayload>(self(vm, a)).v.reserve(static_cast<size_t>(arg(a, 1).i));
        return JavaValue();
    });
    registerFor(vm, LISTS, "<init>", "(Ljava/util/Collection;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto items = collectionItems(vm, arg(a, 1).ref);
        ensurePayload<VectorPayload>(self(vm, a)).v = std::move(items);
        return JavaValue();
    });
    registerFor(vm, LISTS, "iterator", "()Ljava/util/Iterator;", [](CldcVirtualMachine* vm, const Args& a) {
        return refV(newIterator(vm, collectionItems(vm, self(vm, a))));
    });
    registerFor(vm, LISTS, "addAll", "(Ljava/util/Collection;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto items = collectionItems(vm, arg(a, 1).ref);
        auto& p = ensurePayload<VectorPayload>(self(vm, a));
        Lock l(p.mtx);
        p.v.insert(p.v.end(), items.begin(), items.end());
        return boolV(!items.empty());
    });
    registerFor(vm, LISTS, "equals", "(Ljava/lang/Object;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* me = self(vm, a);
        JavaObject* other = arg(a, 1).ref;
        if (me == other) return boolV(true);
        if (!getPayload<VectorPayload>(other)) return boolV(false);
        auto x = collectionItems(vm, me);
        auto y = collectionItems(vm, other);
        if (x.size() != y.size()) return boolV(false);
        for (size_t i = 0; i < x.size(); ++i) {
            if (!javaEquals(vm, x[i].ref, y[i].ref)) return boolV(false);
        }
        return boolV(true);
    });
    registerFor(vm, LISTS, "hashCode", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        uint32_t h = 1;
        for (auto& e : collectionItems(vm, self(vm, a))) h = 31u * h + static_cast<uint32_t>(javaHashCode(vm, e.ref));
        return JavaValue(static_cast<int32_t>(h));
    });

    // Queue / Deque subset (LinkedList)
    auto front = [](bool remove, bool throwIfEmpty) {
        return [remove, throwIfEmpty](CldcVirtualMachine* vm, const Args& a) {
            auto& p = ensurePayload<VectorPayload>(self(vm, a));
            Lock l(p.mtx);
            if (p.v.empty()) {
                if (throwIfEmpty) vm->throwJava("java/util/NoSuchElementException");
                return nullV();
            }
            JavaValue v = p.v.front();
            if (remove) p.v.erase(p.v.begin());
            return v;
        };
    };
    registerFor(vm, LISTS, "poll", "()Ljava/lang/Object;", front(true, false));
    registerFor(vm, LISTS, "peek", "()Ljava/lang/Object;", front(false, false));
    registerFor(vm, LISTS, "removeFirst", "()Ljava/lang/Object;", front(true, true));
    registerFor(vm, LISTS, "getFirst", "()Ljava/lang/Object;", front(false, true));
    auto pushBack = [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<VectorPayload>(self(vm, a));
        Lock l(p.mtx);
        p.v.push_back(arg(a, 1));
    };
    registerFor(vm, LISTS, "offer", "(Ljava/lang/Object;)Z", [pushBack](CldcVirtualMachine* vm, const Args& a) {
        pushBack(vm, a);
        return boolV(true);
    });
    registerFor(vm, LISTS, "addLast", "(Ljava/lang/Object;)V", [pushBack](CldcVirtualMachine* vm, const Args& a) {
        pushBack(vm, a);
        return JavaValue();
    });
    registerFor(vm, LISTS, "addFirst", "(Ljava/lang/Object;)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<VectorPayload>(self(vm, a));
        Lock l(p.mtx);
        p.v.insert(p.v.begin(), arg(a, 1));
        return JavaValue();
    });

    // HashMap / LinkedHashMap use the Hashtable implementation; add Map views
    const auto MAPS = {"java/util/Hashtable", "java/util/HashMap", "java/util/LinkedHashMap"};
    registerFor(vm, MAPS, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        ensurePayload<HashtablePayload>(self(vm, a));
        return JavaValue();
    });
    registerFor(vm, MAPS, "<init>", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "Illegal Capacity");
        ensurePayload<HashtablePayload>(self(vm, a));
        return JavaValue();
    });
    auto mapEntries = [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<HashtablePayload>(self(vm, a));
        Lock l(p.mtx);
        std::vector<std::pair<JavaValue, JavaValue>> out;
        for (auto& [h, bucket] : p.buckets) out.insert(out.end(), bucket.begin(), bucket.end());
        return out;
    };
    registerFor(vm, MAPS, "keySet", "()Ljava/util/Set;", [mapEntries](CldcVirtualMachine* vm, const Args& a) {
        std::vector<JavaValue> keys;
        for (auto& e : mapEntries(vm, a)) keys.push_back(e.first);
        return refV(newListSnapshot(vm, std::move(keys)));
    });
    registerFor(vm, MAPS, "values", "()Ljava/util/Collection;", [mapEntries](CldcVirtualMachine* vm, const Args& a) {
        std::vector<JavaValue> values;
        for (auto& e : mapEntries(vm, a)) values.push_back(e.second);
        return refV(newListSnapshot(vm, std::move(values)));
    });
    registerFor(vm, MAPS, "entrySet", "()Ljava/util/Set;", [mapEntries](CldcVirtualMachine* vm, const Args& a) {
        std::vector<JavaValue> entries;
        for (auto& e : mapEntries(vm, a)) {
            JavaObject* entry = newNativeObject(vm, "java/util/Map$Entry");
            auto& ep = ensurePayload<EntryPayload>(entry);
            ep.key = e.first;
            ep.value = e.second;
            entries.push_back(refV(entry));
        }
        return refV(newListSnapshot(vm, std::move(entries)));
    });
    registerFor(vm, MAPS, "getOrDefault", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", [](CldcVirtualMachine* vm, const Args& a) {
        const std::string cls = vm->classNameOf(self(vm, a));
        if (vm->executeMethodByName(cls, "containsKey", "(Ljava/lang/Object;)Z", {a[0], arg(a, 1)}).i) {
            return vm->executeMethodByName(cls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;", {a[0], arg(a, 1)});
        }
        return arg(a, 2);
    });
    vm->registerNative("java/util/Map$Entry", "getKey", "()Ljava/lang/Object;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EntryPayload>(self(vm, a));
        return p ? p->key : nullV();
    });
    vm->registerNative("java/util/Map$Entry", "getValue", "()Ljava/lang/Object;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<EntryPayload>(self(vm, a));
        return p ? p->value : nullV();
    });

    // HashSet: the keys of a Hashtable payload
    const char* HS = "java/util/HashSet";
    auto hs = [](CldcVirtualMachine* vm, const Args& a) -> HashtablePayload& { return ensurePayload<HashtablePayload>(self(vm, a)); };
    auto hsFind = [](CldcVirtualMachine* vm, HashtablePayload& p, JavaObject* o, int32_t h) -> int {
        auto it = p.buckets.find(h);
        if (it == p.buckets.end()) return -1;
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (javaEquals(vm, o, it->second[i].first.ref)) return static_cast<int>(i);
        }
        return -1;
    };
    vm->registerNative(HS, "<init>", "()V", [hs](CldcVirtualMachine* vm, const Args& a) { hs(vm, a); return JavaValue(); });
    vm->registerNative(HS, "add", "(Ljava/lang/Object;)Z", [hs, hsFind](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        int32_t h = javaHashCode(vm, arg(a, 1).ref);
        if (hsFind(vm, p, arg(a, 1).ref, h) >= 0) return boolV(false);
        p.buckets[h].emplace_back(arg(a, 1), arg(a, 1));
        p.count++;
        return boolV(true);
    });
    vm->registerNative(HS, "contains", "(Ljava/lang/Object;)Z", [hs, hsFind](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        return boolV(hsFind(vm, p, arg(a, 1).ref, javaHashCode(vm, arg(a, 1).ref)) >= 0);
    });
    vm->registerNative(HS, "remove", "(Ljava/lang/Object;)Z", [hs, hsFind](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        int32_t h = javaHashCode(vm, arg(a, 1).ref);
        int idx = hsFind(vm, p, arg(a, 1).ref, h);
        if (idx < 0) return boolV(false);
        auto& bucket = p.buckets[h];
        bucket.erase(bucket.begin() + idx);
        if (bucket.empty()) p.buckets.erase(h);
        p.count--;
        return boolV(true);
    });
    vm->registerNative(HS, "size", "()I", [hs](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        return JavaValue(static_cast<int32_t>(p.count));
    });
    vm->registerNative(HS, "isEmpty", "()Z", [hs](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        return boolV(p.count == 0);
    });
    vm->registerNative(HS, "clear", "()V", [hs](CldcVirtualMachine* vm, const Args& a) {
        auto& p = hs(vm, a);
        Lock l(p.mtx);
        p.buckets.clear();
        p.count = 0;
        return JavaValue();
    });
    vm->registerNative(HS, "iterator", "()Ljava/util/Iterator;", [](CldcVirtualMachine* vm, const Args& a) {
        return refV(newIterator(vm, collectionItems(vm, self(vm, a))));
    });

    // java.util.Collections / java.util.Arrays helpers
    vm->registerNative("java/util/Collections", "addAll", "(Ljava/util/Collection;[Ljava/lang/Object;)Z", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* c = arg(a, 0).ref;
        JavaArray* arr = arrayArg(vm, a, 1);
        if (!c) vm->throwJava("java/lang/NullPointerException");
        bool changed = false;
        for (auto& e : arr->elements) {
            changed |= vm->executeMethodByName(vm->classNameOf(c), "add", "(Ljava/lang/Object;)Z", {refV(c), e}).i != 0;
        }
        return boolV(changed);
    });
    for (char t : std::string("BCSIJFDZ")) {
        const std::string arr = std::string("[") + t;
        vm->registerNative("java/util/Arrays", "copyOf", "(" + arr + "I)" + arr, [t](CldcVirtualMachine* vm, const Args& a) {
            JavaArray* src = arrayArg(vm, a, 0);
            int32_t n = arg(a, 1).i;
            if (n < 0) vm->throwJava("java/lang/NegativeArraySizeException");
            JavaArray* dst = vm->allocateArray(t, n);
            for (int32_t i = 0; i < std::min(n, src->length); ++i) dst->elements[i] = src->elements[i];
            return refV(dst);
        });
        vm->registerNative("java/util/Arrays", "fill", "(" + arr + t + ")V", [](CldcVirtualMachine* vm, const Args& a) {
            for (auto& e : arrayArg(vm, a, 0)->elements) e = arg(a, 1);
            return JavaValue();
        });
    }
    vm->registerNative("java/util/Arrays", "fill", "([Ljava/lang/Object;Ljava/lang/Object;)V", [](CldcVirtualMachine* vm, const Args& a) {
        for (auto& e : arrayArg(vm, a, 0)->elements) e = arg(a, 1);
        return JavaValue();
    });
}

} // namespace universal_loader::jvm
