#import "NativeExtBridge.h"
#import <UIKit/UIKit.h>
#if __has_include(<CoreBluetooth/CoreBluetooth.h>)
#import <CoreBluetooth/CoreBluetooth.h>
#endif
#if __has_include(<CoreLocation/CoreLocation.h>)
#import <CoreLocation/CoreLocation.h>
#endif
#if __has_include(<Contacts/Contacts.h>)
#import <Contacts/Contacts.h>
#endif
#if __has_include(<EventKit/EventKit.h>)
#import <EventKit/EventKit.h>
#endif
#if __has_include(<MessageUI/MessageUI.h>)
#import <MessageUI/MessageUI.h>
#endif
#if __has_include(<AVFoundation/AVFoundation.h>)
#import <AVFoundation/AVFoundation.h>
#endif
#if __has_include(<AudioToolbox/AudioToolbox.h>)
#import <AudioToolbox/AudioToolbox.h>
#endif

void native_free(void *p) { if (p) free(p); }

#if __has_include(<CoreBluetooth/CoreBluetooth.h>)
@interface J2MEBTScanHelper : NSObject<CBCentralManagerDelegate>
@property (nonatomic, strong) NSMutableArray<NSString *> *found;
@property (nonatomic, assign) BOOL powered;
@end
@implementation J2MEBTScanHelper
- (instancetype)init { self = [super init]; if (self) { _found = [NSMutableArray array]; } return self; }
- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    self.powered = (central.state == CBManagerStatePoweredOn);
    if (self.powered) [central scanForPeripheralsWithServices:nil options:nil];
}
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary *)ad RSSI:(NSNumber *)RSSI {
    NSString *n = peripheral.name;
    if (!n || n.length == 0) n = ad[CBAdvertisementDataLocalNameKey];
    if (!n || n.length == 0) n = @"BT-Device";
    @synchronized (self.found) { if (![self.found containsObject:n]) [self.found addObject:n]; }
}
@end
#endif

#if __has_include(<CoreLocation/CoreLocation.h>)
@interface J2MELocHelper : NSObject<CLLocationManagerDelegate>
@property (nonatomic, strong) CLLocation *fix;
@property (nonatomic, assign) BOOL done;
@end
@implementation J2MELocHelper
- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations {
    self.fix = locations.lastObject; self.done = YES;
}
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error { self.done = YES; }
@end
#endif

#if __has_include(<AVFoundation/AVFoundation.h>)
@interface J2MEPhotoHelper : NSObject<AVCapturePhotoCaptureDelegate>
@property (nonatomic, strong) NSData *jpeg;
@property (nonatomic, assign) BOOL done;
@end
@implementation J2MEPhotoHelper
- (void)captureOutput:(AVCapturePhotoOutput *)output didFinishProcessingPhoto:(AVCapturePhoto *)photo error:(NSError *)error {
    if (!error && photo) self.jpeg = [photo fileDataRepresentation];
    self.done = YES;
}
@end
#endif

bool native_http_fetch(const char *url, const char *method,
                       uint8_t **outData, int *outLen,
                       int *outCode, char *outType, int typeCap) {
    if (!url || !outData || !outLen) return false;
    *outData = NULL; *outLen = 0;
    if (outCode) *outCode = 0;
    if (outType && typeCap > 0) outType[0] = 0;
    NSString *nsURL = [NSString stringWithUTF8String:url];
    if (!nsURL) return false;
    NSURL *u = [NSURL URLWithString:nsURL];
    if (!u) return false;
    NSString *m = method ? [NSString stringWithUTF8String:method] : @"GET";
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:u];
    req.HTTPMethod = m.length ? m : @"GET";
    req.timeoutInterval = 20.0;
    [req setValue:@"J2MELoader-iOS/1.8.2 (MIDP-2.0; CLDC-1.1)" forHTTPHeaderField:@"User-Agent"];
    UIBackgroundTaskIdentifier bg = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"j2me-http" expirationHandler:^{}];
    __block NSData *respData = nil;
    __block NSHTTPURLResponse *httpResp = nil;
    __block NSError *respErr = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSession *session = [NSURLSession sharedSession];
    [[session dataTaskWithRequest:req completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
        respData = d; httpResp = (NSHTTPURLResponse *)r; respErr = e;
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(25 * NSEC_PER_SEC)));
    [[UIApplication sharedApplication] endBackgroundTask:bg];
    if (respErr || !respData) return false;
    if (httpResp && [httpResp isKindOfClass:[NSHTTPURLResponse class]]) {
        if (outCode) *outCode = (int)httpResp.statusCode;
        NSString *mime = httpResp.MIMEType ?: @"";
        if (outType && typeCap > 0) {
            strncpy(outType, mime.UTF8String ?: "", typeCap - 1);
            outType[typeCap - 1] = 0;
        }
    } else {
        if (outCode) *outCode = 200;
    }
    size_t n = respData.length;
    uint8_t *buf = (uint8_t *)malloc(n ? n : 1);
    if (!buf) return false;
    if (n) memcpy(buf, respData.bytes, n);
    *outData = buf; *outLen = (int)n;
    return true;
}

bool native_http_send(const char *url, const char *method,
                      const uint8_t *body, int bodyLen,
                      uint8_t **outData, int *outLen,
                      int *outCode, char *outType, int typeCap) {
    if (!url || !outData || !outLen) return false;
    *outData = NULL; *outLen = 0;
    if (outCode) *outCode = 0;
    if (outType && typeCap > 0) outType[0] = 0;
    NSString *nsURL = [NSString stringWithUTF8String:url];
    NSURL *u = nsURL ? [NSURL URLWithString:nsURL] : nil;
    if (!u) return false;
    NSString *m = method ? [NSString stringWithUTF8String:method] : @"GET";
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:u];
    req.HTTPMethod = m.length ? m : @"GET";
    req.timeoutInterval = 25.0;
    [req setValue:@"J2MELoader-iOS/1.8.2 (MIDP-2.0; CLDC-1.1)" forHTTPHeaderField:@"User-Agent"];
    if (body && bodyLen > 0) {
        req.HTTPBody = [NSData dataWithBytes:body length:bodyLen];
        [req setValue:@"application/octet-stream" forHTTPHeaderField:@"Content-Type"];
    }
    UIBackgroundTaskIdentifier bg = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"j2me-http" expirationHandler:^{}];
    __block NSData *respData = nil;
    __block NSHTTPURLResponse *httpResp = nil;
    __block NSError *respErr = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [[NSURLSession.sharedSession dataTaskWithRequest:req completionHandler:^(NSData *d, NSURLResponse *r, NSError *e) {
        respData = d; httpResp = (NSHTTPURLResponse *)r; respErr = e;
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)));
    [[UIApplication sharedApplication] endBackgroundTask:bg];
    if (respErr || !respData) return false;
    if (httpResp && [httpResp isKindOfClass:[NSHTTPURLResponse class]]) {
        if (outCode) *outCode = (int)httpResp.statusCode;
        NSString *mime = httpResp.MIMEType ?: @"";
        if (outType && typeCap > 0) { strncpy(outType, mime.UTF8String ?: "", typeCap - 1); outType[typeCap - 1] = 0; }
    } else if (outCode) *outCode = 200;
    size_t n = respData.length;
    uint8_t *buf = (uint8_t *)malloc(n ? n : 1);
    if (!buf) return false;
    if (n) memcpy(buf, respData.bytes, n);
    *outData = buf; *outLen = (int)n;
    return true;
}

bool native_socket_test(const char *url) {
    if (!url) return false;
    NSString *s = [NSString stringWithUTF8String:url];
    NSRange r = [s rangeOfString:@"://"];
    NSString *hostport = (r.location != NSNotFound) ? [s substringFromIndex:r.location + 3] : s;
    NSArray *parts = [hostport componentsSeparatedByString:@":"];
    if (parts.count == 0) return false;
    NSString *host = parts[0];
    if (host.length == 0) return false;
    CFHostRef cf = CFHostCreateWithName(NULL, (__bridge CFStringRef)host);
    if (!cf) return false;
    Boolean ok = CFHostStartInfoResolution(cf, kCFHostAddresses, NULL);
    CFRelease(cf);
    return ok ? true : false;
}

int native_bluetooth_state(void) {
#if __has_include(<CoreBluetooth/CoreBluetooth.h>)
    if (@available(iOS 13.0, *)) {
        CBManagerAuthorization auth = [CBCentralManager authorization];
        if (auth == CBManagerAuthorizationDenied || auth == CBManagerAuthorizationRestricted) return 2;
    }
    CBCentralManager *mgr = [[CBCentralManager alloc] initWithDelegate:nil queue:nil options:@{CBCentralManagerOptionShowPowerAlertKey: @NO}];
    CBManagerState st = mgr.state;
    if (st == CBManagerStatePoweredOff || st == CBManagerStateUnsupported) return 0;
    if (st == CBManagerStateUnauthorized) return 2;
    return 1;
#else
    return 0;
#endif
}

int native_bluetooth_scan(int timeoutSec, char *outNames, int cap) {
    if (outNames && cap > 0) outNames[0] = 0;
#if __has_include(<CoreBluetooth/CoreBluetooth.h>)
    J2MEBTScanHelper *helper = [[J2MEBTScanHelper alloc] init];
    CBCentralManager *mgr = [[CBCentralManager alloc] initWithDelegate:helper queue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
    (void)mgr;
    int waitMs = (timeoutSec > 0 ? timeoutSec : 3) * 1000;
    for (int t = 0; t < waitMs; t += 200) {
        [NSThread sleepForTimeInterval:0.2];
        if (helper.found.count >= 8) break;
    }
    [mgr stopScan];
    if (outNames && cap > 0) {
        NSString *joined = [helper.found componentsJoinedByString:@"\n"];
        strncpy(outNames, joined.UTF8String ?: "", cap - 1);
        outNames[cap - 1] = 0;
    }
    return (int)helper.found.count;
#else
    return 0;
#endif
}

bool native_location_get(double *lat, double *lon, float *accuracy) {
#if __has_include(<CoreLocation/CoreLocation.h>)
    CLLocationManager *mgr = [[CLLocationManager alloc] init];
    J2MELocHelper *helper = [[J2MELocHelper alloc] init];
    mgr.delegate = helper;
    mgr.desiredAccuracy = kCLLocationAccuracyHundredMeters;
    if ([mgr respondsToSelector:@selector(requestWhenInUseAuthorization)]) [mgr requestWhenInUseAuthorization];
    [mgr startUpdatingLocation];
    for (int i = 0; i < 40 && !helper.done; i++) [NSThread sleepForTimeInterval:0.2];
    [mgr stopUpdatingLocation];
    if (helper.fix) {
        if (lat) *lat = helper.fix.coordinate.latitude;
        if (lon) *lon = helper.fix.coordinate.longitude;
        if (accuracy) *accuracy = (float)helper.fix.horizontalAccuracy;
        return true;
    }
    return false;
#else
    return false;
#endif
}

void native_contacts_request(void) {
#if __has_include(<Contacts/Contacts.h>)
    CNContactStore *store = [[CNContactStore alloc] init];
    if ([CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts] == CNAuthorizationStatusNotDetermined) {
        [store requestAccessForEntityType:CNEntityTypeContacts completionHandler:^(BOOL g, NSError *e) {}];
    }
#endif
}
void native_calendar_request(void) {
#if __has_include(<EventKit/EventKit.h>)
    EKEventStore *store = [[EKEventStore alloc] init];
    if ([EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent] == EKAuthorizationStatusNotDetermined) {
        if ([store respondsToSelector:@selector(requestAccessToEntityType:completion:)])
            [store requestAccessToEntityType:EKEntityTypeEvent completion:^(BOOL g, NSError *e) {}];
    }
#endif
}
void native_location_request(void) {
#if __has_include(<CoreLocation/CoreLocation.h>)
    CLLocationManager *mgr = [[CLLocationManager alloc] init];
    if ([mgr respondsToSelector:@selector(requestWhenInUseAuthorization)]) [mgr requestWhenInUseAuthorization];
#endif
}
bool native_contact_get(int index, char *name, int nameCap, char *phone, int phoneCap) {
    if (name && nameCap > 0) name[0] = 0;
    if (phone && phoneCap > 0) phone[0] = 0;
#if __has_include(<Contacts/Contacts.h>)
    CNContactStore *store = [[CNContactStore alloc] init];
    if ([CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts] != CNAuthorizationStatusAuthorized) return false;
    NSArray *keys = @[CNContactGivenNameKey, CNContactFamilyNameKey, CNContactPhoneNumbersKey];
    CNContactFetchRequest *req = [[CNContactFetchRequest alloc] initWithKeysToFetch:keys];
    __block int i = 0; __block BOOL ok = NO;
    NSError *err = nil;
    [store enumerateContactsWithFetchRequest:req error:&err usingBlock:^(CNContact *c, BOOL *stop) {
        if (i == index) {
            NSString *full = [NSString stringWithFormat:@"%@ %@", c.givenName, c.familyName];
            full = [full stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            if (name && nameCap > 0) { strncpy(name, full.UTF8String ?: "", nameCap - 1); name[nameCap-1]=0; }
            NSString *ph = @"";
            if (c.phoneNumbers.count > 0) ph = ((CNLabeledValue<CNPhoneNumber*>*)c.phoneNumbers[0]).value.stringValue ?: @"";
            if (phone && phoneCap > 0) { strncpy(phone, ph.UTF8String ?: "", phoneCap - 1); phone[phoneCap-1]=0; }
            ok = YES; *stop = YES;
        }
        i++;
    }];
    return ok ? true : false;
#else
    return false;
#endif
}
int native_contacts_count(void) {
#if __has_include(<Contacts/Contacts.h>)
    CNContactStore *store = [[CNContactStore alloc] init];
    CNAuthorizationStatus st = [CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts];
    if (st == CNAuthorizationStatusDenied || st == CNAuthorizationStatusRestricted) return -1;
    if (st == CNAuthorizationStatusNotDetermined) return 0;
    NSError *err = nil;
    NSArray *keys = @[CNContactIdentifierKey];
    CNContactFetchRequest *req = [[CNContactFetchRequest alloc] initWithKeysToFetch:keys];
    __block int count = 0;
    [store enumerateContactsWithFetchRequest:req error:&err usingBlock:^(CNContact *c, BOOL *stop) { (void)c; (void)stop; count++; }];
    return err ? -1 : count;
#else
    return 0;
#endif
}

int native_calendar_count(void) {
#if __has_include(<EventKit/EventKit.h>)
    EKEventStore *store = [[EKEventStore alloc] init];
    EKAuthorizationStatus st = [EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent];
    if (st == EKAuthorizationStatusDenied || st == EKAuthorizationStatusRestricted) return -1;
    if (st == EKAuthorizationStatusNotDetermined) return 0;
    NSDate *now = [NSDate date];
    NSPredicate *pred = [store predicateForEventsWithStartDate:[now dateByAddingTimeInterval:-30*24*3600] endDate:[now dateByAddingTimeInterval:30*24*3600] calendars:nil];
    NSArray *events = [store eventsMatchingPredicate:pred];
    return (int)events.count;
#else
    return 0;
#endif
}

bool native_can_send_text(void) {
#if __has_include(<MessageUI/MessageUI.h>)
    return [MFMessageComposeViewController canSendText] ? true : false;
#else
    return false;
#endif
}

bool native_camera_snapshot(uint8_t **outPNG, int *outLen) {
    if (outPNG) *outPNG = NULL;
    if (outLen) *outLen = 0;
#if __has_include(<AVFoundation/AVFoundation.h>)
    AVAuthorizationStatus st = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (st == AVAuthorizationStatusDenied || st == AVAuthorizationStatusRestricted) return false;
    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    session.sessionPreset = AVCaptureSessionPreset640x480;
    AVCaptureDevice *dev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!dev) return false;
    NSError *err = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
    if (!input || err) return false;
    if (![session canAddInput:input]) return false;
    [session addInput:input];
    AVCapturePhotoOutput *output = [[AVCapturePhotoOutput alloc] init];
    if (![session canAddOutput:output]) return false;
    [session addOutput:output];
    J2MEPhotoHelper *helper = [[J2MEPhotoHelper alloc] init];
    [session startRunning];
    [NSThread sleepForTimeInterval:0.6];
    AVCapturePhotoSettings *settings = [AVCapturePhotoSettings photoSettings];
    [output capturePhotoWithSettings:settings delegate:helper];
    for (int i = 0; i < 25 && !helper.done; i++) [NSThread sleepForTimeInterval:0.2];
    [session stopRunning];
    if (!helper.jpeg || helper.jpeg.length == 0) return false;
    uint8_t *b = (uint8_t *)malloc(helper.jpeg.length);
    if (!b) return false;
    memcpy(b, helper.jpeg.bytes, helper.jpeg.length);
    *outPNG = b; *outLen = (int)helper.jpeg.length;
    return true;
#else
    return false;
#endif
}

void native_vibrate(int ms) {
#if __has_include(<AudioToolbox/AudioToolbox.h>)
    if (ms <= 0) return;
    // iOS has no timed vibrate API; play system vibrate, repeat for long durations
    AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
    if (ms > 600) {
        int reps = (ms / 700);
        if (reps > 4) reps = 4;
        for (int i = 0; i < reps; i++) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)((i + 1) * 750 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
                AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
            });
        }
    }
#else
    (void)ms;
#endif
}

static UIBackgroundTaskIdentifier g_keepAliveTask = UIBackgroundTaskInvalid;

void native_background_keepalive_start(void) {
    if (g_keepAliveTask != UIBackgroundTaskInvalid) return;
    g_keepAliveTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"j2me-keepalive" expirationHandler:^{
        if (g_keepAliveTask != UIBackgroundTaskInvalid) {
            [[UIApplication sharedApplication] endBackgroundTask:g_keepAliveTask];
            g_keepAliveTask = UIBackgroundTaskInvalid;
        }
    }];
}

void native_background_keepalive_stop(void) {
    if (g_keepAliveTask != UIBackgroundTaskInvalid) {
        [[UIApplication sharedApplication] endBackgroundTask:g_keepAliveTask];
        g_keepAliveTask = UIBackgroundTaskInvalid;
    }
}

@implementation NativeExtBridge
+ (NSData *)fetchHttp:(NSString *)url method:(NSString *)method status:(int *)code mime:(NSString **)mime {
    uint8_t *data = NULL; int len = 0, c = 0; char type[128] = {0};
    bool ok = native_http_fetch(url.UTF8String, method.UTF8String, &data, &len, &c, type, sizeof(type));
    if (code) *code = c;
    if (mime) *mime = [NSString stringWithUTF8String:type];
    if (!ok || !data) return nil;
    return [NSData dataWithBytesNoCopy:data length:len freeWhenDone:YES];
}
+ (BOOL)canSendText { return native_can_send_text() ? YES : NO; }
+ (void)keepAliveStart { native_background_keepalive_start(); }
+ (void)keepAliveStop { native_background_keepalive_stop(); }
@end
