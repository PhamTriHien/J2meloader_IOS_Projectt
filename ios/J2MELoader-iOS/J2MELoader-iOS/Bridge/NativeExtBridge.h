#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Real iOS native extensions for J2ME JSRs.
// C API callable from C++ (j2me_full_apis.cpp). All functions are
// synchronous with timeouts so the JVM thread stays simple.
// Memory returned via outData must be released with native_free().

#ifdef __cplusplus
extern "C" {
#endif

void native_free(void *p);

// HTTP(S) foreground + background URLSession (javax.microedition.io.HttpConnection)
// Returns true on HTTP complete (even 404). outCode = HTTP status, outType = MIME.
bool native_http_fetch(const char *url, const char *method,
                       uint8_t **outData, int *outLen,
                       int *outCode, char *outType, int typeCap);

// Socket/Datagram reachability check (socket://, datagram://)
bool native_socket_test(const char *url);

// Bluetooth LE (JSR-82 javax.bluetooth): 0=off/unsupported,1=on,2=unauthorized
int native_bluetooth_state(void);
// Scan BLE peripherals timeoutSec, returns count, names joined by '\n' into outNames
int native_bluetooth_scan(int timeoutSec, char *outNames, int cap);

// Location (JSR-179): single fix, timeout ~8s. Returns true if fix.
bool native_location_get(double *lat, double *lon, float *accuracy);

// PIM (JSR-75): Contacts + Calendar counts. -1 = denied.
int native_contacts_count(void);
int native_calendar_count(void);
void native_contacts_request(void);
void native_calendar_request(void);
void native_location_request(void);
// index-th contact name/phone, true on success
bool native_contact_get(int index, char *name, int nameCap, char *phone, int phoneCap);

// Messaging (WMA): iOS requires user UI, only capability check.
bool native_can_send_text(void);

// Camera snapshot (MMAPI capture://video + AMMS): PNG bytes, true on success.
bool native_camera_snapshot(uint8_t **outPNG, int *outLen);

// Background keep-alive for socket/game (24/7 treo game)
void native_background_keepalive_start(void);
void native_background_keepalive_stop(void);

#ifdef __cplusplus
}
#endif

@interface NativeExtBridge : NSObject
+ (NSData *_Nullable)fetchHttp:(NSString *)url method:(NSString *)method status:(int *)code mime:(NSString * _Nullable * _Nullable)mime;
+ (BOOL)canSendText;
+ (void)keepAliveStart;
+ (void)keepAliveStop;
@end

NS_ASSUME_NONNULL_END
