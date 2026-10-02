#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Delegates HTTP(S) requests to the host application (Dart HttpClient on Android/iOS),
// which brings the platform TLS stack and certificate store.
namespace j2me::http_bridge {

using Handler = void (*)(int64_t requestId);

struct Request {
    std::string method;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<uint8_t> body;
};

void setHandler(Handler handler);
bool available();

// Blocks until the host completes the request, `cancelled` returns true, or 60 s pass.
// On success `response` is "status line\r\nheaders\r\n\r\nbody".
bool fetch(const Request& request, std::string& response, std::string& error, const std::function<bool()>& cancelled);

// Host side. Both return the full size (the info string includes its NUL terminator); 0 if the id is unknown.
size_t requestInfo(int64_t id, char* out, size_t capacity);
size_t requestBody(int64_t id, uint8_t* out, size_t capacity);
void complete(int64_t id, const char* head, const uint8_t* body, size_t bodyLength, const char* error);

} // namespace j2me::http_bridge
