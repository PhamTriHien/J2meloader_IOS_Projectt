#import "AudioBridge.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include "../Audio/eas_engine_bridge.h"

@interface AudioBridge ()
@property (nonatomic, strong) AVAudioEngine *engine;
@property (nonatomic, strong) AVAudioSourceNode *synthSourceNode;
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
        eas_engine_init();
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
        if (!strongSelf) {
            *isSilence = YES;
            return noErr;
        }

        float *leftChannel = (float *)outputData->mBuffers[0].mData;
        float *rightChannel = (outputData->mNumberBuffers > 1) ? (float *)outputData->mBuffers[1].mData : leftChannel;

        // Render Sonivox EAS MIDI Synth PCM
        int16_t pcmTemp[2048 * 2];
        AVAudioFrameCount framesToRender = MIN(frameCount, (AVAudioFrameCount)2048);
        int rendered = eas_engine_render_pcm(pcmTemp, (int)framesToRender);

        for (AVAudioFrameCount i = 0; i < frameCount; ++i) {
            float sampleL = 0.0f;
            float sampleR = 0.0f;

            if (i < (AVAudioFrameCount)rendered) {
                sampleL = ((float)pcmTemp[i * 2]) / 32768.0f;
                sampleR = ((float)pcmTemp[i * 2 + 1]) / 32768.0f;
            }

            // Mix Tone generator if active
            if (strongSelf->_isPlayingTone) {
                float toneSample = (float)(sin(strongSelf->_tonePhase) * strongSelf->_volume * 0.4);
                sampleL += toneSample;
                sampleR += toneSample;
                strongSelf->_tonePhase += strongSelf->_tonePhaseInc;
                if (strongSelf->_tonePhase >= M_PI * 2.0) strongSelf->_tonePhase -= M_PI * 2.0;
            }

            leftChannel[i] = sampleL * strongSelf->_volume;
            if (outputData->mNumberBuffers > 1) {
                rightChannel[i] = sampleR * strongSelf->_volume;
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

    instance.tonePhaseInc = (2.0 * M_PI * freq) / 44100.0;
    instance.isPlayingTone = YES;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(duration * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
        instance.isPlayingTone = NO;
    });
}

+ (void)playMidiData:(NSData *)data {
    if (!data || data.length == 0) return;
    eas_engine_play_midi_data((const uint8_t *)data.bytes, data.length);
}

+ (void)stopAudio {
    AudioBridge *instance = [AudioBridge sharedInstance];
    instance.isPlayingTone = NO;
    eas_engine_stop_midi();
    if (instance.engine && instance.engine.isRunning) {
        [instance.engine stop];
    }
}

+ (void)setVolume:(float)volume {
    [AudioBridge sharedInstance].volume = volume;
    eas_engine_set_volume(volume);
}

@end