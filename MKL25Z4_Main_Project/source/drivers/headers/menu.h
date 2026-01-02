/*
 * menu.h
 * Header pentru logica de meniu si UI
 * Include ecranele de intro, meniu, selectie si game over
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/*============================================================================
 * PUBLIC FUNCTIONS - MENU NAVIGATION
 *============================================================================*/

/**
 * Deseneaza ecranul curent bazat pe g_currentScreen
 */
void Menu_DrawCurrent(void);

/**
 * Proceseaza selectia curenta si face tranzitia intre ecrane
 */
void Menu_Select(void);

/**
 * Verifica daca se poate incepe jocul
 * (P2 are input selectat)
 * @return true daca putem incepe
 */
bool Menu_CanStartGame(void);

/**
 * Muta selectia in sus
 */
void Menu_MoveUp(void);

/**
 * Muta selectia in jos
 */
void Menu_MoveDown(void);

/*============================================================================
 * PUBLIC FUNCTIONS - SPECIAL SCREENS
 *============================================================================*/

/**
 * Ruleaza animatia de intro (BMO + Pong Demo)
 * Blocant - asteapta apasarea butonului
 */
void Menu_PlayIntroAnimation(void);

/**
 * Deseneaza ecranul de pauza (overlay)
 */
void Menu_DrawPauseScreen(void);

/**
 * Deseneaza ecranul de Game Over
 */
void Menu_DrawGameOverScreen(void);

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * Returneaza numele unui tip de input
 */
const char* GetInputName(InputType_t input);

/**
 * Verifica daca un input este disponibil pentru un jucator
 * (nu e folosit de celalalt jucator)
 */
bool IsInputAvailable(InputType_t input, uint8_t forPlayer);

#endif /* MENU_H */
