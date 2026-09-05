#import "NativeFontBridge.h"
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>

bool native_text_measure(const char *utf8, int px, int *outW, int *outH) {
    if (!utf8 || !outW || !outH) return false;
    NSString *s = [NSString stringWithUTF8String:utf8];
    if (!s) return false;
    CGSize sz = [NativeFontBridge measure:s px:px > 0 ? px : 12];
    *outW = (int)ceil(sz.width); *outH = (int)ceil(sz.height);
    return true;
}

bool native_text_render(const char *utf8, int px, uint8_t **outAlpha, int *outW, int *outH) {
    if (!utf8 || !outAlpha || !outW || !outH) return false;
    NSString *s = [NSString stringWithUTF8String:utf8];
    if (!s || s.length == 0) return false;
    int w = 0, h = 0;
    NSData *d = [NativeFontBridge renderAlpha:s px:px > 0 ? px : 12 w:&w h:&h];
    if (!d || w <= 0 || h <= 0) return false;
    uint8_t *buf = (uint8_t *)malloc(w * h);
    if (!buf) return false;
    memcpy(buf, d.bytes, w * h);
    *outAlpha = buf; *outW = w; *outH = h;
    return true;
}

@implementation NativeFontBridge

+ (UIFont *)fontForPx:(int)px {
    // Monospaced retro-ish; falls back through system fonts for Vietnamese/CJK
    UIFont *f = [UIFont monospacedSystemFontOfSize:px weight:UIFontWeightRegular];
    return f ?: [UIFont systemFontOfSize:px];
}

+ (CGSize)measure:(NSString *)s px:(int)px {
    UIFont *f = [self fontForPx:px];
    NSDictionary *at = @{NSFontAttributeName: f};
    CGSize sz = [s sizeWithAttributes:at];
    return CGSizeMake(ceil(sz.width), ceil(sz.height));
}

+ (NSData *)renderAlpha:(NSString *)s px:(int)px w:(int *)w h:(int *)h {
    UIFont *f = [self fontForPx:px];
    NSDictionary *at = @{NSFontAttributeName: f, NSForegroundColorAttributeName: UIColor.whiteColor};
    CGSize sz = [s sizeWithAttributes:at];
    int W = (int)ceil(sz.width), H = (int)ceil(sz.height);
    if (W <= 0 || H <= 0 || W > 2048 || H > 256) return nil;
    // Grayscale bitmap: white text on black, alpha = luminance
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    if (!cs) return nil;
    NSMutableData *alpha = [NSMutableData dataWithLength:W * H];
    CGContextRef ctx = CGBitmapContextCreate((void *)alpha.mutableBytes, W, H, 8, W,
        cs, (CGBitmapInfo)kCGImageAlphaNone);
    CGColorSpaceRelease(cs);
    if (!ctx) return nil;
    CGContextSetFillColorWithColor(ctx, UIColor.blackColor.CGColor);
    CGContextFillRect(ctx, CGRectMake(0, 0, W, H));
    // Flip for UIKit coordinates
    CGContextTranslateCTM(ctx, 0, H);
    CGContextScaleCTM(ctx, 1, -1);
    UIGraphicsPushContext(ctx);
    [s drawAtPoint:CGPointZero withAttributes:at];
    UIGraphicsPopContext();
    CGContextRelease(ctx);
    if (w) *w = W;
    if (h) *h = H;
    return alpha;
}

@end
