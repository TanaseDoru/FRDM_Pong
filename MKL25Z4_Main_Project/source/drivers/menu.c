#include "headers/menu.h"
#include "headers/st7735_simple.h"
#include "fsl_debug_console.h"
#include <stdio.h>

/* Importam variabilele externe definite in main.c */
extern Screen_t g_currentScreen;
extern MenuState_t g_menuState;
extern InputType_t g_player1_input;
extern InputType_t g_player2_input;
extern volatile uint8_t g_needsRedraw;

/* --- Helpers pentru desenare --- */
static void DrawTitle(const char* title) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, 30, COLOR_BLACK);
    ST7735_DrawStringCentered(8, title, COLOR_CYAN, COLOR_BLACK, 2);
    ST7735_DrawHLine(0, 32, ST7735_WIDTH, COLOR_CYAN);
}

static void DrawMenuItem(uint8_t index, const char* text, bool selected) {
    int16_t y = 35 + index * 18;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BLACK;
    uint16_t textColor = selected ? COLOR_YELLOW : COLOR_WHITE;

    ST7735_FillRect(0, y, ST7735_WIDTH, 18, bgColor);
    if (selected) ST7735_DrawString(10, y + 6, ">", COLOR_YELLOW, bgColor);
    ST7735_DrawString(22, y + 6, text, textColor, bgColor);
}

/* Helper pt nume input */
static const char* GetInputName(InputType_t input) {
    switch (input) {
        case INPUT_JOYSTICK:  return "Joystick";
        case INPUT_REMOTE:    return "Remote";
        default:              return "None";
    }
}

/* --- FUNCTII PUBLICE --- */

void Menu_DrawCurrent(void) {
    ST7735_FillScreen(COLOR_BLACK);

    switch (g_currentScreen) {
        case SCREEN_MAIN:
            DrawTitle("PONG");
            DrawMenuItem(0, "Start Game", g_menuState.selectedIndex == 0);
            DrawMenuItem(1, "Select Input", g_menuState.selectedIndex == 1);
            DrawMenuItem(2, "Help", g_menuState.selectedIndex == 2);
            break;

        case SCREEN_START:
            DrawTitle("GAME MODE");
            DrawMenuItem(0, "Player vs Player", g_menuState.selectedIndex == 0);
            DrawMenuItem(1, "Player vs CPU", g_menuState.selectedIndex == 1); // NotImplemented
            DrawMenuItem(2, "Back", g_menuState.selectedIndex == 2);
            break;

        case SCREEN_SELECT_INPUT:
            DrawTitle("SETUP");

            char buf[30];
            snprintf(buf, sizeof(buf), "P1: %s", GetInputName(g_player1_input));
            DrawMenuItem(0, buf, g_menuState.selectedIndex == 0);

            snprintf(buf, sizeof(buf), "P2: %s", GetInputName(g_player2_input));
            DrawMenuItem(1, buf, g_menuState.selectedIndex == 1);

            DrawMenuItem(2, "Back", g_menuState.selectedIndex == 2);
            break;

        case SCREEN_SELECT_P1:
            DrawTitle("P1 INPUT");
            DrawMenuItem(0, "Joystick", g_menuState.selectedIndex == 0);
            DrawMenuItem(1, "Remote", g_menuState.selectedIndex == 1);
            DrawMenuItem(2, "None", g_menuState.selectedIndex == 2);
            DrawMenuItem(3, "Back", g_menuState.selectedIndex == 3);
            break;

        case SCREEN_SELECT_P2:
            DrawTitle("P2 INPUT");
            DrawMenuItem(0, "Joystick", g_menuState.selectedIndex == 0);
            DrawMenuItem(1, "Remote", g_menuState.selectedIndex == 1);
            DrawMenuItem(2, "None", g_menuState.selectedIndex == 2);
            DrawMenuItem(3, "Back", g_menuState.selectedIndex == 3);
            break;

        default:
            break;
    }
}

/* --- LOGICA DE TRANZITIE (Aceasta e functia care lipsea!) --- */
void Menu_Select(void) {
    switch (g_currentScreen) {
        case SCREEN_MAIN:
            if (g_menuState.selectedIndex == 0) { // Start
                g_currentScreen = SCREEN_START;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else if (g_menuState.selectedIndex == 1) { // Setup
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            }
            break;

        case SCREEN_START:
            if (g_menuState.selectedIndex == 2) { // Back
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            }
            // Start game este gestionat direct in main.c prin ACTION_SELECT + Menu_CanStartGame
            break;

        case SCREEN_SELECT_INPUT:
            if (g_menuState.selectedIndex == 0) { // P1
                g_currentScreen = SCREEN_SELECT_P1;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 4;
            } else if (g_menuState.selectedIndex == 1) { // P2
                g_currentScreen = SCREEN_SELECT_P2;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 4;
            } else { // Back
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 1;
                g_menuState.maxItems = 3;
            }
            break;

        case SCREEN_SELECT_P1:
            if (g_menuState.selectedIndex == 3) { // Back
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else {
                if (g_menuState.selectedIndex == 0) g_player1_input = INPUT_JOYSTICK;
                if (g_menuState.selectedIndex == 1) g_player1_input = INPUT_REMOTE;
                if (g_menuState.selectedIndex == 2) g_player1_input = INPUT_NONE;

                // Auto-return
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            }
            break;

        case SCREEN_SELECT_P2:
            if (g_menuState.selectedIndex == 3) { // Back
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 1;
                g_menuState.maxItems = 3;
            } else {
                if (g_menuState.selectedIndex == 0) g_player2_input = INPUT_JOYSTICK;
                if (g_menuState.selectedIndex == 1) g_player2_input = INPUT_REMOTE;
                if (g_menuState.selectedIndex == 2) g_player2_input = INPUT_NONE;

                // Auto-return
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 1;
                g_menuState.maxItems = 3;
            }
            break;

        default: break;
    }
    g_needsRedraw = 1;
}

bool Menu_CanStartGame(void) {
    if (g_player2_input == INPUT_NONE) {
        ST7735_FillRect(0, 100, 160, 28, COLOR_BLACK);
        ST7735_DrawString(10, 105, "ERR: P2 missing!", COLOR_RED, COLOR_BLACK);
        return false;
    }
    return true;
}
