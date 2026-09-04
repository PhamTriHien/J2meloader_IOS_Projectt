#include "eas_engine_bridge.h"
#include "eas/eas.h"
#include "eas/eas_reverb.h"
#include <stdlib.h>
#include <string.h>

static EAS_DATA_HANDLE g_pEASData = NULL;
static EAS_HANDLE g_pMediaHandle = NULL;
static EAS_FILE g_easFile;
static const uint8_t* g_pStreamData = NULL;
static size_t g_streamSize = 0;
static size_t g_streamPos = 0;
static float g_masterVolume = 0.8f;

// Memory stream callbacks for Sonivox EAS
static EAS_I32 eas_read_fn(void *handle, void *buffer, EAS_I32 offset, EAS_I32 size) {
    if (offset < 0 || (size_t)offset >= g_streamSize) return 0;
    size_t toRead = (size_t)size;
    if ((size_t)offset + toRead > g_streamSize) {
        toRead = g_streamSize - (size_t)offset;
    }
    memcpy(buffer, g_pStreamData + offset, toRead);
    return (EAS_I32)toRead;
}

static EAS_I32 eas_size_fn(void *handle) {
    return (EAS_I32)g_streamSize;
}

bool eas_engine_init(void) {
    if (g_pEASData != NULL) return true;
    EAS_RESULT res = EAS_Init(&g_pEASData);
    if (res != EAS_SUCCESS) {
        g_pEASData = NULL;
        return false;
    }
    
    // Set 64 voices & Reverb preset
    EAS_SetParameter(g_pEASData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_PRESET, EAS_PARAM_REVERB_ROOM);
    EAS_SetParameter(g_pEASData, EAS_MODULE_REVERB, EAS_PARAM_REVERB_BYPASS, 0);
    return true;
}

void eas_engine_shutdown(void) {
    eas_engine_stop_midi();
    if (g_pEASData != NULL) {
        EAS_Shutdown(g_pEASData);
        g_pEASData = NULL;
    }
}

bool eas_engine_play_midi_data(const uint8_t* data, size_t size) {
    if (!eas_engine_init() || !data || size == 0) return false;
    eas_engine_stop_midi();

    g_pStreamData = data;
    g_streamSize = size;
    g_streamPos = 0;

    g_easFile.handle = (void*)1;
    g_easFile.readAt = eas_read_fn;
    g_easFile.size = eas_size_fn;

    EAS_RESULT res = EAS_OpenFile(g_pEASData, &g_easFile, &g_pMediaHandle);
    if (res != EAS_SUCCESS || !g_pMediaHandle) {
        g_pStreamData = NULL;
        g_streamSize = 0;
        return false;
    }

    EAS_Prepare(g_pEASData, g_pMediaHandle);
    EAS_SetVolume(g_pEASData, g_pMediaHandle, (EAS_I32)(g_masterVolume * 100));
    return true;
}

void eas_engine_stop_midi(void) {
    if (g_pEASData && g_pMediaHandle) {
        EAS_CloseFile(g_pEASData, g_pMediaHandle);
        g_pMediaHandle = NULL;
    }
    g_pStreamData = NULL;
    g_streamSize = 0;
    g_streamPos = 0;
}

void eas_engine_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_masterVolume = volume;
    if (g_pEASData && g_pMediaHandle) {
        EAS_SetVolume(g_pEASData, g_pMediaHandle, (EAS_I32)(g_masterVolume * 100));
    }
}

int eas_engine_render_pcm(int16_t* pcmBuffer, int numFrames) {
    if (!g_pEASData || !pcmBuffer || numFrames <= 0) return 0;

    EAS_I32 countGenerated = 0;
    EAS_RESULT res = EAS_Render(g_pEASData, pcmBuffer, (EAS_I32)numFrames, &countGenerated);
    if (res != EAS_SUCCESS) {
        memset(pcmBuffer, 0, numFrames * sizeof(int16_t) * 2);
        return 0;
    }
    return (int)countGenerated;
}