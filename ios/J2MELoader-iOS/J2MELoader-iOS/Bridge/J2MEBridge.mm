#import "J2MEBridge.h"
#import "AudioBridge.h"
#import "NativeExtBridge.h"
#include "../Core/jar_loader.h"
#include "../Core/jvm_interpreter.h"
#include "../Core/rms_storage.h"

@implementation J2MEBridge

+ (void)initialize {
    if (self == [J2MEBridge class]) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        if (paths.count > 0) {
            NSString *rmsPath = [paths[0] stringByAppendingPathComponent:@"RMS"];
            RmsStorage::getInstance().setBaseDirectory(rmsPath.UTF8String);
        }
        
        // Setup audio callback to AudioBridge
        JvmInterpreter::getInstance().setAudioCallback(
            [](const uint8_t* data, size_t size) {
                NSData *nsData = [NSData dataWithBytes:data length:size];
                [AudioBridge playMidiData:nsData];
            },
            [](int freq, int duration) {
                [AudioBridge playTone:freq duration:duration];
            }
        );
    }
}

+ (NSDictionary<NSString *, NSString *> *)parseJarManifest:(NSString *)jarPath {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    JarLoader loader;
    if (loader.open(jarPath.UTF8String)) {
        auto manifest = loader.parseManifest();
        for (const auto& pair : manifest) {
            NSString *key = [NSString stringWithUTF8String:pair.first.c_str()];
            NSString *value = [NSString stringWithUTF8String:pair.second.c_str()];
            if (key && value) {
                dict[key] = value;
            }
        }
    }
    return dict;
}

+ (BOOL)extractJarEntry:(NSString *)jarPath entryName:(NSString *)entryName outputPath:(NSString *)outputPath {
    JarLoader loader;
    if (!loader.open(jarPath.UTF8String)) return NO;
    
    // Normalize entry name (remove leading '/')
    std::string entry = entryName.UTF8String;
    if (!entry.empty() && entry[0] == '/') {
        entry.erase(0, 1);
    }
    
    return loader.extractEntryToFile(entry, outputPath.UTF8String);
}

+ (void)startEmulator:(NSString *)jarPath
            mainClass:(NSString *)mainClass
                width:(int)width
               height:(int)height
         soundEnabled:(BOOL)soundEnabled {
    
    [AudioBridge initializeAudio];
    [NativeExtBridge keepAliveStart];
    JvmInterpreter::getInstance().init(
        jarPath.UTF8String,
        mainClass.UTF8String,
        width,
        height,
        soundEnabled
    );
}

+ (void)stopEmulator {
    JvmInterpreter::getInstance().shutdown();
    [AudioBridge stopAudio];
    [NativeExtBridge keepAliveStop];
}

+ (void)setPaused:(BOOL)paused {
    if (paused) {
        JvmInterpreter::getInstance().pause();
    } else {
        JvmInterpreter::getInstance().resume();
    }
}

+ (void)sendKeyEvent:(int)keyCode isDown:(BOOL)isDown {
    JvmInterpreter::getInstance().postKeyEvent(keyCode, isDown);
}

+ (void)sendTouchEvent:(int)x y:(int)y action:(int)action {
    JvmInterpreter::getInstance().postTouchEvent(x, y, action);
}

+ (nullable const void *)getFrameBufferBytes {
    LcduiDisplay *disp = JvmInterpreter::getInstance().getDisplay();
    if (!disp) return NULL;
    return disp->getBuffer();
}

+ (nullable NSData *)getFramebufferData {
    LcduiDisplay *disp = JvmInterpreter::getInstance().getDisplay();
    if (!disp || !disp->getBuffer()) return nil;
    size_t size = disp->getWidth() * disp->getHeight() * sizeof(uint32_t);
    return [NSData dataWithBytes:disp->getBuffer() length:size];
}

+ (int)getFrameBufferWidth {
    LcduiDisplay *disp = JvmInterpreter::getInstance().getDisplay();
    return disp ? disp->getWidth() : 0;
}

+ (int)getFrameBufferHeight {
    LcduiDisplay *disp = JvmInterpreter::getInstance().getDisplay();
    return disp ? disp->getHeight() : 0;
}

+ (NSString *)getBootStatus {
    std::string s = JvmInterpreter::getInstance().getBootStatus();
    return [NSString stringWithUTF8String:s.c_str()];
}

+ (int)getPaintTick {
    return JvmInterpreter::getInstance().getPaintTick();
}

@end