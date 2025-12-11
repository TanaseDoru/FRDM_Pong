#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"

/* Modulele noastre */
#include "drivers/headers/game_config.h"
#include "drivers/headers/st7735_simple.h"
#include "drivers/headers/joystick.h"
#include "drivers/headers/ir_remote.h"
#include "drivers/headers/menu.h"
#include "drivers/headers/pong_game.h"

/* --- Variabile Globale (definite in game_config.h ca extern) --- */
InputType_t g_player1_input = INPUT_JOYSTICK; // Implicit P1 = Joystick
InputType_t g_player2_input = INPUT_NONE;
Screen_t g_currentScreen = SCREEN_BOOT_HELP;
MenuState_t g_menuState = {0, 3};
volatile uint8_t g_needsRedraw = 1;
Difficulty_t g_currentDifficulty = DIFF_EASY;


void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 6000; i++) __asm volatile("nop");
}

/* Functie unificata de procesare input pentru MENIU */
void ProcessMenuInput(void) {
    UI_Action_t action = ACTION_NONE;

    /* 1. Verificam Joystick (Doar daca P1 il are sau in meniu principal oricine) */
    action = Joystick_GetMenuAction();

    /* 2. Daca nu e joystick, verificam IR */
    if (action == ACTION_NONE) {
        action = IR_GetMenuAction();
    }

    /* 3. Executam actiunea */
    if (action != ACTION_NONE) {
        switch(action) {
            case ACTION_UP:
                PRINTF("UI: UP\r\n");
                // Logica Menu_MoveUp()
                if (g_menuState.selectedIndex > 0) g_menuState.selectedIndex--;
                else g_menuState.selectedIndex = g_menuState.maxItems - 1;
                g_needsRedraw = 1;
                break;

            case ACTION_DOWN:
                PRINTF("UI: DOWN\r\n");
                // Logica Menu_MoveDown()
                g_menuState.selectedIndex++;
                if (g_menuState.selectedIndex >= g_menuState.maxItems) g_menuState.selectedIndex = 0;
                g_needsRedraw = 1;
                break;

            case ACTION_SELECT:
                PRINTF("UI: SELECT\r\n");
                // Logica speciala pentru Start Game
                if (g_currentScreen == SCREEN_DIFFICULTY && g_menuState.selectedIndex != 3) { // != BACK
                     if (Menu_CanStartGame()) {
                         g_currentScreen = SCREEN_GAMEPLAY;
                         g_currentDifficulty = (Difficulty_t)g_menuState.selectedIndex;
                         Game_Init();
                     }
                } else {
                    Menu_Select(); // Functia veche de tranzitie ecrane
                }
                break;

            case ACTION_BACK:
                PRINTF("UI: BACK\r\n");
                // Logica Back
                g_menuState.selectedIndex = g_menuState.maxItems - 1;
                Menu_Select(); // Presupunem ca ultima optiune e mereu Back
                break;
            case ACTION_NONE:
            	break;
        }
    }
}

void processFPS()
{
	switch(g_currentDifficulty)
	{
	case DIFF_EASY:
		delay_ms(20);
		break;
	case DIFF_NORMAL:
		delay_ms(15);
		break;
	case DIFF_HARD:
		delay_ms(8);
		break;
	}
}

int main(void) {
    /* Init Hardware Board */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    PRINTF("--- PONG SYSTEM INIT ---\r\n");

    /* Init Module */
    ST7735_Init();
    Joystick_Init();
    IR_Init(); // Include Timer Init

    /* Initial Draw */
    Menu_DrawCurrent();

    while(1) {
        /* Procesare Drivere */
        Joystick_Process();
        IR_Process(); // Decodare NEC

        if (g_currentScreen == SCREEN_GAMEPLAY) {
            /* --- GAME LOOP --- */
            Game_Update();
            processFPS();

            // Verificare buton Back/Exit din joc (Joystick click sau IR 0)
            if (Joystick_GetMenuAction() == ACTION_SELECT) {
                g_currentScreen = SCREEN_MAIN;
                g_needsRedraw = 1;
            }

        } else {
            /* --- MENU LOOP --- */
            ProcessMenuInput();

            if (g_needsRedraw) {
                Menu_DrawCurrent();
                g_needsRedraw = 0;
            }

            delay_ms(50);
        }
    }
    return 0;
}
