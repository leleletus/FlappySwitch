#ifndef DRAW_H_
#define DRAW_H_

#include <switch.h>
#include <stdbool.h>
#include <stdint.h>


// Animación monocroma con bitmaps escalados
typedef struct {
    const u8** frames;
    int frame_count;
    int tick_per_frame;
    int width, height;
    int scale;
    int tick, frame_index;
} Animation;

// Inicializa animación
void init_animation(Animation* anim,
                    const u8** frames, int count,
                    int tick_per_frame, int w, int h, int scale);

// Actualiza animación
void update_animation(Animation* anim);

// Dibuja el frame actual de la animación
void draw_animation(u8* fb, u32 stride, u32 screen_w, u32 screen_h,
                    int x, int y, Animation* anim);

// Limpia framebuffer a color RGB
void clear_buffer(u8* fb, u32 stride, u32 screen_w, u32 screen_h,
                  u8 r, u8 g, u8 b);


// Dibuja bitmap monocromo estilo Nokia5110 escalado con color personalizado
// bitmap: arreglo de (w * (h/8)) bytes, cada byte = 8 píxeles verticales
void draw_bitmap_scaled(
    u8* fb,
    u32 stride,
    u32 screen_w,
    u32 screen_h,
    int x, int y,
    const u8* bitmap,
    int w,      // ancho en bytes del bitmap
    int h,      // alto en píxeles (múltiplo de 8)
    int scale,  // factor de escala
    u8 r_col, u8 g_col, u8 b_col // Color RGB
);

// Colisión AABB
bool rect_collide(int x1, int y1, int w1, int h1,
                  int x2, int y2, int w2, int h2);


// Prototipos para dibujar texto sobre el framebuffer
void drawChar(uint8_t* fbptr, uint32_t stride,
              int x, int y, char c,
              int scale, uint32_t color);

void drawString(uint8_t* fbptr, uint32_t stride,
                int x, int y, const char *s,
                int scale, uint32_t color);



#endif // DRAW_H_
