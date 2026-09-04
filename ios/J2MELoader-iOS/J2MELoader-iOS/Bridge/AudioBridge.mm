#import "AudioBridge.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include <math.h>

@interface AudioBridge ()
@property (nonatomic, strong) AVAudioEngine *engine;
@property (nonatomic, strong) AVAudioSourceNode *synthSourceNode;
@property (nonatomic, strong) AVMIDIPlayer *midiPlayer;
@property (nonatomic, assign) float volume;
@property (nonatomic, assign) double tonePhase;
@property (nonatomic, assign) double tonePhaseInc;
@property (nonatomic, assign) BOOL isPlayingTone;
@end

@implementation AudioBridge

static AudioBridge *s_sharedInstance = nil;

+ (AudioBridge *)sharedInstance {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_sharedInstance = [[AudioBridge alloc] init];
    });
    return s_sharedInstance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _volume = 0.8f;
        _tonePhase = 0.0;
        _tonePhaseInc = 0.0;
        _isPlayingTone = NO;
        [self setupAudioSession];
    }
    return self;
}

- (void)setupAudioSession {
    NSError *error = nil;
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback
             withOptions:AVAudioSessionCategoryOptionMixWithOthers
                   error:&error];
    [session setActive:YES error:&error];
}

+ (void)initializeAudio {
    [[AudioBridge sharedInstance] setupAudioEngine];
}

- (void)setupAudioEngine {
    if (self.engine && self.engine.isRunning) return;

    self.engine = [[AVAudioEngine alloc] init];
    AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100.0 channels:2];

    __weak typeof(self) weakSelf = self;
    self.synthSourceNode = [[AVAudioSourceNode alloc] initWithRenderBlock:^OSStatus(
        BOOL * _Nonnull isSilence,
        const AudioTimeStamp * _Nonnull timestamp,
        AVAudioFrameCount frameCount,
        AudioBufferList * _Nonnull outputData) {
        
        AudioBridge *strongSelf = weakSelf;
        float *leftChannel = (float *)outputData->mBuffers[0].mData;
        float *rightChannel = (outputData->mNumberBuffers > 1) ? (float *)outputData->mBuffers[1].mData : leftChannel;

        if (!strongSelf || !strongSelf->_isPlayingTone) {
            memset(leftChannel, 0, frameCount * sizeof(float));
            if (outputData->mNumberBuffers > 1) {
                memset(rightChannel, 0, frameCount * sizeof(float));
            }
            *isSilence = NO;
            return noErr;
        }

        for (AVAudioFrameCount i = 0; i < frameCount; ++i) {
            float toneSample = (float)(sin(strongSelf->_tonePhase) * strongSelf->_volume * 0.4f);
            strongSelf->_tonePhase += strongSelf->_tonePhaseInc;
            if (strongSelf->_tonePhase >= M_PI * 2.0) strongSelf->_tonePhase -= M_PI * 2.0;

            leftChannel[i] = toneSample;
            if (outputData->mNumberBuffers > 1) {
                rightChannel[i] = toneSample;
            }
        }

        *isSilence = NO;
        return noErr;
    }];

    [self.engine attachNode:self.synthSourceNode];
    [self.engine connect:self.synthSourceNode to:self.engine.mainMixerNode format:format];

    NSError *err = nil;
    [self.engine startAndReturnError:&err];
}

+ (void)playTone:(int)freq duration:(int)duration {
    AudioBridge *instance = [AudioBridge sharedInstance];
    if (freq <= 0) return;

    [instance setupAudioEngine];
    instance.tonePhaseInc = (2.0 * M_PI * freq) / 44100.0;
    instance.isPlayingTone = YES;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(duration * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
        instance.isPlayingTone = NO;
    });
}

+ (void)playMidiData:(NSData *)data {
    if (!data || data.length == 0) return;
    AudioBridge *instance = [AudioBridge sharedInstance];
    
    [instance stopCurrentMidi];
    
    NSError *error = nil;
    instance.midiPlayer = [[AVMIDIPlayer alloc] initWithData:data soundBankURL:nil error:&error];
    if (!error && instance.midiPlayer) {
        [instance.midiPlayer prepareToPlay];
        [instance.midiPlayer play:^{
            // Playback completed block
        }];
    }
}

- (void)stopCurrentMidi {
    if (self.midiPlayer) {
        if (self.midiPlayer.isPlaying) {
            [self.midiPlayer stop];
        }
        self.midiPlayer = nil;
    }
}

+ (void)stopAudio {
    AudioBridge *instance = [AudioBridge sharedInstance];
    instance.isPlayingTone = NO;
    [instance stopCurrentMidi];
    if (instance.engine && instance.engine.isRunning) {
        [instance.engine stop];
    }
}

+ (void)setVolume:(float)volume {
    AudioBridge *instance = [AudioBridge sharedInstance];
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    instance.volume = volume;
}

@end