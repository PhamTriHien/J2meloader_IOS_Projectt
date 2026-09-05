#import "AudioBridge.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include "../Audio/eas_engine_bridge.h"

@interface AudioBridge ()
@property (nonatomic, strong) AVAudioEngine *engine;
@property (nonatomic, strong) AVAudioSourceNode *synthSourceNode;
@property (nonatomic, strong) AVMIDIPlayer *midiPlayer;
@property (nonatomic, strong) AVAudioPlayer *audioPlayer;
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
    eas_engine_init();

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

        // 1) Real Sonivox EAS render (MIDI/SMF/iMelody/OTT). Fixed buffer to avoid heap malloc in real-time audio thread.
        int16_t easBuf[4096 * 2];
        AVAudioFrameCount framesToRender = (frameCount <= 4096) ? frameCount : 4096;
        eas_engine_render_pcm(easBuf, framesToRender);
        int hasEAS = 0;
        for (AVAudioFrameCount i = 0; i < framesToRender * 2; i++) { if (easBuf[i] != 0) { hasEAS = 1; break; } }
        if (hasEAS) {
            for (AVAudioFrameCount i = 0; i < framesToRender; ++i) {
                leftChannel[i] = easBuf[i * 2] / 32768.0f;
                if (outputData->mNumberBuffers > 1) rightChannel[i] = easBuf[i * 2 + 1] / 32768.0f;
            }
            if (framesToRender < frameCount) {
                memset(leftChannel + framesToRender, 0, (frameCount - framesToRender) * sizeof(float));
                if (outputData->mNumberBuffers > 1) {
                    memset(rightChannel + framesToRender, 0, (frameCount - framesToRender) * sizeof(float));
                }
            }
            // Mix legacy sine tone on top if active
            if (strongSelf && strongSelf->_isPlayingTone) {
                for (AVAudioFrameCount i = 0; i < frameCount; ++i) {
                    float toneSample = (float)(sin(strongSelf->_tonePhase) * strongSelf->_volume * 0.3f);
                    strongSelf->_tonePhase += strongSelf->_tonePhaseInc;
                    if (strongSelf->_tonePhase >= M_PI * 2.0) strongSelf->_tonePhase -= M_PI * 2.0;
                    leftChannel[i] += toneSample;
                    if (outputData->mNumberBuffers > 1) rightChannel[i] += toneSample;
                }
            }
            *isSilence = NO;
            return noErr;
        }

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
    [instance setupAudioEngine];

    // Detect WAV/MP3/AMR vs MIDI: WAV=RIFF, MP3=ID3/FFFB, AMR=#AMR
    const uint8_t *b = (const uint8_t *)data.bytes;
    BOOL isWav = (data.length >= 4 && b[0]=='R'&&b[1]=='I'&&b[2]=='F'&&b[3]=='F');
    BOOL isMp3 = (data.length >= 3 && ((b[0]=='I'&&b[1]=='D'&&b[2]=='3') || (b[0]==0xFF&&(b[1]&0xE0)==0xE0)));
    BOOL isAmr = (data.length >= 5 && b[0]=='#'&&b[1]=='A'&&b[2]=='M'&&b[3]=='R');
    if (isWav || isMp3 || isAmr) {
        [instance stopCurrentMidi];
        NSError *err = nil;
        instance.audioPlayer = [[AVAudioPlayer alloc] initWithData:data error:&err];
        if (!err && instance.audioPlayer) {
            instance.audioPlayer.volume = instance.volume;
            instance.audioPlayer.numberOfLoops = 0;
            [instance.audioPlayer prepareToPlay];
            [instance.audioPlayer play];
        }
        return;
    }
    // Stop any existing MIDI/audio before starting a new track
    [instance stopCurrentMidi];

    // Primary: real Sonivox EAS (SMF/MIDI/iMelody). Fallback: AVMIDIPlayer.
    bool easOK = eas_engine_play_midi_data((const uint8_t *)data.bytes, data.length);
    if (easOK) {
        return;
    }

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
    if (self.audioPlayer) {
        if (self.audioPlayer.isPlaying) [self.audioPlayer stop];
        self.audioPlayer = nil;
    }
    eas_engine_stop_midi();
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
    eas_engine_set_volume(volume);
    if (instance.audioPlayer) instance.audioPlayer.volume = volume;
}

@end