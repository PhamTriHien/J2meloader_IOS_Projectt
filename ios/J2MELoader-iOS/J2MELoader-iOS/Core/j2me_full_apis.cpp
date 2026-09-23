#ifndef NOMINMAX
#define NOMINMAX
#endif
#define _CRT_SECURE_NO_WARNINGS

#include "j2me_full_apis.h"
#include "jvm_bytecode.h"
#include "jvm_interpreter.h"
#include "lcdui_display.h"
#include "jar_loader.h"
#include "game_canvas.h"
#include "m3g_engine.h"
#include "micro3d_engine.h"
#include "png_decoder.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define fs_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define fs_mkdir(p) mkdir(p, 0755)
#endif
#include <sys/stat.h>

#if defined(_MSC_VER)
#define WEAK_ATTR
#else
#define WEAK_ATTR __attribute__((weak))
#endif

// Real iOS native extensions (Bridge/NativeExtBridge.mm). Weak-linked:
// on non-Apple builds these symbols are absent -> fallback stubs below.
extern "C" {
bool native_http_fetch(const char *url, const char *method, uint8_t **outData, int *outLen, int *outCode, char *outType, int typeCap) WEAK_ATTR;
void native_free(void *p) WEAK_ATTR;
bool native_socket_test(const char *url) WEAK_ATTR;
int native_bluetooth_state(void) WEAK_ATTR;
int native_bluetooth_scan(int timeoutSec, char *outNames, int cap) WEAK_ATTR;
bool native_location_get(double *lat, double *lon, float *accuracy) WEAK_ATTR;
void native_location_request(void) WEAK_ATTR;
int native_contacts_count(void) WEAK_ATTR;
int native_calendar_count(void) WEAK_ATTR;
void native_contacts_request(void) WEAK_ATTR;
void native_calendar_request(void) WEAK_ATTR;
bool native_contact_get(int index, char *name, int nameCap, char *phone, int phoneCap) WEAK_ATTR;
bool native_http_send(const char *url, const char *method, const uint8_t *body, int bodyLen, uint8_t **outData, int *outLen, int *outCode, char *outType, int typeCap) WEAK_ATTR;
bool native_can_send_text(void) WEAK_ATTR;
bool native_prompt_text_input(const char *title, const char *initialText, char *outBuffer, int maxLen) WEAK_ATTR;
void native_vibrate(int ms) WEAK_ATTR;
bool native_camera_snapshot(uint8_t **outPNG, int *outLen) WEAK_ATTR;
void native_background_keepalive_start(void) WEAK_ATTR;
void native_background_keepalive_stop(void) WEAK_ATTR;
}
static bool hasNative(const void *f) { return f != nullptr; }

// ---- Real TCP socket helpers (POSIX, AppStore-safe: user-initiated game connections) ----
static bool parseHostPort(const std::string& url, std::string& host, int& port){
    // socket://host:port , datagram://host:port
    auto p = url.find("://"); std::string hp = (p==std::string::npos)?url:url.substr(p+3);
    auto c = hp.rfind(':'); if(c==std::string::npos) return false;
    host = hp.substr(0,c); try{ port = std::stoi(hp.substr(c+1)); }catch(...){ return false; }
    return !host.empty() && port>0 && port<65536;
}
static int tcpConnect(const std::string& host, int port){
#if defined(_WIN32)||defined(_WIN64)
    return -1;
#else
    struct addrinfo hints{}, *res=nullptr;
    hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM;
    char ps[16]; snprintf(ps,sizeof(ps),"%d",port);
    if(getaddrinfo(host.c_str(),ps,&hints,&res)!=0||!res) return -1;
    int fd=-1;
    for(auto *ai=res;ai;ai=ai->ai_next){
        fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
        if(fd<0) continue;
        int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
        int rc=connect(fd,ai->ai_addr,ai->ai_addrlen);
        if(rc==0){ fcntl(fd,F_SETFL,fl); break; }
        if(errno==EINPROGRESS){
            fd_set ws; FD_ZERO(&ws); FD_SET(fd,&ws);
            struct timeval tv{5,0};
            int s=select(fd+1,nullptr,&ws,nullptr,&tv);
            int err=0; socklen_t el=sizeof(err); getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el);
            if(s>0&&err==0){ fcntl(fd,F_SETFL,fl); break; }
        }
        close(fd); fd=-1;
    }
    freeaddrinfo(res);
    if(fd>=0){
        // Online games: low latency + survive NAT timeout during play
        int one=1;
        setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
        setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&one,sizeof(one));
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
        int idle=30, intvl=15, cnt=4;
        setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,&idle,sizeof(idle));
        setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,&intvl,sizeof(intvl));
        setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,&cnt,sizeof(cnt));
#endif
        int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
        if(hasNative((const void*)native_background_keepalive_start)) native_background_keepalive_start();
    }
    return fd;
#endif
}
static void tcpClose(int fd){
#if !defined(_WIN32)&&!defined(_WIN64)
    if(fd>=0){ close(fd); }
#endif
    if(hasNative((const void*)native_background_keepalive_stop)) native_background_keepalive_stop();
}
static std::vector<uint8_t> tcpRecvOnce(int fd, int maxN=8192){
    std::vector<uint8_t> out;
#if !defined(_WIN32)&&!defined(_WIN64)
    if(fd<0) return out;
    fd_set rs; FD_ZERO(&rs); FD_SET(fd,&rs);
    struct timeval tv{0,200000};
    if(select(fd+1,&rs,nullptr,nullptr,&tv)>0 && FD_ISSET(fd,&rs)){
        uint8_t buf[8192]; ssize_t n=recv(fd,buf,std::min(maxN,8192),0);
        if(n>0) out.assign(buf,buf+n);
    }
#endif
    return out;
}
// Blocking read for MIDP InputStream (game expects block up to ~8s)
static std::vector<uint8_t> tcpRecvBlock(int fd, int maxN=8192, int timeoutMs=8000){
    std::vector<uint8_t> out;
#if !defined(_WIN32)&&!defined(_WIN64)
    if(fd<0) return out;
    int waited=0;
    while(waited<timeoutMs){
        fd_set rs; FD_ZERO(&rs); FD_SET(fd,&rs);
        struct timeval tv{0,200000};
        int r=select(fd+1,&rs,nullptr,nullptr,&tv);
        if(r>0 && FD_ISSET(fd,&rs)){
            uint8_t buf[8192]; ssize_t n=recv(fd,buf,std::min(maxN,8192),0);
            if(n>0) out.assign(buf,buf+n);
            break;
        }
        waited+=200;
    }
#endif
    return out;
}
#if !defined(_WIN32)&&!defined(_WIN64)
static int tcpListen(int port){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0) return -1;
    int one=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
    struct sockaddr_in a{}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_ANY); a.sin_port=htons(port);
    if(bind(fd,(struct sockaddr*)&a,sizeof(a))!=0){ close(fd); return -1; }
    if(listen(fd,1)!=0){ close(fd); return -1; }
    if(hasNative((const void*)native_background_keepalive_start)) native_background_keepalive_start();
    return fd;
}
static int tcpAccept(int listenFd, int timeoutMs=8000){
    int waited=0;
    while(waited<timeoutMs){
        fd_set rs; FD_ZERO(&rs); FD_SET(listenFd,&rs);
        struct timeval tv{0,200000};
        if(select(listenFd+1,&rs,nullptr,nullptr,&tv)>0){
            int c=accept(listenFd,nullptr,nullptr);
            if(c>=0){ int fl=fcntl(c,F_GETFL,0); fcntl(c,F_SETFL,fl|O_NONBLOCK); return c; }
            break;
        }
        waited+=200;
    }
    return -1;
}
static int udpSocket(){ int fd=socket(AF_INET,SOCK_DGRAM,0); if(fd>=0){ int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK); } return fd; }
#endif
// WMA loopback inbox (same-device SMS games proceed; real carrier SMS needs user UI)
struct WmaMsg { std::string addr; std::string text; std::vector<uint8_t> bin; bool isText=true; };
static std::vector<WmaMsg> g_wmaInbox;
static std::mutex g_wmaMutex;
static bool tcpSendAll(int fd, const uint8_t* d, size_t n){
#if defined(_WIN32)||defined(_WIN64)
    return false;
#else
    size_t off=0;
    while(off<n){ ssize_t s=send(fd,d+off,n-off,0); if(s<=0){ if(errno==EAGAIN||errno==EWOULDBLOCK){ continue; } return false; } off+=s; }
    return true;
#endif
}

// ---------- helpers ----------
static JvmBytecodeEngine& ENG() { return JvmBytecodeEngine::getInstance(); }
static std::string toLowerStr(std::string s){ std::transform(s.begin(),s.end(),s.begin(),::tolower); return s; }
static std::string trimStr(const std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return "";
    size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);
}
static std::string returnTypeOf(const std::string& desc){
    auto p=desc.find(')'); if(p==std::string::npos||p+1>=desc.size()) return "V";
    return desc.substr(p+1);
}
static std::string objectTypeOf(const std::string& ret){
    // ret like "Ljava/lang/String;" or "[B"
    if(ret.empty()||ret[0]!='L') return "";
    auto e=ret.find(';'); if(e==std::string::npos) return "";
    return ret.substr(1,e-1);
}

// ---------- stores ----------
struct ScreenData {
    std::string title;
    std::string text;
    int type = -1;
    int selected = 0;
    std::vector<std::string> items;
    std::vector<uint32_t> commands;
    uint32_t listener = 0;
    uint32_t ticker = 0;
    int caret = 0;
};
static uint32_t g_currentScreen = 0;
static std::map<uint32_t, ScreenData> g_screens;
static std::map<uint32_t, std::shared_ptr<Sprite>> g_sprites;
static std::map<uint32_t, std::shared_ptr<TiledLayer>> g_tiled;
static std::map<uint32_t, std::shared_ptr<LayerManager>> g_layerMgr;
static std::map<uint32_t, std::string> g_m3gType;
struct M3GWorldData { std::vector<M3GVertex> verts; std::vector<uint16_t> idx; std::vector<uint32_t> tex; int tw=0,th=0; uint32_t bg=0xFF000000; };
static std::map<uint32_t, M3GWorldData> g_m3gWorlds;
static std::map<uint32_t, std::shared_ptr<Micro3DFigure>> g_microFig;
static std::map<uint32_t, std::shared_ptr<Micro3DTexture>> g_microTex;
static std::map<uint32_t, uint32_t> g_micro3dGfx; // Graphics3D ref -> bound Graphics ref (offscreen-aware)
static std::map<uint32_t, uint32_t> g_m3dTarget; // M3G Graphics3D ref -> bound Graphics/Image ref
static std::map<uint32_t, int> g_sockFd; // SocketConnection fd
static std::map<uint32_t, std::vector<uint8_t>> g_baos; // ByteArrayOutputStream buffer
static std::map<uint32_t, std::vector<std::pair<uint32_t,uint32_t>>> g_hashtable; // keyRef->valRef
struct EnumData { std::vector<uint32_t> items; size_t idx=0; };
static std::map<uint32_t, EnumData> g_enums;

static std::map<uint32_t, std::shared_ptr<std::atomic<bool>>> g_taskCancelFlags;
static std::map<uint32_t, std::vector<std::shared_ptr<std::atomic<bool>>>> g_timerTaskFlags;
static std::mutex g_timerMutex;

struct RecordEnumState {
    std::string storeName;
    std::vector<int> recordIds;
    size_t cursor = 0;
};
static std::map<uint32_t, RecordEnumState> g_recordEnums;
static std::mutex g_enumMutex;

static bool keysEqual(uint32_t k1, uint32_t k2) {
    if (k1 == k2) return true;
    if (k1 == 0 || k2 == 0) return false;
    JavaObject* a = ENG().getObject(k1);
    JavaObject* b = ENG().getObject(k2);
    if (!a || !b) return false;
    if (a->className != b->className) return false;
    if (a->className == "java/lang/String") return a->stringVal == b->stringVal;
    if (a->fields.count("value") && b->fields.count("value")) {
        const auto& va = a->fields["value"];
        const auto& vb = b->fields["value"];
        if (va.type == JavaValue::LONG || vb.type == JavaValue::LONG) return va.asLong() == vb.asLong();
        return va.asInt() == vb.asInt();
    }
    if (!a->stringVal.empty() && a->stringVal == b->stringVal) return true;
    return false;
}
struct PlayerData { std::vector<uint8_t> data; std::string ctype; std::string locator; int loop=1; bool playing=false; int state=200; };
static std::map<uint32_t, PlayerData> g_players;
struct ConnData { std::string url; std::string kind; std::string method="GET"; std::vector<uint8_t> body; std::vector<uint8_t> postBody; int code=0; std::string mime; bool fetched=false; };
static std::map<uint32_t, ConnData> g_conns;

// Fetch HTTP once per connection via real iOS NSURLSession (background-capable).
// POSTs pending request body (score submit/login) collected from openOutputStream.
static ConnData& httpEnsure(uint32_t ref){
    ConnData &c = g_conns[ref];
    if(!c.fetched && (c.kind=="javax/microedition/io/HttpConnection"||c.kind=="javax/microedition/io/HttpsConnection")){
        // Collect any pending POST body written via openOutputStream before openInputStream
        if(c.postBody.empty()){
            for(auto &kv : g_baos){
                JavaObject*so=ENG().getObject(kv.first);
                if(so && so->fields["connRef"].asInt()==(int32_t)ref && !kv.second.empty()){
                    c.postBody=kv.second; kv.second.clear(); break;
                }
            }
        }
        if(c.method!="GET"&&c.method!="HEAD"&&c.postBody.empty()){
            for(auto &kv : g_baos){
                JavaObject*so=ENG().getObject(kv.first);
                if(so && so->fields["connRef"].asInt()==(int32_t)ref && !kv.second.empty()){
                    c.postBody=kv.second; kv.second.clear(); break;
                }
            }
        }
        uint8_t *d=nullptr; int n=0, code=0; char mt[128]={0};
        bool ok=false;
        if(!c.postBody.empty() && hasNative((const void*)native_http_send)){
            std::string m = (c.method=="GET"||c.method=="HEAD") ? "POST" : c.method;
            ok = native_http_send(c.url.c_str(), m.c_str(), c.postBody.data(), (int)c.postBody.size(), &d, &n, &code, mt, sizeof(mt));
        } else if(hasNative((const void*)native_http_fetch)){
            ok = native_http_fetch(c.url.c_str(), c.method.c_str(), &d, &n, &code, mt, sizeof(mt));
        }
        c.code = ok ? (code?code:200) : 404;
        c.mime = mt[0]?mt:"application/octet-stream";
        if(ok && d && n>0) c.body.assign(d, d+n);
        if(d && hasNative((const void*)native_free)) native_free(d); else if(d) free(d);
        c.fetched = true;
        // keep socket/game alive during transfer
        if(hasNative((const void*)native_background_keepalive_start)) native_background_keepalive_start();
    } else if(!c.fetched){
        c.code = 200; c.mime="application/octet-stream"; c.fetched=true;
    }
    return c;
}

static std::vector<uint8_t> fetchHttpSync(const std::string& url){
    uint8_t *d = nullptr; int n = 0, code = 0; char mt[128] = {0};
    bool ok = false;
    if(hasNative((const void*)native_http_fetch)){
        ok = native_http_fetch(url.c_str(), "GET", &d, &n, &code, mt, sizeof(mt));
    }
    std::vector<uint8_t> result;
    if(ok && d && n > 0){
        result.assign(d, d + n);
    }
    if(d && hasNative((const void*)native_free)) native_free(d); else if(d) free(d);
    return result;
}

static std::string s_cachedServerList = 
    "Vũ trụ 1:dragon1.teamobi.com:14445:0:0:0,"
    "Vũ trụ 2:dragon2.teamobi.com:14445:0:0:0,"
    "Vũ trụ 3:dragon3.teamobi.com:14445:0:0:0,"
    "Vũ trụ 4:dragon4.teamobi.com:14445:0:0:0,"
    "Vũ trụ 5:dragon5.teamobi.com:14445:0:0:0,"
    "Vũ trụ 6:dragon6.teamobi.com:14445:0:0:0,"
    "Vũ trụ 7:dragon7.teamobi.com:14445:0:0:0,"
    "Vũ trụ 8:dragon8.teamobi.com:14445:0:0:0,"
    "Vũ trụ 9:dragon9.teamobi.com:14445:0:0:0,"
    "Vũ trụ 10:dragon10.teamobi.com:14445:0:0:0,"
    "Võ Đài Liên Vũ Trụ:dragonwar.teamobi.com:14445:0:0:0,"
    "Đông Nam Á:dragonsea.teamobi.com:14445:0:0:0";
static std::mutex g_fullApisMutex;
static std::mutex s_serverListMutex;
static std::atomic<bool> s_serverListUpdating{false};

static std::string getServerListText() {
    if (!s_serverListUpdating.exchange(true)) {
        std::thread([]() {
            std::string url = "https://raw.githubusercontent.com/2chinese2onetopup/chinese/refs/heads/main/ServerListScreen.txt";
            auto body = fetchHttpSync(url);
            if (!body.empty()) {
                std::string text(body.begin(), body.end());
                if (!text.empty() && text.find("dragon") != std::string::npos) {
                    std::lock_guard<std::mutex> slk(s_serverListMutex);
                    s_cachedServerList = text;
                }
            }
            s_serverListUpdating.store(false);
        }).detach();
    }
    std::lock_guard<std::mutex> slk(s_serverListMutex);
    return s_cachedServerList;
}

void FullApis::reset(){
    g_screens.clear(); g_sprites.clear(); g_tiled.clear(); g_layerMgr.clear();
    g_m3gType.clear(); g_m3gWorlds.clear(); g_microFig.clear(); g_microTex.clear(); g_micro3dGfx.clear(); g_m3dTarget.clear();
    for (const auto& pair : g_sockFd) {
        if (pair.second >= 0) tcpClose(pair.second);
    }
    g_sockFd.clear();
    g_baos.clear(); g_hashtable.clear(); g_enums.clear();
    {
        std::lock_guard<std::mutex> lk(g_timerMutex);
        for (auto& pair : g_taskCancelFlags) {
            if (pair.second) pair.second->store(true);
        }
        g_taskCancelFlags.clear();
        g_timerTaskFlags.clear();
    }
    {
        std::lock_guard<std::mutex> lk(g_enumMutex);
        g_recordEnums.clear();
    }
    g_players.clear(); g_conns.clear(); g_currentScreen=0;
    if(hasNative((const void*)native_background_keepalive_stop)) native_background_keepalive_stop();
    // Pre-seed static constants used via GETSTATIC fallback (also patched in jvm_bytecode)
    ENG().setStaticField("javax/microedition/m3g/Graphics3D:VERSION", JavaValue(1));
}
uint32_t FullApis::currentScreen(){ return g_currentScreen; }
int FullApis::reconnectSocket(uint32_t streamRef){
    JavaObject*so=ENG().getObject(streamRef);
    if(!so) return -1;
    auto cf=so->fields.find("connRef"); if(cf==so->fields.end()) return -1;
    uint32_t conn=(uint32_t)cf->second.asInt();
    auto it=g_conns.find(conn); if(it==g_conns.end()) return -1;
    std::string host; int port=0;
    if(!parseHostPort(it->second.url, host, port)) return -1;
    int fd=tcpConnect(host, port);
    if(fd>=0){
        auto sf=g_sockFd.find(conn);
        if(sf!=g_sockFd.end()) tcpClose(sf->second);
        g_sockFd[conn]=fd;
        so->fields["sockFd"]=JavaValue(fd);
    }
    return fd;
}

void FullApis::markRoots(std::function<void(uint32_t)> addRoot) {
    std::lock_guard<std::mutex> lk(g_fullApisMutex);
    addRoot(g_currentScreen);
    if (g_currentScreen != 0) {
        auto it = g_screens.find(g_currentScreen);
        if (it != g_screens.end()) {
            for (uint32_t c : it->second.commands) addRoot(c);
            addRoot(it->second.listener);
            addRoot(it->second.ticker);
        }
    }
}

void FullApis::traverseReachable(uint32_t curr, std::function<void(uint32_t)> addRoot) {
    std::lock_guard<std::mutex> lk(g_fullApisMutex);
    auto htIt = g_hashtable.find(curr);
    if (htIt != g_hashtable.end()) {
        for (const auto& p : htIt->second) {
            addRoot(p.first);
            addRoot(p.second);
        }
    }
    auto enIt = g_enums.find(curr);
    if (enIt != g_enums.end()) {
        for (uint32_t item : enIt->second.items) {
            addRoot(item);
        }
    }
    auto scIt = g_screens.find(curr);
    if (scIt != g_screens.end()) {
        for (uint32_t c : scIt->second.commands) addRoot(c);
        addRoot(scIt->second.listener);
        addRoot(scIt->second.ticker);
    }
    auto m3It = g_micro3dGfx.find(curr);
    if (m3It != g_micro3dGfx.end()) {
        addRoot(m3It->second);
    }
    auto mtIt = g_m3dTarget.find(curr);
    if (mtIt != g_m3dTarget.end()) {
        addRoot(mtIt->second);
    }
}

void FullApis::sweep(const std::unordered_set<uint32_t>& marked) {
    std::lock_guard<std::mutex> lk(g_fullApisMutex);
    for (auto it = g_hashtable.begin(); it != g_hashtable.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_hashtable.erase(it);
        else ++it;
    }
    for (auto it = g_enums.begin(); it != g_enums.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_enums.erase(it);
        else ++it;
    }
    for (auto it = g_sprites.begin(); it != g_sprites.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_sprites.erase(it);
        else ++it;
    }
    for (auto it = g_tiled.begin(); it != g_tiled.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_tiled.erase(it);
        else ++it;
    }
    for (auto it = g_layerMgr.begin(); it != g_layerMgr.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_layerMgr.erase(it);
        else ++it;
    }
    for (auto it = g_m3gWorlds.begin(); it != g_m3gWorlds.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_m3gWorlds.erase(it);
        else ++it;
    }
    for (auto it = g_microFig.begin(); it != g_microFig.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_microFig.erase(it);
        else ++it;
    }
    for (auto it = g_microTex.begin(); it != g_microTex.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_microTex.erase(it);
        else ++it;
    }
    for (auto it = g_micro3dGfx.begin(); it != g_micro3dGfx.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_micro3dGfx.erase(it);
        else ++it;
    }
    for (auto it = g_m3dTarget.begin(); it != g_m3dTarget.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_m3dTarget.erase(it);
        else ++it;
    }
    for (auto it = g_baos.begin(); it != g_baos.end(); ) {
        if (it->first != 0 && marked.find(it->first) == marked.end()) it = g_baos.erase(it);
        else ++it;
    }
    for (auto it = g_screens.begin(); it != g_screens.end(); ) {
        if (it->first != 0 && it->first != g_currentScreen && marked.find(it->first) == marked.end()) it = g_screens.erase(it);
        else ++it;
    }
}

static void renderScreen(uint32_t ref, LcduiDisplay* display);

void FullApis::onKey(int keyCode, bool isDown, LcduiDisplay* display){
    if(!isDown) return;
    if(g_currentScreen==0) return;
    auto it=g_screens.find(g_currentScreen);
    if(it==g_screens.end()||it->second.listener==0||it->second.commands.empty()) return;
    // Softkey/FIRE triggers first command (OK/Select). D-pad navigates List.
    if(keyCode==-6||keyCode==-7||keyCode==-5||keyCode==10||keyCode==13){
        uint32_t lis=it->second.listener;
        uint32_t cmd=it->second.commands[0];
        JavaObject*lo=ENG().getObject(lis);
        if(!lo||lo->className.empty()) return;
        auto cls=ENG().findOrLoadClass(lo->className, ENG().getJarLoader());
        if(!cls) return;
        ENG().executeMethod(cls,"commandAction","(Ljavax/microedition/lcdui/Command;Ljavax/microedition/lcdui/Displayable;)V",
            {JavaValue(lis,true),JavaValue(cmd,true),JavaValue(g_currentScreen,true)},display);
    } else if(keyCode==-1||keyCode==-2||keyCode=='2'||keyCode=='8'){
        // Up/down navigates List selection
        if(!it->second.items.empty()){
            if(keyCode==-1||keyCode=='2') it->second.selected=(it->second.selected-1+(int)it->second.items.size())%(int)it->second.items.size();
            else it->second.selected=(it->second.selected+1)%(int)it->second.items.size();
            renderScreen(g_currentScreen, display);
        }
    }
}

// Render high-level screen (Form/List/TextBox/Alert) onto framebuffer so user sees clear text
static void renderScreen(uint32_t ref, LcduiDisplay* display){
    if(!display) return;
    auto it=g_screens.find(ref);
    std::string title = (it!=g_screens.end()? it->second.title : "Nhập văn bản");
    std::string text = (it!=g_screens.end()? it->second.text : "");
    std::vector<std::string> items = (it!=g_screens.end()? it->second.items : std::vector<std::string>());
    int sel = (it!=g_screens.end()? it->second.selected : 0);
    int w=display->getWidth(), h=display->getHeight();
    display->clear(0xFFF8F9FA); // Clean modern background
    display->fillRect(0,0,w,28,0xFF0078D7); // Modern header bar
    display->drawString(title.empty()?"Nhập văn bản":title.substr(0,28), w/2, 14, 1|2, 0xFFFFFFFF);
    int y=38;
    if(!text.empty()){
        // Draw modern text card
        int boxH = 36;
        display->fillRect(8, y, w - 16, boxH, 0xFFFFFFFF);
        display->drawRect(8, y, w - 16, boxH, 0xFF0078D7);
        display->drawString(text, 14, y + boxH / 2, 4|2, 0xFF111827);
        y += boxH + 12;
    }
    for(size_t i=0;i<items.size() && y<h-10;i++){
        uint32_t bg = ((int)i==sel)? 0xFF0078D7 : 0xFFFFFFFF;
        uint32_t fg = ((int)i==sel)? 0xFFFFFFFF : 0xFF1F2937;
        display->fillRect(8,y,w-16,20,bg);
        display->drawRect(8,y,w-16,20,0xFFE5E7EB);
        display->drawString(std::string(((int)i==sel)?"> ":"  ")+items[i].substr(0,28), 14, y+10, 4|2, fg);
        y+=24;
    }
}

void FullApis::renderCurrentScreen(LcduiDisplay* display){
    if(g_currentScreen!=0 && display){
        renderScreen(g_currentScreen, display);
    }
}

static NativeImage* imgOf(uint32_t ref){ return ENG().getNativeImage(ref); }
// Read image pixels through the offscreen display when the game drew on it
// via getGraphics() (syncs back first), else raw pixels.
static const uint32_t* imgPx(uint32_t ref){
    ENG().syncImageFromDisplay(ref);
    NativeImage* ni = ENG().getNativeImage(ref);
    if (ni && !ni->pixels.empty()) return ni->pixels.data();
    return nullptr;
}
// Resolve the LcduiDisplay a Nokia DirectGraphics should draw on:
// the Graphics it was created from (offscreen-aware), else the screen.
static LcduiDisplay* dgTarget(uint32_t dgRef, LcduiDisplay* screen){
    JavaObject*o=ENG().getObject(dgRef);
    if(o){
        auto it=o->fields.find("gfx");
        if(it!=o->fields.end() && it->second.asRef()!=0)
            return ENG().resolveGraphics(it->second.asRef(), screen);
    }
    return screen;
}

static std::vector<uint8_t> streamBytes(uint32_t streamRef){
    JavaObject* o = ENG().getObject(streamRef);
    if(!o) return {};
    auto it=o->fields.find("buf");
    if(it==o->fields.end()) return {};
    JavaArray* a=ENG().getArray(it->second.asRef());
    if(!a) return {};
    return a->byteData;
}

bool FullApis::dispatch(const std::string& className, const std::string& methodName,
                        const std::string& desc, const std::vector<JavaValue>& args,
                        JavaValue& outResult, LcduiDisplay* display){
    // ============ java/lang/Object ============
    if(className=="java/lang/Object"){
        if(methodName=="hashCode"){ outResult=JavaValue((int32_t)(args.empty()?0:(int)args[0].asRef())); return true; }
        if(methodName=="equals"&&args.size()>=2){ outResult=JavaValue(args[0].asRef()==args[1].asRef()?1:0); return true; }
        if(methodName=="toString"){
            std::string s = "";
            if(!args.empty() && args[0].asRef() != 0){
                JavaObject* o = ENG().getObject(args[0].asRef());
                if(o && !o->stringVal.empty()) s = o->stringVal;
            }
            outResult = JavaValue(ENG().createString(s), true);
            return true;
        }
        if(methodName=="getClass"){
            uint32_t cRef = ENG().allocObject("java/lang/Class");
            JavaObject* cObj = ENG().getObject(cRef);
            if(cObj && !args.empty()){
                JavaObject* srcObj = ENG().getObject(args[0].asRef());
                if(srcObj) cObj->stringVal = srcObj->className;
            }
            outResult = JavaValue(cRef, true);
            return true;
        }
        if(methodName=="notify"||methodName=="notifyAll"||methodName=="wait"||methodName=="<init>") return true;
    }
    // ============ java/lang/System extended ============
    if(className=="java/lang/Runtime"){
        if(methodName=="getRuntime"){ outResult=JavaValue(ENG().allocObject("java/lang/Runtime"),true); return true; }
        if(methodName=="gc"||methodName=="runFinalization") { ENG().runGarbageCollector(); return true; }
        if(methodName=="totalMemory"){ outResult=JavaValue((int32_t)(16*1024*1024)); return true; }
        if(methodName=="freeMemory"){ outResult=JavaValue((int32_t)(8*1024*1024)); return true; }
        if(methodName=="exit") return true;
    }
    // ============ java/lang wrappers ============
    if(className=="java/lang/Boolean"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="booleanValue"&&!args.empty()){ JavaObject*o=ENG().getObject(args[0].asRef()); int v=o?o->fields["value"].asInt():0; outResult=JavaValue(v); return true; }
        if(methodName=="parseBoolean"||methodName=="valueOf"){ std::string s=args.empty()?"":ENG().getString(args[0].asRef()); outResult=JavaValue(toLowerStr(s)=="true"?1:0); if(methodName=="valueOf"){ uint32_t r=ENG().allocObject("java/lang/Boolean"); JavaObject*o=ENG().getObject(r); if(o)o->fields["value"]=outResult; outResult=JavaValue(r,true);} return true; }
        if(methodName=="toString"){ std::string s=(args.size()>=1&&args[0].asInt()!=0)?"true":"false"; if(!args.empty()&&args[0].type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(args[0].asRef()); if(o) s=(o->fields["value"].asInt()?"true":"false"); } outResult=JavaValue(ENG().createString(s),true); return true; }
    }
    if(className=="java/lang/Byte"||className=="java/lang/Short"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="parseByte"||methodName=="parseShort"){ std::string s=args.empty()?"":ENG().getString(args[0].asRef()); try{outResult=JavaValue((int32_t)std::stoi(s));}catch(...){outResult=JavaValue(0);} return true; }
        if(methodName=="toString"&&args.size()>=1){ outResult=JavaValue(ENG().createString(std::to_string(args[0].asInt())),true); return true; }
        if(methodName=="byteValue"||methodName=="shortValue"||methodName=="intValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["value"].asInt():0); return true; }
    }
    if(className=="java/lang/Character"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="isDigit"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue(std::isdigit(ch)?1:0); return true; }
        if(methodName=="isWhitespace"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue(std::isspace(ch)?1:0); return true; }
        if(methodName=="isUpperCase"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue(std::isupper(ch)?1:0); return true; }
        if(methodName=="isLowerCase"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue(std::islower(ch)?1:0); return true; }
        if(methodName=="toUpperCase"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue((int32_t)std::toupper(ch)); return true; }
        if(methodName=="toLowerCase"&&!args.empty()){ int ch=args[0].asInt(); outResult=JavaValue((int32_t)std::tolower(ch)); return true; }
        if(methodName=="digit"){ int ch=args.size()>=1?args[0].asInt():0, r=args.size()>=2?args[1].asInt():10; int v=-1; if(ch>='0'&&ch<='9')v=ch-'0'; else if(ch>='a'&&ch<='z')v=ch-'a'+10; else if(ch>='A'&&ch<='Z')v=ch-'A'+10; if(v>=r)v=-1; outResult=JavaValue(v); return true; }
        if(methodName=="charValue"&&!args.empty()){ JavaObject*o=ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["value"].asInt():0); return true; }
        if(methodName=="toString"&&args.size()>=1){ char c=(char)args[0].asInt(); outResult=JavaValue(ENG().createString(std::string(1, c)), true); return true; }
    }
    if(className=="java/lang/Integer"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="intValue"||methodName=="shortValue"||methodName=="byteValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["value"].asInt():0); return true; }
        if(methodName=="longValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?(int64_t)o->fields["value"].asInt():(int64_t)0); return true; }
        if(methodName=="floatValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?(float)o->fields["value"].asInt():0.0f); return true; }
        if(methodName=="doubleValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?(double)o->fields["value"].asInt():0.0); return true; }
        if(methodName=="valueOf"&&!args.empty()){ uint32_t r=ENG().allocObject("java/lang/Integer"); JavaObject*o=ENG().getObject(r); if(o) o->fields["value"]=JavaValue(args[0].asInt()); outResult=JavaValue(r,true); return true; }
        if(methodName=="parseInt"&&!args.empty()){ std::string s=ENG().getString(args[0].asRef()); int radix=args.size()>=2?args[1].asInt():10; try{outResult=JavaValue((int32_t)std::stoi(s,nullptr,radix));}catch(...){outResult=JavaValue(0);} return true; }
        if(methodName=="toHexString"&&!args.empty()){ uint32_t v=(uint32_t)args[0].asInt(); char buf[32]; snprintf(buf,sizeof(buf),"%x",v); outResult=JavaValue(ENG().createString(buf),true); return true; }
        if(methodName=="toOctalString"&&!args.empty()){ uint32_t v=(uint32_t)args[0].asInt(); char buf[32]; snprintf(buf,sizeof(buf),"%o",v); outResult=JavaValue(ENG().createString(buf),true); return true; }
        if(methodName=="toBinaryString"&&!args.empty()){ uint32_t v=(uint32_t)args[0].asInt(); std::string b=""; if(v==0) b="0"; else { while(v>0){ b=((v&1)?'1':'0')+b; v>>=1; } } outResult=JavaValue(ENG().createString(b),true); return true; }
        if(methodName=="toString"){ int32_t v=0; if(args.size()==1&&args[0].type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(args[0].asRef()); v=o?o->fields["value"].asInt():0; } else if(!args.empty()){ v=args[0].asInt(); } outResult=JavaValue(ENG().createString(std::to_string(v)),true); return true; }
    }
    if(className=="java/lang/Long"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="parseLong"){ std::string s=args.empty()?"":ENG().getString(args[0].asRef()); try{outResult=JavaValue((int64_t)std::stoll(s));}catch(...){outResult=JavaValue((int64_t)0);} return true; }
        if(methodName=="valueOf"&&!args.empty()){ uint32_t r=ENG().allocObject("java/lang/Long"); JavaObject*o=ENG().getObject(r); if(o) o->fields["value"]=JavaValue(args[0].asLong()); outResult=JavaValue(r,true); return true; }
        if(methodName=="toString"){ int64_t v=0; if(args.size()==1&&args[0].type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(args[0].asRef()); v=o?o->fields["value"].asLong():0; } else if(!args.empty()){ v=args[0].asLong(); } outResult=JavaValue(ENG().createString(std::to_string(v)),true); return true; }
        if(methodName=="longValue"||methodName=="intValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["value"]:JavaValue((int64_t)0); return true; }
    }
    if(className=="java/lang/Float"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="parseFloat"&&!args.empty()){ std::string s=ENG().getString(args[0].asRef()); try{outResult=JavaValue((float)std::stof(s));}catch(...){outResult=JavaValue(0.0f);} return true; }
        if(methodName=="valueOf"&&!args.empty()){ uint32_t r=ENG().allocObject("java/lang/Float"); JavaObject*o=ENG().getObject(r); if(o) o->fields["value"]=JavaValue(args[0].asFloat()); outResult=JavaValue(r,true); return true; }
        if(methodName=="floatToIntBits"||methodName=="floatToRawIntBits"){
            float f = args.empty() ? 0.0f : args[0].asFloat();
            uint32_t u = 0; std::memcpy(&u, &f, 4);
            outResult = JavaValue((int32_t)u);
            return true;
        }
        if(methodName=="intBitsToFloat"){
            int32_t i = args.empty() ? 0 : args[0].asInt();
            float f = 0.0f; std::memcpy(&f, &i, 4);
            outResult = JavaValue(f);
            return true;
        }
        if(methodName=="toString"){ float f=0.0f; if(args.size()==1&&args[0].type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(args[0].asRef()); f=o?o->fields["value"].asFloat():0.0f; } else if(!args.empty()){ f=args[0].asFloat(); } outResult=JavaValue(ENG().createString(std::to_string(f)),true); return true; }
        if(methodName=="floatValue"||methodName=="intValue"||methodName=="longValue"||methodName=="doubleValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); JavaValue v=o?o->fields["value"]:JavaValue(0.0f); if(methodName=="intValue") outResult=JavaValue(v.asInt()); else if(methodName=="longValue") outResult=JavaValue(v.asLong()); else if(methodName=="doubleValue") outResult=JavaValue((double)v.asFloat()); else outResult=JavaValue(v.asFloat()); return true; }
        if(methodName=="isNaN"){ outResult=JavaValue(std::isnan(args.empty()?0.0f:args[0].asFloat())?1:0); return true; }
        if(methodName=="isInfinite"){ outResult=JavaValue(std::isinf(args.empty()?0.0f:args[0].asFloat())?1:0); return true; }
    }
    if(className=="java/lang/Double"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o&&args.size()>=2) o->fields["value"]=args[1]; return true; }
        if(methodName=="parseDouble"&&!args.empty()){ std::string s=ENG().getString(args[0].asRef()); try{outResult=JavaValue(std::stod(s));}catch(...){outResult=JavaValue(0.0);} return true; }
        if(methodName=="valueOf"&&!args.empty()){ uint32_t r=ENG().allocObject("java/lang/Double"); JavaObject*o=ENG().getObject(r); if(o) o->fields["value"]=JavaValue(args[0].asDouble()); outResult=JavaValue(r,true); return true; }
        if(methodName=="doubleToLongBits"||methodName=="doubleToRawLongBits"){
            double d = args.empty() ? 0.0 : args[0].asDouble();
            uint64_t u = 0; std::memcpy(&u, &d, 8);
            outResult = JavaValue((int64_t)u);
            return true;
        }
        if(methodName=="longBitsToDouble"){
            int64_t l = args.empty() ? 0 : args[0].asLong();
            double d = 0.0; std::memcpy(&d, &l, 8);
            outResult = JavaValue(d);
            return true;
        }
        if(methodName=="toString"){ double d=0.0; if(args.size()==1&&args[0].type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(args[0].asRef()); d=o?o->fields["value"].asDouble():0.0; } else if(!args.empty()){ d=args[0].asDouble(); } outResult=JavaValue(ENG().createString(std::to_string(d)),true); return true; }
        if(methodName=="doubleValue"||methodName=="floatValue"||methodName=="intValue"||methodName=="longValue"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); JavaValue v=o?o->fields["value"]:JavaValue(0.0); if(methodName=="intValue") outResult=JavaValue(v.asInt()); else if(methodName=="longValue") outResult=JavaValue(v.asLong()); else if(methodName=="floatValue") outResult=JavaValue(v.asFloat()); else outResult=JavaValue(v.asDouble()); return true; }
        if(methodName=="isNaN"){ outResult=JavaValue(std::isnan(args.empty()?0.0:args[0].asDouble())?1:0); return true; }
        if(methodName=="isInfinite"){ outResult=JavaValue(std::isinf(args.empty()?0.0:args[0].asDouble())?1:0); return true; }
    }
    // ============ java/lang/String extended (core handles basics) ============
    if(className=="java/lang/String"){
        std::string s0 = args.empty()? "":ENG().getString(args[0].asRef());
        if(methodName=="trim"){ outResult=JavaValue(ENG().createString(trimStr(s0)),true); return true; }
        if(methodName=="toLowerCase"){ std::string s=s0; std::transform(s.begin(),s.end(),s.begin(),::tolower); outResult=JavaValue(ENG().createString(s),true); return true; }
        if(methodName=="toUpperCase"){ std::string s=s0; std::transform(s.begin(),s.end(),s.begin(),::toupper); outResult=JavaValue(ENG().createString(s),true); return true; }
        if(methodName=="startsWith"&&args.size()>=2){ std::string p=ENG().getString(args[1].asRef()); outResult=JavaValue(s0.rfind(p,0)==0?1:0); return true; }
        if(methodName=="endsWith"&&args.size()>=2){ std::string p=ENG().getString(args[1].asRef()); outResult=JavaValue(s0.size()>=p.size()&&s0.compare(s0.size()-p.size(),p.size(),p)==0?1:0); return true; }
        if(methodName=="compareTo"&&args.size()>=2){ std::string o=ENG().getString(args[1].asRef()); outResult=JavaValue((int32_t)s0.compare(o)); return true; }
        if(methodName=="compareToIgnoreCase"&&args.size()>=2){ std::string o=ENG().getString(args[1].asRef()); outResult=JavaValue((int32_t)toLowerStr(s0).compare(toLowerStr(o))); return true; }
        if(methodName=="hashCode"){
            int32_t h = 0;
            for (size_t i = 0; i < s0.size();) {
                uint32_t cp = 0;
                uint8_t b0 = (uint8_t)s0[i++];
                if (b0 < 0x80) {
                    cp = b0;
                } else if ((b0 & 0xE0) == 0xC0 && i < s0.size()) {
                    cp = ((b0 & 0x1F) << 6) | ((uint8_t)s0[i++] & 0x3F);
                } else if ((b0 & 0xF0) == 0xE0 && i + 1 < s0.size()) {
                    cp = ((b0 & 0x0F) << 12) | (((uint8_t)s0[i] & 0x3F) << 6) | ((uint8_t)s0[i + 1] & 0x3F);
                    i += 2;
                } else {
                    cp = b0;
                }
                h = 31 * h + (int32_t)cp;
            }
            outResult = JavaValue(h);
            return true;
        }
        if(methodName=="replace"&&args.size()>=3){ char a=(char)args[1].asInt(),b=(char)args[2].asInt(); std::string s=s0; std::replace(s.begin(),s.end(),a,b); outResult=JavaValue(ENG().createString(s),true); return true; }
        if(methodName=="toCharArray"){
            std::vector<uint16_t> utf16;
            for (size_t i = 0; i < s0.size();) {
                uint32_t cp = 0;
                uint8_t b0 = (uint8_t)s0[i++];
                if (b0 < 0x80) cp = b0;
                else if ((b0 & 0xE0) == 0xC0 && i < s0.size()) {
                    cp = ((b0 & 0x1F) << 6) | ((uint8_t)s0[i++] & 0x3F);
                } else if ((b0 & 0xF0) == 0xE0 && i + 1 < s0.size()) {
                    cp = ((b0 & 0x0F) << 12) | (((uint8_t)s0[i] & 0x3F) << 6) | ((uint8_t)s0[i + 1] & 0x3F);
                    i += 2;
                } else cp = b0;
                utf16.push_back((uint16_t)cp);
            }
            uint32_t r = ENG().allocArray(5, (int)utf16.size());
            JavaArray* a = ENG().getArray(r);
            if (a) a->charData = std::move(utf16);
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="getChars"&&args.size()>=5){
            JavaArray* a = ENG().getArray(args[4].asRef());
            int sb = args[1].asInt(), eb = args[2].asInt(), db = args[3].asInt();
            if (a) {
                std::vector<uint16_t> utf16;
                for (size_t i = 0; i < s0.size();) {
                    uint32_t cp = 0;
                    uint8_t b0 = (uint8_t)s0[i++];
                    if (b0 < 0x80) cp = b0;
                    else if ((b0 & 0xE0) == 0xC0 && i < s0.size()) {
                        cp = ((b0 & 0x1F) << 6) | ((uint8_t)s0[i++] & 0x3F);
                    } else if ((b0 & 0xF0) == 0xE0 && i + 1 < s0.size()) {
                        cp = ((b0 & 0x0F) << 12) | (((uint8_t)s0[i] & 0x3F) << 6) | ((uint8_t)s0[i + 1] & 0x3F);
                        i += 2;
                    } else cp = b0;
                    utf16.push_back((uint16_t)cp);
                }
                for (int i = sb; i < eb && i < (int)utf16.size() && db + (i - sb) < (int)a->charData.size(); ++i) {
                    a->charData[db + (i - sb)] = utf16[i];
                }
            }
            return true;
        }
        if(methodName=="getBytes"&&args.size()>=2){ // getBytes(String enc)
            uint32_t r=ENG().allocArray(8,(int)s0.size()); JavaArray*a=ENG().getArray(r); if(a)for(size_t i=0;i<s0.size();i++)a->byteData[i]=(uint8_t)s0[i]; outResult=JavaValue(r,true); return true; }
        if(methodName=="valueOf"&&args.size()>=1){
            JavaValue v=args[0]; std::string s;
            if(v.type==JavaValue::OBJ_REF){
                if(v.asRef()==0) s="null";
                else {
                    JavaObject*o=ENG().getObject(v.asRef());
                    if(o && (o->className=="java/lang/String"||o->className=="java/lang/StringBuffer"||o->className=="java/lang/StringBuilder")) s=o->stringVal;
                    else if(o && o->fields.find("value")!=o->fields.end()) s=std::to_string(o->fields["value"].asInt());
                    else if(o) s=o->stringVal;
                }
            }
            else if(desc.find("(D)")!=std::string::npos||desc.find("(F)")!=std::string::npos) s=std::to_string(v.asDouble());
            else if(desc.find("(J)")!=std::string::npos) s=std::to_string(v.asLong());
            else if(desc.find("(Z)")!=std::string::npos) s=v.asInt()?"true":"false";
            else if(desc.find("(C)")!=std::string::npos) s=std::string(1,(char)v.asInt());
            else s=std::to_string(v.asInt());
            outResult=JavaValue(ENG().createString(s),true); return true;
        }
        if(methodName=="intern"){ outResult=JavaValue(args[0].asRef(),true); return true; }
        if(methodName=="lastIndexOf"&&args.size()>=2){ std::string s=s0; int r=-1; if(desc.find("(Ljava/lang/String;)")!=std::string::npos){ std::string p=ENG().getString(args[1].asRef()); auto pos=s.rfind(p); r=pos==std::string::npos?-1:(int)pos; } else { char c=(char)args[1].asInt(); auto pos=s.rfind(c); r=pos==std::string::npos?-1:(int)pos; } outResult=JavaValue(r); return true; }
    }
    // ============ java/lang/Math full double ============
    if(className=="java/lang/Math"){
        auto D=[&](size_t i){ return i<args.size()?args[i].asDouble():0; };
        auto I=[&](size_t i){ return i<args.size()?args[i].asInt():0; };
        if(methodName=="abs"){
            if(desc.find("(D)")!=std::string::npos){outResult=JavaValue(std::abs(D(0)));return true;}
            if(desc.find("(F)")!=std::string::npos){outResult=JavaValue(std::abs(args[0].asFloat()));return true;}
            if(desc.find("(J)")!=std::string::npos){int64_t v=args[0].asLong();outResult=JavaValue(v<0?-v:v);return true;}
            outResult=JavaValue(std::abs(I(0)));return true;
        }
        if(methodName=="min"||methodName=="max"){
            bool isMin=(methodName=="min");
            if(desc.find("(FF)")!=std::string::npos){
                float a = args.size() >= 1 ? args[0].asFloat() : 0.0f;
                float b = args.size() >= 2 ? args[1].asFloat() : 0.0f;
                outResult = JavaValue(isMin ? std::min(a, b) : std::max(a, b));
                return true;
            }
            if(desc.find("(DD)")!=std::string::npos){double a=D(0),b=D(1);outResult=JavaValue(isMin?std::min(a,b):std::max(a,b));return true;}
            if(desc.find("(JJ)")!=std::string::npos){int64_t a=args[0].asLong(),b=args[1].asLong();outResult=JavaValue(isMin?std::min(a,b):std::max(a,b));return true;}
            outResult=JavaValue(isMin?std::min(I(0),I(1)):std::max(I(0),I(1)));return true;
        }
        if(methodName=="sqrt"){outResult=JavaValue(std::sqrt(D(0)));return true;}
        if(methodName=="sin"){outResult=JavaValue(std::sin(D(0)));return true;}
        if(methodName=="cos"){outResult=JavaValue(std::cos(D(0)));return true;}
        if(methodName=="tan"){outResult=JavaValue(std::tan(D(0)));return true;}
        if(methodName=="asin"){outResult=JavaValue(std::asin(D(0)));return true;}
        if(methodName=="acos"){outResult=JavaValue(std::acos(D(0)));return true;}
        if(methodName=="atan"){outResult=JavaValue(std::atan(D(0)));return true;}
        if(methodName=="atan2"){outResult=JavaValue(std::atan2(D(0),D(1)));return true;}
        if(methodName=="pow"){outResult=JavaValue(std::pow(D(0),D(1)));return true;}
        if(methodName=="exp"){outResult=JavaValue(std::exp(D(0)));return true;}
        if(methodName=="log"){outResult=JavaValue(std::log(D(0)));return true;}
        if(methodName=="ceil"){outResult=JavaValue(std::ceil(D(0)));return true;}
        if(methodName=="floor"){outResult=JavaValue(std::floor(D(0)));return true;}
        if(methodName=="round"){
            if(desc.find("(D)")!=std::string::npos) {
                double a = D(0);
                outResult = JavaValue((int64_t)std::floor(a + 0.5));
            } else {
                float a = args.size() >= 1 ? args[0].asFloat() : 0.0f;
                outResult = JavaValue((int32_t)std::floor(a + 0.5f));
            }
            return true;
        }
        if(methodName=="IEEEremainder"){ outResult=JavaValue(std::remainder(D(0), D(1))); return true; }
        if(methodName=="random"){outResult=JavaValue((double)rand()/(double)RAND_MAX);return true;}
        if(methodName=="toRadians"){outResult=JavaValue(D(0)*3.14159265358979/180.0);return true;}
        if(methodName=="toDegrees"){outResult=JavaValue(D(0)*180.0/3.14159265358979);return true;}
    }
    // ============ java/lang/Class extended ============
    if(className=="java/lang/Class"){
        if(methodName=="forName"&&!args.empty()){
            std::string n=ENG().getString(args[0].asRef());
            std::replace(n.begin(),n.end(),'.','/');
            uint32_t cRef = ENG().allocObject("java/lang/Class");
            JavaObject* cObj = ENG().getObject(cRef);
            if(cObj) cObj->stringVal = n;
            outResult=JavaValue(cRef,true);
            return true;
        }
        if(methodName=="getName"&&!args.empty()){
            JavaObject* cObj = ENG().getObject(args[0].asRef());
            std::string cn = (cObj && !cObj->stringVal.empty()) ? cObj->stringVal : "java/lang/Object";
            std::replace(cn.begin(), cn.end(), '/', '.');
            outResult=JavaValue(ENG().createString(cn),true);
            return true;
        }
        if(methodName=="newInstance"&&!args.empty()){
            JavaObject* cObj = ENG().getObject(args[0].asRef());
            std::string targetClass = (cObj && !cObj->stringVal.empty()) ? cObj->stringVal : "java/lang/Object";
            uint32_t newRef = ENG().allocObject(targetClass);
            auto cls = ENG().findOrLoadClass(targetClass, ENG().getJarLoader());
            if (cls) {
                ENG().executeMethod(cls, "<init>", "()V", { JavaValue(newRef, true) }, display);
            }
            outResult=JavaValue(newRef,true);
            return true;
        }
        if(methodName=="isAssignableFrom"||methodName=="isInstance"){ outResult=JavaValue(1); return true; }
        if(methodName=="getResourceAsStream") return false; // handled in core
    }
    // ============ java/util/Hashtable ============
    if(className=="java/util/Hashtable"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){ g_hashtable[self]={}; return true; }
        if(methodName=="clear"){ g_hashtable[self].clear(); return true; }
        if(methodName=="size"){ auto it=g_hashtable.find(self); outResult=JavaValue(it==g_hashtable.end()?0:(int32_t)it->second.size()); return true; }
        if(methodName=="isEmpty"){ auto it=g_hashtable.find(self); outResult=JavaValue(it==g_hashtable.end()||it->second.empty()?1:0); return true; }
        if(methodName=="put"&&args.size()>=3){
            uint32_t k=args[1].asRef(), v=args[2].asRef();
            if(k == 0 || v == 0){
                ENG().setPendingException(ENG().allocObject("java/lang/NullPointerException"));
                return true;
            }
            auto &vec=g_hashtable[self];
            for(auto &p:vec){
                if(keysEqual(p.first, k)){
                    uint32_t old=p.second; p.second=v; outResult=JavaValue(old,true); return true;
                }
            }
            vec.emplace_back(k,v); outResult=JavaValue(0,true); return true;
        }
        if(methodName=="get"&&args.size()>=2){
            uint32_t k=args[1].asRef();
            if(k == 0){
                ENG().setPendingException(ENG().allocObject("java/lang/NullPointerException"));
                return true;
            }
            auto it=g_hashtable.find(self);
            if(it!=g_hashtable.end()){
                for(auto &p:it->second){
                    if(keysEqual(p.first, k)){
                        outResult=JavaValue(p.second,true); return true;
                    }
                }
            }
            outResult=JavaValue(0,true); return true;
        }
        if(methodName=="remove"&&args.size()>=2){
            uint32_t k=args[1].asRef();
            if(k == 0){
                ENG().setPendingException(ENG().allocObject("java/lang/NullPointerException"));
                return true;
            }
            auto it=g_hashtable.find(self);
            if(it!=g_hashtable.end()){
                for(size_t i=0;i<it->second.size();i++){
                    if(keysEqual(it->second[i].first, k)){
                        uint32_t old=it->second[i].second; it->second.erase(it->second.begin()+i); outResult=JavaValue(old,true); return true;
                    }
                }
            }
            outResult=JavaValue(0,true); return true;
        }
        if(methodName=="containsKey"&&args.size()>=2){
            uint32_t k=args[1].asRef();
            if(k == 0){
                ENG().setPendingException(ENG().allocObject("java/lang/NullPointerException"));
                return true;
            }
            auto it=g_hashtable.find(self); bool f=false;
            if(it!=g_hashtable.end()){
                for(auto&p:it->second){
                    if(keysEqual(p.first, k)){ f=true; break; }
                }
            }
            outResult=JavaValue(f?1:0); return true;
        }
        if(methodName=="contains"&&args.size()>=2){
            uint32_t v=args[1].asRef();
            if(v == 0){
                ENG().setPendingException(ENG().allocObject("java/lang/NullPointerException"));
                return true;
            }
            auto it=g_hashtable.find(self); bool f=false;
            if(it!=g_hashtable.end()) for(auto&p:it->second) if(p.second==v) f=true;
            outResult=JavaValue(f?1:0); return true;
        }
        if(methodName=="keys"||methodName=="elements"){
            auto it=g_hashtable.find(self); uint32_t er=ENG().allocObject("java/util/Enumeration"); EnumData e;
            if(it!=g_hashtable.end()) for(auto&p:it->second) e.items.push_back(methodName=="keys"?p.first:p.second);
            g_enums[er]=e; outResult=JavaValue(er,true); return true;
        }
    }
    if(className=="java/util/Enumeration"||className=="java/util/Vector$1"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="hasMoreElements"){ auto it=g_enums.find(self); outResult=JavaValue(it!=g_enums.end()&&it->second.idx<it->second.items.size()?1:0); return true; }
        if(methodName=="nextElement"){
            auto it=g_enums.find(self);
            if(it!=g_enums.end()&&it->second.idx<it->second.items.size()){
                outResult=JavaValue(it->second.items[it->second.idx++],true);
            } else {
                ENG().setPendingException(ENG().allocObject("java/util/NoSuchElementException"));
                outResult=JavaValue(0,true);
            }
            return true;
        }
    }
    if(className=="java/util/Vector"){
        JavaObject* o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
        if(methodName=="elements"&&o){
            uint32_t arr=o->fields["elements"].asRef();
            JavaArray*a=ENG().getArray(arr);
            int cnt=o->fields["elementCount"].asInt();
            uint32_t er=ENG().allocObject("java/util/Enumeration");
            EnumData e;
            if(a)for(int i=0;i<cnt&&i<(int)a->refData.size();i++) e.items.push_back(a->refData[i]);
            g_enums[er]=std::move(e);
            outResult=JavaValue(er,true);
            return true;
        }
        if(methodName=="copyInto"&&args.size()>=2&&o){
            uint32_t arr=o->fields["elements"].asRef();
            JavaArray*a=ENG().getArray(arr);
            int cnt=o->fields["elementCount"].asInt();
            JavaArray*dst=ENG().getArray(args[1].asRef());
            if(a&&dst)for(int i=0;i<cnt&&i<(int)dst->refData.size();i++) dst->refData[i]=a->refData[i];
            return true;
        }
        if(methodName=="capacity"&&o){
            uint32_t arr=o->fields["elements"].asRef();
            JavaArray*a=ENG().getArray(arr);
            outResult=JavaValue(a?(int32_t)a->refData.size():0);
            return true;
        }
        if(methodName=="ensureCapacity"&&args.size()>=2&&o){
            uint32_t arr=o->fields["elements"].asRef();
            JavaArray*a=ENG().getArray(arr);
            int minCap=args[1].asInt();
            if(a&&minCap>(int)a->refData.size()) a->refData.resize(minCap,0);
            return true;
        }
        if(methodName=="trimToSize"&&o){
            uint32_t arr=o->fields["elements"].asRef();
            JavaArray*a=ENG().getArray(arr);
            int cnt=o->fields["elementCount"].asInt();
            if(a&&cnt<(int)a->refData.size()) a->refData.resize(std::max(0,cnt),0);
            return true;
        }
        if(methodName=="removeElement"&&args.size()>=2&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); uint32_t t=args[1].asRef(); bool f=false; if(a) for(int i=0;i<cnt;i++) if(a->refData[i]==t){ for(int j=i;j<cnt-1;j++)a->refData[j]=a->refData[j+1]; o->fields["elementCount"]=JavaValue(cnt-1); f=true; break; } outResult=JavaValue(f?1:0); return true; }
        if(methodName=="contains"&&args.size()>=2&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); uint32_t t=args[1].asRef(); bool f=false; if(a) for(int i=0;i<cnt;i++) if(a->refData[i]==t) f=true; outResult=JavaValue(f?1:0); return true; }
        if(methodName=="indexOf"&&args.size()>=2&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); uint32_t t=args[1].asRef(); int r=-1; if(a) for(int i=0;i<cnt;i++) if(a->refData[i]==t){r=i;break;} outResult=JavaValue(r); return true; }
        if((methodName=="firstElement"||methodName=="lastElement")&&o){
            uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt();
            if(!a || cnt <= 0){
                ENG().setPendingException(ENG().allocObject("java/util/NoSuchElementException"));
                outResult=JavaValue(0,true); return true;
            }
            uint32_t r=(methodName=="firstElement"?a->refData[0]:a->refData[cnt-1]);
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="insertElementAt"&&args.size()>=3&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); int idx=args[2].asInt(); if(a&&idx>=0&&idx<=cnt){ if(cnt>=(int)a->refData.size()) a->refData.resize(a->refData.size()*2+8,0); for(int i=cnt;i>idx;i--)a->refData[i]=a->refData[i-1]; a->refData[idx]=args[1].asRef(); o->fields["elementCount"]=JavaValue(cnt+1);} else { ENG().setPendingException(ENG().allocObject("java/lang/ArrayIndexOutOfBoundsException")); } return true; }
        if(methodName=="removeElementAt"&&args.size()>=2&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); int idx=args[1].asInt(); if(a&&idx>=0&&idx<cnt){ for(int i=idx;i<cnt-1;i++)a->refData[i]=a->refData[i+1]; o->fields["elementCount"]=JavaValue(cnt-1);} else { ENG().setPendingException(ENG().allocObject("java/lang/ArrayIndexOutOfBoundsException")); } return true; }
        if(methodName=="setElementAt"&&args.size()>=3&&o){ uint32_t arr=o->fields["elements"].asRef(); JavaArray*a=ENG().getArray(arr); int cnt=o->fields["elementCount"].asInt(); int idx=args[2].asInt(); if(a&&idx>=0&&idx<cnt) a->refData[idx]=args[1].asRef(); else { ENG().setPendingException(ENG().allocObject("java/lang/ArrayIndexOutOfBoundsException")); } return true; }
        if(methodName=="isEmpty"){ outResult=JavaValue(o&&o->fields["elementCount"].asInt()==0?1:0); return true; }
    }
    if(className=="java/util/Stack"){
        if(methodName=="<init>"){ uint32_t self=args[0].asRef(); JavaObject*o=ENG().getObject(self); if(o){uint32_t a=ENG().allocArray(0,16); o->fields["elements"]=JavaValue(a,true); o->fields["elementCount"]=JavaValue(0);} return true; }
        if(methodName=="push"&&args.size()>=2){ JavaObject*o=ENG().getObject(args[0].asRef()); if(o){int c=o->fields["elementCount"].asInt(); JavaArray*a=ENG().getArray(o->fields["elements"].asRef()); if(a){ if(c>=(int)a->refData.size())a->refData.resize(a->refData.size()*2+8,0); a->refData[c]=args[1].asRef(); o->fields["elementCount"]=JavaValue(c+1);} } outResult=JavaValue(args[1].asRef(),true); return true; }
        if(methodName=="pop"||methodName=="peek"){
            JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            int c = o ? o->fields["elementCount"].asInt() : 0;
            JavaArray*a = o ? ENG().getArray(o->fields["elements"].asRef()) : nullptr;
            if(!a || c <= 0){
                ENG().setPendingException(ENG().allocObject("java/util/EmptyStackException"));
                outResult=JavaValue(0,true); return true;
            }
            uint32_t r = a->refData[c-1];
            if(methodName=="pop") o->fields["elementCount"]=JavaValue(c-1);
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="empty"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(!o||o->fields["elementCount"].asInt()==0?1:0); return true; }
    }
    if(className=="java/util/Calendar"){
        if(methodName=="getInstance"){
            uint32_t r=ENG().allocObject("java/util/Calendar");
            JavaObject* o=ENG().getObject(r);
            auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(o) o->fields["time"]=JavaValue((int64_t)now);
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="<init>"){
            JavaObject* o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(o) o->fields["time"]=JavaValue((int64_t)now);
            return true;
        }
        if(methodName=="setTime"&&args.size()>=2){
            JavaObject* co=ENG().getObject(args[0].asRef());
            JavaObject* d=ENG().getObject(args[1].asRef());
            if(co&&d) co->fields["time"]=d->fields["time"];
            return true;
        }
        if(methodName=="setTimeInMillis"&&args.size()>=2){
            JavaObject* co=ENG().getObject(args[0].asRef());
            if(co) co->fields["time"]=args[1];
            return true;
        }
        if(methodName=="getTimeInMillis"||methodName=="getTime"){
            JavaObject* co=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            int64_t t = (co && co->fields.count("time")) ? co->fields["time"].asLong() : (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if(desc.find("()J")!=std::string::npos) outResult=JavaValue((int64_t)t);
            else {
                uint32_t d=ENG().allocObject("java/util/Date");
                JavaObject* o=ENG().getObject(d);
                if(o) o->fields["time"]=JavaValue((int64_t)t);
                outResult=JavaValue(d,true);
            }
            return true;
        }
        if(methodName=="get"&&args.size()>=2){
            JavaObject* co=ENG().getObject(args[0].asRef());
            int64_t tMs = (co && co->fields.count("time")) ? co->fields["time"].asLong() : (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::time_t tSec = (std::time_t)(tMs / 1000);
            std::tm* tm = std::localtime(&tSec);
            int f = args[1].asInt();
            int v = 0;
            if (tm) {
                switch(f){
                    case 0: v=1; break; // ERA (AD)
                    case 1: v=tm->tm_year+1900; break; // YEAR
                    case 2: v=tm->tm_mon; break; // MONTH
                    case 5: v=tm->tm_mday; break; // DATE
                    case 7: v=tm->tm_wday+1; break; // DAY_OF_WEEK
                    case 9: v=tm->tm_hour < 12 ? 0 : 1; break; // AM_PM
                    case 10: v=tm->tm_hour % 12; break; // HOUR
                    case 11: v=tm->tm_hour; break; // HOUR_OF_DAY
                    case 12: v=tm->tm_min; break; // MINUTE
                    case 13: v=tm->tm_sec; break; // SECOND
                    case 14: v=(int)(tMs % 1000); break; // MILLISECOND
                    case 15: v=0; break; // ZONE_OFFSET
                    default: v=0; break;
                }
            }
            outResult=JavaValue(v); return true;
        }
    }
    if(className=="java/util/Date"){
        if(methodName=="<init>"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); if(o){ int64_t t=args.size()>=2?args[1].asLong():std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); o->fields["time"]=JavaValue(t);} return true; }
        if(methodName=="getTime"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["time"]:JavaValue((int64_t)0); return true; }
    }
    if(className=="java/util/Random"){
        if(methodName=="nextLong"){ outResult=JavaValue((int64_t)(((int64_t)rand()<<32)|rand())); return true; }
        if(methodName=="nextFloat"){ outResult=JavaValue((float)rand()/(float)RAND_MAX); return true; }
        if(methodName=="nextDouble"){ outResult=JavaValue((double)rand()/(double)RAND_MAX); return true; }
        if(methodName=="setSeed") return true;
    }
    if(className=="java/util/Timer"||className=="java/util/TimerTask"){
        if(methodName=="<init>") return true;
        if((methodName=="schedule"||methodName=="scheduleAtFixedRate")&&args.size()>=2){
            uint32_t timerRef = args[0].asRef();
            uint32_t task = args[1].asRef();
            JavaObject* to = ENG().getObject(task);
            int64_t delayMs = args.size() >= 3 ? args[2].asLong() : 0;
            int64_t periodMs = args.size() >= 4 ? args[3].asLong() : 0;
            if(to && !to->className.empty()){
                auto cls = ENG().findOrLoadClass(to->className, ENG().getJarLoader());
                if(cls) {
                    if (periodMs > 0) {
                        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
                        {
                            std::lock_guard<std::mutex> lk(g_timerMutex);
                            g_taskCancelFlags[task] = cancelFlag;
                            g_timerTaskFlags[timerRef].push_back(cancelFlag);
                        }
                        JvmThread::spawnDetached([task, cls, delayMs, periodMs, cancelFlag]() {
                            auto& jvm = JvmBytecodeEngine::getInstance();
                            // Non-stalling cancellable initial delay
                            int64_t d = std::min<int64_t>(std::max<int64_t>(0, delayMs), 5000);
                            while (d > 0 && !jvm.isCancelled() && !cancelFlag->load()) {
                                int64_t step = std::min<int64_t>(50, d);
                                std::this_thread::sleep_for(std::chrono::milliseconds(step));
                                d -= step;
                            }
                            while (!jvm.isCancelled() && !cancelFlag->load()) {
                                jvm.executeMethod(cls, "run", "()V", { JavaValue(task, true) }, nullptr);
                                // Non-stalling cancellable periodic sleep
                                int64_t p = std::max<int64_t>(periodMs, 10);
                                while (p > 0 && !jvm.isCancelled() && !cancelFlag->load()) {
                                    int64_t step = std::min<int64_t>(50, p);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(step));
                                    p -= step;
                                }
                            }
                        });
                    } else {
                        JvmInterpreter::getInstance().registerRunnable(task, cls);
                    }
                }
            }
            return true;
        }
        if(methodName=="cancel"){
            uint32_t target = args.empty() ? 0 : args[0].asRef();
            if (target != 0) {
                std::lock_guard<std::mutex> lk(g_timerMutex);
                auto itTask = g_taskCancelFlags.find(target);
                if (itTask != g_taskCancelFlags.end() && itTask->second) {
                    itTask->second->store(true);
                }
                auto itTimer = g_timerTaskFlags.find(target);
                if (itTimer != g_timerTaskFlags.end()) {
                    for (auto& flag : itTimer->second) {
                        if (flag) flag->store(true);
                    }
                }
            }
            outResult = JavaValue(1);
            return true;
        }
    }
    // ============ java/io extended ============
    if(className=="java/io/ByteArrayOutputStream"||className=="java/io/OutputStream"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){ g_baos[self]={}; return true; }
        if(methodName=="write"&&args.size()>=2){
            if(args.size()==2){ g_baos[self].push_back((uint8_t)args[1].asInt()); }
            else if(args.size()>=4){ JavaArray*a=ENG().getArray(args[1].asRef()); int off=args[2].asInt(),len=args[3].asInt(); if(a) for(int i=0;i<len&&off+i<(int)a->byteData.size();i++) g_baos[self].push_back(a->byteData[off+i]); }
            else if(args.size()>=2){ JavaArray*a=ENG().getArray(args[1].asRef()); if(a&&!a->byteData.empty()) g_baos[self].insert(g_baos[self].end(),a->byteData.begin(),a->byteData.end()); }
            // Socket-backed: send through immediately
            JavaObject*so=ENG().getObject(self);
            if(so){ auto f=so->fields.find("sockFd"); if(f!=so->fields.end()&&f->second.asInt()>=0){ auto it=g_baos.find(self); if(it!=g_baos.end()&&!it->second.empty()){ tcpSendAll(f->second.asInt(), it->second.data(), it->second.size()); it->second.clear(); } } }
            return true;
        }
        if(methodName=="flush"){
            JavaObject*so=ENG().getObject(self);
            if(so){
                auto f=so->fields.find("sockFd"); if(f!=so->fields.end()&&f->second.asInt()>=0){ auto it=g_baos.find(self); if(it!=g_baos.end()&&!it->second.empty()){ tcpSendAll(f->second.asInt(), it->second.data(), it->second.size()); it->second.clear(); } }
                auto ff=so->fields.find("fileUrl"); if(ff!=so->fields.end()&&ff->second.asRef()!=0){
                    std::string u=ENG().getString(ff->second.asRef()); std::string p=u; if(p.rfind("file://",0)==0) p=p.substr(7);
                    if(p.rfind("/root/",0)==0) p=p.substr(6); else if(p.rfind("/SDCard/",0)==0) p=p.substr(8);
                    else if(p.rfind("root/",0)==0) p=p.substr(5); else if(p.rfind("SDCard/",0)==0) p=p.substr(7);
                    if(!p.empty()&&p[0]=='/') p.erase(0,1);
                    auto it=g_baos.find(self); if(it!=g_baos.end()&&!p.empty()){ FILE*fw=fopen(p.c_str(),"wb"); if(fw){ fwrite(it->second.data(),1,it->second.size(),fw); fclose(fw);} }
                }
            }
            return true;
        }
        if(methodName=="toByteArray"){ auto it=g_baos.find(self); int n=it==g_baos.end()?0:(int)it->second.size(); uint32_t r=ENG().allocArray(8,n); JavaArray*a=ENG().getArray(r); if(a&&it!=g_baos.end()) a->byteData=it->second; outResult=JavaValue(r,true); return true; }
        if(methodName=="size"){ auto it=g_baos.find(self); outResult=JavaValue(it==g_baos.end()?0:(int32_t)it->second.size()); return true; }
        if(methodName=="reset"){ g_baos[self].clear(); return true; }
        if(methodName=="close"){
            JavaObject*so=ENG().getObject(self);
            if(so){
                auto f=so->fields.find("sockFd"); if(f!=so->fields.end()&&f->second.asInt()>=0){ auto it=g_baos.find(self); if(it!=g_baos.end()&&!it->second.empty()){ tcpSendAll(f->second.asInt(), it->second.data(), it->second.size()); it->second.clear(); } }
                auto ff=so->fields.find("fileUrl"); if(ff!=so->fields.end()&&ff->second.asRef()!=0){
                    std::string u=ENG().getString(ff->second.asRef()); std::string p=u; if(p.rfind("file://",0)==0) p=p.substr(7);
                    if(p.rfind("/root/",0)==0) p=p.substr(6); else if(p.rfind("/SDCard/",0)==0) p=p.substr(8);
                    else if(p.rfind("root/",0)==0) p=p.substr(5); else if(p.rfind("SDCard/",0)==0) p=p.substr(7);
                    if(!p.empty()&&p[0]=='/') p.erase(0,1);
                    auto it=g_baos.find(self); if(it!=g_baos.end()&&!p.empty()){ FILE*fw=fopen(p.c_str(),"wb"); if(fw){ fwrite(it->second.data(),1,it->second.size(),fw); fclose(fw);} it->second.clear(); }
                }
            }
            return true;
        }
        if(methodName=="toString"){ auto it=g_baos.find(self); std::string s=it==g_baos.end()?"":std::string((char*)it->second.data(),it->second.size()); outResult=JavaValue(ENG().createString(s),true); return true; }
    }
    if(className=="java/io/DataOutputStream"||className=="java/io/DataInputStream") {
        // DataOutputStream writes into BAOS-like buffer keyed by stream ref
        if(className=="java/io/DataOutputStream"){
            uint32_t self=args.empty()?0:args[0].asRef();
            if(methodName=="<init>"){
                g_baos[self]={};
                JavaObject* so = ENG().getObject(self);
                if(so && args.size() >= 2 && args[1].type == JavaValue::OBJ_REF){
                    so->fields["out"] = args[1];
                    JavaObject* inner = ENG().getObject(args[1].asRef());
                    if(inner){
                        auto sf = inner->fields.find("sockFd");
                        if(sf != inner->fields.end()) so->fields["sockFd"] = sf->second;
                    }
                }
                return true;
            }
            auto getTargetBuf = [self]() -> std::vector<uint8_t>& {
                JavaObject* so = ENG().getObject(self);
                if(so){
                    auto oit = so->fields.find("out");
                    if(oit != so->fields.end() && oit->second.asRef() != 0){
                        return g_baos[oit->second.asRef()];
                    }
                }
                return g_baos[self];
            };
            if(methodName=="writeInt"&&args.size()>=2){ int32_t v=args[1].asInt(); auto&b=getTargetBuf(); b.push_back((v>>24)&0xFF); b.push_back((v>>16)&0xFF); b.push_back((v>>8)&0xFF); b.push_back(v&0xFF); return true; }
            if(methodName=="writeShort"&&args.size()>=2){ int v=args[1].asInt(); auto&b=getTargetBuf(); b.push_back((v>>8)&0xFF); b.push_back(v&0xFF); return true; }
            if(methodName=="writeChar"&&args.size()>=2){ int v=args[1].asInt(); auto&b=getTargetBuf(); b.push_back((v>>8)&0xFF); b.push_back(v&0xFF); return true; }
            if(methodName=="writeBoolean"&&args.size()>=2){ getTargetBuf().push_back((uint8_t)(args[1].asInt() ? 1 : 0)); return true; }
            if(methodName=="writeByte"&&args.size()>=2){ getTargetBuf().push_back((uint8_t)args[1].asInt()); return true; }
            if(methodName=="writeLong"&&args.size()>=2){
                int64_t v=args[1].asLong(); auto&b=getTargetBuf();
                for(int i=56;i>=0;i-=8) b.push_back((uint8_t)((v>>i)&0xFF));
                return true;
            }
            if(methodName=="writeFloat"&&args.size()>=2){
                float f=args[1].asFloat(); uint32_t u=0; std::memcpy(&u,&f,4); auto&b=getTargetBuf();
                b.push_back((u>>24)&0xFF); b.push_back((u>>16)&0xFF); b.push_back((u>>8)&0xFF); b.push_back(u&0xFF);
                return true;
            }
            if(methodName=="writeDouble"&&args.size()>=2){
                double d=args[1].asDouble(); uint64_t u=0; std::memcpy(&u,&d,8); auto&b=getTargetBuf();
                for(int i=56;i>=0;i-=8) b.push_back((uint8_t)((u>>i)&0xFF));
                return true;
            }
            if(methodName=="writeUTF"&&args.size()>=2){ std::string s=ENG().getString(args[1].asRef()); auto&b=getTargetBuf(); uint16_t l=(uint16_t)s.size(); b.push_back((l>>8)&0xFF); b.push_back(l&0xFF); for(char c:s)b.push_back(c); return true; }
            if(methodName=="writeChars"&&args.size()>=2){ std::string s=ENG().getString(args[1].asRef()); auto&b=getTargetBuf(); for(char c:s){ b.push_back(0); b.push_back((uint8_t)c); } return true; }
            if(methodName=="writeBytes"&&args.size()>=2){ std::string s=ENG().getString(args[1].asRef()); auto&b=getTargetBuf(); for(char c:s) b.push_back((uint8_t)c); return true; }
            if(methodName=="write"&&args.size()>=2){
                auto& b = getTargetBuf();
                if(args.size()==2&&args[1].type!=JavaValue::OBJ_REF){ b.push_back((uint8_t)args[1].asInt()); }
                else if(args.size()>=4){ JavaArray*a=ENG().getArray(args[1].asRef()); int off=args[2].asInt(),len=args[3].asInt(); if(a) for(int i=0;i<len&&off+i<(int)a->byteData.size();i++) b.push_back(a->byteData[off+i]); }
                else if(args.size()>=2){ JavaArray*a=ENG().getArray(args[1].asRef()); if(a&&!a->byteData.empty()) b.insert(b.end(),a->byteData.begin(),a->byteData.end()); }
                return true;
            }
            if(methodName=="flush"||methodName=="close"){
                JavaObject* so = ENG().getObject(self);
                uint32_t tBuf = self;
                if(so){
                    auto oit = so->fields.find("out");
                    if(oit != so->fields.end() && oit->second.asRef() != 0) tBuf = oit->second.asRef();
                    auto sf = so->fields.find("sockFd");
                    if(sf != so->fields.end() && sf->second.asInt() >= 0){
                        auto it = g_baos.find(tBuf);
                        if(it != g_baos.end() && !it->second.empty()){
#if !defined(_WIN32)&&!defined(_WIN64)
                            tcpSendAll(sf->second.asInt(), it->second.data(), it->second.size());
#endif
                            it->second.clear();
                        }
                    }
                }
                return true;
            }
            if(methodName=="size"){ auto&b=getTargetBuf(); outResult=JavaValue((int32_t)b.size()); return true; }
            if(methodName=="toByteArray"){ auto&b=getTargetBuf(); uint32_t r=ENG().allocArray(8,(int)b.size()); JavaArray*a=ENG().getArray(r); if(a)a->byteData=b; outResult=JavaValue(r,true); return true; }
        }
    }
    if(className=="java/io/OutputStream"||className=="java/io/PrintStream"||className=="java/io/Writer"||className=="java/io/OutputStreamWriter"){
        if(methodName=="<init>"||methodName=="print"||methodName=="println"||methodName=="write"||methodName=="flush"||methodName=="close") return true;
    }
    if(className=="java/io/InputStreamReader"||className=="java/io/ByteArrayInputStream") {
        if(methodName=="<init>"||methodName=="close") return true;
    }
    // ============ MIDlet ============
    if(className=="javax/microedition/midlet/MIDlet"||className.find("MIDlet")!=std::string::npos){
        if(methodName=="getAppProperty"&&args.size()>=2){ std::string k=ENG().getString(args[1].asRef()); std::string v; JarLoader* jar=ENG().getJarLoader(); if(jar){ auto m=jar->parseManifest(); auto it=m.find(k); if(it!=m.end()) v=it->second;
                // JAD sibling override: <jarBase>.jad next to .jar
                if(v.empty()&&!jar->getFilePath().empty()){
                    std::string jp=jar->getFilePath(); auto dot=jp.rfind('.'); std::string jadP=(dot==std::string::npos?jp:jp.substr(0,dot))+".jad";
                    FILE* f=fopen(jadP.c_str(),"r"); if(f){ char line[1024]; while(fgets(line,sizeof(line),f)){ std::string s=trimStr(line); if(s.empty()||s[0]=='#') continue; auto c=s.find(':'); if(c==std::string::npos) continue; std::string kk=trimStr(s.substr(0,c)), vv=trimStr(s.substr(c+1)); if(kk==k){ v=vv; break; } } fclose(f); }
                }
            }
            if(v.empty()) outResult=JavaValue(0,true); else outResult=JavaValue(ENG().createString(v),true); return true; }
        if(methodName=="checkPermission"){ outResult=JavaValue(1); return true; }
        if(methodName=="platformRequest"){ outResult=JavaValue(0); return true; }
        if(methodName=="notifyPaused"||methodName=="notifyDestroyed"||methodName=="resumeRequest"||methodName=="<init>") return true;
    }
    // ============ LCDUI high-level ============
    auto ensureScreen=[&](uint32_t r)->ScreenData&{ if(g_screens.find(r)==g_screens.end()) g_screens[r]=ScreenData(); return g_screens[r]; };
    if(className=="javax/microedition/lcdui/Displayable"||className=="javax/microedition/lcdui/Screen"||className=="javax/microedition/lcdui/Canvas"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="addCommand"&&args.size()>=2){ ensureScreen(self).commands.push_back(args[1].asRef()); return true; }
        if(methodName=="removeCommand"&&args.size()>=2){ auto&v=ensureScreen(self).commands; v.erase(std::remove(v.begin(),v.end(),args[1].asRef()),v.end()); return true; }
        if(methodName=="setCommandListener"&&args.size()>=2){ ensureScreen(self).listener=args[1].asRef(); return true; }
        if(methodName=="setTicker"||methodName=="setTitle"){ if(methodName=="setTitle"&&args.size()>=2) ensureScreen(self).title=ENG().getString(args[1].asRef()); if(methodName=="setTicker"&&args.size()>=2) ensureScreen(self).ticker=args[1].asRef(); return true; }
        if(methodName=="getTitle"){ outResult=JavaValue(ENG().createString(ensureScreen(self).title),true); return true; }
        if(methodName=="getTicker"){ outResult=JavaValue(ensureScreen(self).ticker,true); return true; }
        if(methodName=="isShown"){ outResult=JavaValue(1); return true; }
        if(methodName=="_getTitle"||methodName=="getWidth"||methodName=="getHeight"){
            if(display){ outResult=JavaValue(methodName=="getHeight"?display->getHeight():display->getWidth()); return true; }
        }
    }
    if(className=="javax/microedition/lcdui/Form"||className=="javax/microedition/lcdui/List"||className=="javax/microedition/lcdui/TextBox"||className=="javax/microedition/lcdui/Alert"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){
            ScreenData& sd=ensureScreen(self);
            if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF) sd.title=ENG().getString(args[1].asRef());
            // List(TEXT/IMPLICIT, string[], image[]) etc: collect items
            if(className=="javax/microedition/lcdui/List"){
                // args: this, title, listType [, string[], image[]]
                for(size_t i=2;i<args.size();i++) if(args[i].type==JavaValue::OBJ_REF){ JavaArray*a=ENG().getArray(args[i].asRef()); if(a&&!a->refData.empty()){ for(uint32_t r:a->refData){ if(r==0) continue; JavaObject*o=ENG().getObject(r); sd.items.push_back(o?o->stringVal:""); } break; } }
            }
            if(className=="javax/microedition/lcdui/TextBox"&&args.size()>=3) sd.text=ENG().getString(args[2].asRef());
            if(className=="javax/microedition/lcdui/Alert"&&args.size()>=3) sd.text=ENG().getString(args[2].asRef());
            if(display) renderScreen(self, display);
            return true;
        }
        if(methodName=="append"&&args.size()>=2){
            uint32_t argRef = args[1].asRef();
            std::string s = ENG().getString(argRef);
            if (s.empty() && argRef != 0) {
                JavaObject* itemObj = ENG().getObject(argRef);
                if (itemObj) {
                    if (!itemObj->stringVal.empty()) s = itemObj->stringVal;
                    else {
                        auto it = g_screens.find(argRef);
                        if (it != g_screens.end()) {
                            s = it->second.text.empty() ? it->second.title : (it->second.title.empty() ? it->second.text : (it->second.title + ": " + it->second.text));
                        }
                    }
                }
            }
            ensureScreen(self).items.push_back(s);
            if(display) renderScreen(self,display);
            outResult=JavaValue((int32_t)(ensureScreen(self).items.size()-1));
            return true;
        }
        if(methodName=="insert"&&args.size()>=3){ std::string s=ENG().getString(args[2].asRef()); int idx=args[1].asInt(); auto&it=ensureScreen(self).items; if(idx<0)idx=0; if(idx>(int)it.size())idx=(int)it.size(); it.insert(it.begin()+idx,s); if(display)renderScreen(self,display); return true; }
        if(methodName=="delete"&&args.size()>=2){ int idx=args[1].asInt(); auto&it=ensureScreen(self).items; if(idx>=0&&idx<(int)it.size()) it.erase(it.begin()+idx); if(display)renderScreen(self,display); return true; }
        if(methodName=="deleteAll"){ ensureScreen(self).items.clear(); if(display)renderScreen(self,display); return true; }
        if(methodName=="size"){ outResult=JavaValue((int32_t)ensureScreen(self).items.size()); return true; }
        if(methodName=="getString"&&args.size()>=2){ int idx=args[1].asInt(); auto&it=ensureScreen(self).items; std::string s=(idx>=0&&idx<(int)it.size())?it[idx]:ensureScreen(self).text; outResult=JavaValue(ENG().createString(s),true); return true; }
        if(methodName=="setString"&&args.size()>=3){ int idx=args[1].asInt(); std::string s=ENG().getString(args[2].asRef()); auto&sd=ensureScreen(self); if(idx>=0&&idx<(int)sd.items.size()) sd.items[idx]=s; else sd.text=s; if(display)renderScreen(self,display); return true; }
        if(methodName=="getSelectedIndex"||methodName=="getSelectedFlags"){ outResult=JavaValue(ensureScreen(self).selected); return true; }
        if(methodName=="setSelectedIndex"&&args.size()>=3){ ensureScreen(self).selected=args[1].asInt(); if(display)renderScreen(self,display); return true; }
        if(methodName=="set"||methodName=="setTitle"||methodName=="setTicker"||methodName=="setTimeout"||methodName=="setType"||methodName=="setString"||methodName=="setImage"){ if(methodName=="setTitle"&&args.size()>=2) ensureScreen(self).title=ENG().getString(args[1].asRef()); if(display)renderScreen(self,display); return true; }
        if(methodName=="getTitle"){ outResult=JavaValue(ENG().createString(ensureScreen(self).title),true); return true; }
        if(methodName=="setCurrent"||methodName=="showNotify"||methodName=="hideNotify") return true;
        if(methodName=="setCommandListener"&&args.size()>=2){ ensureScreen(self).listener=args[1].asRef(); return true; }
        if(methodName=="addCommand"&&args.size()>=2){ ensureScreen(self).commands.push_back(args[1].asRef()); return true; }
        if(methodName=="removeCommand"&&args.size()>=2){ auto&v=ensureScreen(self).commands; v.erase(std::remove(v.begin(),v.end(),args[1].asRef()),v.end()); return true; }
        if(methodName=="getText"||methodName=="getString"){ outResult=JavaValue(ENG().createString(ensureScreen(self).text),true); return true; }
        if(methodName=="setText"&&args.size()>=2){ ensureScreen(self).text=ENG().getString(args[1].asRef()); if(display)renderScreen(self,display); return true; }
        if(methodName=="getCaretPosition"){ outResult=JavaValue(ensureScreen(self).caret); return true; }
        if(methodName=="getMaxSize"){ outResult=JavaValue(1024); return true; }
    }
    if(className=="javax/microedition/lcdui/TextField"||className=="javax/microedition/lcdui/ChoiceGroup"||className=="javax/microedition/lcdui/StringItem"||className=="javax/microedition/lcdui/ImageItem"||className=="javax/microedition/lcdui/DateField"||className=="javax/microedition/lcdui/Gauge"||className=="javax/microedition/lcdui/Spacer"||className=="javax/microedition/lcdui/CustomItem"||className=="javax/microedition/lcdui/Item"){
        uint32_t self=args.empty()?0:args[0].asRef();
        JavaObject* o = self ? ENG().getObject(self) : nullptr;
        if(methodName=="<init>"){
            ensureScreen(self);
            if(className=="javax/microedition/lcdui/TextField"){
                if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF) ensureScreen(self).title=ENG().getString(args[1].asRef());
                if(args.size()>=3&&args[2].type==JavaValue::OBJ_REF) ensureScreen(self).text=ENG().getString(args[2].asRef());
                int maxSz = args.size()>=4 ? args[3].asInt() : 1024;
                int constr = args.size()>=5 ? args[4].asInt() : 0;
                if(o){ o->fields["maxSize"]=JavaValue(maxSz); o->fields["constraints"]=JavaValue(constr); }
            } else if(className=="javax/microedition/lcdui/ChoiceGroup"){
                if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF) ensureScreen(self).title=ENG().getString(args[1].asRef());
                if(args.size()>=3) { if(o) o->fields["choiceType"]=args[2]; }
                if(args.size()>=4&&args[3].type==JavaValue::OBJ_REF){
                    JavaArray* strArr = ENG().getArray(args[3].asRef());
                    if(strArr) {
                        for(uint32_t r : strArr->refData){
                            if(r) ensureScreen(self).items.push_back(ENG().getString(r));
                        }
                    }
                }
            } else {
                if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF) ensureScreen(self).title=ENG().getString(args[1].asRef());
                if(args.size()>=3&&args[2].type==JavaValue::OBJ_REF) ensureScreen(self).text=ENG().getString(args[2].asRef());
            }
            return true;
        }
        if(methodName=="getLabel"){ outResult=JavaValue(ENG().createString(ensureScreen(self).title),true); return true; }
        if(methodName=="setLabel"&&args.size()>=2){ ensureScreen(self).title=ENG().getString(args[1].asRef()); return true; }
        if(methodName=="getText"||methodName=="getString"){ outResult=JavaValue(ENG().createString(ensureScreen(self).text),true); return true; }
        if(methodName=="setText"||methodName=="setString"){
            if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF) ensureScreen(self).text=ENG().getString(args[1].asRef());
            return true;
        }
        if(methodName=="size"){
            if(className=="javax/microedition/lcdui/ChoiceGroup") outResult=JavaValue((int32_t)ensureScreen(self).items.size());
            else outResult=JavaValue((int32_t)ensureScreen(self).text.length());
            return true;
        }
        if(methodName=="getMaxSize"){
            int ms = (o && o->fields.count("maxSize")) ? o->fields["maxSize"].asInt() : 1024;
            outResult=JavaValue(ms); return true;
        }
        if(methodName=="getConstraints"){
            int c = (o && o->fields.count("constraints")) ? o->fields["constraints"].asInt() : 0;
            outResult=JavaValue(c); return true;
        }
        if(methodName=="getSelectedIndex"){ outResult=JavaValue(ensureScreen(self).selected); return true; }
        if(methodName=="setSelectedIndex"&&args.size()>=2){
            if(args.size()>=3){ if(args[2].asInt()!=0) ensureScreen(self).selected=args[1].asInt(); }
            else { ensureScreen(self).selected=args[1].asInt(); }
            return true;
        }
        if(methodName=="append"&&args.size()>=2){
            std::string s=ENG().getString(args[1].asRef());
            ensureScreen(self).items.push_back(s);
            outResult=JavaValue((int32_t)(ensureScreen(self).items.size()-1));
            return true;
        }
        if(methodName=="insert"&&args.size()>=3){
            int idx=args[1].asInt(); std::string s=ENG().getString(args[2].asRef());
            auto& it=ensureScreen(self).items;
            if(idx<0) idx=0; if(idx>(int)it.size()) idx=(int)it.size();
            it.insert(it.begin()+idx, s);
            return true;
        }
        if(methodName=="delete"&&args.size()>=2){
            int idx=args[1].asInt(); auto& it=ensureScreen(self).items;
            if(idx>=0&&idx<(int)it.size()) it.erase(it.begin()+idx);
            return true;
        }
        if(methodName=="deleteAll"){ ensureScreen(self).items.clear(); return true; }
        if(methodName=="set"&&args.size()>=3){
            int idx=args[1].asInt(); std::string s=ENG().getString(args[2].asRef());
            auto& it=ensureScreen(self).items;
            if(idx>=0&&idx<(int)it.size()) it[idx]=s;
            return true;
        }
        if(methodName=="getValue"||methodName=="getDate"){ outResult=JavaValue(0); return true; }
        if(methodName=="setValue"||methodName=="setDate"||methodName=="addCommand"||methodName=="setLayout"||methodName=="setPreferredSize") return true;
    }
    if(className=="javax/microedition/lcdui/Command"||className=="javax/microedition/lcdui/AlertType"||className=="javax/microedition/lcdui/Ticker"){
        uint32_t self=args.empty()?0:args[0].asRef();
        JavaObject* o = self ? ENG().getObject(self) : nullptr;
        if(methodName=="<init>"){
            if(className=="javax/microedition/lcdui/Ticker"&&args.size()>=2) ensureScreen(self).text=ENG().getString(args[1].asRef());
            else if(className=="javax/microedition/lcdui/Command"&&o){
                if(args.size()>=4 && desc.find("(Ljava/lang/String;II)")!=std::string::npos){
                    o->fields["shortLabel"] = args[1];
                    o->fields["longLabel"] = args[1];
                    o->fields["commandType"] = args[2];
                    o->fields["priority"] = args[3];
                } else if(args.size()>=5 && desc.find("(Ljava/lang/String;Ljava/lang/String;II)")!=std::string::npos){
                    o->fields["shortLabel"] = args[1];
                    o->fields["longLabel"] = args[2];
                    o->fields["commandType"] = args[3];
                    o->fields["priority"] = args[4];
                } else if(args.size()>=2){
                    o->fields["shortLabel"] = args[1];
                    o->fields["longLabel"] = args[1];
                }
            }
            return true;
        }
        if(methodName=="getLabel"){
            std::string lbl = (o && o->fields.count("shortLabel")) ? ENG().getString(o->fields["shortLabel"].asRef()) : "OK";
            outResult=JavaValue(ENG().createString(lbl),true); return true;
        }
        if(methodName=="getLongLabel"){
            std::string lbl = (o && o->fields.count("longLabel")) ? ENG().getString(o->fields["longLabel"].asRef()) : ((o && o->fields.count("shortLabel")) ? ENG().getString(o->fields["shortLabel"].asRef()) : "OK");
            outResult=JavaValue(ENG().createString(lbl),true); return true;
        }
        if(methodName=="getString"){ outResult=JavaValue(ENG().createString(ensureScreen(self).text),true); return true; }
        if(methodName=="getCommandType"){
            int ct = (o && o->fields.count("commandType")) ? o->fields["commandType"].asInt() : 1;
            outResult=JavaValue(ct); return true;
        }
        if(methodName=="getPriority"){
            int pri = (o && o->fields.count("priority")) ? o->fields["priority"].asInt() : 1;
            outResult=JavaValue(pri); return true;
        }
    }
    // Display.setCurrent for high-level screens: render them + track for CommandListener
    if(className=="javax/microedition/lcdui/Display"){
        if(methodName=="setCurrent"&&args.size()>=2){
            uint32_t nxt=args[1].asRef(); JavaObject*o=ENG().getObject(nxt);
            g_currentScreen=nxt;
            if(display && o&&(o->className=="javax/microedition/lcdui/Form"||o->className=="javax/microedition/lcdui/List"||o->className=="javax/microedition/lcdui/TextBox"||o->className=="javax/microedition/lcdui/Alert")){
                if (o->className == "javax/microedition/lcdui/TextBox") {
                    if (hasNative((const void*)native_prompt_text_input)) {
                        char buf[512] = {0};
                        auto &sd = ensureScreen(nxt);
                        if (native_prompt_text_input(sd.title.c_str(), sd.text.c_str(), buf, sizeof(buf))) {
                            sd.text = buf;
                        }
                    }
                }
                renderScreen(nxt, display);
                return true;
            }
            if (o && !o->className.empty()) {
                auto cls = ENG().findOrLoadClass(o->className, ENG().getJarLoader());
                JvmInterpreter::getInstance().setCurrentCanvas(nxt, cls);
            }
            return true;
        }
        if(methodName=="getDisplay"){ outResult=JavaValue(ENG().allocObject("javax/microedition/lcdui/Display"),true); return true; }
    }
    // ============ lcdui/game ============
    if(className=="javax/microedition/lcdui/game/GameCanvas"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>") return true;
        if(methodName=="getGraphics"){ outResult=JavaValue(ENG().allocObject("javax/microedition/lcdui/Graphics"),true); return true; }
        if(methodName=="flushGraphics"||methodName=="repaint"||methodName=="serviceRepaints"){
            // Continuous 60 FPS refresh loop handles frame presentation without recursive stack exhaustion
            return true;
        }
        if(methodName=="getKeyStates"){ outResult=JavaValue(JvmInterpreter::getInstance().getKeyStates()); return true; }
        if(methodName=="getWidth"){ outResult=JavaValue(display?display->getWidth():240); return true; }
        if(methodName=="getHeight"){ outResult=JavaValue(display?display->getHeight():320); return true; }
    }
    if(className=="javax/microedition/lcdui/game/Sprite"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){
            // (LImage;)V , (LImage;II)V , (LSprite;)V — pixels synced from offscreen if drawn
            NativeImage* src=nullptr; const uint32_t* px=nullptr; int fw=0,fh=0;
            if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF){ src=imgOf(args[1].asRef()); px=imgPx(args[1].asRef()); }
            if(args.size()>=4){ fw=args[2].asInt(); fh=args[3].asInt(); }
            if(src&&px){
                if(fw<=0) fw=src->width; if(fh<=0) fh=src->height;
                g_sprites[self]=std::make_shared<Sprite>(std::vector<uint32_t>(px,px+src->width*src->height),src->width,src->height,fw,fh);
            } else {
                std::vector<uint32_t> px(16*16,0xFFFF0000); g_sprites[self]=std::make_shared<Sprite>(px,16,16,16,16);
            }
            return true;
        }
        auto it=g_sprites.find(self); std::shared_ptr<Sprite> sp=it==g_sprites.end()?nullptr:it->second;
        if(!sp) return true;
        if(methodName=="setFrame"&&args.size()>=2){ sp->setFrame(args[1].asInt()); return true; }
        if(methodName=="getFrame"){ outResult=JavaValue(sp->getFrame()); return true; }
        if(methodName=="getRawFrameCount"){ outResult=JavaValue(sp->getRawFrameCount()); return true; }
        if(methodName=="nextFrame"){ sp->nextFrame(); return true; }
        if(methodName=="prevFrame"){ sp->prevFrame(); return true; }
        if(methodName=="setFrameSequence"&&args.size()>=2){ JavaArray*a=ENG().getArray(args[1].asRef()); if(a){ std::vector<int> seq(a->intData.begin(),a->intData.end()); sp->setFrameSequence(seq);} return true; }
        if(methodName=="setImage"&&args.size()>=4){ NativeImage*ni=imgOf(args[1].asRef()); const uint32_t*px=imgPx(args[1].asRef()); int fw=args[2].asInt(),fh=args[3].asInt(); if(ni&&px){ g_sprites[self]=std::make_shared<Sprite>(std::vector<uint32_t>(px,px+ni->width*ni->height),ni->width,ni->height,fw,fh);} return true; }
        if(methodName=="setTransform"&&args.size()>=2){ sp->setTransform((SpriteTransform)args[1].asInt()); return true; }
        if(methodName=="defineReferencePixel"&&args.size()>=3){ sp->defineReferencePixel(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="setRefPixelPosition"&&args.size()>=3){ sp->setRefPixelPosition(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="getRefPixelX"){ outResult=JavaValue(sp->getRefPixelX()); return true; }
        if(methodName=="getRefPixelY"){ outResult=JavaValue(sp->getRefPixelY()); return true; }
        if(methodName=="setPosition"&&args.size()>=3){ sp->setPosition(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="move"&&args.size()>=3){ sp->move(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="setVisible"&&args.size()>=2){ sp->setVisible(args[1].asInt()!=0); return true; }
        if(methodName=="getX"){ outResult=JavaValue(sp->getX()); return true; }
        if(methodName=="getY"){ outResult=JavaValue(sp->getY()); return true; }
        if(methodName=="getWidth"){ outResult=JavaValue(sp->getWidth()); return true; }
        if(methodName=="getHeight"){ outResult=JavaValue(sp->getHeight()); return true; }
        if(methodName=="isVisible"){ outResult=JavaValue(sp->isVisible()?1:0); return true; }
        if(methodName=="defineCollisionRectangle"&&args.size()>=5){ sp->defineCollisionRectangle(args[1].asInt(),args[2].asInt(),args[3].asInt(),args[4].asInt()); return true; }
        if(methodName=="collidesWith"){
            bool r=false;
            if(args.size()>=4&&args[1].type==JavaValue::INT){
                r=sp->collidesWith(args[1].asInt(),args[2].asInt(),args[3].asInt(),args.size()>=5?args[4].asInt():0);
            } else if(args.size()>=2){
                uint32_t targetRef = args[1].asRef();
                auto jt=g_sprites.find(targetRef);
                if(jt!=g_sprites.end()&&jt->second){
                    r=sp->collidesWith(*jt->second, args.size()>=3?args[2].asInt()!=0:false);
                } else {
                    auto kt=g_tiled.find(targetRef);
                    if(kt!=g_tiled.end()&&kt->second){
                        r=sp->collidesWith(*kt->second, args.size()>=3?args[2].asInt()!=0:false);
                    } else if(args.size()>=5){
                        NativeImage* ni=imgOf(targetRef);
                        const uint32_t* px=imgPx(targetRef);
                        if(ni&&px){
                            std::vector<uint32_t> pxVec(px, px + ni->width * ni->height);
                            r=sp->collidesWith(pxVec, ni->width, ni->height, args[2].asInt(), args[3].asInt(), args[4].asInt()!=0);
                        }
                    }
                }
            }
            outResult=JavaValue(r?1:0); return true;
        }
        if(methodName=="paint"&&args.size()>=2&&display){
            LcduiDisplay* tgt = (args[1].type==JavaValue::OBJ_REF) ? ENG().resolveGraphics(args[1].asRef(), display) : display;
            if(tgt) sp->paint(tgt); return true;
        }
    }
    if(className=="javax/microedition/lcdui/game/TiledLayer"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"&&args.size()>=6){
            int cols=args[1].asInt(),rows=args[2].asInt(); NativeImage*ni=imgOf(args[3].asRef()); const uint32_t*px0=imgPx(args[3].asRef()); int tw=args[4].asInt(),th=args[5].asInt();
            std::vector<uint32_t> px; int iw=16,ih=16; if(ni&&px0){px.assign(px0,px0+ni->width*ni->height);iw=ni->width;ih=ni->height;}
            g_tiled[self]=std::make_shared<TiledLayer>(cols,rows,px,iw,ih,tw>0?tw:16,th>0?th:16);
            return true;
        }
        auto it=g_tiled.find(self); if(it==g_tiled.end()) return true; auto tl=it->second;
        if(methodName=="setCell"&&args.size()>=4){ tl->setCell(args[1].asInt(),args[2].asInt(),args[3].asInt()); return true; }
        if(methodName=="getCell"&&args.size()>=3){ outResult=JavaValue(tl->getCell(args[1].asInt(),args[2].asInt())); return true; }
        if(methodName=="fillCells"&&args.size()>=6){ tl->fillCells(args[1].asInt(),args[2].asInt(),args[3].asInt(),args[4].asInt(),args[5].asInt()); return true; }
        if(methodName=="createAnimatedTile"&&args.size()>=2){ outResult=JavaValue(tl->createAnimatedTile(args[1].asInt())); return true; }
        if(methodName=="setAnimatedTile"&&args.size()>=3){ tl->setAnimatedTile(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="getAnimatedTile"&&args.size()>=2){ outResult=JavaValue(tl->getAnimatedTile(args[1].asInt())); return true; }
        if(methodName=="setStaticTileSet"&&args.size()>=4){
            NativeImage*ni=imgOf(args[1].asRef());
            const uint32_t*px=imgPx(args[1].asRef());
            int tw=args[2].asInt(), th=args[3].asInt();
            if(ni&&px&&tw>0&&th>0){
                std::vector<uint32_t> pxVec(px, px + ni->width * ni->height);
                tl->setStaticTileSet(pxVec, ni->width, ni->height, tw, th);
            }
            return true;
        }
        if(methodName=="setPosition"&&args.size()>=3){ tl->setPosition(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="move"&&args.size()>=3){ tl->move(args[1].asInt(),args[2].asInt()); return true; }
        if(methodName=="setVisible"&&args.size()>=2){ tl->setVisible(args[1].asInt()!=0); return true; }
        if(methodName=="getX"){ outResult=JavaValue(tl->getX()); return true; }
        if(methodName=="getY"){ outResult=JavaValue(tl->getY()); return true; }
        if(methodName=="getWidth"){ outResult=JavaValue(tl->getWidth()); return true; }
        if(methodName=="getHeight"){ outResult=JavaValue(tl->getHeight()); return true; }
        if(methodName=="getColumns"){ outResult=JavaValue(tl->getColumns()); return true; }
        if(methodName=="getRows"){ outResult=JavaValue(tl->getRows()); return true; }
        if(methodName=="getCellWidth"){ outResult=JavaValue(tl->getCellWidth()); return true; }
        if(methodName=="getCellHeight"){ outResult=JavaValue(tl->getCellHeight()); return true; }
        if(methodName=="paint"&&args.size()>=2&&display){
            LcduiDisplay* tgt = (args[1].type==JavaValue::OBJ_REF) ? ENG().resolveGraphics(args[1].asRef(), display) : display;
            if(tgt) tl->paint(tgt); return true;
        }
    }
    if(className=="javax/microedition/lcdui/game/LayerManager"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){ g_layerMgr[self]=std::make_shared<LayerManager>(); return true; }
        auto it=g_layerMgr.find(self); if(it==g_layerMgr.end()) return true; auto lm=it->second;
        if((methodName=="append"||methodName=="insert"||methodName=="remove")&&args.size()>=2){
            uint32_t lr=args[1].asRef();
            std::shared_ptr<Layer> lay=nullptr;
            auto si=g_sprites.find(lr); if(si!=g_sprites.end()) lay=si->second;
            else { auto ti=g_tiled.find(lr); if(ti!=g_tiled.end()) lay=ti->second; }
            if(lay){ if(methodName=="append") lm->append(lay); else if(methodName=="remove") lm->remove(lay); else lm->insert(lay, args.size()>=3?args[2].asInt():0); }
            return true;
        }
        if(methodName=="setViewWindow"&&args.size()>=5){ lm->setViewWindow(args[1].asInt(),args[2].asInt(),args[3].asInt(),args[4].asInt()); return true; }
        if(methodName=="paint"&&args.size()>=4&&display){
            LcduiDisplay* tgt = (args[1].type==JavaValue::OBJ_REF) ? ENG().resolveGraphics(args[1].asRef(), display) : display;
            if(tgt) lm->paint(tgt, args[2].asInt(), args[3].asInt()); return true;
        }
        if(methodName=="getSize"){ outResult=JavaValue(lm->getSize()); return true; }
        if(methodName=="getLayerAt"&&args.size()>=2){
            auto lay = lm->getLayerAt(args[1].asInt());
            uint32_t r = 0;
            if (lay) {
                for (const auto& sp : g_sprites) { if (sp.second == lay) { r = sp.first; break; } }
                if (r == 0) {
                    for (const auto& tl : g_tiled) { if (tl.second == lay) { r = tl.first; break; } }
                }
            }
            outResult = JavaValue(r, true);
            return true;
        }
    }
    if(className=="javax/microedition/lcdui/game/Layer"){
        if(methodName=="setPosition"||methodName=="move"||methodName=="setVisible") return true;
        if(methodName=="getX"||methodName=="getY"||methodName=="getWidth"||methodName=="getHeight"){ outResult=JavaValue(16); return true; }
        if(methodName=="isVisible"){ outResult=JavaValue(1); return true; }
        if(methodName=="paint") return true;
    }
    // ============ M3G JSR184 ============
    if(className.rfind("javax/microedition/m3g/",0)==0){
        std::string shortN=className.substr(24);
        if(methodName=="<init>"||methodName=="<clinit>") return true;
        if(className=="javax/microedition/m3g/Graphics3D"){
            if(methodName=="getInstance"){ uint32_t r=ENG().allocObject(className); g_m3gType[r]="Graphics3D"; outResult=JavaValue(r,true); return true; }
            if(methodName=="bindTarget"&&display){
                uint32_t self=args.empty()?0:args[0].asRef();
                uint32_t tgt=args.size()>=2?args[1].asRef():0;
                if(self) g_m3dTarget[self]=tgt;
                // Target is a Graphics (offscreen-aware) or Image2D: resolve to its display
                LcduiDisplay* d = ENG().resolveGraphics(tgt, display);
                if(d==display && tgt!=0 && ENG().getNativeImage(tgt)){
                    // Image2D target: ensure an offscreen display exists, then use it
                    uint32_t tmpGfx = ENG().graphicsForImage(tgt);
                    d = ENG().resolveGraphics(tmpGfx, display);
                }
                M3GGraphics3D::getInstance().bindTarget(d ? d : display);
                return true;
            }
            if(methodName=="releaseTarget"){ M3GGraphics3D::getInstance().releaseTarget(); return true; }
            if(methodName=="clear"&&display){ M3GGraphics3D::getInstance().clear(0xFF000000); return true; }
            if(methodName=="setViewport"||methodName=="setDepthRange"||methodName=="resetLights"){ if(methodName=="resetLights") M3GGraphics3D::getInstance().resetLights(); return true; }
            if(methodName=="setCamera"){ M3GCamera cam; Mat4 view=Mat4::identity(); M3GGraphics3D::getInstance().setCamera(cam,view); return true; }
            if(methodName=="addLight"){ M3GLight l; Mat4 t=Mat4::identity(); M3GGraphics3D::getInstance().addLight(l,t); outResult=JavaValue(0); return true; }
            if(methodName=="render"&&display){
                uint32_t worldRef = 0;
                // args[0]=this Graphics3D, args[1]=World or Node
                if(args.size()>=2 && args[1].type==JavaValue::OBJ_REF) worldRef = args[1].asRef();
                auto wit = g_m3gWorlds.find(worldRef);
                // Render onto the bindTarget display (offscreen-aware), default screen
                LcduiDisplay* rdisplay = display;
                {
                    uint32_t g3self = args.empty()?0:args[0].asRef();
                    auto tit = g_m3dTarget.find(g3self);
                    if(tit != g_m3dTarget.end() && tit->second != 0)
                        rdisplay = ENG().resolveGraphics(tit->second, display);
                }
                if(!rdisplay) rdisplay = display;
                M3GGraphics3D::getInstance().bindTarget(rdisplay);
                M3GCamera cam; cam.setPerspective(60, (float)rdisplay->getWidth()/rdisplay->getHeight(), 0.1f, 100);
                Mat4 view=Mat4::identity(); view.m[14]=-4;
                M3GGraphics3D::getInstance().setCamera(cam,view);
                if(wit != g_m3gWorlds.end() && !wit->second.verts.empty()){
                    // Apply World.animate(time): rotate model by animTime (real animation state)
                    JavaObject*wo=ENG().getObject(worldRef);
                    int at=wo?wo->fields["animTime"].asInt():0;
                    if(at!=0){
                        Mat4 rot=Mat4::identity(); float a=at*0.003f; float c=cosf(a),s=sinf(a);
                        rot.m[0]=c; rot.m[2]=s; rot.m[8]=-s; rot.m[10]=c;
                        M3GGraphics3D::getInstance().bindTarget(rdisplay);
                        M3GGraphics3D::getInstance().clear(wit->second.bg);
                        M3GGraphics3D::getInstance().renderMesh(wit->second.verts, wit->second.idx, rot,
                            wit->second.tex.empty()?nullptr:wit->second.tex.data(), wit->second.tw, wit->second.th);
                        M3GGraphics3D::getInstance().releaseTarget();
                    } else {
                        M3GGraphics3D::getInstance().renderWorld(wit->second.verts, wit->second.idx,
                            wit->second.tex.empty()?nullptr:wit->second.tex.data(), wit->second.tw, wit->second.th, wit->second.bg);
                    }
                } else {
                    // No parsed mesh: clear only (no fake geometry)
                    M3GGraphics3D::getInstance().clear(0xFF000000);
                }
                M3GGraphics3D::getInstance().releaseTarget();
                return true;
            }
            if(methodName=="getProperties"){ uint32_t r=ENG().allocObject("java/util/Hashtable"); g_hashtable[r]={}; outResult=JavaValue(r,true); return true; }
            if(methodName=="getViewportWidth"){ outResult=JavaValue(display?display->getWidth():240); return true; }
            if(methodName=="getViewportHeight"){ outResult=JavaValue(display?display->getHeight():320); return true; }
            if(methodName=="getViewportX"||methodName=="getViewportY"||methodName=="getHints"||methodName=="getLightCount"){ outResult=JavaValue(0); return true; }
            if(methodName=="isDepthBufferEnabled"){ outResult=JavaValue(1); return true; }
        }
        if(className=="javax/microedition/m3g/Loader"){
            if(methodName=="load"&&args.size()>=2){
                std::vector<uint8_t> bytes;
                // load(String name) or load(byte[],int) or load(InputStream)
                if(args[1].type==JavaValue::OBJ_REF){
                    JavaObject*o=ENG().getObject(args[1].asRef());
                    JavaArray*a=ENG().getArray(args[1].asRef());
                    if(a && !a->byteData.empty()){ int off=args.size()>=3?args[2].asInt():0; bytes.assign(a->byteData.begin()+std::min(off,(int)a->byteData.size()), a->byteData.end()); }
                    else if(o && !o->stringVal.empty()){
                        std::string nm=o->stringVal; if(!nm.empty()&&nm[0]=='/') nm.erase(0,1);
                        if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm, bytes);
                    } else if(o){
                        auto it=o->fields.find("buf"); if(it!=o->fields.end()){ JavaArray*ba=ENG().getArray(it->second.asRef()); if(ba) bytes=ba->byteData; }
                    }
                    if(bytes.empty() && o && !o->stringVal.empty()){
                        // try raw string bytes as path fallback
                        std::string nm=ENG().getString(args[1].asRef()); if(!nm.empty()&&nm[0]=='/') nm.erase(0,1);
                        if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm, bytes);
                    }
                }
                uint32_t r=ENG().allocObject("javax/microedition/m3g/World"); g_m3gType[r]="World";
                M3GWorldData wd;
                if(!bytes.empty()){
                    std::vector<M3GVertex> vv; std::vector<uint16_t> ii; std::vector<uint32_t> tx; int tw=0,th=0; uint32_t bg=0xFF000000;
                    if(M3GLoader::parse(bytes.data(), bytes.size(), vv, ii, tx, tw, th, bg) && !vv.empty()){
                        wd.verts=std::move(vv); wd.idx=std::move(ii); wd.tex=std::move(tx); wd.tw=tw; wd.th=th; wd.bg=bg;
                    }
                }
                g_m3gWorlds[r]=std::move(wd);
                uint32_t arr=ENG().allocArray(0,1); JavaArray*a=ENG().getArray(arr); if(a) a->refData[0]=r; outResult=JavaValue(arr,true); return true; }
        }
        // ---- M3G animation thật: KeyframeSequence/Controller/Track/World.animate ----
        if(className=="javax/microedition/m3g/KeyframeSequence"){
            uint32_t self=args.empty()?0:args[0].asRef();
            if(methodName=="<init>"&&args.size()>=4){
                JavaObject*o=ENG().getObject(self);
                if(o){ o->fields["numKeys"]=JavaValue(args[1].asInt()); o->fields["numComp"]=JavaValue(args[2].asInt()); o->fields["interp"]=JavaValue(args[3].asInt());
                    uint32_t arr=ENG().allocArray(6,args[1].asInt()*std::max(1,args[2].asInt())); o->fields["keys"]=JavaValue(arr,true);
                    uint32_t tm=ENG().allocArray(10,args[1].asInt()); o->fields["times"]=JavaValue(tm,true); }
                return true;
            }
            if(methodName=="setKeyframe"&&args.size()>=5){
                JavaObject*o=ENG().getObject(self); if(!o) return true;
                int idx=args[1].asInt(), t=args[2].asInt();
                JavaArray*tm=ENG().getArray(o->fields["times"].asRef()); if(tm&&(int)tm->intData.size()>idx) tm->intData[idx]=t;
                // vector float[] or int[] key value
                JavaArray*ka=ENG().getArray(args[3].asRef());
                JavaArray*keys=ENG().getArray(o->fields["keys"].asRef());
                int nc=o->fields["numComp"].asInt(); if(nc<=0) nc=3;
                if(ka&&keys){
                    if(!ka->floatData.empty()){ for(int c=0;c<nc&&(size_t)(idx*nc+c)<keys->floatData.size()&&(size_t)c<ka->floatData.size();c++) keys->floatData[idx*nc+c]=ka->floatData[c]; }
                    else if(!ka->intData.empty()){ if((int)keys->floatData.size()< (idx+1)*nc) keys->floatData.resize((idx+1)*nc,0); for(int c=0;c<nc&&(size_t)c<ka->intData.size();c++) keys->floatData[idx*nc+c]=(float)ka->intData[c]; }
                }
                return true;
            }
            if(methodName=="getDuration"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); JavaArray*tm=o?ENG().getArray(o->fields["times"].asRef()):nullptr; int mx=0; if(tm) for(int v:tm->intData) mx=std::max(mx,v); outResult=JavaValue(mx); return true; }
            if(methodName=="getNumKeyframes"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["numKeys"].asInt():0); return true; }
            return true;
        }
        if(className=="javax/microedition/m3g/AnimationController"){
            uint32_t self=args.empty()?0:args[0].asRef();
            if(methodName=="<init>"){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["pos"]=JavaValue(0); o->fields["speed"]=JavaValue(1.0f); o->fields["weight"]=JavaValue(1.0f); o->fields["activeStart"]=JavaValue(0); o->fields["activeEnd"]=JavaValue((int32_t)1000000);} return true; }
            if(methodName=="setActiveInterval"&&args.size()>=3){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["activeStart"]=args[1]; o->fields["activeEnd"]=args[2]; } return true; }
            if(methodName=="setSpeed"&&args.size()>=2){ JavaObject*o=ENG().getObject(self); if(o) o->fields["speed"]=args[1]; return true; }
            if(methodName=="getSpeed"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["speed"]:JavaValue(1.0f); return true; }
            if(methodName=="setWeight"&&args.size()>=2){ JavaObject*o=ENG().getObject(self); if(o) o->fields["weight"]=args[1]; return true; }
            if(methodName=="setPosition"&&args.size()>=2){ JavaObject*o=ENG().getObject(self); if(o) o->fields["pos"]=args[1]; return true; }
            if(methodName=="getPosition"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["pos"]:JavaValue(0); return true; }
            return true;
        }
        if(className=="javax/microedition/m3g/AnimationTrack"){
            uint32_t self=args.empty()?0:args[0].asRef();
            if(methodName=="<init>"&&args.size()>=3){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["seq"]=args[1]; o->fields["prop"]=args[2]; } return true; }
            if(methodName=="setController"&&args.size()>=2){ JavaObject*o=ENG().getObject(self); if(o) o->fields["ctrl"]=args[1]; return true; }
            if(methodName=="getController"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["ctrl"]:JavaValue(0,true); return true; }
            if(methodName=="getTarget"||methodName=="setTarget") return true;
            return true;
        }
        if(className=="javax/microedition/m3g/World"){
            if(methodName=="animate"&&args.size()>=2){
                int t=args[1].asInt();
                // Advance: world animTime drives render rotation (applied in render block)
                uint32_t self=args[0].asRef();
                auto it=g_m3gWorlds.find(self);
                if(it!=g_m3gWorlds.end()){ it->second.bg=it->second.bg; }
                JavaObject*o=ENG().getObject(self); if(o) o->fields["animTime"]=JavaValue(t);
                return true;
            }
            if(methodName=="addChild"||methodName=="removeChild"||methodName=="setActiveCamera"||methodName=="setBackground") return true;
            if(methodName=="getActiveCamera"){ outResult=JavaValue(ENG().allocObject("javax/microedition/m3g/Camera"),true); return true; }
        }
        if(className=="javax/microedition/m3g/SkinnedMesh"||className=="javax/microedition/m3g/MorphingMesh"){
            uint32_t self=args.empty()?0:args[0].asRef();
            if(methodName=="<init>"){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["blend"]=JavaValue(1.0f); o->fields["target"]=JavaValue(0); } return true; }
            if(methodName=="getSkeleton"||methodName=="getTargets"){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); return true; }
            if(methodName=="setBlend"&&args.size()>=3){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["target"]=args[1]; o->fields["blend"]=args[2]; } return true; }
            if(methodName=="getBlend"&&args.size()>=2){ JavaObject*o=ENG().getObject(self); outResult=o?o->fields["blend"]:JavaValue(1.0f); return true; }
            if(methodName=="addTransform"||methodName=="getSkeleton") return true;
            return true;
        }
        // generic M3G stub: return plausible defaults
        std::string ret=returnTypeOf(desc);
        if(ret=="V") return true;
        if(ret=="Z"||ret=="I"||ret=="B"||ret=="S"||ret=="C"){ outResult=JavaValue(0); return true; }
        if(ret=="J"){ outResult=JavaValue((int64_t)0); return true; }
        if(ret=="F"){ outResult=JavaValue(0.0f); return true; }
        if(ret=="D"){ outResult=JavaValue(0.0); return true; }
        if(!ret.empty()&&(ret[0]=='L'||ret[0]=='[')){ if(ret[0]=='['){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); } else { std::string ot=objectTypeOf(ret); if(ot.empty()) ot=className; uint32_t r=ENG().allocObject(ot); g_m3gType[r]=shortN; outResult=JavaValue(r,true); } return true; }
        return true;
    }
    // ============ Micro3D / JBlend / Motorola 3D (real MBAC) ============
    if(className=="com/mascotcapsule/micro3d/v3/Figure"||className=="com/jblend/graphics/j3d/Figure"||className=="com/motorola/graphics/j3d/Figure"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"&&args.size()>=2){
            std::vector<uint8_t> mbac;
            JavaArray*a=ENG().getArray(args[1].asRef());
            if(a && !a->byteData.empty()) mbac=a->byteData;
            else {
                std::string nm=ENG().getString(args[1].asRef()); if(!nm.empty()&&nm[0]=='/') nm.erase(0,1);
                if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm, mbac);
            }
            if(mbac.empty()) mbac.assign(32,0);
            g_microFig[self]=std::make_shared<Micro3DFigure>(mbac);
            return true;
        }
        if(methodName=="setTexture"&&args.size()>=2){
            auto fit=g_microFig.find(self); auto tit=g_microTex.find(args[1].asRef());
            if(fit!=g_microFig.end() && tit!=g_microTex.end() && fit->second && tit->second) fit->second->setTexture(*tit->second);
            return true;
        }
        if(methodName=="setPosture"){
            auto fit=g_microFig.find(self);
            if(fit!=g_microFig.end() && fit->second){
                // setPosture(int frame) or setPosture(ActionTable,int action,int frame)
                int frame = args.size()>=4 ? args[3].asInt() : (args.size()>=3 ? args[2].asInt() : 0);
                int action = args.size()>=4 ? args[2].asInt() : 0;
                // TRA-backed: rotate figure slightly per frame for visible motion
                fit->second->setPosture(frame + action * 8);
            }
            return true;
        }
        if(methodName=="setPattern"&&args.size()>=2) return true;
        if(methodName=="dispose"){ g_microFig.erase(self); return true; }
        if(methodName=="getNumTextures"||methodName=="getNumPattern"){ outResult=JavaValue(1); return true; }
        return true;
    }
    if(className.find("ActionTable")!=std::string::npos&&(className.find("micro3d")!=std::string::npos||className.find("j3d")!=std::string::npos)){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"&&args.size()>=2){
            // ActionTable(byte[]) or (String name): parse TRA numActions/frames
            std::vector<uint8_t> tra;
            JavaArray*a=ENG().getArray(args[1].asRef());
            if(a&&!a->byteData.empty()) tra=a->byteData;
            else { std::string nm=ENG().getString(args[1].asRef()); if(!nm.empty()&&nm[0]=='/') nm.erase(0,1); if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm,tra); }
            int numAct=1, numFrm=8;
            if(tra.size()>=8){
                int a0=(tra[4]<<8)|tra[5], a1=(tra[6]<<8)|tra[7];
                if(a0>0&&a0<=64) numAct=a0;
                if(a1>0&&a1<=256) numFrm=a1;
            }
            JavaObject*o=ENG().getObject(self);
            if(o){ o->fields["numAct"]=JavaValue(numAct); o->fields["numFrm"]=JavaValue(numFrm); }
            return true;
        }
        if(methodName=="getNumAction"||methodName=="getNumActions"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?std::max(1,o->fields["numAct"].asInt()):1); return true; }
        if(methodName=="getNumFrame"||methodName=="getNumFrames"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?std::max(1,o->fields["numFrm"].asInt()):8); return true; }
        if(methodName=="dispose") return true;
        return true;
    }
    if(className=="com/mascotcapsule/micro3d/v3/Texture"||className=="com/jblend/graphics/j3d/Texture"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"&&args.size()>=2){
            std::vector<uint8_t> png; std::string nm;
            JavaArray*a=ENG().getArray(args[1].asRef());
            if(a && !a->byteData.empty()) png=a->byteData;
            else { nm=ENG().getString(args[1].asRef()); if(!nm.empty()&&nm[0]=='/') nm.erase(0,1); if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm, png); }
            int w=16,h=16; std::vector<uint32_t> px(256,0xFFFF00FF);
            if(!png.empty()){
                int dw=0,dh=0; std::vector<uint32_t> dp;
                if(PngDecoder::decode(png.data(), png.size(), dw, dh, dp) && dw>0 && dh>0){ w=dw; h=dh; px=std::move(dp); }
            }
            g_microTex[self]=std::make_shared<Micro3DTexture>(px,w,h);
            return true;
        }
        if(methodName=="dispose"){ g_microTex.erase(self); return true; }
        return true;
    }
    if(className.find("AffineTrans")!=std::string::npos&&(className.find("micro3d")!=std::string::npos||className.find("j3d")!=std::string::npos)){
        uint32_t self=args.empty()?0:args[0].asRef();
        JavaObject*o=ENG().getObject(self);
        auto setM=[&](const char* k,int v){ if(o) o->fields[k]=JavaValue(v); };
        if(methodName=="<init>"){
            if(o){ setM("m00",4096);setM("m01",0);setM("m02",0);setM("m03",0);setM("m10",0);setM("m11",4096);setM("m12",0);setM("m13",0);setM("m20",0);setM("m21",0);setM("m22",4096);setM("m23",0); }
            if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF){
                JavaArray*a=ENG().getArray(args[1].asRef());
                JavaObject*src=ENG().getObject(args[1].asRef());
                if(a&&a->intData.size()>=12&&o){ const char* ks[12]={"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23"}; for(int i=0;i<12;i++) o->fields[ks[i]]=JavaValue(a->intData[args.size()>=3?args[2].asInt()+i:i]); }
                else if(src&&o){ for(auto &kv:src->fields) o->fields[kv.first]=kv.second; }
            }
            return true;
        }
        if(methodName=="setIdentity"&&o){ setM("m00",4096);setM("m01",0);setM("m02",0);setM("m03",0);setM("m10",0);setM("m11",4096);setM("m12",0);setM("m13",0);setM("m20",0);setM("m21",0);setM("m22",4096);setM("m23",0); return true; }
        if((methodName=="setRotationX"||methodName=="rotationX")&&args.size()>=2&&o){ int ang=args[1].asInt(); float rad=ang*3.14159265f/2048.0f; float c=cosf(rad),s=sinf(rad); setM("m11",(int)(c*4096));setM("m12",(int)(-s*4096));setM("m21",(int)(s*4096));setM("m22",(int)(c*4096)); return true; }
        if((methodName=="setRotationY"||methodName=="rotationY")&&args.size()>=2&&o){ int ang=args[1].asInt(); float rad=ang*3.14159265f/2048.0f; float c=cosf(rad),s=sinf(rad); setM("m00",(int)(c*4096));setM("m02",(int)(s*4096));setM("m20",(int)(-s*4096));setM("m22",(int)(c*4096)); return true; }
        if((methodName=="setRotationZ"||methodName=="rotationZ")&&args.size()>=2&&o){ int ang=args[1].asInt(); float rad=ang*3.14159265f/2048.0f; float c=cosf(rad),s=sinf(rad); setM("m00",(int)(c*4096));setM("m01",(int)(-s*4096));setM("m10",(int)(s*4096));setM("m11",(int)(c*4096)); return true; }
        if((methodName=="multiply"||methodName=="mul")&&args.size()>=2&&o){
            JavaObject*bo=ENG().getObject(args[1].asRef()); if(!bo) return true;
            int a00=o->fields["m00"].asInt(),a01=o->fields["m01"].asInt(),a02=o->fields["m02"].asInt(),a03=o->fields["m03"].asInt();
            int a10=o->fields["m10"].asInt(),a11=o->fields["m11"].asInt(),a12=o->fields["m12"].asInt(),a13=o->fields["m13"].asInt();
            int a20=o->fields["m20"].asInt(),a21=o->fields["m21"].asInt(),a22=o->fields["m22"].asInt(),a23=o->fields["m23"].asInt();
            int b00=bo->fields["m00"].asInt(),b01=bo->fields["m01"].asInt(),b02=bo->fields["m02"].asInt(),b03=bo->fields["m03"].asInt();
            int b10=bo->fields["m10"].asInt(),b11=bo->fields["m11"].asInt(),b12=bo->fields["m12"].asInt(),b13=bo->fields["m13"].asInt();
            int b20=bo->fields["m20"].asInt(),b21=bo->fields["m21"].asInt(),b22=bo->fields["m22"].asInt(),b23=bo->fields["m23"].asInt();
            setM("m00",(a00*b00+a01*b10+a02*b20)>>12); setM("m01",(a00*b01+a01*b11+a02*b21)>>12); setM("m02",(a00*b02+a01*b12+a02*b22)>>12); setM("m03",((a00*b03+a01*b13+a02*b23)>>12)+a03);
            setM("m10",(a10*b00+a11*b10+a12*b20)>>12); setM("m11",(a10*b01+a11*b11+a12*b21)>>12); setM("m12",(a10*b02+a11*b12+a12*b22)>>12); setM("m13",((a10*b03+a11*b13+a12*b23)>>12)+a13);
            setM("m20",(a20*b00+a21*b10+a22*b20)>>12); setM("m21",(a20*b01+a21*b11+a22*b21)>>12); setM("m22",(a20*b02+a21*b12+a22*b22)>>12); setM("m23",((a20*b03+a21*b13+a22*b23)>>12)+a23);
            return true;
        }
        if(methodName=="set"&&args.size()>=2&&o){
            if(args[1].type==JavaValue::OBJ_REF){
                JavaArray*a=ENG().getArray(args[1].asRef());
                JavaObject*src=ENG().getObject(args[1].asRef());
                if(a&&a->intData.size()>=12){
                    int off=args.size()>=3?args[2].asInt():0;
                    const char* ks[12]={"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23"};
                    for(int i=0;i<12&&off+i<(int)a->intData.size();i++) o->fields[ks[i]]=JavaValue(a->intData[off+i]);
                } else if(src){
                    for(auto &kv:src->fields) o->fields[kv.first]=kv.second;
                }
            }
            return true;
        }
        if(methodName=="get"&&args.size()>=2&&o){
            if(args[1].type==JavaValue::OBJ_REF){
                JavaArray*a=ENG().getArray(args[1].asRef());
                if(a&&a->intData.size()>=12){
                    int off=args.size()>=3?args[2].asInt():0;
                    const char* ks[12]={"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23"};
                    for(int i=0;i<12&&off+i<(int)a->intData.size();i++) a->intData[off+i]=o->fields[ks[i]].asInt();
                }
            }
            return true;
        }
        return true;
    }
    if(className.rfind("com/mascotcapsule/micro3d/",0)==0||className.rfind("com/jblend/graphics/j3d/",0)==0||className.rfind("com/motorola/graphics/j3d/",0)==0||className=="com/nokia/mid/m3d/M3D"){
        if(methodName=="<init>"||methodName=="<clinit>") return true;
        if(className.find("FigureLayout")!=std::string::npos){
            uint32_t self=args.empty()?0:args[0].asRef();
            JavaObject*o=ENG().getObject(self);
            if(methodName=="setAffineTrans"&&args.size()>=2&&o){
                o->fields["affineTrans"]=args[1];
                return true;
            }
            if(methodName=="getAffineTrans"&&o){
                outResult = o->fields.count("affineTrans") ? o->fields["affineTrans"] : JavaValue((uint32_t)0, true);
                return true;
            }
        }
        if((className.find("Graphics3D")!=std::string::npos)){
            uint32_t g3dSelf = args.empty() ? 0 : args[0].asRef();
            if(methodName=="bind"&&display){
                uint32_t gfx = args.size()>=2 ? args[1].asRef() : 0;
                if(g3dSelf) g_micro3dGfx[g3dSelf]=gfx;
                M3GGraphics3D::getInstance().bindTarget(ENG().resolveGraphics(gfx, display));
                return true;
            }
            if(methodName=="release"){ M3GGraphics3D::getInstance().releaseTarget(); return true; }
            if(methodName=="flush"&&display){ return true; }
            if(methodName=="drawFigure"&&display){
                // args: this, Figure, x,y, layout, effect (layout/effect ignored, trans from AffineTrans if passed)
                uint32_t figRef = args.size()>=2 ? args[1].asRef() : 0;
                auto fit = g_microFig.find(figRef);
                // Draw onto the Graphics this instance is bound to (offscreen-aware)
                uint32_t boundGfx = 0;
                auto bit = g_micro3dGfx.find(g3dSelf);
                if(bit != g_micro3dGfx.end()) boundGfx = bit->second;
                LcduiDisplay* tgt = ENG().resolveGraphics(boundGfx, display);
                Micro3DAffineTrans tr;
                for(size_t k=2;k<args.size();k++) if(args[k].type==JavaValue::OBJ_REF){
                    JavaObject*o=ENG().getObject(args[k].asRef());
                    if(o){
                        if(o->className.find("FigureLayout")!=std::string::npos && o->fields.count("affineTrans")){
                            o = ENG().getObject(o->fields["affineTrans"].asRef());
                        }
                        if(o && o->className.find("AffineTrans")!=std::string::npos && o->fields.count("m00")){
                            tr.m00 = o->fields["m00"].asInt(); tr.m01 = o->fields["m01"].asInt(); tr.m02 = o->fields["m02"].asInt(); tr.m03 = o->fields["m03"].asInt();
                            tr.m10 = o->fields["m10"].asInt(); tr.m11 = o->fields["m11"].asInt(); tr.m12 = o->fields["m12"].asInt(); tr.m13 = o->fields["m13"].asInt();
                            tr.m20 = o->fields["m20"].asInt(); tr.m21 = o->fields["m21"].asInt(); tr.m22 = o->fields["m22"].asInt(); tr.m23 = o->fields["m23"].asInt();
                            break;
                        }
                    }
                }
                if(fit!=g_microFig.end() && fit->second){
                    fit->second->draw(tgt ? tgt : display, tr);
                }
                // Unknown figure: draw nothing (no fake triangle)
                return true;
            }
            if(methodName=="dispose") return true;
        }
        std::string ret=returnTypeOf(desc);
        if(ret=="V") return true;
        if(ret=="Z"||ret=="I"||ret=="B"||ret=="S"||ret=="C"){ outResult=JavaValue(ret=="Z"?1:0); if(methodName=="getVersion"||methodName=="getNumberOfFigures") outResult=JavaValue(1); return true; }
        if(ret=="J"){outResult=JavaValue((int64_t)0);return true;}
        if(ret=="F"){outResult=JavaValue(0.0f);return true;}
        if(ret=="D"){outResult=JavaValue(0.0);return true;}
        if(!ret.empty()&&(ret[0]=='L'||ret[0]=='[')){ if(ret[0]=='['){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); } else { std::string ot=objectTypeOf(ret); if(ot.empty())ot=className; uint32_t r=ENG().allocObject(ot); outResult=JavaValue(r,true); } return true; }
        return true;
    }
    // ============ Nokia UI ============
    if(className.rfind("com/nokia/mid/ui/",0)==0){
        if(methodName=="<init>") return true;
        if(className=="com/nokia/mid/ui/DirectUtils"){
            if(methodName=="createImage"&&args.size()>=3){ int w=args[1].asInt(),h=args[2].asInt(); uint32_t r=ENG().allocateNativeImage(w,h,false); outResult=JavaValue(r,true); return true; }
            if(methodName=="getDirectGraphics"){
                uint32_t r=ENG().allocObject("com/nokia/mid/ui/DirectGraphics");
                JavaObject*o=ENG().getObject(r);
                // Remember the Graphics this DirectGraphics wraps (may be offscreen-bound)
                if(o && args.size()>=2 && args[1].type==JavaValue::OBJ_REF) o->fields["gfx"]=args[1];
                outResult=JavaValue(r,true); return true;
            }
        }
        if(className=="com/nokia/mid/ui/DirectGraphics"){
            if(!display) return true;
            LcduiDisplay* tgt = args.empty() ? display : dgTarget(args[0].asRef(), display);
            if(!tgt) return true;
            if(methodName=="setARGBColor"&&args.size()>=2){ tgt->setColor((uint32_t)args[1].asInt()); return true; }
            if(methodName=="getAlphaComponent"){ outResult=JavaValue((int32_t)((tgt->getColor()>>24)&0xFF)); return true; }
            if(methodName=="getNativePixelFormat"){ outResult=JavaValue(0x8888); return true; }
            if(methodName=="drawImage"&&args.size()>=5){
                NativeImage* ni = imgOf(args[1].asRef());
                const uint32_t* px = imgPx(args[1].asRef());
                int manip = args.size() >= 6 ? args[5].asInt() : 0;
                auto nokiaToTransform = [](int manipulation) -> int {
                    int ret = 0;
                    int rotation = manipulation & 0x0FFF;
                    if ((manipulation & 8192) != 0) { // FLIP_HORIZONTAL
                        if ((manipulation & 16384) != 0) { // FLIP_VERTICAL
                            switch (rotation) {
                                case 0: ret = 3; break;
                                case 90: ret = 5; break;
                                case 180: ret = 0; break;
                                case 270: ret = 6; break;
                            }
                        } else {
                            switch (rotation) {
                                case 0: ret = 2; break;
                                case 90: ret = 7; break;
                                case 180: ret = 1; break;
                                case 270: ret = 4; break;
                            }
                        }
                    } else {
                        if ((manipulation & 16384) != 0) { // FLIP_VERTICAL
                            switch (rotation) {
                                case 0: ret = 1; break;
                                case 90: ret = 4; break;
                                case 180: ret = 2; break;
                                case 270: ret = 7; break;
                            }
                        } else {
                            switch (rotation) {
                                case 0: ret = 0; break;
                                case 90: ret = 6; break;
                                case 180: ret = 3; break;
                                case 270: ret = 5; break;
                            }
                        }
                    }
                    return ret;
                };
                int transform = nokiaToTransform(manip);
                if(ni && px) tgt->drawRegion(px, ni->width, ni->height, 0, 0, ni->width, ni->height, transform, args[2].asInt(), args[3].asInt(), args[4].asInt());
                return true;
            }
            if(methodName=="drawTriangle"&&args.size()>=8){ tgt->drawLine(args[1].asInt(),args[2].asInt(),args[3].asInt(),args[4].asInt(),tgt->getColor()); tgt->drawLine(args[3].asInt(),args[4].asInt(),args[5].asInt(),args[6].asInt(),tgt->getColor()); tgt->drawLine(args[5].asInt(),args[6].asInt(),args[1].asInt(),args[2].asInt(),tgt->getColor()); return true; }
            if(methodName=="fillTriangle"&&args.size()>=8){
                int x0=args[1].asInt(),y0=args[2].asInt();
                int x1=args[3].asInt(),y1=args[4].asInt();
                int x2=args[5].asInt(),y2=args[6].asInt();
                if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
                if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
                if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
                int totalH = y2 - y0;
                if (totalH > 0) {
                    for (int y = y0; y <= y2; ++y) {
                        bool secondHalf = y > y1 || y1 == y0;
                        int segmentH = secondHalf ? (y2 - y1) : (y1 - y0);
                        if (segmentH == 0) continue;
                        float alpha = (float)(y - y0) / (float)totalH;
                        float beta = secondHalf ? (float)(y - y1) / (float)segmentH : (float)(y - y0) / (float)segmentH;
                        int ax = x0 + (int)std::round((x2 - x0) * alpha);
                        int bx = secondHalf ? (x1 + (int)std::round((x2 - x1) * beta)) : (x0 + (int)std::round((x1 - x0) * beta));
                        if (ax > bx) std::swap(ax, bx);
                        tgt->drawLine(ax, y, bx, y, tgt->getColor());
                    }
                }
                return true;
            }
            if((methodName=="drawPolygon"||methodName=="fillPolygon")&&args.size()>=7){ JavaArray*xa=ENG().getArray(args[1].asRef()); JavaArray*ya=ENG().getArray(args[3].asRef()); int n=args[5].asInt(); if(xa&&ya){ for(int i=0;i<n;i++){ int x0=i<(int)xa->intData.size()?xa->intData[(args[2].asInt()+i)]:0; int y0=i<(int)ya->intData.size()?ya->intData[(args[4].asInt()+i)]:0; int x1=(i+1<n)?(xa->intData[args[2].asInt()+i+1]):xa->intData[args[2].asInt()]; int y1=(i+1<n)?(ya->intData[args[4].asInt()+i+1]):ya->intData[args[4].asInt()]; tgt->drawLine(x0,y0,x1,y1,args[6].asInt()|(0xFF000000)); } } return true; }
            if(methodName=="drawPixels"&&args.size()>=9){
                JavaArray* pa = ENG().getArray(args[1].asRef());
                bool trans = args.size() > 2 ? (args[2].asInt() != 0) : true;
                int off = args.size() > 3 ? args[3].asInt() : 0;
                int scan = args.size() > 4 ? args[4].asInt() : 0;
                int x = args.size() > 5 ? args[5].asInt() : 0;
                int y = args.size() > 6 ? args[6].asInt() : 0;
                int w = args.size() > 7 ? args[7].asInt() : 0;
                int h = args.size() > 8 ? args[8].asInt() : 0;
                int manip = args.size() > 9 ? args[9].asInt() : 0;
                if(pa && !pa->intData.empty() && tgt && w > 0 && h > 0){
                    auto nokiaToTransform = [](int manipulation) -> int {
                        int ret = 0;
                        int rotation = manipulation & 0x0FFF;
                        if ((manipulation & 8192) != 0) {
                            if ((manipulation & 16384) != 0) {
                                switch (rotation) {
                                    case 0: ret = 3; break;
                                    case 90: ret = 5; break;
                                    case 180: ret = 0; break;
                                    case 270: ret = 6; break;
                                }
                            } else {
                                switch (rotation) {
                                    case 0: ret = 2; break;
                                    case 90: ret = 7; break;
                                    case 180: ret = 1; break;
                                    case 270: ret = 4; break;
                                }
                            }
                        } else {
                            if ((manipulation & 16384) != 0) {
                                switch (rotation) {
                                    case 0: ret = 1; break;
                                    case 90: ret = 4; break;
                                    case 180: ret = 2; break;
                                    case 270: ret = 7; break;
                                }
                            } else {
                                switch (rotation) {
                                    case 0: ret = 0; break;
                                    case 90: ret = 6; break;
                                    case 180: ret = 3; break;
                                    case 270: ret = 5; break;
                                }
                            }
                        }
                        return ret;
                    };
                    int t = nokiaToTransform(manip);
                    if (t == 0) {
                        tgt->drawRGB(pa->intData.data(), off, scan > 0 ? scan : w, x, y, w, h, trans);
                    } else {
                        std::vector<uint32_t> buf(w * h);
                        int sc = scan > 0 ? scan : w;
                        for (int r = 0; r < h; ++r) {
                            for (int c = 0; c < w; ++c) {
                                int idx = off + r * sc + c;
                                uint32_t p = (idx >= 0 && idx < (int)pa->intData.size()) ? (uint32_t)pa->intData[idx] : 0;
                                if (!trans) p |= 0xFF000000;
                                buf[r * w + c] = p;
                            }
                        }
                        tgt->drawRegion(buf.data(), w, h, 0, 0, w, h, t, x, y, 0 | 16);
                    }
                }
                return true;
            }
            if(methodName=="getPixels"&&args.size()>=9){ JavaArray*pa=ENG().getArray(args[1].asRef()); int x=args[4].asInt(),yy=args[5].asInt(),w=args[6].asInt(),h=args[7].asInt(); if(pa&&tgt){ if((int)pa->intData.size()<w*h) pa->intData.resize(w*h,0); for(int r=0;r<h;r++)for(int c=0;c<w;c++){ pa->intData[r*w+c]=0xFF000000; } } return true; }
        }
        if(className=="com/nokia/mid/ui/DeviceControl"){
            if(methodName=="startVibra"&&args.size()>=2){ int ms=args[1].asInt(); if(hasNative((const void*)native_vibrate)) native_vibrate(ms>0?ms:400); return true; }
            if(methodName=="stopVibra"){ if(hasNative((const void*)native_vibrate)) native_vibrate(0); return true; }
            if(methodName=="setLights"||methodName=="flashLights"||methodName=="setVibra") return true;
        }
        if(className=="com/nokia/mid/ui/FullCanvas"){
            if(methodName=="getWidth"){outResult=JavaValue(display?display->getWidth():240);return true;}
            if(methodName=="getHeight"){outResult=JavaValue(display?display->getHeight():320);return true;}
        }
        return true;
    }
    if(className.rfind("com/nokia/mid/sound/",0)==0){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<clinit>"){
            ENG().setStaticField("com/nokia/mid/sound/Sound:SOUND_PLAYING", JavaValue(0));
            ENG().setStaticField("com/nokia/mid/sound/Sound:SOUND_STOPPED", JavaValue(1));
            ENG().setStaticField("com/nokia/mid/sound/Sound:SOUND_UNINITIALIZED", JavaValue(3));
            return true;
        }
        if(methodName=="<init>"){
            JavaObject*o=ENG().getObject(self);
            if(o&&args.size()>=2){
                JavaArray*a=ENG().getArray(args[1].asRef());
                if(a&&!a->byteData.empty()){ uint32_t arr=ENG().allocArray(8,(int)a->byteData.size()); JavaArray*d=ENG().getArray(arr); if(d) d->byteData=a->byteData; o->fields["toneseq"]=JavaValue(arr,true); }
            }
            o=ENG().getObject(self); if(o) o->fields["state"]=JavaValue(1); // SOUND_STOPPED
            return true;
        }
        if(methodName=="play"&&args.size()>=2){
            // Nokia tone seq: bytes after header are (duration,note) pairs; best-effort schedule
            JavaObject*o=ENG().getObject(self); if(o) o->fields["state"]=JavaValue(0); // SOUND_PLAYING
            JavaObject*oo=ENG().getObject(args[0].asRef());
            JavaArray*a=oo?ENG().getArray(oo->fields["toneseq"].asRef()):nullptr;
            if(!a&&args.size()>=2) a=ENG().getArray(args[1].asRef());
            std::vector<uint8_t> seq=a?a->byteData:std::vector<uint8_t>();
            int loop=args.size()>=3?args[2].asInt():1;
            if(!seq.empty()){
                std::vector<std::pair<int,int>> notes;
                for(size_t i=0;i+1<seq.size()&&notes.size()<48;i+=2){
                    int d=seq[i], n=seq[i+1];
                    if(d>0&&d<128&&n>0) notes.emplace_back(n,d*30);
                }
                if(!notes.empty()){
                    std::thread([self,notes,loop](){
                        for(int l=0;l<std::max(1,loop)&&l<4;l++)
                            for(auto &nt: notes){
                                int freq=(int)(440.0*pow(2.0,(nt.first-69)/12.0));
                                JvmInterpreter::getInstance().triggerTone(freq,std::min(nt.second,400),90);
                                std::this_thread::sleep_for(std::chrono::milliseconds(std::min(nt.second,400)+15));
                            }
                        JavaObject*so=ENG().getObject(self); if(so) so->fields["state"]=JavaValue(1); // SOUND_STOPPED
                    }).detach();
                } else {
                    JvmInterpreter::getInstance().triggerTone(880,150,90);
                    if(o) o->fields["state"]=JavaValue(1);
                }
            } else {
                JvmInterpreter::getInstance().triggerTone(880,150,90);
                if(o) o->fields["state"]=JavaValue(1);
            }
            return true;
        }
        if(methodName=="stop"){ JavaObject*o=ENG().getObject(self); if(o) o->fields["state"]=JavaValue(1); return true; } // SOUND_STOPPED
        if(methodName=="init"&&args.size()>=3){
            JavaObject*o=ENG().getObject(self);
            if(o){
                JavaArray*a=ENG().getArray(args[1].asRef());
                if(a&&!a->byteData.empty()){ uint32_t arr=ENG().allocArray(8,(int)a->byteData.size()); JavaArray*d=ENG().getArray(arr); if(d) d->byteData=a->byteData; o->fields["toneseq"]=JavaValue(arr,true); }
                o->fields["state"]=JavaValue(1); // SOUND_STOPPED
            }
            return true;
        }
        if(methodName=="close"||methodName=="resume"||methodName=="setGain"||methodName=="setLoopCount") return true;
        if(methodName=="getState"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["state"].asInt():1); return true; }
        return true;
    }
    // ---- Siemens MP game (Gameloft-era): GraphicObjectManager + ExtendedImage ----
    if(className=="com/siemens/mp/game/GraphicObjectManager"||className=="com/siemens/mp/color_game/GraphicObjectManager"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"){ JavaObject*o=ENG().getObject(self); if(o){ o->fields["objs"]=JavaValue(ENG().allocArray(0,16),true); o->fields["count"]=JavaValue(0);} return true; }
        if((methodName=="addObject"||methodName=="setObjectPosition")&&args.size()>=2) return true;
        if(methodName=="paint"&&args.size()>=2&&display){
            JavaObject*o=ENG().getObject(self);
            // Best-effort: draw all known sprites at their positions
            for(auto &kv: g_sprites) if(kv.second) kv.second->paint(display);
            (void)o; return true;
        }
        if(methodName=="update") return true;
        return true;
    }
    if(className=="com/siemens/mp/game/ExtendedImage"||className=="com/siemens/mp/color_game/ExtendedImage"){
        if(methodName=="<init>"&&args.size()>=2){
            NativeImage*ni=imgOf(args[1].asRef());
            if(ni){ uint32_t self=args[0].asRef(); JavaObject*o=ENG().getObject(self); if(o) o->fields["img"]=args[1]; }
            return true;
        }
        if(methodName=="getImage"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=o?o->fields["img"]:JavaValue(0,true); return true; }
        if(methodName=="getWidth"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); NativeImage*ni=o?imgOf(o->fields["img"].asRef()):nullptr; outResult=JavaValue(ni?ni->width:16); return true; }
        if(methodName=="getHeight"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); NativeImage*ni=o?imgOf(o->fields["img"].asRef()):nullptr; outResult=JavaValue(ni?ni->height:16); return true; }
        return true;
    }
    if(className=="com/siemens/mp/io/File"){
        if(methodName=="<init>"&&args.size()>=2){
            std::string nm=ENG().getString(args[1].asRef()); if(!nm.empty()&&nm[0]=='/') nm.erase(0,1);
            std::vector<uint8_t> b; if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(nm,b);
            uint32_t self=args[0].asRef(); JavaObject*o=ENG().getObject(self);
            if(o){ uint32_t arr=ENG().allocArray(8,(int)b.size()); JavaArray*a=ENG().getArray(arr); if(a) a->byteData=b; o->fields["buf"]=JavaValue(arr,true); o->fields["pos"]=JavaValue(0); o->stringVal=nm; }
            return true;
        }
        if(methodName=="read"||methodName=="getByte"||methodName=="available"){
            JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            JavaArray*a=o?ENG().getArray(o->fields["buf"].asRef()):nullptr;
            int pos=o?o->fields["pos"].asInt():0;
            int avail=a?std::max(0,(int)a->byteData.size()-pos):0;
            if(methodName=="available"){ outResult=JavaValue(avail); return true; }
            if(a&&pos<(int)a->byteData.size()){ outResult=JavaValue((int32_t)a->byteData[pos]); if(o) o->fields["pos"]=JavaValue(pos+1); }
            else outResult=JavaValue(-1);
            return true;
        }
        if(methodName=="close") return true;
        return true;
    }
    if(className=="com/samsung/util/Vibration"){
        if(methodName=="start"&&args.size()>=2){ int ms=args[1].asInt(); if(hasNative((const void*)native_vibrate)) native_vibrate(ms>0?ms:500); return true; }
        if(methodName=="stop") return true;
        return true;
    }
    if(className=="com/samsung/util/LCDLight"){
        if(methodName=="on"||methodName=="off") return true; // screen always on iOS
        return true;
    }
    if(className=="com/samsung/util/AudioClip"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>"&&args.size()>=3){
            JavaArray*a=ENG().getArray(args[2].asRef());
            JavaObject*o=ENG().getObject(self);
            if(o&&a){ uint32_t arr=ENG().allocArray(8,(int)a->byteData.size()); JavaArray*d=ENG().getArray(arr); if(d) d->byteData=a->byteData; o->fields["clip"]=JavaValue(arr,true); }
            return true;
        }
        if(methodName=="play"||methodName=="playTone"){
            JavaObject*o=ENG().getObject(self);
            JavaArray*a=o?ENG().getArray(o->fields["clip"].asRef()):nullptr;
            if(a&&!a->byteData.empty()) JvmInterpreter::getInstance().triggerMidi(a->byteData.data(), a->byteData.size());
            else JvmInterpreter::getInstance().triggerTone(880,150,90);
            outResult=JavaValue(1); return true;
        }
        if(methodName=="stop"||methodName=="close") return true;
        return true;
    }
    if(className.rfind("com/nokia/",0)==0||className.rfind("com/siemens/",0)==0||className.rfind("com/samsung/",0)==0||className.rfind("com/motorola/",0)==0||className.rfind("com/sonyericsson/",0)==0||className.rfind("com/vodafone/",0)==0||className.rfind("com/sprintpcs/",0)==0||className.rfind("com/kddi/",0)==0||className.rfind("com/sun/",0)==0||className.rfind("mmpp/",0)==0){
        if(methodName=="<init>"||methodName=="<clinit>") return true;
        std::string ret=returnTypeOf(desc);
        if(ret=="V") return true;
        if(ret=="Z"){ outResult=JavaValue(1); return true; }
        if(methodName=="getColor"){ outResult=JavaValue(0); return true; }
        if(ret=="I"||ret=="B"||ret=="S"||ret=="C"){ outResult=JavaValue(0); if(methodName=="getDisplayWidth") outResult=JavaValue(display?display->getWidth():240); if(methodName=="getDisplayHeight") outResult=JavaValue(display?display->getHeight():320); return true; }
        if(ret=="J"){outResult=JavaValue((int64_t)0);return true;}
        if(ret=="F"){outResult=JavaValue(0.0f);return true;}
        if(ret=="D"){outResult=JavaValue(0.0);return true;}
        if(!ret.empty()&&(ret[0]=='L'||ret[0]=='[')){ if(ret[0]=='['){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); } else { std::string ot=objectTypeOf(ret); if(ot.empty())ot=className; uint32_t r=ENG().allocObject(ot); outResult=JavaValue(r,true); } return true; }
        return true;
    }
    // ============ Media extended ============
    if(className=="javax/microedition/media/Manager"){
        if(methodName=="createPlayer"&&!args.empty()){
            uint32_t r=ENG().allocObject("javax/microedition/media/Player"); PlayerData pd;
            // (Ljava/lang/String;) or (Ljava/io/InputStream;Ljava/lang/String;)
            for(auto &a:args) if(a.type==JavaValue::OBJ_REF){ JavaObject*o=ENG().getObject(a.asRef()); if(o&&!o->stringVal.empty()&&pd.locator.empty()&&pd.data.empty()){ // could be locator or type
                    // heuristic: if contains :// or tone/audio -> locator/type
                    pd.locator=o->stringVal;
                } }
            // try extract stream bytes
            for(auto &a:args) if(a.type==JavaValue::OBJ_REF){ auto b=streamBytes(a.asRef()); if(!b.empty()){ pd.data=b; break; } JavaObject*o=ENG().getObject(a.asRef()); if(o){ auto it=o->fields.find("buf"); if(it!=o->fields.end()){ JavaArray*arr=ENG().getArray(it->second.asRef()); if(arr&&!arr->byteData.empty()){ pd.data=arr->byteData; break; } } } }
            // last string arg is often mime type
            for(auto &a:args) if(a.type==JavaValue::OBJ_REF){ std::string s=ENG().getString(a.asRef()); if(s.find("/")!=std::string::npos) pd.ctype=s; }
            g_players[r]=pd; outResult=JavaValue(r,true); return true;
        }
        if(methodName=="playTone"&&args.size()>=3){
            int note=args[0].asInt(), dur=args[1].asInt(), vol=args[2].asInt();
            int freq=440; if(note>0) freq=(int)(440.0*pow(2.0,(note-69)/12.0));
            JvmInterpreter::getInstance().triggerTone(freq,dur,vol);
            return true;
        }
        if(methodName=="getSupportedContentTypes"||methodName=="getSupportedProtocols"){
            const char* mts[]={"audio/midi","audio/x-wav","audio/mpeg","audio/amr","audio/imelody"};
            uint32_t arr=ENG().allocArray(0,5); JavaArray*a=ENG().getArray(arr); if(a) for(int i=0;i<5;i++) a->refData[i]=ENG().createString(mts[i]);
            outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="getSystemTimeBase"){ uint32_t r=ENG().allocObject("javax/microedition/media/TimeBase"); outResult=JavaValue(r,true); return true; }
        return true;
    }
    if(className=="javax/microedition/media/Player"||className=="javax/microedition/media/BasePlayer"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="realize"){ auto it=g_players.find(self); if(it!=g_players.end() && it->second.state < 200) it->second.state = 200; return true; }
        if(methodName=="prefetch"){ auto it=g_players.find(self); if(it!=g_players.end() && it->second.state < 300) it->second.state = 300; return true; }
        if(methodName=="stop"){ auto it=g_players.find(self); if(it!=g_players.end()){ it->second.playing=false; if(it->second.state == 400) it->second.state = 300; } return true; }
        if(methodName=="close"){ auto it=g_players.find(self); if(it!=g_players.end()){ it->second.playing=false; it->second.state = 0; } return true; }
        if(methodName=="<init>") return true;
        if(methodName=="start"){
            auto it=g_players.find(self);
            if(it!=g_players.end()){
                it->second.playing=true;
                it->second.state=400; // STARTED
                if(!it->second.data.empty()) JvmInterpreter::getInstance().triggerMidi(it->second.data.data(), it->second.data.size());
                else if(!it->second.locator.empty()){ JvmInterpreter::getInstance().triggerTone(880,200,100); }
            }
            return true;
        }
        if(methodName=="setLoopCount"&&args.size()>=2){ g_players[self].loop=args[1].asInt(); return true; }
        if(methodName=="getControl"&&args.size()>=2){ std::string t=ENG().getString(args[1].asRef()); uint32_t r=ENG().allocObject(t.empty()?"javax/microedition/media/control/VolumeControl":t); outResult=JavaValue(r,true); return true; }
        if(methodName=="getControls"){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); return true; }
        if(methodName=="getDuration"){ outResult=JavaValue((int64_t)1000000); return true; }
        if(methodName=="getState"){
            auto it=g_players.find(self);
            outResult = JavaValue(it != g_players.end() ? it->second.state : 300);
            return true;
        }
        if(methodName=="setListener"||methodName=="addPlayerListener"||methodName=="removePlayerListener"||methodName=="setTimeBase") return true;
        return true;
    }
    if(className.find("ToneControl")!=std::string::npos){
        if(methodName=="setSequence"&&args.size()>=2){
            // JSR-135 tone sequence: byte[] version/tempo/res + notes. Schedule via triggerTone thread.
            JavaArray*a=ENG().getArray(args[1].asRef());
            std::vector<uint8_t> seq = a ? a->byteData : std::vector<uint8_t>();
            if(seq.size()>=4){
                // Best-effort parse: skip header, each note = ...
                // Format: version(1) tempo(1) res(1) ... then blocks. Simplify: bytes >=64 && <127 are notes.
                std::vector<std::pair<int,int>> notes;
                for(size_t i=4;i+1<seq.size();i+=2){
                    int n=seq[i], d=seq[i+1];
                    if(n>=0&&n<=127&&d>0&&d<128) notes.emplace_back(n,d*40);
                    if(notes.size()>64) break;
                }
                if(!notes.empty()){
                    std::thread([notes](){
                        for(auto &nt: notes){
                            int freq=(nt.first==0)?0:(int)(440.0*pow(2.0,(nt.first-69)/12.0));
                            if(freq>0) JvmInterpreter::getInstance().triggerTone(freq, std::min(nt.second,500), 80);
                            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(nt.second,500)+20));
                        }
                    }).detach();
                }
            }
            return true;
        }
        return true;
    }
    if(className.find("VolumeControl")!=std::string::npos){
        uint32_t self=args.empty()?0:args[0].asRef();
        JavaObject*o=ENG().getObject(self);
        if(methodName=="<init>"&&o){
            o->fields["level"]=JavaValue(100);
            o->fields["mute"]=JavaValue(0);
            return true;
        }
        if(methodName=="setLevel"&&args.size()>=2&&o){
            int lv=args[1].asInt();
            if(lv<0) lv=0;
            if(lv>100) lv=100;
            o->fields["level"]=JavaValue(lv);
            JvmInterpreter::getInstance().triggerTone(0,0,0); /* volume applied via AudioBridge */
            outResult=JavaValue(lv);
            return true;
        }
        if(methodName=="setMute"&&args.size()>=2&&o){ o->fields["mute"]=JavaValue(args[1].asInt()!=0?1:0); return true; }
        if(methodName=="getLevel"){
            JavaObject*oo=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            int lv=100;
            if(oo && oo->fields.count("level")) lv=oo->fields["level"].asInt();
            outResult=JavaValue(lv);
            return true;
        }
        if(methodName=="isMuted"){
            JavaObject*oo=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            outResult=JavaValue((oo && oo->fields.count("mute")) ? oo->fields["mute"].asInt() : 0);
            return true;
        }
        return true;
    }
    if(className.find("Control")!=std::string::npos){
        if(methodName=="setLevel"||methodName=="setMute"||methodName=="setRate"||methodName=="setTempo") return true;
        if(methodName=="getLevel"){ outResult=JavaValue(100); return true; }
        if(methodName=="isMuted"){ outResult=JavaValue(0); return true; }
        return true;
    }
    // ============ IO / Connector (real HTTP/nền + TCP socket thật) ============
    if(className=="javax/microedition/io/Connector"){
        if(methodName.rfind("open",0)==0&&!args.empty()){
            std::string url=ENG().getString(args[0].asRef());
            std::string kind="javax/microedition/io/Connection";
            if(url.rfind("http",0)==0) kind="javax/microedition/io/HttpConnection";
            else if(url.rfind("socket://",0)==0) kind="javax/microedition/io/SocketConnection";
            else if(url.rfind("datagram://",0)==0) kind="javax/microedition/io/DatagramConnection";
            else if(url.rfind("file://",0)==0) kind="javax/microedition/io/file/FileConnection";
            else if(url.rfind("sms://",0)==0||url.rfind("mms://",0)==0) kind="javax/wireless/messaging/MessageConnection";
            else if(url.rfind("bluetooth://",0)==0||url.rfind("btspp://",0)==0||url.rfind("btl2cap://",0)==0) kind="javax/bluetooth/L2CAPConnection";
            else if(url.rfind("capture://",0)==0) kind="javax/microedition/media/Player";
            uint32_t r=ENG().allocObject(kind); ConnData cd; cd.url=url; cd.kind=kind; g_conns[r]=cd;
            JavaObject* jo=ENG().getObject(r); if(jo) jo->stringVal=url;
            // Real TCP connect ngay (timeout 5s), lưu fd; socket://:port = server listen
            if(kind=="javax/microedition/io/SocketConnection"){
                std::string host; int port=0;
                if(parseHostPort(url, host, port)){
                    if(host.empty()){
#if !defined(_WIN32)&&!defined(_WIN64)
                        int lfd=tcpListen(port);
                        if(lfd>=0){ g_sockFd[r]=lfd; if(jo){ jo->fields["sockFd"]=JavaValue(lfd); jo->fields["isServer"]=JavaValue(1); } }
#endif
                    } else {
                        int fd=tcpConnect(host, port);
                        if(fd>=0){ g_sockFd[r]=fd; if(jo) jo->fields["sockFd"]=JavaValue(fd); }
                    }
                }
                if(hasNative((const void*)native_socket_test)) (void)native_socket_test(url.c_str());
                if(hasNative((const void*)native_background_keepalive_start)) native_background_keepalive_start();
            }
            if(kind=="javax/microedition/io/DatagramConnection"){
#if !defined(_WIN32)&&!defined(_WIN64)
                int fd=udpSocket();
                if(fd>=0){ g_sockFd[r]=fd; if(jo) jo->fields["sockFd"]=JavaValue(fd); }
#endif
            }
            if(methodName=="openInputStream"||methodName=="openDataInputStream"){
                std::vector<JavaValue> streamArgs = { JavaValue(r, true) };
                JavaValue streamResult;
                FullApis::dispatch(kind, methodName, desc, streamArgs, streamResult, display);
                outResult = streamResult;
                return true;
            }
            if(methodName=="openOutputStream"||methodName=="openDataOutputStream"){
                std::vector<JavaValue> streamArgs = { JavaValue(r, true) };
                JavaValue streamResult;
                FullApis::dispatch(kind, methodName, desc, streamArgs, streamResult, display);
                outResult = streamResult;
                return true;
            }
            outResult=JavaValue(r,true); return true;
        }
        return true;
    }
    if(className=="javax/microedition/io/HttpConnection"||className=="javax/microedition/io/HttpsConnection"||className=="javax/microedition/io/SocketConnection"||className=="javax/microedition/io/DatagramConnection"||className=="javax/microedition/io/Connection"||className=="javax/microedition/io/InputConnection"||className=="javax/microedition/io/OutputConnection"||className=="javax/microedition/io/StreamConnection"){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="getResponseCode"){ int c=200; auto it=g_conns.find(self); if(it!=g_conns.end()&&(it->second.kind=="javax/microedition/io/HttpConnection"||it->second.kind=="javax/microedition/io/HttpsConnection")) c=httpEnsure(self).code?httpEnsure(self).code:200; outResult=JavaValue(c); return true; }
        if(methodName=="getLength"||methodName=="getContentLength"){ int n=-1; auto it=g_conns.find(self); if(it!=g_conns.end()){ ConnData &c=httpEnsure(self); if(!c.body.empty()) n=(int)c.body.size(); } outResult=JavaValue(n); return true; }
        if(methodName=="getType"){ auto it=g_conns.find(self); std::string t="application/octet-stream"; if(it!=g_conns.end()) t=httpEnsure(self).mime; outResult=JavaValue(ENG().createString(t),true); return true; }
        if(methodName=="getEncoding"){ outResult=JavaValue(ENG().createString("UTF-8"),true); return true; }
        if(methodName=="getRequestMethod"){ auto it=g_conns.find(self); outResult=JavaValue(ENG().createString(it==g_conns.end()?"GET":it->second.method),true); return true; }
        if(methodName=="getHeaderField"&&args.size()>=2){ outResult=JavaValue(0,true); return true; }
        if(methodName=="acceptAndOpen"||methodName=="accept"){
            // ServerSocketConnection.acceptAndOpen(): block accept, return new SocketConnection
            int lfd=-1; auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()) lfd=sf->second;
            if(lfd<0){ JavaObject*jo=ENG().getObject(self); if(jo){ auto f=jo->fields.find("sockFd"); if(f!=jo->fields.end()) lfd=f->second.asInt(); } }
#if !defined(_WIN32)&&!defined(_WIN64)
            int cfd=tcpAccept(lfd);
#else
            int cfd=-1;
#endif
            uint32_t r=ENG().allocObject("javax/microedition/io/SocketConnection");
            ConnData cd; cd.url="socket://accepted"; cd.kind="javax/microedition/io/SocketConnection"; g_conns[r]=cd;
            if(cfd>=0){ g_sockFd[r]=cfd; JavaObject*jo=ENG().getObject(r); if(jo){ jo->stringVal=cd.url; jo->fields["sockFd"]=JavaValue(cfd); } }
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="openInputStream"||methodName=="openDataInputStream"){
            std::vector<uint8_t> body; auto it=g_conns.find(self);
            int sockFd=-1;
            if(it!=g_conns.end()){
                if(it->second.kind=="javax/microedition/io/HttpConnection"||it->second.kind=="javax/microedition/io/HttpsConnection") body=httpEnsure(self).body;
                else if(it->second.kind=="javax/microedition/io/SocketConnection"){
                    auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()) sockFd=sf->second;
                    else { JavaObject*jo=ENG().getObject(self); if(jo){ auto f=jo->fields.find("sockFd"); if(f!=jo->fields.end()) sockFd=f->second.asInt(); } }
                    if(sockFd>=0){
                        // Server socket with no client yet: accept first
                        JavaObject*jo=ENG().getObject(self);
                        bool isSrv=jo&&jo->fields["isServer"].asInt()!=0;
                        if(isSrv){
#if !defined(_WIN32)&&!defined(_WIN64)
                            int cfd=tcpAccept(sockFd);
                            if(cfd>=0){ tcpClose(sockFd); g_sockFd[self]=cfd; if(jo) jo->fields["sockFd"]=JavaValue(cfd); sockFd=cfd; }
#endif
                        }
                        body=tcpRecvOnce(sockFd);
                    }
                }
            }
            uint32_t r=ENG().allocObject("java/io/ByteArrayInputStream"); uint32_t arr=ENG().allocArray(8,(int)body.size()); JavaArray*a=ENG().getArray(arr); if(a) a->byteData=body; JavaObject*o=ENG().getObject(r); if(o){o->fields["buf"]=JavaValue(arr,true); o->fields["pos"]=JavaValue(0); if(sockFd>=0) o->fields["sockFd"]=JavaValue(sockFd);} outResult=JavaValue(r,true); return true;
        }
        if(methodName=="openDataOutputStream"||methodName=="openOutputStream"){
            int sockFd=-1; auto it=g_conns.find(self);
            if(it!=g_conns.end()&&it->second.kind=="javax/microedition/io/SocketConnection"){
                auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()) sockFd=sf->second;
            }
            uint32_t r=ENG().allocObject("java/io/ByteArrayOutputStream"); g_baos[r]={};
            // Always link stream->connection so HTTP POST bodies are collected on openInputStream
            JavaObject*o=ENG().getObject(r); if(o){ o->fields["connRef"]=JavaValue((int32_t)self); if(sockFd>=0) o->fields["sockFd"]=JavaValue(sockFd); }
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="close"){
            auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()){ tcpClose(sf->second); g_sockFd.erase(sf); }
            if(hasNative((const void*)native_background_keepalive_stop)) native_background_keepalive_stop(); return true;
        }
        if(methodName=="setRequestMethod"&&args.size()>=2){ auto it=g_conns.find(self); if(it!=g_conns.end()) it->second.method=ENG().getString(args[1].asRef()); return true; }
        if(methodName=="setRequestProperty") return true;
        if(methodName=="getRequestProperty"||methodName=="getURL"){ auto it=g_conns.find(self); outResult=JavaValue(ENG().createString(it==g_conns.end()?"":it->second.url),true); return true; }
        if(methodName=="getLocalAddress"){ outResult=JavaValue(ENG().createString("127.0.0.1"),true); return true; }
        if(methodName=="getPort"){ outResult=JavaValue(80); return true; }
        if(methodName=="getAddress"){ auto it=g_conns.find(self); outResult=JavaValue(ENG().createString(it==g_conns.end()?"":it->second.url),true); return true; }
        if(methodName=="newDatagram"&&args.size()>=2){
            // newDatagram(byte[] buf, int size[, String addr])
            uint32_t dg=ENG().allocObject("javax/microedition/io/Datagram");
            JavaObject*o=ENG().getObject(dg);
            JavaArray*src=ENG().getArray(args[1].asRef());
            int sz=args.size()>=3?args[2].asInt():(src?(int)src->byteData.size():0);
            if(o){
                uint32_t arr=ENG().allocArray(8,std::max(0,sz)); JavaArray*d=ENG().getArray(arr);
                if(d&&src) for(int i=0;i<sz&&(size_t)i<src->byteData.size()&&(size_t)i<d->byteData.size();i++) d->byteData[i]=src->byteData[i];
                o->fields["buf"]=JavaValue(arr,true); o->fields["len"]=JavaValue(sz);
                o->fields["addr"]=JavaValue(args.size()>=4?args[3].asRef():0,true);
                o->fields["sockFd"]=JavaValue((int32_t)self);
            }
            outResult=JavaValue(dg,true); return true;
        }
        if(methodName=="send"&&args.size()>=2){
#if !defined(_WIN32)&&!defined(_WIN64)
            JavaObject*dg=ENG().getObject(args[1].asRef());
            if(dg){
                JavaArray*b=ENG().getArray(dg->fields["buf"].asRef());
                int len=dg->fields["len"].asInt(); std::string addr=ENG().getString(dg->fields["addr"].asRef());
                int fd=-1; auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()) fd=sf->second;
                std::string host; int port=0;
                std::string dst=addr.empty()?g_conns[self].url:addr;
                if(fd>=0&&parseHostPort(dst,host,port)&&b){
                    struct addrinfo hints{},*res=nullptr; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_DGRAM;
                    char ps[16]; snprintf(ps,sizeof(ps),"%d",port);
                    if(getaddrinfo(host.c_str(),ps,&hints,&res)==0&&res){
                        sendto(fd,b->byteData.data(),std::min((size_t)len,b->byteData.size()),0,res->ai_addr,res->ai_addrlen);
                        freeaddrinfo(res);
                    }
                }
            }
#endif
            return true;
        }
        if(methodName=="receive"&&args.size()>=2){
#if !defined(_WIN32)&&!defined(_WIN64)
            JavaObject*dg=ENG().getObject(args[1].asRef());
            int fd=-1; auto sf=g_sockFd.find(self); if(sf!=g_sockFd.end()) fd=sf->second;
            if(dg&&fd>=0){
                fd_set rs; FD_ZERO(&rs); FD_SET(fd,&rs);
                struct timeval tv{8,0};
                if(select(fd+1,&rs,nullptr,nullptr,&tv)>0){
                    uint8_t tmp[2048]; struct sockaddr_storage from{}; socklen_t fl=sizeof(from);
                    ssize_t n=recvfrom(fd,tmp,sizeof(tmp),0,(struct sockaddr*)&from,&fl);
                    if(n>0){
                        JavaArray*b=ENG().getArray(dg->fields["buf"].asRef());
                        if(b){ if((int)b->byteData.size()<(int)n) b->byteData.resize(n); memcpy(b->byteData.data(),tmp,n); }
                        dg->fields["len"]=JavaValue((int32_t)n);
                    }
                }
            }
#endif
            return true;
        }
        if(methodName=="getMaximumLength"){ outResult=JavaValue(2048); return true; }
        if(methodName=="getNominalLength"){ outResult=JavaValue(1500); return true; }
        return true;
    }

    // ============ DragonBoy Server List Method b/ci.Gd ============
    if(className=="b/ci" && methodName=="Gd"){
        outResult = JavaValue(ENG().createString(getServerListText()), true);
        return true;
    }

    // ============ Standard Java SE Networking (Socket, URL, HttpURLConnection) ============
    if(className=="java/net/Socket"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="<init>"){
            JavaObject* so = ENG().getObject(self);
            if(so) so->fields["sockFd"] = JavaValue(-1);
            if(args.size()>=3 && args[1].type==JavaValue::OBJ_REF){
                std::string host = ENG().getString(args[1].asRef());
                int port = args[2].asInt();
                int fd = tcpConnect(host, port);
                if(fd>=0){
                    g_sockFd[self] = fd;
                    if(so) so->fields["sockFd"] = JavaValue(fd);
                }
            }
            return true;
        }
        if(methodName=="connect" && args.size()>=2){
            if(args[1].type==JavaValue::OBJ_REF){
                JavaObject* addrObj = ENG().getObject(args[1].asRef());
                std::string host = "";
                int port = 0;
                if(addrObj){
                    uint32_t hr = addrObj->fields["host"].asRef();
                    if(hr != 0) host = ENG().getString(hr);
                    port = addrObj->fields["port"].asInt();
                }
                if(!host.empty() && port > 0){
                    int fd = tcpConnect(host, port);
                    if(fd>=0){
                        g_sockFd[self] = fd;
                        JavaObject* so = ENG().getObject(self);
                        if(so) so->fields["sockFd"] = JavaValue(fd);
                    }
                }
            }
            return true;
        }
        if(methodName=="getInputStream"){
            int sockFd = -1;
            auto it = g_sockFd.find(self);
            if(it != g_sockFd.end()) sockFd = it->second;
            else { JavaObject* so = ENG().getObject(self); if(so) sockFd = so->fields["sockFd"].asInt(); }
            uint32_t r = ENG().allocObject("java/io/InputStream");
            uint32_t arr = ENG().allocArray(8, 0);
            JavaObject* o = ENG().getObject(r);
            if(o){
                o->fields["buf"] = JavaValue(arr, true);
                o->fields["pos"] = JavaValue(0);
                o->fields["sockFd"] = JavaValue(sockFd);
            }
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="getOutputStream"){
            int sockFd = -1;
            auto it = g_sockFd.find(self);
            if(it != g_sockFd.end()) sockFd = it->second;
            else { JavaObject* so = ENG().getObject(self); if(so) sockFd = so->fields["sockFd"].asInt(); }
            uint32_t r = ENG().allocObject("java/io/OutputStream");
            g_baos[r] = {};
            JavaObject* o = ENG().getObject(r);
            if(o){
                o->fields["sockFd"] = JavaValue(sockFd);
            }
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="close"){
            auto it = g_sockFd.find(self);
            if(it != g_sockFd.end()){ tcpClose(it->second); g_sockFd.erase(it); }
            JavaObject* so = ENG().getObject(self);
            if(so) so->fields["sockFd"] = JavaValue(-1);
            return true;
        }
        if(methodName=="isConnected"){
            int sockFd = -1;
            auto it = g_sockFd.find(self);
            if(it != g_sockFd.end()) sockFd = it->second;
            else { JavaObject* so = ENG().getObject(self); if(so) sockFd = so->fields["sockFd"].asInt(); }
            outResult = JavaValue(sockFd >= 0 ? 1 : 0);
            return true;
        }
        if(methodName=="isClosed"){
            int sockFd = -1;
            auto it = g_sockFd.find(self);
            if(it != g_sockFd.end()) sockFd = it->second;
            else { JavaObject* so = ENG().getObject(self); if(so) sockFd = so->fields["sockFd"].asInt(); }
            outResult = JavaValue(sockFd < 0 ? 1 : 0);
            return true;
        }
        if(methodName=="setSoTimeout"||methodName=="setTcpNoDelay"||methodName=="setKeepAlive") return true;
        if(methodName=="getInetAddress"){
            uint32_t r = ENG().allocObject("java/net/InetAddress");
            outResult = JavaValue(r, true);
            return true;
        }
        return true;
    }
    if(className=="java/net/InetSocketAddress"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="<init>"){
            JavaObject* so = ENG().getObject(self);
            if(so && args.size() >= 3){
                so->fields["host"] = args[1];
                so->fields["port"] = args[2];
            }
            return true;
        }
        if(methodName=="getHostName"||methodName=="getHostString"){
            JavaObject* so = ENG().getObject(self);
            outResult = so ? so->fields["host"] : JavaValue(ENG().createString(""), true);
            return true;
        }
        if(methodName=="getPort"){
            JavaObject* so = ENG().getObject(self);
            outResult = so ? so->fields["port"] : JavaValue(0);
            return true;
        }
        return true;
    }
    if(className=="java/net/InetAddress"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="getByName"||methodName=="getAllByName"||methodName=="getLocalHost"){
            uint32_t r = ENG().allocObject("java/net/InetAddress");
            JavaObject* o = ENG().getObject(r);
            if(o && !args.empty() && args[0].type==JavaValue::OBJ_REF) o->fields["host"] = args[0];
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="getHostAddress"||methodName=="getHostName"){
            JavaObject* so = ENG().getObject(self);
            outResult = so ? so->fields["host"] : JavaValue(ENG().createString("127.0.0.1"), true);
            return true;
        }
        return true;
    }
    if(className=="java/net/URL"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="<init>"){
            JavaObject* so = ENG().getObject(self);
            if(so && args.size() >= 2 && args[1].type==JavaValue::OBJ_REF){
                so->stringVal = ENG().getString(args[1].asRef());
            }
            return true;
        }
        if(methodName=="openConnection"){
            JavaObject* so = ENG().getObject(self);
            std::string url = so ? so->stringVal : "";
            uint32_t conn = ENG().allocObject("java/net/HttpURLConnection");
            JavaObject* co = ENG().getObject(conn);
            if(co) co->stringVal = url;
            outResult = JavaValue(conn, true);
            return true;
        }
        return true;
    }
    if(className=="java/net/HttpURLConnection"||className=="java/net/URLConnection"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="setRequestMethod"||methodName=="setRequestProperty"||methodName=="connect"||methodName=="setConnectTimeout"||methodName=="setReadTimeout"||methodName=="setDoInput"||methodName=="setDoOutput"||methodName=="disconnect"||methodName=="close") return true;
        if(methodName=="getResponseCode"){ outResult = JavaValue(200); return true; }
        if(methodName=="getInputStream"||methodName=="getErrorStream"){
            JavaObject* so = ENG().getObject(self);
            std::string url = so ? so->stringVal : "";
            std::vector<uint8_t> body;
            if(!url.empty()){
                if(url.find("ServerListScreen") != std::string::npos || url.find("raw.githubusercontent.com") != std::string::npos){
                    std::string s = getServerListText();
                    body.assign(s.begin(), s.end());
                } else {
                    body = fetchHttpSync(url);
                }
            }
            if(body.empty() && (url.find("ServerListScreen") != std::string::npos || url.find("raw.githubusercontent.com") != std::string::npos)){
                std::string fallback = getServerListText();
                body.assign(fallback.begin(), fallback.end());
            }
            uint32_t r = ENG().allocObject("java/io/InputStream");
            uint32_t arr = ENG().allocArray(8, (int)body.size());
            JavaArray* a = ENG().getArray(arr);
            if(a) a->byteData = std::move(body);
            JavaObject* o = ENG().getObject(r);
            if(o){
                o->fields["buf"] = JavaValue(arr, true);
                o->fields["pos"] = JavaValue(0);
            }
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="getOutputStream"){
            uint32_t r = ENG().allocObject("java/io/OutputStream");
            g_baos[r] = {};
            outResult = JavaValue(r, true);
            return true;
        }
        return true;
    }

    // ============ java/util/Scanner ============
    if(className=="java/util/Scanner"){
        uint32_t self = args.empty() ? 0 : args[0].asRef();
        if(methodName=="<init>"){
            JavaObject* so = ENG().getObject(self);
            std::string fullText = "";
            if(so && args.size() >= 2 && args[1].type == JavaValue::OBJ_REF){
                JavaObject* inStream = ENG().getObject(args[1].asRef());
                if(inStream){
                    JavaArray* ba = ENG().getArray(inStream->fields["buf"].asRef());
                    int pos = inStream->fields["pos"].asInt();
                    if(ba && pos >= 0 && pos < (int)ba->byteData.size()){
                        fullText = std::string((char*)(ba->byteData.data() + pos), ba->byteData.size() - pos);
                        inStream->fields["pos"] = JavaValue((int)ba->byteData.size());
                    }
                }
            }
            if(so){
                so->stringVal = fullText;
                so->fields["scanPos"] = JavaValue(0);
            }
            return true;
        }
        if(methodName=="hasNext"){
            JavaObject* so = ENG().getObject(self);
            int pos = so ? so->fields["scanPos"].asInt() : 0;
            int len = so ? (int)so->stringVal.size() : 0;
            while(pos < len && (uint8_t)so->stringVal[pos] <= 32) pos++;
            if(so) so->fields["scanPos"] = JavaValue(pos);
            outResult = JavaValue(pos < len ? 1 : 0);
            return true;
        }
        if(methodName=="hasNextLine"){
            JavaObject* so = ENG().getObject(self);
            int pos = so ? so->fields["scanPos"].asInt() : 0;
            int len = so ? (int)so->stringVal.size() : 0;
            outResult = JavaValue(pos < len ? 1 : 0);
            return true;
        }
        if(methodName=="next"){
            JavaObject* so = ENG().getObject(self);
            int pos = so ? so->fields["scanPos"].asInt() : 0;
            int len = so ? (int)so->stringVal.size() : 0;
            while(pos < len && (uint8_t)so->stringVal[pos] <= 32) pos++;
            int start = pos;
            while(pos < len && (uint8_t)so->stringVal[pos] > 32) pos++;
            std::string token = (start < len) ? so->stringVal.substr(start, pos - start) : "";
            if(so) so->fields["scanPos"] = JavaValue(pos);
            outResult = JavaValue(ENG().createString(token), true);
            return true;
        }
        if(methodName=="nextLine"){
            JavaObject* so = ENG().getObject(self);
            int pos = so ? so->fields["scanPos"].asInt() : 0;
            int len = so ? (int)so->stringVal.size() : 0;
            int start = pos;
            while(pos < len && so->stringVal[pos] != '\n' && so->stringVal[pos] != '\r') pos++;
            std::string line = (start <= len) ? so->stringVal.substr(start, pos - start) : "";
            if(pos < len && so->stringVal[pos] == '\r') pos++;
            if(pos < len && so->stringVal[pos] == '\n') pos++;
            if(so) so->fields["scanPos"] = JavaValue(pos);
            outResult = JavaValue(ENG().createString(line), true);
            return true;
        }
        if(methodName=="useDelimiter"||methodName=="close") return true;
        return true;
    }

    // ============ java/nio/charset/Charset & StandardCharsets ============
    if(className=="java/nio/charset/Charset"||className=="java/nio/charset/StandardCharsets"){
        if(methodName=="forName"||methodName=="defaultCharset"){
            uint32_t r = ENG().allocObject("java/nio/charset/Charset");
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="name"){
            outResult = JavaValue(ENG().createString("UTF-8"), true);
            return true;
        }
        return true;
    }

    if(className.rfind("javax/microedition/io/file/",0)==0){
        // Real FileConnection: JAR entries first, then local filesystem (Documents/Games shared via RMS dir)
        auto fileUrlOf = [&](uint32_t self)->std::string{
            JavaObject*o=ENG().getObject(self); if(o&&!o->stringVal.empty()) return o->stringVal;
            auto it=g_conns.find(self); if(it!=g_conns.end()) return it->second.url;
            return "file:///root/";
        };
        auto localPathOf = [&](const std::string& url)->std::string{
            std::string p=url;
            if(p.rfind("file://",0)==0) p=p.substr(7);
            // Normalize: /root/xxx -> xxx, /SDCard/xxx -> xxx, root/xxx -> xxx
            if(p.rfind("/root/",0)==0) p=p.substr(6);
            else if(p.rfind("/SDCard/",0)==0) p=p.substr(8);
            else if(p.rfind("root/",0)==0) p=p.substr(5);
            else if(p.rfind("SDCard/",0)==0) p=p.substr(7);
            if(!p.empty()&&p[0]=='/') p.erase(0,1);
            return p;
        };
        if(methodName=="<init>"&&!args.empty()){
            uint32_t self=args[0].asRef();
            // Connector.open already created conn; FileConnection.<init>(String) may carry url in args[1]
            if(args.size()>=2&&args[1].type==JavaValue::OBJ_REF){
                std::string u=ENG().getString(args[1].asRef());
                if(!u.empty()){ g_conns[self]={u,"javax/microedition/io/file/FileConnection"}; JavaObject*o=ENG().getObject(self); if(o) o->stringVal=u; }
            }
            return true;
        }
        if(methodName=="listRoots"){ uint32_t arr=ENG().allocArray(0,2); JavaArray*a=ENG().getArray(arr); if(a){ a->refData[0]=ENG().createString("root/"); a->refData[1]=ENG().createString("SDCard/"); } outResult=JavaValue(arr,true); return true; }
        if(methodName=="list"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            std::vector<std::string> names;
            // JAR entries under prefix
            if(ENG().getJarLoader()){
                for(auto &e: ENG().getJarLoader()->listEntries()){
                    if(e.rfind(lp,0)==0){
                        std::string rest=e.substr(lp.size());
                        auto sl=rest.find('/'); names.push_back(sl==std::string::npos?rest:rest.substr(0,sl+1));
                    }
                }
            }
            std::sort(names.begin(),names.end()); names.erase(std::unique(names.begin(),names.end()),names.end());
            uint32_t arr=ENG().allocArray(0,(int)names.size()); JavaArray*a=ENG().getArray(arr);
            if(a) for(size_t i=0;i<names.size();i++) a->refData[i]=ENG().createString(names[i]);
            outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="exists"||methodName=="isDirectory"||methodName=="canRead"||methodName=="canWrite"||methodName=="isHidden"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            bool isDir = (!url.empty()&&url.back()=='/');
            bool ex=false, dir=isDir;
            if(ENG().getJarLoader()){
                if(ENG().getJarLoader()->hasEntry(lp)) ex=true;
                else { for(auto &e: ENG().getJarLoader()->listEntries()) if(e.rfind(lp,0)==0){ ex=true; dir=true; break; } }
            }
            if(!ex){
                struct stat st;
                if(stat(lp.c_str(), &st) == 0){
                    ex = true;
                    if(st.st_mode & S_IFDIR) dir = true;
                    else dir = false;
                }
            }
            if(methodName=="exists") outResult=JavaValue(ex?1:0);
            else if(methodName=="isDirectory") outResult=JavaValue(dir?1:0);
            else if(methodName=="isHidden") outResult=JavaValue(0);
            else outResult=JavaValue(ex?1:0);
            return true;
        }
        if(methodName=="create"||methodName=="mkdir"||methodName=="delete"||methodName=="close"||methodName=="setReadable"||methodName=="setWritable"||methodName=="truncate"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            if(methodName=="create"){
                FILE* fw = fopen(lp.c_str(), "wb");
                if(fw) fclose(fw);
            } else if(methodName=="mkdir"){
                fs_mkdir(lp.c_str());
            } else if(methodName=="delete"){
                ::remove(lp.c_str());
            }
            return true;
        }
        if(methodName=="lastModified"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            int64_t lm = 0;
            struct stat st;
            if(stat(lp.c_str(), &st) == 0){
                lm = (int64_t)st.st_mtime * 1000LL;
            }
            outResult = JavaValue(lm);
            return true;
        }
        if(methodName=="fileSize"||methodName=="availableSize"||methodName=="totalSize"||methodName=="directorySize"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            int64_t sz=0;
            if(ENG().getJarLoader()){
                std::vector<uint8_t> b; if(ENG().getJarLoader()->extractEntry(lp,b)) sz=b.size();
            }
            if(sz==0){ FILE*f=fopen(lp.c_str(),"rb"); if(f){ fseek(f,0,SEEK_END); sz=ftell(f); fclose(f);} }
            if(methodName=="fileSize") outResult=JavaValue(sz);
            else outResult=JavaValue((int64_t)(64LL<<20));
            return true;
        }
        if(methodName=="openInputStream"||methodName=="openDataInputStream"){
            uint32_t self=args.empty()?0:args[0].asRef();
            std::string url=fileUrlOf(self); std::string lp=localPathOf(url);
            std::vector<uint8_t> data;
            if(ENG().getJarLoader()) ENG().getJarLoader()->extractEntry(lp,data);
            if(data.empty()){ FILE*f=fopen(lp.c_str(),"rb"); if(f){ fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET); if(n>0&&n<(10<<20)){ data.resize(n); fread(data.data(),1,n,f);} fclose(f);} }
            uint32_t r=ENG().allocObject("java/io/ByteArrayInputStream"); uint32_t arr=ENG().allocArray(8,(int)data.size()); JavaArray*a=ENG().getArray(arr); if(a) a->byteData=data; JavaObject*o=ENG().getObject(r); if(o){o->fields["buf"]=JavaValue(arr,true); o->fields["pos"]=JavaValue(0);} outResult=JavaValue(r,true); return true;
        }
        if(methodName=="openOutputStream"||methodName=="openDataOutputStream"){
            uint32_t r=ENG().allocObject("java/io/ByteArrayOutputStream"); g_baos[r]={};
            JavaObject*o=ENG().getObject(r); uint32_t self=args.empty()?0:args[0].asRef();
            if(o){ o->fields["fileUrl"]=JavaValue(ENG().createString(fileUrlOf(self)),true); }
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="getName"){ uint32_t self=args.empty()?0:args[0].asRef(); std::string u=fileUrlOf(self); auto s=u.find_last_of('/'); outResult=JavaValue(ENG().createString(s==std::string::npos?u:u.substr(s+1)),true); return true; }
        if(methodName=="getPath"||methodName=="getURL"){ uint32_t self=args.empty()?0:args[0].asRef(); outResult=JavaValue(ENG().createString(fileUrlOf(self)),true); return true; }
        return true;
    }
    if(className.rfind("javax/wireless/messaging/",0)==0){
        if(methodName=="<init>") return true;
        if(methodName=="newMessage"&&args.size()>=2){ std::string t=args.size()>=2?ENG().getString(args[1].asRef()):""; uint32_t r=ENG().allocObject(className.find("MessageConnection")!=std::string::npos?"javax/wireless/messaging/TextMessage":className); JavaObject*o=ENG().getObject(r); if(o) o->stringVal=t; outResult=JavaValue(r,true); return true; }
        if(methodName=="send"&&args.size()>=2){
            // Loopback cùng máy để game SMS 1 người chơi tiếp tục được; carrier thật cần UI
            bool can = hasNative((const void*)native_can_send_text) ? native_can_send_text() : false;
            (void)can;
            JavaObject*msg=ENG().getObject(args[1].asRef());
            if(msg){
                WmaMsg m; m.isText=(msg->className.find("TextMessage")!=std::string::npos);
                m.addr=ENG().getString(msg->fields["addr"].asRef());
                if(m.isText) m.text=ENG().getString(msg->fields["payload"].asRef());
                else { JavaArray*b=ENG().getArray(msg->fields["payload"].asRef()); if(b) m.bin=b->byteData; }
                if(m.text.empty()&&!msg->stringVal.empty()) m.text=msg->stringVal;
                std::lock_guard<std::mutex> lk(g_wmaMutex);
                if(g_wmaInbox.size()<32) g_wmaInbox.push_back(std::move(m));
            }
            return true;
        }
        if(methodName=="receive"){
            std::lock_guard<std::mutex> lk(g_wmaMutex);
            if(!g_wmaInbox.empty()){
                WmaMsg m=std::move(g_wmaInbox.front()); g_wmaInbox.erase(g_wmaInbox.begin());
                uint32_t r=ENG().allocObject(m.isText?"javax/wireless/messaging/TextMessage":"javax/wireless/messaging/BinaryMessage");
                JavaObject*o=ENG().getObject(r);
                if(o){ o->fields["addr"]=JavaValue(ENG().createString(m.addr),true);
                    if(m.isText){ o->fields["payload"]=JavaValue(ENG().createString(m.text),true); o->stringVal=m.text; }
                    else { uint32_t arr=ENG().allocArray(8,(int)m.bin.size()); JavaArray*a=ENG().getArray(arr); if(a) a->byteData=m.bin; o->fields["payload"]=JavaValue(arr,true); } }
                outResult=JavaValue(r,true);
            } else outResult=JavaValue(0,true);
            return true;
        }
        if(methodName=="close"||methodName=="setMessageListener") return true;
        if(methodName=="numberOfSegments"){ outResult=JavaValue(1); return true; }
        if(methodName=="getPayloadText"){
            JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            std::string v=o?ENG().getString(o->fields["payload"].asRef()):"";
            if(v.empty()&&o) v=o->stringVal;
            outResult=JavaValue(ENG().createString(v),true); return true;
        }
        if(methodName=="getAddress"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["addr"]:JavaValue(ENG().createString(""),true)); return true; }
        if(methodName=="setPayloadText"&&args.size()>=2){ JavaObject*o=ENG().getObject(args[0].asRef()); if(o){ o->fields["payload"]=args[1]; o->stringVal=ENG().getString(args[1].asRef()); } return true; }
        if(methodName=="setAddress"&&args.size()>=2){ JavaObject*o=ENG().getObject(args[0].asRef()); if(o) o->fields["addr"]=args[1]; return true; }
        if(methodName=="setPayloadData"&&args.size()>=2){ JavaObject*o=ENG().getObject(args[0].asRef()); if(o) o->fields["payload"]=args[1]; return true; }
        if(methodName=="getPayloadData"){
            JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef());
            JavaArray*a=o?ENG().getArray(o->fields["payload"].asRef()):nullptr;
            if(a){ outResult=JavaValue(o->fields["payload"].asRef(),true); }
            else { uint32_t arr=ENG().allocArray(8,0); outResult=JavaValue(arr,true); }
            return true;
        }
        if(methodName=="getTimestamp"){ outResult=JavaValue((int64_t)0); return true; }
        return true;
    }
    // ---- VideoControl snapshot thật (capture://video) ----
    if(className.find("VideoControl")!=std::string::npos){
        if(methodName=="getSnapshot"&&!args.empty()){
            std::vector<uint8_t> png;
            if(hasNative((const void*)native_camera_snapshot)){
                uint8_t *d=nullptr; int n=0;
                if(native_camera_snapshot(&d,&n) && d && n>0) png.assign(d,d+n);
                if(d) { if(hasNative((const void*)native_free)) native_free(d); else free(d); }
            }
            uint32_t arr=ENG().allocArray(8,(int)png.size()); JavaArray*a=ENG().getArray(arr); if(a) a->byteData=png;
            outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="getDisplayWidth"){ outResult=JavaValue(display?display->getWidth():240); return true; }
        if(methodName=="getDisplayHeight"){ outResult=JavaValue(display?display->getHeight():320); return true; }
        return true;
    }
    // ============ RMS extended ============
    if(className=="javax/microedition/rms/RecordStore"){
        if(methodName=="listRecordStores"){
            auto list = RmsStorage::getInstance().listRecordStores(JvmInterpreter::getInstance().getSuiteName());
            uint32_t arr = ENG().allocArray(0, (int)list.size());
            JavaArray* ja = ENG().getArray(arr);
            if (ja) {
                for (size_t i = 0; i < list.size(); ++i) {
                    ja->refData[i] = ENG().createString(list[i]);
                }
            }
            outResult = JavaValue(arr, true);
            return true;
        }
        if(methodName=="deleteRecordStore"&&args.size()>=1){
            std::string name = (args[0].asRef() != 0) ? ENG().getString(args[0].asRef()) : "";
            if(!name.empty()) RmsStorage::getInstance().deleteRecordStore(JvmInterpreter::getInstance().getSuiteName(), name);
            return true;
        }
        if(methodName=="closeRecordStore"&&args.size()>=1){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            if (obj && !obj->stringVal.empty()) RmsStorage::getInstance().closeRecordStore(obj->stringVal);
            return true;
        }
        if(methodName=="getSize"){
            JavaObject* obj = (!args.empty() && args[0].asRef()) ? ENG().getObject(args[0].asRef()) : nullptr;
            int sz = (obj && !obj->stringVal.empty()) ? RmsStorage::getInstance().getSize(obj->stringVal) : 0;
            outResult = JavaValue(sz);
            return true;
        }
        if(methodName=="getSizeAvailable"){
            JavaObject* obj = (!args.empty() && args[0].asRef()) ? ENG().getObject(args[0].asRef()) : nullptr;
            int sz = (obj && !obj->stringVal.empty()) ? RmsStorage::getInstance().getSize(obj->stringVal) : 0;
            outResult = JavaValue(std::max(0, 32 * 1024 * 1024 - sz));
            return true;
        }
        if(methodName=="getVersion"||methodName=="getLastModified"){ outResult=JavaValue(1); return true; }
        if(methodName=="setRecord"&&args.size()>=5){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            int recId = args[1].asInt();
            JavaArray* arr = ENG().getArray(args[2].asRef());
            int off = args[3].asInt(), len = args[4].asInt();
            if (obj && arr && off >= 0 && off + len <= (int)arr->byteData.size()) {
                RmsStorage::getInstance().setRecord(obj->stringVal, recId, arr->byteData.data() + off, len);
            }
            return true;
        }
        if(methodName=="deleteRecord"&&args.size()>=2){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            int recId = args[1].asInt();
            if (obj) RmsStorage::getInstance().deleteRecord(obj->stringVal, recId);
            return true;
        }
        if(methodName=="addRecordListener"||methodName=="removeRecordListener") return true;
        if(methodName=="enumerateRecords"&&args.size()>=1){
            JavaObject* rsObj = ENG().getObject(args[0].asRef());
            std::string storeName = rsObj ? rsObj->stringVal : "";
            uint32_t r = ENG().allocObject("javax/microedition/rms/RecordEnumeration");
            RecordEnumState state;
            state.storeName = storeName;
            state.recordIds = RmsStorage::getInstance().getRecordIds(storeName);
            state.cursor = 0;
            {
                std::lock_guard<std::mutex> lk(g_enumMutex);
                g_recordEnums[r] = std::move(state);
            }
            outResult = JavaValue(r, true);
            return true;
        }
        if(methodName=="getRecord"){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            int recId = args.size() >= 2 ? args[1].asInt() : 0;
            std::vector<uint8_t> data;
            if (obj && RmsStorage::getInstance().getRecord(obj->stringVal, recId, data)) {
                if (args.size() >= 4) {
                    JavaArray* dst = ENG().getArray(args[2].asRef());
                    int off = args[3].asInt();
                    if (dst && off >= 0) {
                        int copyLen = std::min((int)data.size(), (int)dst->byteData.size() - off);
                        for (int i = 0; i < copyLen; ++i) dst->byteData[off + i] = data[i];
                        outResult = JavaValue((int32_t)data.size());
                    } else {
                        outResult = JavaValue(0);
                    }
                } else {
                    uint32_t arrRef = ENG().allocArray(8, (int)data.size());
                    JavaArray* arr = ENG().getArray(arrRef);
                    if (arr) arr->byteData = std::move(data);
                    outResult = JavaValue(arrRef, true);
                }
            } else {
                outResult = (args.size() >= 4) ? JavaValue(0) : JavaValue(0, true);
            }
            return true;
        }
        if(methodName=="getNextRecordID"&&args.size()>=1){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            outResult = JavaValue(obj ? RmsStorage::getInstance().getNextRecordID(obj->stringVal) : 1);
            return true;
        }
        if(methodName=="getRecordSize"&&args.size()>=2){
            JavaObject* obj = ENG().getObject(args[0].asRef());
            int recId = args[1].asInt();
            outResult = JavaValue(obj ? RmsStorage::getInstance().getRecordSize(obj->stringVal, recId) : 0);
            return true;
        }
        return true;
    }
    if(className.find("RecordEnumeration")!=std::string::npos){
        uint32_t self = args.empty()?0:args[0].asRef();
        std::lock_guard<std::mutex> lk(g_enumMutex);
        auto it = g_recordEnums.find(self);
        if(methodName=="hasNextElement"){
            bool has = (it != g_recordEnums.end() && it->second.cursor < it->second.recordIds.size());
            outResult = JavaValue(has ? 1 : 0);
            return true;
        }
        if(methodName=="hasPreviousElement"){
            bool has = (it != g_recordEnums.end() && it->second.cursor > 0);
            outResult = JavaValue(has ? 1 : 0);
            return true;
        }
        if(methodName=="nextRecordId"){
            int id = (it != g_recordEnums.end() && it->second.cursor < it->second.recordIds.size()) ? it->second.recordIds[it->second.cursor++] : 0;
            outResult = JavaValue(id);
            return true;
        }
        if(methodName=="nextRecord"){
            int id = (it != g_recordEnums.end() && it->second.cursor < it->second.recordIds.size()) ? it->second.recordIds[it->second.cursor++] : 0;
            std::vector<uint8_t> data;
            if (it != g_recordEnums.end() && id > 0 && RmsStorage::getInstance().getRecord(it->second.storeName, id, data)) {
                uint32_t aRef = ENG().allocArray(8, (int)data.size());
                JavaArray* a = ENG().getArray(aRef);
                if (a) a->byteData = std::move(data);
                outResult = JavaValue(aRef, true);
            } else {
                outResult = JavaValue(0, true);
            }
            return true;
        }
        if(methodName=="previousRecordId"){
            int id = (it != g_recordEnums.end() && it->second.cursor > 0) ? it->second.recordIds[--it->second.cursor] : 0;
            outResult = JavaValue(id);
            return true;
        }
        if(methodName=="previousRecord"){
            int id = (it != g_recordEnums.end() && it->second.cursor > 0) ? it->second.recordIds[--it->second.cursor] : 0;
            std::vector<uint8_t> data;
            if (it != g_recordEnums.end() && id > 0 && RmsStorage::getInstance().getRecord(it->second.storeName, id, data)) {
                uint32_t aRef = ENG().allocArray(8, (int)data.size());
                JavaArray* a = ENG().getArray(aRef);
                if (a) a->byteData = std::move(data);
                outResult = JavaValue(aRef, true);
            } else {
                outResult = JavaValue(0, true);
            }
            return true;
        }
        if(methodName=="numRecords"){
            outResult = JavaValue(it != g_recordEnums.end() ? (int32_t)it->second.recordIds.size() : 0);
            return true;
        }
        if(methodName=="reset"){
            if (it != g_recordEnums.end()) it->second.cursor = 0;
            return true;
        }
        if(methodName=="destroy"){
            g_recordEnums.erase(self);
            return true;
        }
        if(methodName=="keepUpdated") return true;
        return true;
    }
    // ============ Bluetooth thật (CoreBluetooth) ============
    if(className=="javax/bluetooth/LocalDevice"){
        if(methodName=="<init>"||methodName=="<clinit>") return true;
        if(methodName=="getLocalDevice"){ uint32_t r=ENG().allocObject(className); outResult=JavaValue(r,true); return true; }
        if(methodName=="isPowerOn"){ int s=hasNative((const void*)native_bluetooth_state)?native_bluetooth_state():0; outResult=JavaValue(s==1?1:0); return true; }
        if(methodName=="getBluetoothAddress"){ outResult=JavaValue(ENG().createString("000000000000"),true); return true; }
        if(methodName=="getFriendlyName"){ outResult=JavaValue(ENG().createString("iPhone"),true); return true; }
        if(methodName=="getDiscoverable"){ outResult=JavaValue(0); return true; }
        if(methodName=="getDiscoveryAgent"){ uint32_t r=ENG().allocObject("javax/bluetooth/DiscoveryAgent"); outResult=JavaValue(r,true); return true; }
        return true;
    }
    if(className=="javax/bluetooth/DiscoveryAgent"){
        if(methodName=="<init>") return true;
        if(methodName=="startInquiry"&&!args.empty()){
            int n=0;
            if(hasNative((const void*)native_bluetooth_scan)){ char buf[2048]={0}; n=native_bluetooth_scan(3,buf,sizeof(buf)); }
            outResult=JavaValue(n>0?1:0); return true;
        }
        if(methodName=="retrieveDevices"){
            // Return last scan as RemoteDevice array (names as string objects for simplicity)
            char buf[2048]={0}; int n=0;
            if(hasNative((const void*)native_bluetooth_scan)){ /* no rescan, return cached empty to avoid block */ }
            uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="selectService"){
            uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="cancelInquiry"||methodName=="cancelServiceSearch") return true;
        return true;
    }
    // ============ Location thật (CoreLocation + request) ============
    if(className=="javax/microedition/location/LocationProvider"||className=="javax/microedition/location/Location"){
        if(methodName=="<init>") return true;
        if(methodName=="getInstance"){ if(hasNative((const void*)native_location_request)) native_location_request(); uint32_t r=ENG().allocObject("javax/microedition/location/LocationProvider"); outResult=JavaValue(r,true); return true; }
        if(methodName=="getLocation"||methodName=="getLastKnownLocation"){
            double la=0,lo=0; float ac=0; bool ok=hasNative((const void*)native_location_get)?native_location_get(&la,&lo,&ac):false;
            uint32_t r=ENG().allocObject("javax/microedition/location/Location");
            JavaObject*o=ENG().getObject(r); if(o){ o->fields["lat"]=JavaValue(la); o->fields["lon"]=JavaValue(lo); o->fields["acc"]=JavaValue(ac); o->fields["valid"]=JavaValue(ok?1:0); }
            outResult=JavaValue(r,true); return true;
        }
        if(methodName=="getLatitude"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); double v=o?o->fields["lat"].asDouble():0; outResult=JavaValue(v); return true; }
        if(methodName=="getLongitude"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); double v=o?o->fields["lon"].asDouble():0; outResult=JavaValue(v); return true; }
        if(methodName=="isValid"){ JavaObject*o=args.empty()?nullptr:ENG().getObject(args[0].asRef()); outResult=JavaValue(o?o->fields["valid"].asInt():0); return true; }
        if(methodName=="getState"){ outResult=JavaValue(2); return true; }
        return true;
    }
    if(className=="javax/microedition/location/Coordinates"){
        if(methodName=="<init>") return true;
        if(methodName=="getLatitude"||methodName=="getLongitude"){ outResult=JavaValue(0.0); return true; }
        return true;
    }
    // ============ PIM thật (Contacts/EventKit + request) ============
    if(className=="javax/microedition/pim/PIM"){
        if(methodName=="getInstance"){ uint32_t r=ENG().allocObject(className); outResult=JavaValue(r,true); return true; }
        if(methodName=="listPIMLists"){
            if(hasNative((const void*)native_contacts_request)) native_contacts_request();
            if(hasNative((const void*)native_calendar_request)) native_calendar_request();
            uint32_t arr=ENG().allocArray(0,2); JavaArray*a=ENG().getArray(arr); if(a){ a->refData[0]=ENG().createString("Contacts"); a->refData[1]=ENG().createString("Events"); } outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="openPIMList"&&args.size()>=3){
            if(hasNative((const void*)native_contacts_request)) native_contacts_request();
            uint32_t r=ENG().allocObject("javax/microedition/pim/ContactList"); outResult=JavaValue(r,true); return true;
        }
        return true;
    }
    if(className.find("ContactList")!=std::string::npos||className.find("PIMList")!=std::string::npos){
        if(methodName=="<init>") return true;
        if(methodName=="size"){
            int n=hasNative((const void*)native_contacts_count)?native_contacts_count():0;
            if(n==0&&hasNative((const void*)native_contacts_request)){ native_contacts_request(); n=hasNative((const void*)native_contacts_count)?native_contacts_count():0; }
            if(n<0) n=0; outResult=JavaValue(n); return true;
        }
        if(methodName=="items"||methodName=="getNames"){
            int n=hasNative((const void*)native_contacts_count)?native_contacts_count():0; if(n<0) n=0; n=std::min(n,50);
            uint32_t arr=ENG().allocArray(0,n); JavaArray*a=ENG().getArray(arr);
            if(a) for(int i=0;i<n;i++){
                char nm[128]={0}, ph[64]={0};
                bool ok=hasNative((const void*)native_contact_get)?native_contact_get(i,nm,sizeof(nm),ph,sizeof(ph)):false;
                uint32_t cr=ENG().allocObject("javax/microedition/pim/Contact");
                JavaObject*co=ENG().getObject(cr); if(co){ co->stringVal=ok?nm:"Contact"; co->fields["name"]=JavaValue(ENG().createString(ok?nm:""),true); co->fields["phone"]=JavaValue(ENG().createString(ok?ph:""),true); co->fields["idx"]=JavaValue(i); }
                a->refData[i]=cr;
            }
            outResult=JavaValue(arr,true); return true;
        }
        if(methodName=="close"||methodName=="removeContact"||methodName=="addContact") return true;
        return true;
    }
    if(className.find("Contact")!=std::string::npos&&className.find("List")==std::string::npos){
        uint32_t self=args.empty()?0:args[0].asRef();
        if(methodName=="<init>") return true;
        if(methodName=="getString"||methodName=="getField"){
            JavaObject*o=ENG().getObject(self);
            std::string v=o?ENG().getString(o->fields["name"].asRef()):"";
            if(v.empty()&&o) v=o->stringVal;
            // Refresh from native by idx if empty
            if(v.empty()&&o&&hasNative((const void*)native_contact_get)){
                int idx=o->fields["idx"].asInt(); char nm[128]={0}, ph[64]={0};
                if(native_contact_get(idx,nm,sizeof(nm),ph,sizeof(ph))) v=nm;
            }
            outResult=JavaValue(ENG().createString(v),true); return true;
        }
        if(methodName=="getFields"||methodName=="getAttributes") return true;
        std::string ret=returnTypeOf(desc); if(ret=="V") return true;
        if(ret=="Z"||ret=="I"||ret=="B"){ outResult=JavaValue(0); return true; }
        if(!ret.empty()&&ret[0]=='L'){ outResult=JavaValue(ENG().createString(""),true); return true; }
        return true;
    }
    // ============ Bluetooth / OBEX / Sensor / AMMS / PKI còn lại ============
    if(className.rfind("javax/bluetooth/",0)==0||className.rfind("javax/obex/",0)==0||className.rfind("javax/microedition/sensor/",0)==0||className.rfind("javax/microedition/amms/",0)==0||className.rfind("javax/microedition/pki/",0)==0||className.rfind("javax/microedition/util/",0)==0){
        if(methodName=="<init>"||methodName=="<clinit>") return true;
        std::string ret=returnTypeOf(desc);
        if(ret=="V") return true;
        if(ret=="Z"){outResult=JavaValue(0);return true;}
        if(ret=="I"||ret=="B"||ret=="S"||ret=="C"){outResult=JavaValue(0);return true;}
        if(ret=="J"){outResult=JavaValue((int64_t)0);return true;}
        if(ret=="F"){outResult=JavaValue(0.0f);return true;}
        if(ret=="D"){outResult=JavaValue(0.0);return true;}
        if(!ret.empty()&&(ret[0]=='L'||ret[0]=='[')){ std::string ot=objectTypeOf(ret); if(ot.empty()) ot="java/lang/Object"; if(ret[0]=='['){ uint32_t arr=ENG().allocArray(0,0); outResult=JavaValue(arr,true);} else { uint32_t r=ENG().allocObject(ot); outResult=JavaValue(r,true);} return true; }
        return true;
    }
    return false;
}
