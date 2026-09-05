#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Full-Unicode LCDUI font via CoreText (Vietnamese + CJK + emoji fallback).
// C API callable from pure C++ (LcduiDisplay). Weak-linked: C++ checks symbol.
#ifdef __cplusplus
extern "C" {
#endif

// Measure UTF-8 string at given pixel height (default 12). Returns true on success.
bool native_text_measure(const char *utf8, int px, int *outW, int *outH);
// Render UTF-8 string to 8-bit alpha bitmap (row-major, w*h bytes). Caller frees with native_free().
bool native_text_render(const char *utf8, int px, uint8_t **outAlpha, int *outW, int *outH);

#ifdef __cplusplus
}
#endif

@interface NativeFontBridge : NSObject
+ (CGSize)measure:(NSString *)s px:(int)px;
+ (NSData *_Nullable)renderAlpha:(NSString *)s px:(int)px w:(int *)w h:(int *)h;
@end

NS_ASSUME_NONNULL_END
