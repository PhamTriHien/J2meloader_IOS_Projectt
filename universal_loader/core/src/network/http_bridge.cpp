#include "http_bridge.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

namespace j2me::http_bridge {
namespace {

struct Pending {
    Request request;
    bool done{false};
    bool ok{false};
    std::string response;
    std::string error;
};

std::mutex g_mutex;
std::condition_variable g_cv;
std::map<int64_t, std::shared_ptr<Pending>> g_pending;
std::atomic<Handler> g_handler{nullptr};
int64_t g_nextId = 1;

std::string sanitize(std::string s) {
    for (auto& c : s) {
        if (c == '\r' || c == '\n') c = ' ';
    }
    return s;
}

} // namespace

void setHandler(Handler handler) { g_handler.store(handler); }

bool available() { return g_handler.load() != nullptr; }

bool fetch(const Request& request, std::string& response, std::string& error, const std::function<bool()>& cancelled) {
    Handler handler = g_handler.load();
    if (!handler) {
        error = "HTTPS is not available: no host HTTP handler registered";
        return false;
    }
    auto pending = std::make_shared<Pending>();
    pending->request = request;
    int64_t id;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        id = g_nextId++;
        g_pending[id] = pending;
    }
    handler(id);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::unique_lock<std::mutex> lock(g_mutex);
    while (!pending->done) {
        if (cancelled && cancelled()) {
            error = "Request cancelled: " + request.url;
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            error = "Request timed out: " + request.url;
            break;
        }
        g_cv.wait_for(lock, std::chrono::milliseconds(100));
    }
    g_pending.erase(id);
    if (!pending->done) return false;
    if (!pending->ok) {
        error = pending->error;
        return false;
    }
    response = std::move(pending->response);
    return true;
}

size_t requestInfo(int64_t id, char* out, size_t capacity) {
    std::string info;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_pending.find(id);
        if (it == g_pending.end()) return 0;
        const Request& r = it->second->request;
        info = sanitize(r.method) + "\n" + sanitize(r.url) + "\n";
        for (const auto& kv : r.headers) info += sanitize(kv.first) + ": " + sanitize(kv.second) + "\n";
    }
    if (out && capacity > info.size()) std::memcpy(out, info.c_str(), info.size() + 1);
    return info.size() + 1;
}

size_t requestBody(int64_t id, uint8_t* out, size_t capacity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_pending.find(id);
    if (it == g_pending.end()) return 0;
    const auto& body = it->second->request.body;
    if (out && capacity >= body.size() && !body.empty()) std::memcpy(out, body.data(), body.size());
    return body.size();
}

void complete(int64_t id, const char* head, const uint8_t* body, size_t bodyLength, const char* error) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_pending.find(id);
    if (it == g_pending.end()) return;
    Pending& p = *it->second;
    p.done = true;
    if (error) {
        p.error = error;
    } else {
        p.ok = true;
        std::string h = head ? head : "";
        while (!h.empty() && (h.back() == '\r' || h.back() == '\n')) h.pop_back();
        p.response = h + "\r\n\r\n";
        if (body && bodyLength) p.response.append(reinterpret_cast<const char*>(body), bodyLength);
    }
    g_cv.notify_all();
}

} // namespace j2me::http_bridge
