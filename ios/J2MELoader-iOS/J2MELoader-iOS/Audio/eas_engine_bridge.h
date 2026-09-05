#ifndef EAS_ENGINE_BRIDGE_H
#define EAS_ENGINE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool eas_engine_init(void);
void eas_engine_shutdown(void);

bool eas_engine_play_midi_data(const uint8_t* data, size_t size);
void eas_engine_play_tone(int freq, int durationMs);
void eas_engine_stop_midi(void);
void eas_engine_set_volume(float volume); // 0.0 to 1.0

// Renders interleaved 16-bit Stereo PCM samples (44100 Hz)
int eas_engine_render_pcm(int16_t* pcmBuffer, int numFrames);

#ifdef __cplusplus
}
#endif

#endif // EAS_ENGINE_BRIDGE_H