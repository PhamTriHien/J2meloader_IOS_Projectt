#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface AudioBridge : NSObject

+ (void)initializeAudio;
+ (void)playMidiData:(NSData *)data;
+ (void)playTone:(int)freq duration:(int)duration;
+ (void)stopAudio;
+ (void)setVolume:(float)volume;

@end

NS_ASSUME_NONNULL_END