#include "eas_engine_bridge.h"
#include "eas/eas.h"
#include "eas/eas_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Real Sonivox EAS bridge: in-memory MIDI/SMF/iMelody/OTT via EAS_OpenFile.
// Falls back to silence if EAS_Init fails (e.g. missing config).

typedef struct {
    const uint8_t *data;
    int size;
} MemFile;

static EAS_DATA_HANDLE g_eas = NULL;
static EAS_HANDLE g_stream = NULL;
static EAS_FILE g_locator;
static MemFile g_mem = { NULL, 0 };
static uint8_t *g_owned = NULL;
static float g_easMasterVolume = 0.8f;
static double g_tonePhase = 0.0;
static double g_tonePhaseInc = 0.0;
static int g_toneFramesLeft = 0;

static int memReadAt(void *handle, void *buf, int offset, int size) {
    MemFile *m = (MemFile *)handle;
    if (!m || !m->data || offset < 0 || size <= 0) return 0;
    if (offset >= m->size) return 0;
    int avail = m->size - offset;
    int n = size < avail ? size : avail;
    memcpy(buf, m->data + offset, n);
    return n;
}
static int memSize(void *handle) {
    MemFile *m = (MemFile *)handle;
    return m ? m->size : 0;
}

static void closeStream(void) {
    if (g_eas && g_stream) {
        EAS_CloseFile(g_eas, g_stream);
        g_stream = NULL;
    }
    if (g_owned) { free(g_owned); g_owned = NULL; }
    g_mem.data = NULL; g_mem.size = 0;
}

bool eas_engine_init(void) {
    if (g_eas) return true;
    if (EAS_Init(&g_eas) != EAS_SUCCESS || !g_eas) {
        g_eas = NULL;
        return false;
    }
    return true;
}

void eas_engine_shutdown(void) {
    closeStream();
    if (g_eas) { EAS_Shutdown(g_eas); g_eas = NULL; }
}

bool eas_engine_play_midi_data(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > (8 << 20)) return false;
    if (!g_eas && !eas_engine_init()) return false;
    closeStream();
    g_owned = (uint8_t *)malloc(size);
    if (!g_owned) return false;
    memcpy(g_owned, data, size);
    g_mem.data = g_owned;
    g_mem.size = (int)size;
    g_locator.handle = &g_mem;
    g_locator.readAt = memReadAt;
    g_locator.size = memSize;
    EAS_HANDLE stream = NULL;
    if (EAS_OpenFile(g_eas, &g_locator, &stream) != EAS_SUCCESS || !stream) {
        closeStream();
        return false;
    }
    if (EAS_Prepare(g_eas, stream) != EAS_SUCCESS) {
        EAS_CloseFile(g_eas, stream);
        closeStream();
        return false;
    }
    EAS_SetVolume(g_eas, stream, (int)(g_easMasterVolume * 100.0f));
    g_stream = stream;
    return true;
}

void eas_engine_stop_midi(void) {
    closeStream();
    g_toneFramesLeft = 0;
}

void eas_engine_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_easMasterVolume = volume;
    if (g_eas && g_stream) EAS_SetVolume(g_eas, g_stream, (int)(volume * 100.0f));
}

// Optional tone injection (Manager.playTone path can synthesize via EAS MIDI live,
// but AudioBridge sine is primary; this keeps EAS render clock alive)
void eas_engine_play_tone(int freq, int durationMs) {
    if (freq <= 0 || durationMs <= 0) return;
    g_tonePhaseInc = (2.0 * 3.14159265358979 * (double)freq) / 44100.0;
    g_toneFramesLeft = (44100 * durationMs) / 1000;
}

int eas_engine_render_pcm(int16_t* pcmBuffer, int numFrames) {
    if (!pcmBuffer || numFrames <= 0) return 0;
    int rendered = 0;
    if (g_eas && g_stream) {
        EAS_I32 gen = 0;
        EAS_RESULT r = EAS_Render(g_eas, (EAS_PCM *)pcmBuffer, (EAS_I32)numFrames, &gen);
        if (r == EAS_SUCCESS && gen > 0) {
            EAS_STATE st = EAS_STATE_PLAY;
            EAS_State(g_eas, g_stream, &st);
            if (st == EAS_STATE_STOPPED || st == EAS_STATE_ERROR || st == EAS_STATE_EMPTY) {
                closeStream();
            }
            rendered = (int)gen;
        }
        // Apply master volume (EAS PCM is 16-bit stereo interleaved)
        for (int i = 0; i < rendered * 2; i++) {
            pcmBuffer[i] = (int16_t)(pcmBuffer[i] * g_easMasterVolume);
        }
    }
    // Mix fallback/overlay tone (short beeps) when no MIDI or alongside
    int start = rendered;
    for (int i = start; i < numFrames; i++) {
        int16_t s = 0;
        if (g_toneFramesLeft > 0) {
            s = (int16_t)(sin(g_tonePhase) * 9000.0 * g_easMasterVolume);
            g_tonePhase += g_tonePhaseInc;
            if (g_tonePhase >= 2.0 * 3.14159265358979) g_tonePhase -= 2.0 * 3.14159265358979;
            g_toneFramesLeft--;
        }
        pcmBuffer[i * 2] = (rendered > i) ? pcmBuffer[i * 2] : s;
        pcmBuffer[i * 2 + 1] = (rendered > i) ? pcmBuffer[i * 2 + 1] : s;
    }
    if (rendered == 0 && g_toneFramesLeft <= 0) {
        memset(pcmBuffer, 0, numFrames * sizeof(int16_t) * 2);
    }
    return numFrames;
}
