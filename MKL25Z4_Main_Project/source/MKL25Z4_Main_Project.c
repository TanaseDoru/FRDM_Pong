/*
 * MKL25Z4_Main_Project.c
 * Sistem de Meniu + Joc Pong Complet
 * ST7735 LCD + FRDM-KL25Z
 * Cu suport pentru Joystick si Telecomanda IR (cu hold)
 * 
 * Timere:
 * - SysTick: Timer global pentru milisecunde
 * - TPM1: Masurare pulsuri IR (folosit in ir_remote.c)
 * - PIT: Update periodic UI (20Hz pentru meniu, 50Hz pentru joc)
 */

#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "fsl_pit.h"

/* Modulele noastre */
#include "drivers/headers/game_config.h"
#include "drivers/headers/st7735_simple.h"
#include "drivers/headers/joystick.h"
#include "drivers/headers/ir_remote.h"
#include "drivers/headers/menu.h"
#include "drivers/headers/pong_game.h"

/*============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/

/* Variabile globale din game_config.h */
InputType_t g_player1_input = INPUT_JOYSTICK;   /* Default: Joystick */
InputType_t g_player2_input = INPUT_REMOTE;     /* Default: Telecomanda */
Screen_t g_currentScreen = SCREEN_INTRO;
MenuState_t g_menuState = {0, 3};
volatile uint8_t g_needsRedraw = 1;
Difficulty_t g_currentDifficulty = DIFF_NORMAL;

/* Timer global - incrementat de SysTick */
volatile uint32_t g_systick_ms = 0;

/* Flag-uri pentru update periodic */
static volatile bool g_update_ui = false;
static volatile bool g_update_game = false;

/* Pause menu state */
static uint8_t g_pause_selection = 0;

/*============================================================================
 * SYSTICK HANDLER - 1ms timer
 *============================================================================*/

void SysTick_Handler(void) {
    g_systick_ms++;
}

/*============================================================================
 * PIT HANDLER - Update periodic
 *============================================================================*/

void PIT_IRQHandler(void) {
    /* Channel 0 - UI/Menu update (50ms = 20Hz) */
    if (PIT_GetStatusFlags(PIT, kPIT_Chnl_0) & kPIT_TimerFlag) {
        PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
        g_update_ui = true;
    }
    
    /* Channel 1 - Game update (20ms = 50Hz) */
    if (PIT_GetStatusFlags(PIT, kPIT_Chnl_1) & kPIT_TimerFlag) {
        PIT_ClearStatusFlags(PIT, kPIT_Chnl_1, kPIT_TimerFlag);
        g_update_game = true;
    }
}

/*============================================================================
 * TIMER INITIALIZATION
 *============================================================================*/

static void Timer_Init(void) {
    /* SysTick - 1ms interrupt */
    SysTick_Config(SystemCoreClock / 1000U);
    
    /* PIT - Periodic Interrupt Timer */
    pit_config_t pitConfig;
    PIT_GetDefaultConfig(&pitConfig);
    PIT_Init(PIT, &pitConfig);
    
    /* Channel 0 - 50ms pentru meniu (20Hz) */
    /* Bus clock = 24MHz, pentru 50ms: 24000000 * 0.05 = 1200000 */
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, CLOCK_GetBusClkFreq() / 20U - 1U);
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
    
    /* Channel 1 - 20ms pentru joc (50Hz) */
    PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, CLOCK_GetBusClkFreq() / 50U - 1U);
    PIT_EnableInterrupts(PIT, kPIT_Chnl_1, kPIT_TimerInterruptEnable);
    
    EnableIRQ(PIT_IRQn);
    
    /* Pornim doar timer-ul pentru UI initial */
    PIT_StartTimer(PIT, kPIT_Chnl_0);
    
    PRINTF("[TIMER] SysTick + PIT initialized\r\n");
}

/*============================================================================
 * INPUT PROCESSING
 *============================================================================*/

/* Proceseaza input pentru meniu */
static void ProcessMenuInput(void) {
    UI_Action_t action = ACTION_NONE;
    
    /* Verificam Joystick */
    action = Joystick_GetMenuAction();
    
    /* Daca nu e joystick, verificam IR */
    if (action == ACTION_NONE) {
        action = IR_GetMenuAction();
    }
    
    /* Executam actiunea */
    if (action != ACTION_NONE) {
        switch(action) {
            case ACTION_UP:
                PRINTF("[UI] UP\r\n");
                Menu_MoveUp();
                break;
                
            case ACTION_DOWN:
                PRINTF("[UI] DOWN\r\n");
                Menu_MoveDown();
                break;
                
            case ACTION_SELECT:
                PRINTF("[UI] SELECT\r\n");
                Menu_Select();
                break;
                
            case ACTION_BACK:
                PRINTF("[UI] BACK\r\n");
                g_menuState.selectedIndex = g_menuState.maxItems - 1;
                Menu_Select();
                break;
                
            default:
                break;
        }
    }
}

/* Proceseaza input pentru ecranul de pauza */
static void ProcessPauseInput(void) {
    UI_Action_t action = ACTION_NONE;
    
    /* Verificam Joystick */
    action = Joystick_GetMenuAction();
    
    /* Daca nu e joystick, verificam IR */
    if (action == ACTION_NONE) {
        action = IR_GetMenuAction();
    }
    
    if (action != ACTION_NONE) {
        switch(action) {
            case ACTION_UP:
                g_pause_selection = 0;
                g_needsRedraw = 1;
                break;
                
            case ACTION_DOWN:
                g_pause_selection = 1;
                g_needsRedraw = 1;
                break;
                
            case ACTION_SELECT:
                if (g_pause_selection == 0) {
                    /* Resume */
                    PRINTF("[GAME] Resumed\r\n");
                    Game_SetPaused(false);
                    g_currentScreen = SCREEN_GAMEPLAY;
                    Game_DrawField();
                    Game_DrawScore();
                } else {
                    /* Exit to menu */
                    PRINTF("[GAME] Exit to menu\r\n");
                    g_currentScreen = SCREEN_MAIN;
                    g_menuState.selectedIndex = 0;
                    g_menuState.maxItems = 3;
                    g_needsRedraw = 1;
                    
                    /* Oprim timer-ul de joc */
                    PIT_StopTimer(PIT, kPIT_Chnl_1);
                }
                break;
                
            default:
                break;
        }
    }
}

/* Verifica daca se cere pauza in timpul jocului */
static void CheckGamePause(void) {
    /* Butonul joystick = pauza */
    if (Joystick_ButtonPressed()) {
        PRINTF("[GAME] Paused\r\n");
        Game_SetPaused(true);
        g_currentScreen = SCREEN_PAUSED;
        g_pause_selection = 0;
        g_needsRedraw = 1;
    }
}

/*============================================================================
 * DELAY FUNCTION (blocant - doar pentru animatii la init)
 *============================================================================*/

void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __asm volatile("nop");
    }
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(void) {
    /* Init Hardware */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();
    
    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("     PONG GAME - FRDM-KL25Z + ST7735   \r\n");
    PRINTF("     Joystick + IR Remote Control      \r\n");
    PRINTF("========================================\r\n\r\n");
    
    PRINTF("=== PINOUT ===\r\n");
    PRINTF("DISPLAY (ST7735):\r\n");
    PRINTF("  SCK -> PTC5    SDA -> PTC6\r\n");
    PRINTF("  DC  -> PTC3    CS  -> PTC4\r\n");
    PRINTF("  RES -> PTC0    BL/VCC -> 3.3V\r\n");
    PRINTF("\r\n");
    PRINTF("JOYSTICK:\r\n");
    PRINTF("  VRY -> PTB1 (ADC0_SE9)\r\n");
    PRINTF("  SW  -> PTD4\r\n");
    PRINTF("  VCC -> 3.3V   GND -> GND\r\n");
    PRINTF("\r\n");
    PRINTF("IR REMOTE:\r\n");
    PRINTF("  OUT -> PTA12\r\n");
    PRINTF("  VCC -> 3.3V   GND -> GND\r\n");
    PRINTF("==============\r\n\r\n");
    
    /* Init Timer (SysTick + PIT) */
    Timer_Init();
    
    /* Seed pentru random */
    srand(g_systick_ms ^ 0xDEADBEEF);
    
    /* Init Module */
    PRINTF("Initializing ST7735...\r\n");
    ST7735_Init();
    PRINTF("ST7735 OK!\r\n");
    
    PRINTF("Initializing Joystick...\r\n");
    Joystick_Init();
    PRINTF("Joystick OK!\r\n");
    
    PRINTF("Initializing IR Remote...\r\n");
    IR_Init();
    PRINTF("IR Remote OK!\r\n");
    
    PRINTF("\r\n=== CONTROLS ===\r\n");
    PRINTF("Joystick: Up/Down = Navigate, Press = Select\r\n");
    PRINTF("Remote:   CH- = Up, CH = Down, PREV = Select\r\n");
    PRINTF("In Game:  Joystick Button = Pause\r\n");
    PRINTF("          Hold CH-/CH for continuous movement\r\n");
    PRINTF("================\r\n\r\n");
    
    /* Deseneaza ecranul initial (intro animation) */
    Menu_DrawCurrent();
    
    PRINTF(">>> System Ready! <<<\r\n\r\n");
    
    /* Main Loop */
    while (1) {
        /* Proceseaza input-urile - mereu */
        Joystick_Process();
        IR_Process();
        
        /* Comportament bazat pe ecranul curent */
        switch (g_currentScreen) {
            case SCREEN_GAMEPLAY:
                /* In timpul jocului */
                if (!Game_IsPaused()) {
                    /* Verifica daca se cere pauza */
                    CheckGamePause();
                    
                    /* Update joc la 50Hz (controlat de PIT) */
                    if (g_update_game) {
                        g_update_game = false;
                        
                        Game_Update();
                        
                        /* Verifica daca jocul s-a terminat */
                        if (!Game_IsRunning() && Game_GetWinner() != 0) {
                            PRINTF("\r\n=== GAME OVER ===\r\n");
                            PRINTF("Winner: Player %d\r\n", Game_GetWinner());
                            PRINTF("Score: %d - %d\r\n\r\n", 
                                   Game_GetScore(1), Game_GetScore(2));
                            
                            /* Oprim timer-ul de joc */
                            PIT_StopTimer(PIT, kPIT_Chnl_1);
                            
                            g_currentScreen = SCREEN_GAME_OVER;
                            g_menuState.selectedIndex = 0;
                            g_menuState.maxItems = 2;
                            g_needsRedraw = 1;
                        }
                    }
                }
                break;
                
            case SCREEN_PAUSED:
                /* In pauza - proceseaza meniul de pauza */
                if (g_update_ui) {
                    g_update_ui = false;
                    ProcessPauseInput();
                    
                    if (g_needsRedraw && g_currentScreen == SCREEN_PAUSED) {
                        Menu_DrawPauseScreen();
                        g_needsRedraw = 0;
                    }
                }
                break;
                
            default:
                /* In meniu - update la 20Hz */
                if (g_update_ui) {
                    g_update_ui = false;
                    ProcessMenuInput();
                    
                    if (g_needsRedraw) {
                        Menu_DrawCurrent();
                    }
                    
                    /* Daca s-a pornit jocul, activeaza timer-ul de joc */
                    if (g_currentScreen == SCREEN_GAMEPLAY) {
                        PIT_StartTimer(PIT, kPIT_Chnl_1);
                    }
                }
                break;
        }
        
        /* Economiseste energie cand nu e nimic de facut */
        __WFI();
    }
    
    return 0;
}
