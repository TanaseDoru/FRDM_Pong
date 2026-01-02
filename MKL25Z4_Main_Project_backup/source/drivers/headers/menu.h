/*
 * menu.h
 * Header pentru logica de meniu și UI
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h" // Necesar pentru tipurile Screen_t si MenuState_t

/* * Structura pentru starea meniului
 * (Definita aici sau in game_config.h, dar daca e folosita doar de meniu si main, e ok aici)
 */
typedef struct {
    uint8_t selectedIndex;
    uint8_t maxItems;
} MenuState_t;

/* --- Variabile Externe (definite in menu.c sau main.c) --- */
/* Le declaram extern pentru ca main.c sa le poata modifica direct la input */
extern MenuState_t g_menuState;
extern volatile uint8_t g_needsRedraw;
extern Screen_t g_currentScreen;

/* --- Funcții Publice --- */

/* Desenează ecranul curent (Main, Settings, Game, etc.) pe baza g_currentScreen */
void Menu_DrawCurrent(void);

/* Procesează selecția curentă (Enter/Select) și face tranzitia între ecrane */
void Menu_Select(void);

/* Verifică dacă condițiile de start sunt îndeplinite (ex: P2 are input selectat) */
/* Returnează true dacă putem începe jocul */
bool Menu_CanStartGame(void);

/* Funcții helper pentru navigare (Opțional, dacă nu modifici direct g_menuState în main) */
void Menu_MoveUp(void);
void Menu_MoveDown(void);

#endif /* MENU_H */
