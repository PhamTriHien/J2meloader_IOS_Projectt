#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface J2MEBridge : NSObject

+ (NSDictionary<NSString *, NSString *> *)parseJarManifest:(NSString *)jarPath;
+ (BOOL)extractJarEntry:(NSString *)jarPath entryName:(NSString *)entryName outputPath:(NSString *)outputPath;

+ (void)startEmulator:(NSString *)jarPath
            mainClass:(NSString *)mainClass
                width:(int)width
               height:(int)height
         soundEnabled:(BOOL)soundEnabled;

+ (void)stopEmulator;
+ (void)setPaused:(BOOL)paused;

+ (void)sendKeyEvent:(int)keyCode isDown:(BOOL)isDown;
+ (void)sendTouchEvent:(int)x y:(int)y action:(int)action;

+ (nullable const void *)getFrameBufferBytes;
+ (nullable NSData *)getFramebufferData;
+ (int)getFrameBufferWidth;
+ (int)getFrameBufferHeight;

@end

NS_ASSUME_NONNULL_END