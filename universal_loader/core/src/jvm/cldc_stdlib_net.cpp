#include "cldc_stdlib_internal.h"

namespace universal_loader::jvm {

struct NioBufferPayload : NativePayload {
    std::vector<uint8_t> bytes;
    size_t pos{0};         // byte position
    bool littleEndian{false};
    int elemSize{1};       // 1 = ByteBuffer, 2 = Short/Char, 4 = Int/Float, 8 = Long/Double
};

void registerNio(CldcVirtualMachine* vm) {
    auto byteOrder = [](bool little) {
        auto cache = std::make_shared<std::atomic<JavaObject*>>(nullptr);
        return [cache, little](CldcVirtualMachine* vm, const Args&) {
            JavaObject* o = cache->load();
            if (!o) {
                o = newNativeObject(vm, "java/nio/ByteOrder");
                ensurePayload<BoxPayload>(o).v = JavaValue(little ? 1 : 0);
                vm->pin(o);
                cache->store(o);
            }
            return refV(o);
        };
    };
    vm->registerNativeStatic("java/nio/ByteOrder", "LITTLE_ENDIAN", byteOrder(true));
    vm->registerNativeStatic("java/nio/ByteOrder", "BIG_ENDIAN", byteOrder(false));

    const char* BB = "java/nio/ByteBuffer";
    vm->registerNative(BB, "wrap", "([B)Ljava/nio/ByteBuffer;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaArray* arr = arrayArg(vm, a, 0);
        JavaObject* b = newNativeObject(vm, "java/nio/ByteBuffer");
        ensurePayload<NioBufferPayload>(b).bytes = bytesOf(arr, 0, arr->length);
        return refV(b);
    });
    vm->registerNative(BB, "order", "(Ljava/nio/ByteOrder;)Ljava/nio/ByteBuffer;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* order = getPayload<BoxPayload>(arg(a, 1).ref);
        ensurePayload<NioBufferPayload>(self(vm, a)).littleEndian = order && order->v.i != 0;
        return a[0];
    });
    const std::pair<const char*, int> views[] = {
        {"asShortBuffer", 2}, {"asCharBuffer", 2}, {"asIntBuffer", 4}, {"asFloatBuffer", 4}, {"asLongBuffer", 8}, {"asDoubleBuffer", 8},
    };
    const char* viewClass[] = {"java/nio/ShortBuffer", "java/nio/CharBuffer", "java/nio/IntBuffer", "java/nio/FloatBuffer", "java/nio/LongBuffer", "java/nio/DoubleBuffer"};
    for (int k = 0; k < 6; ++k) {
        const std::string cls = viewClass[k];
        const int size = views[k].second;
        vm->registerNative(BB, views[k].first, "()L" + cls + ";", [cls, size](CldcVirtualMachine* vm, const Args& a) {
            auto& src = ensurePayload<NioBufferPayload>(self(vm, a));
            JavaObject* view = newNativeObject(vm, cls);
            auto& p = ensurePayload<NioBufferPayload>(view);
            p.bytes.assign(src.bytes.begin() + static_cast<std::ptrdiff_t>(std::min(src.pos, src.bytes.size())), src.bytes.end());
            p.littleEndian = src.littleEndian;
            p.elemSize = size;
            return refV(view);
        });
        // get(dst[]) fills the whole destination array from the current position
        const char elem = std::string("SCIFJD")[k];
        vm->registerNative(cls, "get", std::string("([") + elem + ")L" + cls + ";", [elem](CldcVirtualMachine* vm, const Args& a) {
            auto& p = ensurePayload<NioBufferPayload>(self(vm, a));
            JavaArray* dst = arrayArg(vm, a, 1);
            if (p.pos + static_cast<size_t>(dst->length) * p.elemSize > p.bytes.size()) vm->throwJava("java/nio/BufferUnderflowException");
            for (int32_t i = 0; i < dst->length; ++i) {
                uint64_t v = 0;
                for (int b = 0; b < p.elemSize; ++b) {
                    int idx = p.littleEndian ? (p.elemSize - 1 - b) : b;
                    v = (v << 8) | p.bytes[p.pos + idx];
                }
                p.pos += static_cast<size_t>(p.elemSize);
                switch (elem) {
                    case 'S': dst->elements[i] = JavaValue(static_cast<int32_t>(static_cast<int16_t>(v))); break;
                    case 'C': dst->elements[i] = JavaValue(static_cast<int32_t>(static_cast<uint16_t>(v))); break;
                    case 'I': dst->elements[i] = JavaValue(static_cast<int32_t>(static_cast<uint32_t>(v))); break;
                    case 'J': dst->elements[i] = JavaValue(static_cast<int64_t>(v)); break;
                    case 'F': {
                        uint32_t bits = static_cast<uint32_t>(v);
                        float f;
                        std::memcpy(&f, &bits, 4);
                        dst->elements[i] = JavaValue(f);
                        break;
                    }
                    default: {
                        double d;
                        std::memcpy(&d, &v, 8);
                        dst->elements[i] = JavaValue(d);
                        break;
                    }
                }
            }
            return a[0];
        });
    }
}



// ---------------------------------------------------------------------------
// java.net (Socket, InetSocketAddress, InetAddress)
// ---------------------------------------------------------------------------

// Blocking receive of at least one byte. Returns bytes read, or -1 at end of stream.
// The GIL is released while waiting so other Java threads keep running.
// J2ME_NETLOG=1 logs socket traffic sizes
bool netLog() {
    static const bool on = std::getenv("J2ME_NETLOG") != nullptr;
    return on;
}

int32_t socketRecv(CldcVirtualMachine* vm, SocketPayload& sp, uint8_t* buf, size_t len) {
    if (sp.closed) vm->throwJava("java/net/SocketException", "Socket is closed");
    if (len == 0) return 0;
    if (sp.buffered() > 0) {
        const size_t n = std::min(len, sp.buffered());
        std::memcpy(buf, sp.rbuf.data() + sp.rpos, n);
        sp.rpos += n;
        return static_cast<int32_t>(n);
    }
    // Small reads refill the buffer with whatever has arrived; large reads go straight to the caller
    uint8_t chunk[8192];
    const bool refill = len < sizeof(chunk);
    uint8_t* dst = refill ? chunk : buf;
    const size_t cap = refill ? sizeof(chunk) : len;
    int r = 0;
    bool timedOut = false;
    {
        CldcVirtualMachine::BlockingRegion region(vm);
        auto start = std::chrono::steady_clock::now();
        for (;;) {
            r = sp.sock->recv(dst, cap, 50);
            if (r != 0 || sp.closed || !engineRunning(vm)) break;
            if (sp.soTimeout > 0 && std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(sp.soTimeout)) {
                timedOut = true;
                break;
            }
        }
    }
    if (netLog() && (r != 1 || len > 1)) std::cerr << "[Net] recv " << sp.host << ":" << sp.port << " want=" << len << " got=" << r << (timedOut ? " timeout" : "") << (sp.closed ? " closed" : "") << std::endl;
    if (timedOut) vm->throwJava("java/net/SocketTimeoutException", "Read timed out");
    if (sp.closed || (r == 0 && !engineRunning(vm))) vm->throwJava("java/net/SocketException", "Socket closed");
    if (r > 0 && refill) {
        const size_t n = std::min(len, static_cast<size_t>(r));
        std::memcpy(buf, chunk, n);
        sp.rbuf.assign(chunk + n, chunk + r);
        sp.rpos = 0;
        r = static_cast<int>(n);
    }
    return r < 0 ? -1 : r;
}

std::shared_ptr<SocketPayload> socketOf(CldcVirtualMachine* vm, JavaObject* o) {
    if (!o) vm->throwJava("java/lang/NullPointerException");
    if (auto sp = std::dynamic_pointer_cast<SocketPayload>(o->payload)) return sp;
    auto sp = std::make_shared<SocketPayload>();
    o->payload = sp;
    return sp;
}

void socketConnect(CldcVirtualMachine* vm, JavaObject* sockObj, const std::string& host, int32_t port, int32_t timeoutMs) {
    auto sp = socketOf(vm, sockObj);
    if (sp->closed) vm->throwJava("java/net/SocketException", "Socket is closed");
    if (sp->sock->isConnected()) vm->throwJava("java/net/SocketException", "already connected");
    if (port < 0 || port > 0xFFFF) vm->throwJava("java/lang/IllegalArgumentException", "port out of range:" + std::to_string(port));
    bool ok;
    {
        CldcVirtualMachine::BlockingRegion region(vm);
        ok = sp->sock->connect(host, port, timeoutMs > 0 ? timeoutMs : 15000);
    }
    if (!ok) vm->throwJava("java/net/ConnectException", "Connection refused: " + host + ":" + std::to_string(port));
    sp->host = host;
    sp->port = port;
}

JavaObject* newInetAddress(CldcVirtualMachine* vm, const std::string& host) {
    JavaObject* o = newNativeObject(vm, "java/net/InetAddress");
    ensurePayload<SocketAddressPayload>(o).host = host;
    return o;
}

void registerNet(CldcVirtualMachine* vm) {
    const char* ISA = "java/net/InetSocketAddress";
    vm->registerNative(ISA, "<init>", "(Ljava/lang/String;I)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<SocketAddressPayload>(self(vm, a));
        JavaString* h = asString(arg(a, 1).ref);
        if (!h) vm->throwJava("java/lang/IllegalArgumentException", "hostname can't be null");
        p.host = h->value;
        p.port = arg(a, 2).i;
        return JavaValue();
    });
    vm->registerNative(ISA, "<init>", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<SocketAddressPayload>(self(vm, a));
        p.host = "0.0.0.0";
        p.port = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(ISA, "getPort", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<SocketAddressPayload>(self(vm, a));
        return JavaValue(p ? p->port : 0);
    });
    auto hostName = [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<SocketAddressPayload>(self(vm, a));
        return newStrUtf8(vm, p ? p->host : "");
    };
    vm->registerNative(ISA, "getHostName", "()Ljava/lang/String;", hostName);
    vm->registerNative(ISA, "getHostString", "()Ljava/lang/String;", hostName);
    vm->registerNative(ISA, "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<SocketAddressPayload>(self(vm, a));
        return newStrUtf8(vm, p ? p->host + ":" + std::to_string(p->port) : "");
    });

    const char* IA = "java/net/InetAddress";
    vm->registerNative(IA, "getHostName", "()Ljava/lang/String;", hostName);
    vm->registerNative(IA, "getHostAddress", "()Ljava/lang/String;", hostName);
    vm->registerNative(IA, "toString", "()Ljava/lang/String;", hostName);
    vm->registerNative(IA, "getByName", "(Ljava/lang/String;)Ljava/net/InetAddress;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* h = asString(arg(a, 0).ref);
        return refV(newInetAddress(vm, h ? h->value : "127.0.0.1"));
    });

    const char* S = "java/net/Socket";
    vm->registerNative(S, "<init>", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        socketOf(vm, self(vm, a));
        return JavaValue();
    });
    vm->registerNative(S, "<init>", "(Ljava/lang/String;I)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* h = asString(arg(a, 1).ref);
        socketConnect(vm, self(vm, a), h ? h->value : "127.0.0.1", arg(a, 2).i, 0);
        return JavaValue();
    });
    auto connect = [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* addr = arg(a, 1).ref;
        if (!addr) vm->throwJava("java/lang/IllegalArgumentException", "connect: The address can't be null");
        auto* p = getPayload<SocketAddressPayload>(addr);
        if (!p) vm->throwJava("java/lang/IllegalArgumentException", "Unsupported address type");
        socketConnect(vm, self(vm, a), p->host, p->port, a.size() > 2 ? arg(a, 2).i : 0);
        return JavaValue();
    };
    vm->registerNative(S, "connect", "(Ljava/net/SocketAddress;)V", connect);
    vm->registerNative(S, "connect", "(Ljava/net/SocketAddress;I)V", connect);
    vm->registerNative(S, "getInputStream", "()Ljava/io/InputStream;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        auto sp = socketOf(vm, o);
        if (sp->closed) vm->throwJava("java/net/SocketException", "Socket is closed");
        if (!sp->sock->isConnected()) vm->throwJava("java/net/SocketException", "Socket is not connected");
        if (!sp->in) {
            sp->in = newNativeObject(vm, "java/net/SocketInputStream");
            ensurePayload<SocketStreamPayload>(sp->in).socket = sp;
        }
        return refV(sp->in);
    });
    vm->registerNative(S, "getOutputStream", "()Ljava/io/OutputStream;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* o = self(vm, a);
        auto sp = socketOf(vm, o);
        if (sp->closed) vm->throwJava("java/net/SocketException", "Socket is closed");
        if (!sp->sock->isConnected()) vm->throwJava("java/net/SocketException", "Socket is not connected");
        if (!sp->out) {
            sp->out = newNativeObject(vm, "java/net/SocketOutputStream");
            ensurePayload<SocketStreamPayload>(sp->out).socket = sp;
        }
        return refV(sp->out);
    });
    vm->registerNative(S, "close", "()V", [](CldcVirtualMachine* vm, const Args& a) {
        auto sp = socketOf(vm, self(vm, a));
        if (netLog()) std::cerr << "[Net] socket close " << sp->host << ":" << sp->port << std::endl;
        sp->closed = true;
        sp->sock->close();
        return JavaValue();
    });
    vm->registerNative(S, "isConnected", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto sp = socketOf(vm, self(vm, a));
        return boolV(!sp->host.empty());
    });
    vm->registerNative(S, "isClosed", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        return boolV(socketOf(vm, self(vm, a))->closed);
    });
    vm->registerNative(S, "setSoTimeout", "(I)V", [](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0) vm->throwJava("java/lang/IllegalArgumentException", "timeout can't be negative");
        socketOf(vm, self(vm, a))->soTimeout = arg(a, 1).i;
        return JavaValue();
    });
    vm->registerNative(S, "getSoTimeout", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(socketOf(vm, self(vm, a))->soTimeout);
    });
    auto noopBool = [](CldcVirtualMachine*, const Args&) { return JavaValue(); };
    vm->registerNative(S, "setTcpNoDelay", "(Z)V", noopBool);
    vm->registerNative(S, "setKeepAlive", "(Z)V", noopBool);
    vm->registerNative(S, "setReceiveBufferSize", "(I)V", noopBool);
    vm->registerNative(S, "setSendBufferSize", "(I)V", noopBool);
    vm->registerNative(S, "getInetAddress", "()Ljava/net/InetAddress;", [](CldcVirtualMachine* vm, const Args& a) {
        auto sp = socketOf(vm, self(vm, a));
        return sp->host.empty() ? nullV() : refV(newInetAddress(vm, sp->host));
    });
    vm->registerNative(S, "getPort", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(socketOf(vm, self(vm, a))->port);
    });

    // Closing either stream closes the socket, as on Java SE
    auto closeStream = [](CldcVirtualMachine* vm, const Args& a) {
        if (auto* ss = getPayload<SocketStreamPayload>(self(vm, a))) {
            if (ss->socket->gcfOpen >= 0) {
                if (ss->closed) return JavaValue();
                ss->closed = true;
                if (--ss->socket->gcfOpen > 0) return JavaValue();
            }
            if (netLog()) std::cerr << "[Net] stream close " << ss->socket->host << ":" << ss->socket->port << std::endl;
            ss->socket->closed = true;
            ss->socket->sock->close();
        }
        return JavaValue();
    };
    vm->registerNative("java/net/SocketInputStream", "close", "()V", closeStream);
    vm->registerNative("java/net/SocketOutputStream", "close", "()V", closeStream);
    vm->registerNative("java/net/SocketOutputStream", "flush", "()V", noopBool);
}


// ---------------------------------------------------------------------------
// java.net.URL / HttpURLConnection (plain HTTP/1.0), java.util.Scanner, java.nio.charset
// ---------------------------------------------------------------------------

struct UrlPayload : NativePayload {
    std::string url;
};

struct HttpPayload : NativePayload {
    std::string url;
    std::string method{"GET"};
    std::vector<std::pair<std::string, std::string>> headers;
    JavaObject* body{nullptr}; // ByteArrayOutputStream handed out by getOutputStream
    void trace(std::vector<JavaObject*>& o) const override { if (body) o.push_back(body); }
    bool executed{false};
    bool failed{false};
    std::string error;
    int32_t code{-1};
    std::string message;
    std::map<std::string, std::string> respHeaders;
    std::vector<std::pair<std::string, std::string>> respHeaderList; // original order and case
    std::vector<uint8_t> respBody;
};

struct ScannerPayload : NativePayload {
    std::string text; // UTF-8 contents of the source
    size_t pos{0};
    std::string delim{"\\s+"};
};

JavaObject* newCharset(CldcVirtualMachine* vm, const std::string& name) {
    JavaObject* o = newNativeObject(vm, "java/nio/charset/Charset");
    ensurePayload<CharsetPayload>(o).name = name;
    return o;
}

#ifdef _WIN32
// HTTPS through WinHTTP (system TLS). Produces "status line + headers \r\n\r\n body" like a socket read.
bool winHttpFetch(const HttpPayload& h, const std::vector<uint8_t>& body, std::string& resp, std::string& err) {
    std::wstring wurl(h.url.begin(), h.url.end());
    wchar_t host[256] = {0}, path[4096] = {0}, extra[4096] = {0};
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 4096;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 4096;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { err = "Malformed URL: " + h.url; return false; }
    HINTERNET ses = WinHttpOpen(L"J2ME-Loader", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { err = "WinHttpOpen failed"; return false; }
    WinHttpSetTimeouts(ses, 10000, 10000, 15000, 15000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    std::wstring method(h.method.begin(), h.method.end());
    std::wstring object = std::wstring(path) + extra;
    HINTERNET req = con ? WinHttpOpenRequest(con, method.c_str(), object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                        : nullptr;
    bool ok = false;
    if (req) {
        std::wstring hdrs;
        for (auto& kv : h.headers) {
            std::string line = kv.first + ": " + kv.second + "\r\n";
            hdrs += std::wstring(line.begin(), line.end());
        }
        ok = WinHttpSendRequest(req, hdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrs.c_str(), static_cast<DWORD>(-1L),
                                body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<uint8_t*>(body.data()),
                                static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0)
             && WinHttpReceiveResponse(req, nullptr);
        if (ok) {
            DWORD len = 0;
            WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &len, WINHTTP_NO_HEADER_INDEX);
            std::wstring raw(len / sizeof(wchar_t), L'\0');
            if (len && WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &len, WINHTTP_NO_HEADER_INDEX)) {
                raw.resize(len / sizeof(wchar_t));
                for (wchar_t c : raw) resp += static_cast<char>(c);
            }
            // WinHTTP already de-chunked the body; drop the header so it isn't decoded twice
            size_t te = resp.find("Transfer-Encoding:");
            if (te != std::string::npos) resp.erase(te, resp.find("\r\n", te) + 2 - te);
            while (resp.size() >= 2 && resp.compare(resp.size() - 2, 2, "\r\n") == 0) resp.resize(resp.size() - 2);
            resp += "\r\n\r\n";
            char buf[8192];
            DWORD got = 0;
            while (WinHttpReadData(req, buf, sizeof(buf), &got) && got > 0) resp.append(buf, got);
        } else {
            err = "HTTPS request failed (" + std::to_string(GetLastError()) + "): " + h.url;
        }
        WinHttpCloseHandle(req);
    } else {
        err = "Cannot connect: " + h.url;
    }
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}
#endif

// Performs the request once, off the GIL. http:// over sockets; https:// via WinHTTP on Windows, the host bridge elsewhere.
void httpExecute(CldcVirtualMachine* vm, HttpPayload& h) {
    if (h.executed) {
        if (h.failed) vm->throwJava("java/io/IOException", h.error);
        return;
    }
    h.executed = true;
    std::string rest = h.url;
    std::string resp;
    bool ok = false;
    if (rest.rfind("http://", 0) == 0) {
        rest = rest.substr(7);
    } else if (rest.rfind("https://", 0) == 0) {
        std::vector<uint8_t> body;
        if (h.body) {
            if (auto* bo = getPayload<ByteOutputPayload>(h.body)) body = bo->data;
        }
        std::string err;
        {
            CldcVirtualMachine::BlockingRegion region(vm);
#ifdef _WIN32
            ok = winHttpFetch(h, body, resp, err);
#else
            ok = j2me::http_bridge::fetch({h.method, h.url, h.headers, body}, resp, err, [vm] { return !engineRunning(vm); });
#endif
        }
        if (!ok) {
            h.failed = true;
            h.error = err;
            vm->throwJava("java/io/IOException", h.error);
        }
        rest.clear();
    } else {
        h.failed = true;
        h.error = "Unsupported protocol: " + h.url;
        vm->throwJava("java/io/IOException", h.error);
    }
    std::string hostPort = rest;
    if (!rest.empty()) {
    size_t slash = rest.find('/');
    hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    std::string host = hostPort;
    int port = 80;
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        host = hostPort.substr(0, colon);
        port = std::atoi(hostPort.c_str() + colon + 1);
    }
    std::vector<uint8_t> body;
    if (h.body) {
        if (auto* bo = getPayload<ByteOutputPayload>(h.body)) body = bo->data;
    }
    std::string req = h.method + " " + path + " HTTP/1.0\r\nHost: " + hostPort + "\r\n";
    bool hasLen = false;
    for (auto& kv : h.headers) {
        req += kv.first + ": " + kv.second + "\r\n";
        std::string k = kv.first;
        for (auto& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (k == "content-length") hasLen = true;
    }
    if (!body.empty() && !hasLen) req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";

    {
        CldcVirtualMachine::BlockingRegion region(vm);
        j2me::NetworkSocket sock;
        ok = sock.connect(host, port, 10000);
        if (ok) {
            std::vector<uint8_t> all(req.begin(), req.end());
            all.insert(all.end(), body.begin(), body.end());
            size_t sent = 0;
            while (sent < all.size()) {
                int r = sock.send(all.data() + sent, all.size() - sent);
                if (r <= 0) break;
                sent += static_cast<size_t>(r);
            }
            uint8_t buf[4096];
            auto start = std::chrono::steady_clock::now();
            for (;;) {
                int r = sock.recv(buf, sizeof(buf), 100);
                if (r < 0) break;
                if (r > 0) {
                    resp.append(reinterpret_cast<char*>(buf), r);
                    start = std::chrono::steady_clock::now();
                } else if (!engineRunning(vm) || std::chrono::steady_clock::now() - start > std::chrono::seconds(15)) {
                    break;
                }
            }
            sock.close();
        }
    }
    }
    size_t hdrEnd = resp.find("\r\n\r\n");
    if (!ok || hdrEnd == std::string::npos) {
        h.failed = true;
        h.error = ok ? "Invalid HTTP response" : "Connection refused: " + hostPort;
        vm->throwJava(ok ? "java/io/IOException" : "java/net/ConnectException", h.error);
    }
    std::istringstream hs(resp.substr(0, hdrEnd));
    std::string line;
    std::getline(hs, line);
    {
        std::istringstream st(line);
        std::string ver;
        st >> ver >> h.code;
        std::getline(st, h.message);
        while (!h.message.empty() && (h.message.back() == '\r' || h.message.front() == ' ')) {
            if (h.message.back() == '\r') h.message.pop_back(); else h.message.erase(0, 1);
        }
    }
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::string k = line.substr(0, c), v = line.substr(c + 1);
        while (!v.empty() && v.front() == ' ') v.erase(0, 1);
        h.respHeaderList.emplace_back(k, v);
        for (auto& ch : k) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        h.respHeaders[k] = v;
    }
    h.respBody.assign(resp.begin() + hdrEnd + 4, resp.end());
}

JavaObject* newByteInput(CldcVirtualMachine* vm, const std::vector<uint8_t>& data) {
    JavaObject* o = newNativeObject(vm, "java/io/ByteArrayInputStream");
    ensurePayload<ByteInputPayload>(o).data = data;
    return o;
}

void registerHttpScanner(CldcVirtualMachine* vm) {
    vm->registerNativeStatic("java/nio/charset/StandardCharsets", "UTF_8", [](CldcVirtualMachine* vm, const Args&) {
        return refV(vm->nativeSingleton("java/nio/charset/StandardCharsets.UTF_8", [vm] {
            return newCharset(vm, "UTF-8");
        }));
    });
    vm->registerNativeStatic("java/nio/charset/StandardCharsets", "ISO_8859_1", [](CldcVirtualMachine* vm, const Args&) {
        return refV(vm->nativeSingleton("java/nio/charset/StandardCharsets.ISO_8859_1", [vm] {
            return newCharset(vm, "ISO-8859-1");
        }));
    });
    vm->registerNative("java/nio/charset/Charset", "forName", "(Ljava/lang/String;)Ljava/nio/charset/Charset;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* n = asString(arg(a, 0).ref);
        if (!n) vm->throwJava("java/lang/IllegalArgumentException", "Null charset name");
        return refV(newCharset(vm, n->value));
    });
    auto csName = [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<CharsetPayload>(self(vm, a));
        return newStrUtf8(vm, p ? p->name : "UTF-8");
    };
    vm->registerNative("java/nio/charset/Charset", "name", "()Ljava/lang/String;", csName);
    vm->registerNative("java/nio/charset/Charset", "toString", "()Ljava/lang/String;", csName);
    vm->registerNative("java/nio/charset/Charset", "displayName", "()Ljava/lang/String;", csName);

    const char* U = "java/net/URL";
    vm->registerNative(U, "<init>", "(Ljava/lang/String;)V", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* u = asString(arg(a, 1).ref);
        if (!u) vm->throwJava("java/net/MalformedURLException", "null");
        if (u->value.find("://") == std::string::npos) vm->throwJava("java/net/MalformedURLException", "no protocol: " + u->value);
        ensurePayload<UrlPayload>(self(vm, a)).url = u->value;
        return JavaValue();
    });
    vm->registerNative(U, "toString", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<UrlPayload>(self(vm, a));
        return newStrUtf8(vm, p ? p->url : "");
    });
    vm->registerNative(U, "openConnection", "()Ljava/net/URLConnection;", [](CldcVirtualMachine* vm, const Args& a) {
        auto* p = getPayload<UrlPayload>(self(vm, a));
        JavaObject* c = newNativeObject(vm, "java/net/HttpURLConnection");
        ensurePayload<HttpPayload>(c).url = p ? p->url : "";
        return refV(c);
    });

    auto hp = [](CldcVirtualMachine* vm, const Args& a) -> HttpPayload& { return ensurePayload<HttpPayload>(self(vm, a)); };
    for (const char* H : {"java/net/HttpURLConnection", "java/net/URLConnection"}) {
        vm->registerNative(H, "setRequestMethod", "(Ljava/lang/String;)V", [hp](CldcVirtualMachine* vm, const Args& a) {
            JavaString* m = asString(arg(a, 1).ref);
            if (m) hp(vm, a).method = m->value;
            return JavaValue();
        });
        vm->registerNative(H, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V", [hp](CldcVirtualMachine* vm, const Args& a) {
            JavaString* k = asString(arg(a, 1).ref);
            JavaString* v = asString(arg(a, 2).ref);
            if (k) hp(vm, a).headers.emplace_back(k->value, v ? v->value : "");
            return JavaValue();
        });
        auto noop = [](CldcVirtualMachine*, const Args&) { return JavaValue(); };
        vm->registerNative(H, "setDoOutput", "(Z)V", noop);
        vm->registerNative(H, "setDoInput", "(Z)V", noop);
        vm->registerNative(H, "setUseCaches", "(Z)V", noop);
        vm->registerNative(H, "setConnectTimeout", "(I)V", noop);
        vm->registerNative(H, "setReadTimeout", "(I)V", noop);
        vm->registerNative(H, "setInstanceFollowRedirects", "(Z)V", noop);
        vm->registerNative(H, "connect", "()V", noop);
        vm->registerNative(H, "disconnect", "()V", noop);
        vm->registerNative(H, "getOutputStream", "()Ljava/io/OutputStream;", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            if (h.executed) vm->throwJava("java/net/ProtocolException", "Cannot write output after reading input.");
            if (h.method == "GET") h.method = "POST";
            if (!h.body) {
                h.body = newNativeObject(vm, "java/io/ByteArrayOutputStream");
                ensurePayload<ByteOutputPayload>(h.body);
            }
            return refV(h.body);
        });
        vm->registerNative(H, "getResponseCode", "()I", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            httpExecute(vm, h);
            return JavaValue(h.code);
        });
        vm->registerNative(H, "getResponseMessage", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            httpExecute(vm, h);
            return newStrUtf8(vm, h.message);
        });
        vm->registerNative(H, "getInputStream", "()Ljava/io/InputStream;", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            httpExecute(vm, h);
            if (h.code >= 400) vm->throwJava(h.code == 404 ? "java/io/FileNotFoundException" : "java/io/IOException",
                                             "Server returned HTTP response code: " + std::to_string(h.code) + " for URL: " + h.url);
            return refV(newByteInput(vm, h.respBody));
        });
        vm->registerNative(H, "getErrorStream", "()Ljava/io/InputStream;", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            if (!h.executed || h.failed || h.code < 400) return nullV();
            return refV(newByteInput(vm, h.respBody));
        });
        vm->registerNative(H, "getContentLength", "()I", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            httpExecute(vm, h);
            return JavaValue(static_cast<int32_t>(h.respBody.size()));
        });
        vm->registerNative(H, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
            auto& h = hp(vm, a);
            httpExecute(vm, h);
            JavaString* k = asString(arg(a, 1).ref);
            if (!k) return nullV();
            std::string key = k->value;
            for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            auto it = h.respHeaders.find(key);
            return it == h.respHeaders.end() ? nullV() : newStrUtf8(vm, it->second);
        });
    }

    const char* SC = "java/util/Scanner";
    auto scInit = [](CldcVirtualMachine* vm, const Args& a) {
        JavaObject* in = arg(a, 1).ref;
        if (!in) vm->throwJava("java/lang/NullPointerException");
        auto& p = ensurePayload<ScannerPayload>(self(vm, a));
        if (JavaString* str = asString(in)) {
            p.text = str->value;
            return JavaValue();
        }
        std::vector<uint8_t> zeros(4096);
        JavaArray* buf = newByteArray(vm, zeros.data(), zeros.size());
        for (;;) {
            int32_t n = streamReadBlock(vm, in, buf, 0, 4096);
            if (n < 0) break;
            for (int32_t i = 0; i < n; ++i) p.text.push_back(static_cast<char>(buf->elements[i].i));
        }
        return JavaValue();
    };
    vm->registerNative(SC, "<init>", "(Ljava/io/InputStream;)V", scInit);
    vm->registerNative(SC, "<init>", "(Ljava/io/InputStream;Ljava/lang/String;)V", scInit);
    vm->registerNative(SC, "<init>", "(Ljava/lang/String;)V", scInit);
    vm->registerNative(SC, "useDelimiter", "(Ljava/lang/String;)Ljava/util/Scanner;", [](CldcVirtualMachine* vm, const Args& a) {
        JavaString* d = asString(arg(a, 1).ref);
        ensurePayload<ScannerPayload>(self(vm, a)).delim = d ? d->value : "\\s+";
        return refV(self(vm, a));
    });
    // Finds the next token as [begin, end); returns false when none is left.
    static auto nextToken = [](ScannerPayload& p, size_t& b, size_t& e) -> bool {
        if (p.delim == "\\A" || p.delim == "\\Z" || p.delim == "\\z") {
            if (p.pos >= p.text.size()) return false;
            b = p.pos;
            e = p.text.size();
            return true;
        }
        std::regex re;
        try {
            re = std::regex(p.delim);
        } catch (const std::regex_error&) {
            re = std::regex("\\s+");
        }
        size_t cur = p.pos;
        std::smatch m;
        // Skip leading delimiters
        while (cur < p.text.size()) {
            std::string tail = p.text.substr(cur);
            if (std::regex_search(tail, m, re, std::regex_constants::match_continuous) && m.length(0) > 0) {
                cur += static_cast<size_t>(m.length(0));
            } else {
                break;
            }
        }
        if (cur >= p.text.size()) return false;
        std::string tail = p.text.substr(cur);
        size_t len = tail.size();
        auto it = tail.cbegin();
        while (std::regex_search(it, tail.cend(), m, re)) {
            if (m.length(0) > 0) {
                len = static_cast<size_t>(m.position(0) + (it - tail.cbegin()));
                break;
            }
            if (it == tail.cend()) break;
            ++it;
        }
        b = cur;
        e = cur + len;
        return true;
    };
    vm->registerNative(SC, "hasNext", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        size_t b, e;
        return boolV(nextToken(ensurePayload<ScannerPayload>(self(vm, a)), b, e));
    });
    vm->registerNative(SC, "next", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ScannerPayload>(self(vm, a));
        size_t b, e;
        if (!nextToken(p, b, e)) vm->throwJava("java/util/NoSuchElementException");
        p.pos = e;
        return newStrUtf8(vm, p.text.substr(b, e - b));
    });
    vm->registerNative(SC, "hasNextLine", "()Z", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ScannerPayload>(self(vm, a));
        return boolV(p.pos < p.text.size());
    });
    vm->registerNative(SC, "nextLine", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        auto& p = ensurePayload<ScannerPayload>(self(vm, a));
        if (p.pos >= p.text.size()) vm->throwJava("java/util/NoSuchElementException", "No line found");
        size_t nl = p.text.find('\n', p.pos);
        std::string line = p.text.substr(p.pos, nl == std::string::npos ? std::string::npos : nl - p.pos);
        p.pos = nl == std::string::npos ? p.text.size() : nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return newStrUtf8(vm, line);
    });
    vm->registerNative(SC, "close", "()V", [](CldcVirtualMachine*, const Args&) { return JavaValue(); });
}

// ============================================================================
// javax.microedition.lcdui high-level UI: Command, Displayable/Screen, Form, TextBox,
// List, Alert, AlertType and the Form items
// ============================================================================

// ---------------------------------------------------------------------------
// javax.microedition.io.Connector (Generic Connection Framework): http, https, socket
// ---------------------------------------------------------------------------

struct UrlParts {
    std::string protocol, host, file, query, ref;
    int32_t port{-1};
};

// Drops BlackBerry transport suffixes such as ";deviceside=true" / ";interface=wifi"
std::string stripConnectionParams(const std::string& url) {
    static const char* keys[] = {"deviceside=", "interface=", "connectiontimeout=", "connectionuid=", "retrynocontext=", "connectionhandler=", "apn=", "tunnelauthusername=", "tunnelauthpassword=", "endtoendrequired", "endtoenddesired"};
    std::string lower = lowerAscii(url);
    size_t cut = std::string::npos;
    for (const char* k : keys) {
        size_t p = lower.find(std::string(";") + k);
        if (p != std::string::npos) cut = std::min(cut, p);
    }
    return cut == std::string::npos ? url : url.substr(0, cut);
}

UrlParts parseUrl(const std::string& url) {
    UrlParts u;
    size_t p = url.find("://");
    if (p == std::string::npos) return u;
    u.protocol = lowerAscii(url.substr(0, p));
    std::string rest = url.substr(p + 3);
    size_t hash = rest.find('#');
    if (hash != std::string::npos) { u.ref = rest.substr(hash + 1); rest.resize(hash); }
    size_t q = rest.find('?');
    if (q != std::string::npos) { u.query = rest.substr(q + 1); rest.resize(q); }
    size_t slash = rest.find('/');
    std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
    if (slash != std::string::npos) u.file = rest.substr(slash);
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && hostPort.find(']', colon) == std::string::npos) {
        u.host = hostPort.substr(0, colon);
        std::string ps = hostPort.substr(colon + 1);
        if (!ps.empty()) u.port = std::atoi(ps.c_str());
    } else {
        u.host = hostPort;
    }
    return u;
}

// RFC 1123 ("Sun, 06 Nov 1994 08:49:37 GMT") and RFC 850 ("Sunday, 06-Nov-94 08:49:37 GMT") to epoch ms; 0 if unparsable
int64_t parseHttpDate(const std::string& s) {
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    size_t comma = s.find(',');
    std::string t = comma == std::string::npos ? s : s.substr(comma + 1);
    int d = 0, y = 0, hh = 0, mm = 0, ss = 0;
    char mon[4] = {0};
    if (std::sscanf(t.c_str(), " %d %3s %d %d:%d:%d", &d, mon, &y, &hh, &mm, &ss) != 6) {
        if (std::sscanf(t.c_str(), " %d-%3s-%d %d:%d:%d", &d, mon, &y, &hh, &mm, &ss) != 6) return 0;
        if (y < 100) y += y < 70 ? 2000 : 1900;
    }
    const char* m = std::strstr(months, mon);
    if (!m || mon[0] == 0) return 0;
    int mi = static_cast<int>(m - months) / 3 + 1;
    y -= mi <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (mi + (mi > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = era * 146097 + doe - 719468;
    return (((days * 24 + hh) * 60 + mm) * 60 + ss) * 1000LL;
}
JavaObject* wrapDataInput(CldcVirtualMachine* vm, JavaObject* in) {
    JavaObject* o = newNativeObject(vm, "java/io/DataInputStream");
    ensurePayload<FilterInputPayload>(o).in = in;
    return o;
}

JavaObject* wrapDataOutput(CldcVirtualMachine* vm, JavaObject* out) {
    JavaObject* o = newNativeObject(vm, "java/io/DataOutputStream");
    ensurePayload<FilterOutputPayload>(o).out = out;
    return o;
}

bool isHttpConnection(JavaObject* c) { return getPayload<HttpPayload>(c) != nullptr; }

JavaObject* connectorOpen(CldcVirtualMachine* vm, JavaString* urlStr) {
    if (!urlStr) vm->throwJava("java/lang/IllegalArgumentException", "Null URL");
    std::string url = urlStr->value;
    size_t colon = url.find(':');
    if (colon == std::string::npos || colon == 0) vm->throwJava("java/lang/IllegalArgumentException", "Invalid URL: " + url);
    std::string scheme = lowerAscii(url.substr(0, colon));
    if (netLog()) std::cerr << "[Net] Connector.open " << url << std::endl;
    if (scheme == "http" || scheme == "https") {
        JavaObject* c = newNativeObject(vm, scheme == "https" ? "javax/microedition/io/HttpsConnection" : "javax/microedition/io/HttpConnection");
        ensurePayload<HttpPayload>(c).url = scheme + stripConnectionParams(url.substr(colon));
        return c;
    }
    if (scheme == "socket") {
        std::string clean = url;
        size_t semi = clean.find(';');
        if (semi != std::string::npos) clean.resize(semi);
        UrlParts u = parseUrl(clean);
        if (u.host.empty()) vm->throwJava("javax/microedition/io/ConnectionNotFoundException", "Server sockets are not supported: " + url);
        if (u.port < 0) vm->throwJava("java/lang/IllegalArgumentException", "Missing port: " + url);
        JavaObject* c = newNativeObject(vm, "javax/microedition/io/SocketConnection");
        socketConnect(vm, c, u.host, u.port, 0);
        socketOf(vm, c)->gcfOpen = 1;
        return c;
    }
    vm->throwJava("javax/microedition/io/ConnectionNotFoundException", "The requested protocol does not exist " + url);
}

JavaObject* gcfSocketStream(CldcVirtualMachine* vm, JavaObject* conn, bool input) {
    auto sp = socketOf(vm, conn);
    if (sp->gcfConnClosed || sp->closed) vm->throwJava("java/io/IOException", "Connection closed");
    JavaObject*& slot = input ? sp->in : sp->out;
    if (!slot) {
        slot = newNativeObject(vm, input ? "java/net/SocketInputStream" : "java/net/SocketOutputStream");
        ensurePayload<SocketStreamPayload>(slot).socket = sp;
        ++sp->gcfOpen;
    }
    return slot;
}

void gcfSocketClose(CldcVirtualMachine* vm, JavaObject* conn) {
    auto sp = socketOf(vm, conn);
    if (sp->gcfConnClosed) return;
    sp->gcfConnClosed = true;
    if (--sp->gcfOpen > 0) return;
    if (netLog()) std::cerr << "[Net] connection close " << sp->host << ":" << sp->port << std::endl;
    sp->closed = true;
    sp->sock->close();
}

JavaObject* httpBodyStream(CldcVirtualMachine* vm, HttpPayload& h) {
    if (h.executed) vm->throwJava("java/io/IOException", "Request already sent");
    if (!h.body) {
        h.body = newNativeObject(vm, "java/io/ByteArrayOutputStream");
        ensurePayload<ByteOutputPayload>(h.body);
    }
    return h.body;
}

JavaObject* connInputStream(CldcVirtualMachine* vm, JavaObject* conn) {
    if (auto* h = getPayload<HttpPayload>(conn)) {
        httpExecute(vm, *h);
        return newByteInput(vm, h->respBody);
    }
    return gcfSocketStream(vm, conn, true);
}

JavaObject* connOutputStream(CldcVirtualMachine* vm, JavaObject* conn) {
    if (auto* h = getPayload<HttpPayload>(conn)) return httpBodyStream(vm, *h);
    return gcfSocketStream(vm, conn, false);
}

void connClose(CldcVirtualMachine* vm, JavaObject* conn) {
    if (auto* h = getPayload<HttpPayload>(conn)) {
        // A request whose body was written but whose response was never read still has to be sent
        auto* bo = h->body ? getPayload<ByteOutputPayload>(h->body) : nullptr;
        if (!h->executed && bo && !bo->data.empty()) {
            try { httpExecute(vm, *h); } catch (const JavaException&) {}
        }
        return;
    }
    if (getPayload<SocketPayload>(conn)) gcfSocketClose(vm, conn);
}

void registerConnector(CldcVirtualMachine* vm) {
    const char* C = "javax/microedition/io/Connector";
    auto open = [](CldcVirtualMachine* vm, const Args& a) { return refV(connectorOpen(vm, asString(arg(a, 0).ref))); };
    vm->registerNative(C, "open", "(Ljava/lang/String;)Ljavax/microedition/io/Connection;", open);
    vm->registerNative(C, "open", "(Ljava/lang/String;I)Ljavax/microedition/io/Connection;", open);
    vm->registerNative(C, "open", "(Ljava/lang/String;IZ)Ljavax/microedition/io/Connection;", open);
    // Stream-only opens: the connection itself is closed immediately, the stream keeps it alive
    auto openIn = [](CldcVirtualMachine* vm, const Args& a) -> JavaObject* {
        JavaObject* c = connectorOpen(vm, asString(arg(a, 0).ref));
        JavaObject* s = connInputStream(vm, c);
        connClose(vm, c);
        return s;
    };
    auto openOut = [](CldcVirtualMachine* vm, const Args& a) -> JavaObject* {
        JavaObject* c = connectorOpen(vm, asString(arg(a, 0).ref));
        if (isHttpConnection(c)) vm->throwJava("java/io/IOException", "Cannot send an HTTP request through Connector.openOutputStream");
        JavaObject* s = connOutputStream(vm, c);
        connClose(vm, c);
        return s;
    };
    vm->registerNative(C, "openInputStream", "(Ljava/lang/String;)Ljava/io/InputStream;", [openIn](CldcVirtualMachine* vm, const Args& a) { return refV(openIn(vm, a)); });
    vm->registerNative(C, "openDataInputStream", "(Ljava/lang/String;)Ljava/io/DataInputStream;", [openIn](CldcVirtualMachine* vm, const Args& a) { return refV(wrapDataInput(vm, openIn(vm, a))); });
    vm->registerNative(C, "openOutputStream", "(Ljava/lang/String;)Ljava/io/OutputStream;", [openOut](CldcVirtualMachine* vm, const Args& a) { return refV(openOut(vm, a)); });
    vm->registerNative(C, "openDataOutputStream", "(Ljava/lang/String;)Ljava/io/DataOutputStream;", [openOut](CldcVirtualMachine* vm, const Args& a) { return refV(wrapDataOutput(vm, openOut(vm, a))); });

    // Stream methods shared by HTTP and socket connections
    for (const char* K : {"javax/microedition/io/HttpConnection", "javax/microedition/io/SocketConnection"}) {
        vm->registerNative(K, "openInputStream", "()Ljava/io/InputStream;", [](CldcVirtualMachine* vm, const Args& a) {
            return refV(connInputStream(vm, self(vm, a)));
        });
        vm->registerNative(K, "openDataInputStream", "()Ljava/io/DataInputStream;", [](CldcVirtualMachine* vm, const Args& a) {
            return refV(wrapDataInput(vm, connInputStream(vm, self(vm, a))));
        });
        vm->registerNative(K, "openOutputStream", "()Ljava/io/OutputStream;", [](CldcVirtualMachine* vm, const Args& a) {
            return refV(connOutputStream(vm, self(vm, a)));
        });
        vm->registerNative(K, "openDataOutputStream", "()Ljava/io/DataOutputStream;", [](CldcVirtualMachine* vm, const Args& a) {
            return refV(wrapDataOutput(vm, connOutputStream(vm, self(vm, a))));
        });
        vm->registerNative(K, "close", "()V", [](CldcVirtualMachine* vm, const Args& a) {
            connClose(vm, self(vm, a));
            return JavaValue();
        });
    }

    const char* SK = "javax/microedition/io/SocketConnection";
    vm->registerNative(SK, "setSocketOption", "(BI)V", [](CldcVirtualMachine* vm, const Args& a) {
        if (arg(a, 1).i < 0 || arg(a, 1).i > 4 || arg(a, 2).i < 0) vm->throwJava("java/lang/IllegalArgumentException");
        return JavaValue();
    });
    vm->registerNative(SK, "getSocketOption", "(B)I", [](CldcVirtualMachine* vm, const Args& a) {
        switch (arg(a, 1).i) {
            case 0: return JavaValue(0);     // DELAY
            case 1: return JavaValue(-1);    // LINGER
            case 2: return JavaValue(1);     // KEEPALIVE
            case 3: return JavaValue(65536); // RCVBUF
            case 4: return JavaValue(65536); // SNDBUF
        }
        vm->throwJava("java/lang/IllegalArgumentException");
    });
    vm->registerNative(SK, "getAddress", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, socketOf(vm, self(vm, a))->host);
    });
    vm->registerNative(SK, "getPort", "()I", [](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(socketOf(vm, self(vm, a))->port);
    });
    vm->registerNative(SK, "getLocalAddress", "()Ljava/lang/String;", [](CldcVirtualMachine* vm, const Args&) {
        return newStrUtf8(vm, "127.0.0.1");
    });
    vm->registerNative(SK, "getLocalPort", "()I", [](CldcVirtualMachine*, const Args&) { return JavaValue(0); });

    const char* H = "javax/microedition/io/HttpConnection";
    auto hp = [](CldcVirtualMachine* vm, const Args& a) -> HttpPayload& { return ensurePayload<HttpPayload>(self(vm, a)); };
    auto executed = [hp](CldcVirtualMachine* vm, const Args& a) -> HttpPayload& {
        auto& h = hp(vm, a);
        httpExecute(vm, h);
        return h;
    };
    auto header = [](const HttpPayload& h, const std::string& name) -> const std::string* {
        auto it = h.respHeaders.find(lowerAscii(name));
        return it == h.respHeaders.end() ? nullptr : &it->second;
    };
    vm->registerNative(H, "setRequestMethod", "(Ljava/lang/String;)V", [hp](CldcVirtualMachine* vm, const Args& a) {
        auto& h = hp(vm, a);
        if (h.executed) vm->throwJava("java/io/IOException", "Already connected");
        JavaString* m = asString(arg(a, 1).ref);
        if (!m || (m->value != "GET" && m->value != "POST" && m->value != "HEAD"))
            vm->throwJava("java/lang/IllegalArgumentException", "Invalid method");
        h.method = m->value;
        return JavaValue();
    });
    vm->registerNative(H, "getRequestMethod", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, hp(vm, a).method);
    });
    vm->registerNative(H, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V", [hp](CldcVirtualMachine* vm, const Args& a) {
        auto& h = hp(vm, a);
        if (h.executed) vm->throwJava("java/io/IOException", "Already connected");
        JavaString* k = asString(arg(a, 1).ref);
        JavaString* v = asString(arg(a, 2).ref);
        if (!k) return JavaValue();
        std::string key = lowerAscii(k->value);
        for (auto& kv : h.headers) {
            if (lowerAscii(kv.first) == key) {
                kv.second = v ? v->value : "";
                return JavaValue();
            }
        }
        h.headers.emplace_back(k->value, v ? v->value : "");
        return JavaValue();
    });
    vm->registerNative(H, "getRequestProperty", "(Ljava/lang/String;)Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        auto& h = hp(vm, a);
        JavaString* k = asString(arg(a, 1).ref);
        if (!k) return nullV();
        std::string key = lowerAscii(k->value);
        for (auto& kv : h.headers) {
            if (lowerAscii(kv.first) == key) return newStrUtf8(vm, kv.second);
        }
        return nullV();
    });

    vm->registerNative(H, "getURL", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, hp(vm, a).url);
    });
    vm->registerNative(H, "getProtocol", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, parseUrl(hp(vm, a).url).protocol);
    });
    vm->registerNative(H, "getHost", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        return newStrUtf8(vm, parseUrl(hp(vm, a).url).host);
    });
    vm->registerNative(H, "getPort", "()I", [hp](CldcVirtualMachine* vm, const Args& a) {
        UrlParts u = parseUrl(hp(vm, a).url);
        return JavaValue(u.port >= 0 ? u.port : (u.protocol == "https" ? 443 : 80));
    });
    vm->registerNative(H, "getFile", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        UrlParts u = parseUrl(hp(vm, a).url);
        return u.file.empty() ? nullV() : newStrUtf8(vm, u.file);
    });
    vm->registerNative(H, "getQuery", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        UrlParts u = parseUrl(hp(vm, a).url);
        return u.query.empty() ? nullV() : newStrUtf8(vm, u.query);
    });
    vm->registerNative(H, "getRef", "()Ljava/lang/String;", [hp](CldcVirtualMachine* vm, const Args& a) {
        UrlParts u = parseUrl(hp(vm, a).url);
        return u.ref.empty() ? nullV() : newStrUtf8(vm, u.ref);
    });

    vm->registerNative(H, "getResponseCode", "()I", [executed](CldcVirtualMachine* vm, const Args& a) {
        return JavaValue(executed(vm, a).code);
    });
    vm->registerNative(H, "getResponseMessage", "()Ljava/lang/String;", [executed](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        return h.message.empty() ? nullV() : newStrUtf8(vm, h.message);
    });
    vm->registerNative(H, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;", [executed, header](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        JavaString* k = asString(arg(a, 1).ref);
        const std::string* v = k ? header(h, k->value) : nullptr;
        return v ? newStrUtf8(vm, *v) : nullV();
    });
    vm->registerNative(H, "getHeaderField", "(I)Ljava/lang/String;", [executed](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        int32_t n = arg(a, 1).i;
        return n >= 0 && static_cast<size_t>(n) < h.respHeaderList.size() ? newStrUtf8(vm, h.respHeaderList[n].second) : nullV();
    });
    vm->registerNative(H, "getHeaderFieldKey", "(I)Ljava/lang/String;", [executed](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        int32_t n = arg(a, 1).i;
        return n >= 0 && static_cast<size_t>(n) < h.respHeaderList.size() ? newStrUtf8(vm, h.respHeaderList[n].first) : nullV();
    });
    vm->registerNative(H, "getHeaderFieldInt", "(Ljava/lang/String;I)I", [executed, header](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        JavaString* k = asString(arg(a, 1).ref);
        const std::string* v = k ? header(h, k->value) : nullptr;
        if (!v) return JavaValue(arg(a, 2).i);
        char* end = nullptr;
        long n = std::strtol(v->c_str(), &end, 10);
        return end == v->c_str() ? JavaValue(arg(a, 2).i) : JavaValue(static_cast<int32_t>(n));
    });
    vm->registerNative(H, "getHeaderFieldDate", "(Ljava/lang/String;J)J", [executed, header](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        JavaString* k = asString(arg(a, 1).ref);
        const std::string* v = k ? header(h, k->value) : nullptr;
        int64_t t = v ? parseHttpDate(*v) : 0;
        return JavaValue(t ? t : arg(a, 2).l);
    });
    auto dateHeader = [executed, header](const char* name) {
        return [executed, header, name](CldcVirtualMachine* vm, const Args& a) {
            const std::string* v = header(executed(vm, a), name);
            return JavaValue(v ? parseHttpDate(*v) : int64_t(0));
        };
    };
    vm->registerNative(H, "getDate", "()J", dateHeader("date"));
    vm->registerNative(H, "getExpiration", "()J", dateHeader("expires"));
    vm->registerNative(H, "getLastModified", "()J", dateHeader("last-modified"));
    vm->registerNative(H, "getLength", "()J", [executed, header](CldcVirtualMachine* vm, const Args& a) {
        auto& h = executed(vm, a);
        const std::string* v = header(h, "content-length");
        return JavaValue(v ? static_cast<int64_t>(std::atoll(v->c_str())) : static_cast<int64_t>(h.respBody.size()));
    });
    auto stringHeader = [executed, header](const char* name) {
        return [executed, header, name](CldcVirtualMachine* vm, const Args& a) {
            const std::string* v = header(executed(vm, a), name);
            return v ? newStrUtf8(vm, *v) : nullV();
        };
    };
    vm->registerNative(H, "getType", "()Ljava/lang/String;", stringHeader("content-type"));
    vm->registerNative(H, "getEncoding", "()Ljava/lang/String;", stringHeader("content-encoding"));
}

} // namespace universal_loader::jvm

