#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "audio.h"
#include "bitmapPhotos.h"

// Resolución lógica
#define GAME_W    84
#define GAME_H    48

// Resoluciones físicas de la consola
static u32 screen_w = 1280;
static u32 screen_h = 720;
#define SCREEN_W screen_w
#define SCREEN_H screen_h

// Sprites originales (sin escalar)
#define TUBE_W    6
#define TUBE_H_U 24   // tubería arriba 6×24
#define TUBE_H_D 24   // tubería abajo  6×24
#define MON_W     9
#define MON_H    16

// Márgenes para “corte” aleatorio de la tubería inferior
#define MARGIN_BOTTOM 5
#define MARGIN_TOP    4

// Gap vertical variable (entre MON_H+2 y MON_H+8)
#define GAP_MIN  (MON_H + 2)
#define GAP_MAX  (MON_H + 8)

// Separación horizontal mínima entre obstáculos
#define MIN_PIPE_SPACING 30

// Umbral stick para navegar menús
#define JOY_THRESH 16000

// Estados de la aplicación
typedef enum {
  EST_BIENVENIDA = 0,
  EST_MENU,
  EST_JUEGO,
  EST_PAUSA,
  EST_RETRY,
  EST_SCORE,
  EST_LEVEL,
  EST_GAMEOVER
} EstadoApp;

// Opciones de menú
typedef enum {
  OPT_PLAY = 0,
  OPT_SCORE,
  OPT_LEVEL,
  OPT_COUNT
} OpcionMenu;

// Dificultades
typedef enum {
  FACIL = 0,
  MEDIO,
  DIFICIL
} Dificultad;

// Física vertical
static float posY_monstruo_f = 1.0f;
static float velY            = 0.0f;

// Gravedad por dificultad
static const float gravedadPorNivel[3] = { 0.20f, 0.30f, 0.50f };

// Impulso de salto por dificultad
static const float jumpImpulsos[3] = { 2.0f, 2.5f, 3.5f };

// Velocidad de tubos por dificultad
static const int tubeSpeeds[3] = { 1, 2, 3 };

// Globals de estado
static Framebuffer  fb;
static PadState     pad;
static EstadoApp    estado     = EST_BIENVENIDA;
static OpcionMenu   menuIndex  = OPT_PLAY;
static Dificultad   dificultad = FACIL;
static bool         in_level   = false;
static Dificultad   level_sel  = FACIL;

// Escala y offsets de pantalla
static int scale, offX, offY;

// Juego: posiciones y banderas
static int posY_monstruo;
static int posX_obs[3];
static int posY_obs_down[3], posY_obs_up[3];
static int frameCounter, frameAnim;
static int vida = 3, choque;

// Contadores para animar en bienvenida
static int welcomeFrameCounter = 0;
static int welcomeFrameAnim   = 0;

// Puntuación
static u32 puntaje, mejores[3];

// Puntuación por vida
static u32 vidaScore[3];    // guardará los puntajes de vida 0,1 y 2
static int vidaIndex;       // índice 0..2 de la vida actual


// Escalas para monstruo
static int renderScale;  // dibujo
static int hitboxScale;  // colisión

// Límite lógico de Y para que el sprite escalado toque el suelo real
static float maxPosY_f;

// Flags para puntuar cada obstáculo sólo una vez
static bool scored[3];

// Reinicia el juego
// total==true  → reset total (vidas=3, puntaje=0, menuIndex)
// total==false → reset parcial (mantiene vidas y mejores)
void reset_game(bool total) {
  // Reposicionar monstruo
  posY_monstruo   = 1;
  posY_monstruo_f = 1.0f;
  velY            = 0.0f;

  // Reposicionar tuberías y resetear scored[i]
  for (int i = 0; i < 3; i++) {
    posX_obs[i] = GAME_W + MIN_PIPE_SPACING * i;
    int maxCut = TUBE_H_D + 1 - MARGIN_TOP - MARGIN_BOTTOM;
    int cut    = rand() % maxCut;
    posY_obs_down[i] = GAME_H - (MARGIN_BOTTOM + cut);
    int gap = GAP_MIN + rand() % (GAP_MAX - GAP_MIN + 1);
    posY_obs_up[i]   = posY_obs_down[i] - gap - TUBE_H_U;
    scored[i]        = false;
  }

  choque       = 0;
  frameAnim    = 0;
  frameCounter = 0;

  if (total) {
    vida      = 3;
    puntaje   = 0;
    menuIndex = OPT_PLAY;
    estado    = EST_MENU;

    // reinicia el tracking de vidas
    vidaIndex = 0;
    vidaScore[0] = vidaScore[1] = vidaScore[2] = 0;

  }
}

int main(void) {
  // 1) Calcula scale y centra el área de juego
  int sx = SCREEN_W / GAME_W;
  int sy = SCREEN_H / GAME_H;
  scale = (sx < sy) ? sx : sy;
  offX  = (SCREEN_W - GAME_W * scale) / 2;
  offY  = (SCREEN_H - GAME_H * scale) / 2;

  // 2) Prepara escalas del monstruo
  renderScale = scale / 2;
  if (renderScale < 1) renderScale = 1;
  hitboxScale = renderScale;

  // 3) Calcula límite Y para suelo real
  maxPosY_f = ((float)GAME_H * scale - MON_H * renderScale)
              / (float)renderScale;

  // 4) Inicializa framebuffer y pad
  NWindow* win = nwindowGetDefault();
  framebufferCreate(&fb, win,
                    SCREEN_W, SCREEN_H,
                    PIXEL_FORMAT_RGBA_8888, 2);
  framebufferMakeLinear(&fb);
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);
  audio_init();

  // 5) Primer reset parcial (mantiene pantalla de bienvenida)
  reset_game(false);

  s16 prev_stick_y = 0;

  // 6) Bucle principal
  while (appletMainLoop()) {
    u64 tick_start = armGetSystemTick();

    padUpdate(&pad);
    u64 down = padGetButtonsDown(&pad);

    HidAnalogStickState stick = padGetStickPos(&pad, 0);
    s16 stick_y = stick.y;
    
    // Joystick Y is positive UP in libnx, but let's allow both D-pad and analog stick
    bool stick_up = (stick_y > JOY_THRESH) && !(prev_stick_y > JOY_THRESH);
    bool stick_down = (stick_y < -JOY_THRESH) && !(prev_stick_y < -JOY_THRESH);
    prev_stick_y = stick_y;

    // Cada estado gestiona sus propias pulsaciones de “+”
    // Inicia frame
    u32 stride;
    u8* fbptr = framebufferBegin(&fb, &stride);
    clear_buffer(fbptr, stride, SCREEN_W, SCREEN_H, 0, 0, 0);

    switch (estado) {
      // ───────────────────────────────────────────────
      case EST_BIENVENIDA:
        // Dibuja el fondo
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                          offX, offY,
                          fondo_bienvenida,
                          GAME_W, GAME_H, scale, 255, 255, 255);

        // 1) Actualiza animación del monstruo
        if (++welcomeFrameCounter > 12) {
          welcomeFrameAnim = (welcomeFrameAnim + 1) % 3;
          welcomeFrameCounter = 0;
        }
        // Escoge el frame correspondiente
        const u8* mframe =
          welcomeFrameAnim == 0 ? M_arriba :
          welcomeFrameAnim == 1 ? M_medio  :
                                  M_bajo;

        // 2) Calcula posición para centrar abajo
        int monsterX = offX + (GAME_W*scale - MON_W*renderScale) / 2;
        int monsterY = offY + (GAME_H*scale - MON_H*renderScale) - 4*scale; // 4*scale de margen inferior
        // Ajustes extra
        monsterX += 1 * scale;   // +1 unidad lógica a la derecha
        monsterY -= 4 * scale;   // -1 unidad lógica (sube)


        // 3) Dibuja el monstruo animado
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                          monsterX, monsterY,
                          mframe,
                          MON_W, MON_H,
                          renderScale, 255, 255, 0);

        // 4) Si se pulsa cualquier botón, pasamos al menú
        if (down) estado = EST_MENU;
        break;

      // ───────────────────────────────────────────────
      case EST_MENU: {
        if ((down & HidNpadButton_Up)   || stick_up) {
          menuIndex = (menuIndex + OPT_COUNT - 1) % OPT_COUNT;
          audio_play_note(300, 50);
        }
        if ((down & HidNpadButton_Down) || stick_down) {
          menuIndex = (menuIndex + 1) % OPT_COUNT;
          audio_play_note(300, 50);
        }

        const u8* bmp =
          menuIndex == OPT_PLAY  ? menu_select_play  :
          menuIndex == OPT_SCORE ? menu_select_score:
                                   menu_select_level;
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                           offX, offY,
                           bmp, GAME_W, GAME_H, scale, 255, 255, 255);

        if (down & HidNpadButton_A) {
          audio_play_note(440, 100);
          if (menuIndex == OPT_PLAY) {
            reset_game(false);
            estado = EST_JUEGO;
          }
          else if (menuIndex == OPT_SCORE) {
            estado = EST_SCORE;
          }
          else {
            in_level = false;
            estado   = EST_LEVEL;
          }
        }
        break;
      }

      // ───────────────────────────────────────────────
      case EST_LEVEL:
        if (down & HidNpadButton_B) {
          audio_play_note(300, 100);
          estado = EST_MENU;
          break;
        }
        if (!in_level) {
          in_level  = true;
          level_sel = dificultad;
        }
        if ((down & HidNpadButton_Up)   || stick_up) {
          level_sel = (Dificultad)((level_sel + 2) % 3);
          audio_play_note(300, 50);
        }
        if ((down & HidNpadButton_Down) || stick_down) {
          level_sel = (Dificultad)((level_sel + 1) % 3);
          audio_play_note(300, 50);
        }

        {
          const u8* bmp =
            level_sel == FACIL  ? nivel_facil  :
            level_sel == MEDIO  ? nivel_mediano:
                                  nivel_dificil;
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX, offY,
                             bmp, GAME_W, GAME_H, scale, 255, 255, 255);
        }

        // Al confirmar nivel: reset TOTAL y jugar
        if (down & HidNpadButton_A) {
          audio_play_note(550, 200);
          dificultad = level_sel;
          reset_game(true);
          estado = EST_JUEGO;
        }
        break;

      // ───────────────────────────────────────────────
      case EST_SCORE:
        // 1) Limpia la pantalla
        clear_buffer(fbptr, stride, SCREEN_W, SCREEN_H, 0, 0, 0);

        // 2) Parámetros de texto
        uint32_t white    = 0xFFFFFFFF;
        int      txtScale = renderScale;   // aprox mitad de scale lógico
        char     buf[12];

        // 3) Cabecera "PUNTAJES:" a la izquierda
        int headerX = offX + 6 * scale;
        int headerY = offY + 2 * scale;
        drawString(fbptr, stride,
                  headerX, headerY,
                  "PUNTAJES:", txtScale * 2, white);

        // 4) Y inicial para "FACIL" (sube más abajo)
        //    txtScale*2*8 = alto en px del encabezado
        int y0 = headerY + txtScale * 2 * 8 + 8 * scale;

        // 5) Separación vertical reducida entre líneas
        int dy = 8 * scale;

        // Línea FACIL
        //mejores[FACIL] = 12345678;   // valor de prueba
        sprintf(buf, "%u", mejores[FACIL]); 
        drawString(fbptr, stride,
                  offX + 6 * scale, y0,
                  "FACIL:", txtScale, white);
        drawString(fbptr, stride,
                  offX + 40 * scale, y0,
                  buf, txtScale, white);

        // Línea MEDIO
        sprintf(buf, "%u", mejores[MEDIO]);
        drawString(fbptr, stride,
                  offX + 6 * scale, y0 + dy,
                  "MEDIO:", txtScale, white);
        drawString(fbptr, stride,
                  offX + 40 * scale, y0 + dy,
                  buf, txtScale, white);

        // Línea DIFICIL
        sprintf(buf, "%u", mejores[DIFICIL]);
        drawString(fbptr, stride,
                  offX + 6 * scale, y0 + dy * 2,
                  "DIFICIL:", txtScale, white);
        drawString(fbptr, stride,
                  offX + 40 * scale, y0 + dy * 2,
                  buf, txtScale, white);

        // 6) Volver al menú con cualquier pulsación
        if (down) {
          estado = EST_MENU;
        }
        break;
      // ───────────────────────────────────────────────
      case EST_JUEGO: {
        // Pausa
        if (down & HidNpadButton_Plus) {
          audio_play_note(600, 100);
          estado = EST_PAUSA;
          break;
        }
        // Salto
        if (down & HidNpadButton_A) {
          audio_play_note(880, 100);
          velY = -jumpImpulsos[dificultad];
        }
        // Gravedad y mover
        velY += gravedadPorNivel[dificultad];
        posY_monstruo_f += velY;
        if (posY_monstruo_f > maxPosY_f) {
          posY_monstruo_f = maxPosY_f; velY = 0.0f;
        }
        if (posY_monstruo_f < 0.0f) {
          posY_monstruo_f = 0.0f; velY = 0.0f;
        }
        posY_monstruo = (int)(posY_monstruo_f + 0.5f);
        
        // 1) Mover tuberías
        for (int i = 0; i < 3; i++) {
          posX_obs[i] -= tubeSpeeds[dificultad];
        }

        // 2) Puntaje por obstáculo superado (antes de reposicionar)
        for (int i = 0; i < 3; i++) {
          if (!scored[i] && posX_obs[i] + TUBE_W < 0) {
            puntaje++;
            scored[i] = true;
            audio_play_note(1200, 150); // Sonido de punto
            if (puntaje > mejores[dificultad]) {
              mejores[dificultad] = puntaje;
            }
          }
        }

        // 3) Reposicionar tuberías y permitir nuevo scoring
        for (int i = 0; i < 3; i++) {
          if (posX_obs[i] < -TUBE_W) {
            // encuentra la tubería más a la derecha
            int maxX = posX_obs[0];
            for (int j = 1; j < 3; j++)
              if (posX_obs[j] > maxX) maxX = posX_obs[j];
            posX_obs[i] = maxX + MIN_PIPE_SPACING;

            // recalcula corte y gap
            int maxCut = TUBE_H_D + 1 - MARGIN_TOP - MARGIN_BOTTOM;
            int cut    = rand() % maxCut;
            posY_obs_down[i] = GAME_H - (MARGIN_BOTTOM + cut);
            int gap = GAP_MIN + rand() % (GAP_MAX - GAP_MIN + 1);
            posY_obs_up[i]   = posY_obs_down[i] - gap - TUBE_H_U;

            // listo para puntuarla otra vez
            scored[i] = false;
          }
        }

        // Animación
        if (++frameCounter > 12) {
          frameAnim = (frameAnim + 1) % 3;
          frameCounter = 0;
        }

        // Detección de choque
        {
          int mx = offX;
          int my = offY + posY_monstruo * hitboxScale;
          int mw = MON_W * hitboxScale;
          int mh = MON_H * hitboxScale;
          // Inferior
          for (int i = 0; i < 3; i++) {
            if (rect_collide(mx,my,mw,mh,
                 offX + posX_obs[i]*scale,
                 offY + posY_obs_down[i]*scale,
                 TUBE_W*scale, TUBE_H_D*scale)) {
              choque = 1; break;
            }
          }
          // Superior
          if (!choque) {
            for (int i = 0; i < 3; i++) {
              if (rect_collide(mx,my,mw,mh,
                   offX + posX_obs[i]*scale,
                   offY + posY_obs_up[i]*scale,
                   TUBE_W*scale, TUBE_H_U*scale)) {
                choque = 1; break;
              }
            }
          }
          // Suelo / techo
          if (posY_monstruo_f >= maxPosY_f ||
              posY_monstruo_f <= 0.0f) {
            choque = 1;
          }
        }

        // Vidas / RETRY
        if (choque) {
          choque = 0;
          audio_play_note(150, 400); // Sonido de choque grave

          // 1) Guarda el puntaje de esta vida
          if (vidaIndex < 3) {
            vidaScore[vidaIndex++] = puntaje;
          }

          vida--;  // pierdes una vida

          if (vida > 0) {
            // 2) Para la siguiente vida, arranca de 0
            puntaje = 0;
            estado  = EST_RETRY;
          } else {
            // 3) Ya no quedan vidas: elige el mejor de las tres
            u32 bestLife = vidaScore[0];
            if (vidaScore[1] > bestLife) bestLife = vidaScore[1];
            if (vidaScore[2] > bestLife) bestLife = vidaScore[2];
            
            // por si quieor la suma de las 3 como puntaje comento lo de arriba y descomento esto
            /* u32 sumLife = vidaScore[0] + vidaScore[1] + vidaScore[2];
            if (sumLife > mejores[dificultad]) {
              mejores[dificultad] = sumLife;
            } */


            // 4) Actualiza el high score si corresponde
            if (bestLife > mejores[dificultad]) {
              mejores[dificultad] = bestLife;
            }

            // 5) Ahora sí reinicia todo
            reset_game(true);
            estado = EST_BIENVENIDA;
          }
          break;
        }

        // Render de juego
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                           offX, offY,
                           BACKGROUND,
                           GAME_W, GAME_H, scale, 135, 206, 235); // Cielo
        for (int i = 0; i < 3; i++) {
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX + posX_obs[i]*scale,
                             offY + posY_obs_down[i]*scale,
                             tuberia_abajo,
                             TUBE_W, TUBE_H_D, scale, 0, 255, 0); // Tubo verde
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX + posX_obs[i]*scale,
                             offY + posY_obs_up[i]*scale,
                             tuberia_arriba,
                             TUBE_W, TUBE_H_U, scale, 0, 255, 0); // Tubo verde
        }
        {
          const u8* mframe =
            frameAnim == 0 ? M_arriba :
            frameAnim == 1 ? M_medio  :
                             M_bajo;
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX,
                             offY + posY_monstruo * renderScale,
                             mframe,
                             MON_W, MON_H,
                             renderScale, 255, 255, 0); // Pájaro amarillo
        }
        break;
      }

      // ─────────── EST_PAUSA ────────────────────────────
      case EST_PAUSA:
        // Redibuja el frame congelado
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                           offX, offY,
                           BACKGROUND,
                           GAME_W, GAME_H, scale, 135, 206, 235);
        for (int i = 0; i < 3; i++) {
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX + posX_obs[i]*scale,
                             offY + posY_obs_down[i]*scale,
                             tuberia_abajo,
                             TUBE_W, TUBE_H_D, scale, 0, 255, 0);
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX + posX_obs[i]*scale,
                             offY + posY_obs_up[i]*scale,
                             tuberia_arriba,
                             TUBE_W, TUBE_H_U, scale, 0, 255, 0);
        }
        {
          const u8* mframe =
            frameAnim == 0 ? M_arriba :
            frameAnim == 1 ? M_medio  :
                             M_bajo;
          draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                             offX,
                             offY + posY_monstruo * renderScale,
                             mframe,
                             MON_W, MON_H,
                             renderScale, 255, 255, 0);
        }
        // Reanudar con +
        if (down & HidNpadButton_Plus) {
          audio_play_note(880, 100);
          estado = EST_JUEGO;
        }
        break;

      // ─────────── EST_RETRY ────────────────────────────
      case EST_RETRY:
        draw_bitmap_scaled(fbptr, stride, SCREEN_W, SCREEN_H,
                           offX, offY,
                           retry,
                           GAME_W, GAME_H, scale, 255, 100, 100); // Rojo para retry
        // Reanudar con +
        if (down & HidNpadButton_Plus) {
          audio_play_note(550, 100);
          reset_game(false);
          estado = EST_JUEGO;
        }
        break;

      default:
        break;
    }

    framebufferEnd(&fb);
    u64 tick_end = armGetSystemTick();
    u64 elapsed_ns = (tick_end - tick_start) * 1000000000ULL / armGetSystemTickFreq();
    if (elapsed_ns < 16666667ULL) {
        svcSleepThread(16666667ULL - elapsed_ns); // ~60fps
    }
  }

  audio_exit();
  framebufferClose(&fb);
  return 0;
}