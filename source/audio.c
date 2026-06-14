#include "audio.h"
#include <math.h>
#include <string.h>
#include <malloc.h>

#define SAMPLERATE 48000
#define CHANNELS 2
// BUFFER_SIZE must be a multiple of 0x1000
#define BUFFER_SIZE 0x2000 // 8192 bytes = ~42ms of audio per buffer

static Thread audioThread;
static bool audioRunning = false;
static u32 current_freq = 0;
static u32 current_duration_ms = 0;
static u64 start_time = 0;
static s16* audio_buffers[2] = {NULL, NULL};
static AudioOutBuffer audout_buffers[2];
static AudioOutBuffer* released_buffer = NULL;
static u32 released_count = 0;

static void audio_thread_func(void* arg) {
    u64 phase = 0;

    // Llenar el primer buffer e iniciar
    for (int b = 0; b < 2; b++) {
        memset(audio_buffers[b], 0, BUFFER_SIZE);
        audout_buffers[b].next = NULL;
        audout_buffers[b].buffer = audio_buffers[b];
        audout_buffers[b].buffer_size = BUFFER_SIZE;
        audout_buffers[b].data_size = BUFFER_SIZE;
        audout_buffers[b].data_offset = 0;
        audoutAppendAudioOutBuffer(&audout_buffers[b]);
    }

    audoutStartAudioOut();

    while (audioRunning) {
        audoutWaitPlayFinish(&released_buffer, &released_count, 1000000000ULL);
        if (released_count == 0 || !released_buffer) continue;

        // Comprobar duración de la nota
        if (current_freq > 0) {
            u64 current_time = svcGetSystemTick();
            u64 elapsed_ms = (current_time - start_time) * 1000 / armGetSystemTickFreq();
            if (elapsed_ms >= current_duration_ms) {
                current_freq = 0; // Silencio al terminar la nota
            }
        }

        s16* buf = (s16*)released_buffer->buffer;
        u32 num_samples = released_buffer->buffer_size / (CHANNELS * sizeof(s16));

        if (current_freq > 0) {
            // Generar onda cuadrada (estilo retro/PIC) o seno
            // Usaremos onda cuadrada para mayor fidelidad a tu petición tipo "PIC"
            double half_period = (double)SAMPLERATE / (current_freq * 2.0);
            for (u32 i = 0; i < num_samples; i++) {
                s16 sample = ((u64)(phase / half_period) % 2 == 0) ? 8000 : -8000;
                buf[i * 2 + 0] = sample; // L
                buf[i * 2 + 1] = sample; // R
                phase++;
            }
        } else {
            memset(buf, 0, released_buffer->buffer_size);
        }

        audoutAppendAudioOutBuffer(released_buffer);
    }
    
    audoutStopAudioOut();
}

void audio_init(void) {
    if (R_FAILED(audoutInitialize())) return;
    
    // Asignar memoria alineada (requerimiento de audout)
    audio_buffers[0] = memalign(0x1000, BUFFER_SIZE);
    audio_buffers[1] = memalign(0x1000, BUFFER_SIZE);
    
    audioRunning = true;
    threadCreate(&audioThread, audio_thread_func, NULL, NULL, 0x10000, 0x2B, -2);
    threadStart(&audioThread);
}

void audio_exit(void) {
    if (!audioRunning) return;
    audioRunning = false;
    threadWaitForExit(&audioThread);
    threadClose(&audioThread);
    
    audoutExit();
    free(audio_buffers[0]);
    free(audio_buffers[1]);
}

void audio_play_note(u32 freq, u32 duration_ms) {
    current_freq = freq;
    current_duration_ms = duration_ms;
    start_time = svcGetSystemTick();
}
