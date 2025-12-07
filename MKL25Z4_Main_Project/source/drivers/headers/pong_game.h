/*
 * pong_game.h
 * Header pentru logica jocului (Fizică, Coliziuni, Render Joc)
 */

#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdint.h>
#include "game_config.h"

/* --- Funcții Publice --- */

/* * Inițializează variabilele jocului:
 * - Poziția paletelor la mijloc
 * - Poziția mingii la centru
 * - Scorul la 0
 * - Desenează terenul inițial
 */
void Game_Init(void);

/* * Rulează un cadru (frame) al jocului:
 * 1. Citește input-urile (Joystick/IR)
 * 2. Actualizează poziția paletelor
 * 3. Calculează fizica mingii și coliziunile
 * 4. Desenează elementele noi pe ecran
 * * Această funcție trebuie apelată în bucla infinită din main
 * când g_currentScreen == SCREEN_GAMEPLAY
 */
void Game_Update(void);

#endif /* PONG_GAME_H */
