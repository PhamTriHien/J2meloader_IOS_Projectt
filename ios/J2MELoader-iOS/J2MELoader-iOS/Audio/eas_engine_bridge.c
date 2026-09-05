#include "eas_engine_bridge.h"
#include "eas/eas.h"
#include "eas/eas_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

// Real Sonivox EAS bridge: in-memory MIDI/SMF/iMelody/OTT via EAS_OpenFile.
// Thread-safe: CoreAudio render thread and JVM/UI threads are synchronized via g_easMutex.

typedef struct {
    const uint8_t *data;
    int size;
} MemFile;

static pthread_mutex_t g_easMutex = PTHREAD_MUTEX_INITIALIZER;
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

// Internal: caller must hold g_easMutex
static void closeStreamLocked(void) {
    if (g_eas && g_stream) {
        EAS_CloseFile(g_eas, g_stream);
        g_stream = NULL;
    }
    if (g_owned) {
        free(g_owned);
        g_owned = NULL;
    }
    g_mem.data = NULL;
    g_mem.size = 0;
}

bool eas_engine_init(void) {
    pthread_mutex_lock(&g_easMutex);
    if (g_eas) {
        pthread_mutex_unlock(&g_easMutex);
        return true;
    }
    if (EAS_Init(&g_eas) != EAS_SUCCESS || !g_eas) {
        g_eas = NULL;
        pthread_mutex_unlock(&g_easMutex);
        return false;
    }
    pthread_mutex_unlock(&g_easMutex);
    return true;
}

void eas_engine_shutdown(void) {
    pthread_mutex_lock(&g_easMutex);
    closeStreamLocked();
    if (g_eas) {
        EAS_Shutdown(g_eas);
        g_eas = NULL;
    }
    pthread_mutex_unlock(&g_easMutex);
}

bool eas_engine_play_midi_data(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > (8 << 20)) return false;
    pthread_mutex_lock(&g_easMutex);
    if (!g_eas) {
        if (EAS_Init(&g_eas) != EAS_SUCCESS || !g_eas) {
            g_eas = NULL;
            pthread_mutex_unlock(&g_easMutex);
            return false;
        }
    }
    closeStreamLocked();
    g_owned = (uint8_t *)malloc(size);
    if (!g_owned) {
        pthread_mutex_unlock(&g_easMutex);
        return false;
    }
    memcpy(g_owned, data, size);
    g_mem.data = g_owned;
    g_mem.size = (int)size;
    g_locator.handle = &g_mem;
    g_locator.readAt = memReadAt;
    g_locator.size = memSize;
    EAS_HANDLE stream = NULL;
    if (EAS_OpenFile(g_eas, &g_locator, &stream) != EAS_SUCCESS || !stream) {
        closeStreamLocked();
        pthread_mutex_unlock(&g_easMutex);
        return false;
    }
    if (EAS_Prepare(g_eas, stream) != EAS_SUCCESS) {
        EAS_CloseFile(g_eas, stream);
        closeStreamLocked();
        pthread_mutex_unlock(&g_easMutex);
        return false;
    }
    EAS_SetVolume(g_eas, stream, (int)(g_easMasterVolume * 100.0f));
    g_stream = stream;
    pthread_mutex_unlock(&g_easMutex);
    return true;
}

void eas_engine_stop_midi(void) {
    pthread_mutex_lock(&g_easMutex);
    closeStreamLocked();
    g_toneFramesLeft = 0;
    pthread_mutex_unlock(&g_easMutex);
}

void eas_engine_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    pthread_mutex_lock(&g_easMutex);
    g_easMasterVolume = volume;
    if (g_eas && g_stream) {
        EAS_SetVolume(g_eas, g_stream, (int)(volume * 100.0f));
    }
    pthread_mutex_unlock(&g_easMutex);
}

// Optional tone injection
void eas_engine_play_tone(int freq, int durationMs) {
    if (freq <= 0 || durationMs <= 0) return;
    pthread_mutex_lock(&g_easMutex);
    g_tonePhaseInc = (2.0 * 3.14159265358979 * (double)freq) / 44100.0;
    g_toneFramesLeft = (44100 * durationMs) / 1000;
    pthread_mutex_unlock(&g_easMutex);
}

int eas_engine_render_pcm(int16_t* pcmBuffer, int numFrames) {
    if (!pcmBuffer || numFrames <= 0) return 0;
    if (pthread_mutex_trylock(&g_easMutex) != 0) {
        memset(pcmBuffer, 0, numFrames * sizeof(int16_t) * 2);
        return numFrames;
    }
    int rendered = 0;
    if (g_eas && g_stream) {
        EAS_I32 gen = 0;
        EAS_RESULT r = EAS_Render(g_eas, (EAS_PCM *)pcmBuffer, (EAS_I32)numFrames, &gen);
        if (r == EAS_SUCCESS && gen > 0) {
            EAS_STATE st = EAS_STATE_PLAY;
            EAS_State(g_eas, g_stream, &st);
            if (st == EAS_STATE_STOPPED || st == EAS_STATE_ERROR || st == EAS_STATE_EMPTY) {
                closeStreamLocked();
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
    pthread_mutex_unlock(&g_easMutex);
    return numFrames;
}
