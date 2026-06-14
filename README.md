# FlappySwitch

Un clon estilo Flappy Bird diseñado específicamente (Homebrew) para la consola Nintendo Switch usando **libnx**. Presenta un estilo retro/Nokia 5110 con un motor de renderizado por software desde cero.

## Características

- **Totalmente Nativo:** Escrito en C y compilado con `devkitA64` / `libnx`.
- **Renderizado por Software:** Dibujado manual en el framebuffer, simulando escalado y coloreado de mapas de bits monocromáticos (1-bit).
- **Múltiples Dificultades:** Tres niveles de dificultad (Fácil, Medio, Difícil) que alteran la gravedad, impulso de salto y velocidad.
- **Audio Generado Proceduralmente:** Utiliza la API nativa de `audout` para generar y reproducir ondas de sonido cuadradas retro (tipo chip PIC).
- **Controles Universales:** Navega por los menús usando tanto la cruceta (D-pad) como el Joystick analógico.

## Controles

- **Menús:** Joystick Izquierdo (Arriba/Abajo) o D-pad para navegar. Botón `A` para seleccionar/aceptar, Botón `B` para regresar.
- **Juego:** 
  - **Saltar:** Botón `A`
  - **Pausar:** Botón `+` (Plus)

## Compilación

Este proyecto usa la suite `devkitPro` y la librería `libnx`. 

Asegúrate de tener instaladas las herramientas de desarrollo de Switch. Luego, navega a la carpeta del proyecto en tu terminal y ejecuta:

```bash
make
```

Esto generará el archivo `FlappySwitch.nro` que puedes enviar a tu consola mediante el Homebrew Menu o emuladores compatibles (Ryujinx/Yuzu).

## Instalación

1. Copia el archivo `FlappySwitch.nro` a la carpeta `switch/` en la raíz de la tarjeta SD de tu consola Switch.
2. Inicia el Homebrew Menu mediante Álbum.
3. Inicia el juego.
