/*
 * pong_game.h
 * Logica completa pentru jocul Pong
 * Include fizica, coliziuni, AI si rendering
 */

#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

/**
 * Initializeaza jocul:
 * - Reseteaza pozitiile paletelor si mingii
 * - Reseteaza scorurile
 * - Deseneaza terenul initial
 */
void Game_Init(void);

/**
 * Porneste jocul cu countdown animat
 * Tranzitioneaza la SCREEN_GAMEPLAY
 */
void Game_Start(void);

/**
 * Ruleaza un frame al jocului:
 * 1. Citeste input-urile (Joystick/IR/CPU)
 * 2. Actualizeaza pozitia paletelor
 * 3. Actualizeaza pozitia mingii si fizica
 * 4. Verifica coliziuni
 * 5. Deseneaza elementele modificate
 */
void Game_Update(void);

/**
 * Deseneaza terenul de joc initial
 */
void Game_DrawField(void);

/**
 * Deseneaza scorul curent
 */
void Game_DrawScore(void);

/**
 * Verifica daca jocul s-a terminat
 * @return ID-ul castigatorului (1 sau 2) sau 0 daca jocul continua
 */
uint8_t Game_GetWinner(void);

/**
 * Returneaza scorul unui jucator
 * @param player 1 sau 2
 * @return Scorul jucatorului
 */
int16_t Game_GetScore(uint8_t player);

/**
 * Pune jocul in pauza sau il reia
 * @param paused true pentru pauza, false pentru resume
 */
void Game_SetPaused(bool paused);

/**
 * Verifica daca jocul este in pauza
 * @return true daca jocul este in pauza
 */
bool Game_IsPaused(void);

/**
 * Verifica daca jocul ruleaza
 * @return true daca jocul este activ
 */
bool Game_IsRunning(void);

#endif /* PONG_GAME_H */
