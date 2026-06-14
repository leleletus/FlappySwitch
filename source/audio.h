#ifndef AUDIO_H_
#define AUDIO_H_

#include <switch.h>

void audio_init(void);
void audio_exit(void);

// Reproduce una nota asíncrona (como en PIC)
// freq: frecuencia en Hz (ej. 440 para A4). 0 para silencio.
// duration_ms: duración en milisegundos.
void audio_play_note(u32 freq, u32 duration_ms);

#endif
