#include "eas_engine_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool g_easInitialized = false;
static float g_easMasterVolume = 0.8f;
static const uint8_t* g_pCurrentMidiData = NULL;
static size_t g_currentMidiSize = 0;

bool eas_engine_init(void) {
    g_easInitialized = true;
    return true;
}

void eas_engine_shutdown(void) {
    eas_engine_stop_midi();
    g_easInitialized = false;
}

bool eas_engine_play_midi_data(const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;
    g_pCurrentMidiData = data;
    g_currentMidiSize = size;
    return true;
}

void eas_engine_stop_midi(void) {
    g_pCurrentMidiData = NULL;
    g_currentMidiSize = 0;
}

void eas_engine_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_easMasterVolume = volume;
}

int eas_engine_render_pcm(int16_t* pcmBuffer, int numFrames) {
    if (!pcmBuffer || numFrames <= 0) return 0;
    memset(pcmBuffer, 0, numFrames * sizeof(int16_t) * 2);
    return numFrames;
}