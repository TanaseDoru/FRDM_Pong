/*
 * menu.c
 * Logica completa pentru meniuri si UI
 * Include animatia de intro BMO, toate ecranele si navigarea
 */

#include "headers/menu.h"
#include "headers/st7735_simple.h"
#include "headers/joystick.h"
#include "headers/ir_remote.h"
#include "headers/pong_game.h"
#include "fsl_debug_console.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * EXTERNAL VARIABLES
 *============================================================================*/

extern Screen_t g_currentScreen;
extern MenuState_t g_menuState;
extern InputType_t g_player1_input;
extern InputType_t g_player2_input;
extern volatile uint8_t g_needsRedraw;
extern volatile uint32_t g_systick_ms;
extern Difficulty_t g_currentDifficulty;

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define MENU_START_Y      35
#define MENU_ITEM_HEIGHT  16
#define MENU_MARGIN_X     10
#define TITLE_Y           8

#define COLOR_BG          COLOR_BLACK
#define COLOR_TITLE       COLOR_CYAN
#define COLOR_NORMAL      COLOR_WHITE
#define COLOR_SELECTED    COLOR_YELLOW
#define COLOR_DISABLED    COLOR_GRAY
#define COLOR_HIGHLIGHT   COLOR_GREEN
#define COLOR_BACK        COLOR_ORANGE

#define BMO_SCREEN_COLOR  0x0400
#define BMO_FACE_COLOR    COLOR_BLACK

static const uint16_t rainbow_colors[] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, 
    COLOR_CYAN, COLOR_BLUE, COLOR_MAGENTA
};
#define NUM_RAINBOW_COLORS 7

/* Pause menu selection */
static uint8_t g_pause_selection = 0;

/*============================================================================
 * DELAY FUNCTION
 *============================================================================*/

static void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __asm volatile("nop");
    }
}

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

const char* GetInputName(InputType_t input) {
    switch (input) {
        case INPUT_JOYSTICK:   return "Joystick";
        case INPUT_REMOTE:     return "Remote";
        case INPUT_GYROSCOPE:  return "Gyroscope";
        case INPUT_CPU_EASY:   return "CPU Easy";
        case INPUT_CPU_MEDIUM: return "CPU Med";
        case INPUT_CPU_HARD:   return "CPU Hard";
        default:               return "None";
    }
}

bool IsInputAvailable(InputType_t input, uint8_t forPlayer) {
    if (input == INPUT_NONE) return true;
    if (IS_CPU_INPUT(input)) return true;
    
    if (forPlayer == 1) {
        return (g_player2_input != input);
    } else {
        return (g_player1_input != input);
    }
}

/*============================================================================
 * DRAWING HELPERS
 *============================================================================*/

static void DrawTitle(const char* title) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, 30, COLOR_BG);
    ST7735_DrawStringCentered(TITLE_Y, title, COLOR_TITLE, COLOR_BG, 2);
    ST7735_DrawHLine(0, 32, ST7735_WIDTH, COLOR_TITLE);
}

static void DrawMenuItem(uint8_t index, const char* text, bool selected, bool enabled) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;
    
    if (!enabled) textColor = COLOR_DISABLED;
    else if (selected) textColor = COLOR_SELECTED;
    else textColor = COLOR_NORMAL;
    
    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);
    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 4, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 4, text, textColor, bgColor);
}

static void DrawInputMenuItem(uint8_t index, InputType_t input, bool selected, uint8_t forPlayer) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    bool available = IsInputAvailable(input, forPlayer);
    bool isCurrentSelection = (forPlayer == 1) ? 
        (g_player1_input == input) : (g_player2_input == input);
    
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;
    
    if (!available) textColor = COLOR_DISABLED;
    else if (isCurrentSelection) textColor = COLOR_HIGHLIGHT;
    else if (selected) textColor = COLOR_SELECTED;
    else textColor = COLOR_NORMAL;
    
    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);
    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 4, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 4, GetInputName(input), textColor, bgColor);
    
    if (isCurrentSelection) {
        ST7735_DrawString(115, y + 4, "<-", COLOR_HIGHLIGHT, bgColor);
    } else if (!available) {
        ST7735_DrawString(108, y + 4, forPlayer == 1 ? "[P2]" : "[P1]", 
                         forPlayer == 1 ? COLOR_RED : COLOR_CYAN, bgColor);
    }
}

static void DrawBackOption(uint8_t index, bool selected) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    
    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);
    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 4, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 4, "<< Back", COLOR_BACK, bgColor);
}

/*============================================================================
 * SCREEN DRAWING FUNCTIONS
 *============================================================================*/

static void DrawBootHelpScreen(void) {
    ST7735_FillScreen(COLOR_BLACK);
    DrawTitle("CONTROLS");
    
    ST7735_DrawString(5, 35, "Joystick:", COLOR_CYAN, COLOR_BLACK);
    ST7735_DrawString(10, 47, "Up/Down : Move", COLOR_WHITE, COLOR_BLACK);
    ST7735_DrawString(10, 57, "Press   : Select", COLOR_WHITE, COLOR_BLACK);
    
    ST7735_DrawString(5, 70, "Remote IR:", COLOR_MAGENTA, COLOR_BLACK);
    ST7735_DrawString(10, 82, "CH-  : Up", COLOR_WHITE, COLOR_BLACK);
    ST7735_DrawString(10, 92, "CH   : Down", COLOR_WHITE, COLOR_BLACK);
    ST7735_DrawString(10, 102, "PREV : Select", COLOR_WHITE, COLOR_BLACK);
    
    ST7735_DrawHLine(0, 115, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawStringCentered(118, "Press to Start", COLOR_GREEN, COLOR_BLACK, 1);
}

static void DrawMainScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PONG");
    
    DrawMenuItem(0, "Start Game", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Select Input", g_menuState.selectedIndex == 1, true);
    DrawMenuItem(2, "Help", g_menuState.selectedIndex == 2, true);
    
    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY);
    char buf[32];
    snprintf(buf, sizeof(buf), "P1:%s", GetInputName(g_player1_input));
    ST7735_DrawString(5, 108, buf, COLOR_CYAN, COLOR_BG);
    snprintf(buf, sizeof(buf), "P2:%s", GetInputName(g_player2_input));
    ST7735_DrawString(85, 108, buf, COLOR_MAGENTA, COLOR_BG);
}

static void DrawStartScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("GAME MODE");
    
    DrawMenuItem(0, "Player vs Player", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Player vs CPU", g_menuState.selectedIndex == 1, true);
    DrawBackOption(2, g_menuState.selectedIndex == 2);
}

static void DrawSelectInputScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("SETUP");
    
    char buf[30];
    snprintf(buf, sizeof(buf), "P1: %s", GetInputName(g_player1_input));
    DrawMenuItem(0, buf, g_menuState.selectedIndex == 0, true);
    snprintf(buf, sizeof(buf), "P2: %s", GetInputName(g_player2_input));
    DrawMenuItem(1, buf, g_menuState.selectedIndex == 1, true);
    DrawBackOption(2, g_menuState.selectedIndex == 2);
}

static void DrawSelectP1Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("P1 INPUT");
    
    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 1);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 1);
    DrawInputMenuItem(2, INPUT_NONE, g_menuState.selectedIndex == 2, 1);
    DrawBackOption(3, g_menuState.selectedIndex == 3);
}

static void DrawSelectP2Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("P2 INPUT");
    
    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 2);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 2);
    DrawInputMenuItem(2, INPUT_NONE, g_menuState.selectedIndex == 2, 2);
    DrawBackOption(3, g_menuState.selectedIndex == 3);
}

static void DrawDifficultyScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("DIFFICULTY");
    
    DrawMenuItem(0, "Easy", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Normal", g_menuState.selectedIndex == 1, true);
    DrawMenuItem(2, "Hard", g_menuState.selectedIndex == 2, true);
    DrawBackOption(3, g_menuState.selectedIndex == 3);
    
    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawString(10, 108, "P1: Joystick", COLOR_CYAN, COLOR_BG);
    ST7735_DrawString(90, 108, "P2: CPU", COLOR_MAGENTA, COLOR_BG);
}

void Menu_DrawGameOverScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    
    ST7735_DrawStringCentered(15, "GAME OVER", COLOR_RED, COLOR_BG, 2);
    
    char buf[32];
    uint8_t winner = Game_GetWinner();
    uint16_t winner_color = (winner == 1) ? COLOR_CYAN : COLOR_MAGENTA;
    snprintf(buf, sizeof(buf), "Player %d Wins!", winner);
    ST7735_DrawStringCentered(45, buf, winner_color, COLOR_BG, 1);
    
    snprintf(buf, sizeof(buf), "%d - %d", Game_GetScore(1), Game_GetScore(2));
    ST7735_DrawStringCentered(60, buf, COLOR_WHITE, COLOR_BG, 2);
    
    ST7735_DrawHLine(0, 82, ST7735_WIDTH, COLOR_GRAY);
    
    uint16_t bg0 = (g_menuState.selectedIndex == 0) ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t col0 = (g_menuState.selectedIndex == 0) ? COLOR_SELECTED : COLOR_NORMAL;
    ST7735_FillRect(0, 88, ST7735_WIDTH, 16, bg0);
    if (g_menuState.selectedIndex == 0) ST7735_DrawString(MENU_MARGIN_X, 92, ">", COLOR_SELECTED, bg0);
    ST7735_DrawString(MENU_MARGIN_X + 12, 92, "Play Again", col0, bg0);
    
    uint16_t bg1 = (g_menuState.selectedIndex == 1) ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t col1 = (g_menuState.selectedIndex == 1) ? COLOR_SELECTED : COLOR_NORMAL;
    ST7735_FillRect(0, 106, ST7735_WIDTH, 16, bg1);
    if (g_menuState.selectedIndex == 1) ST7735_DrawString(MENU_MARGIN_X, 110, ">", COLOR_SELECTED, bg1);
    ST7735_DrawString(MENU_MARGIN_X + 12, 110, "Main Menu", col1, bg1);
}

void Menu_DrawPauseScreen(void) {
    ST7735_FillRect(25, 35, 110, 60, COLOR_DARK_GRAY);
    ST7735_DrawRect(25, 35, 110, 60, COLOR_WHITE);
    ST7735_DrawRect(27, 37, 106, 56, COLOR_GRAY);
    
    ST7735_DrawStringCentered(42, "PAUSED", COLOR_YELLOW, COLOR_DARK_GRAY, 2);
    
    if (g_pause_selection == 0) {
        ST7735_FillRect(40, 62, 80, 12, COLOR_CYAN);
        ST7735_DrawString(45, 64, "> Resume", COLOR_BLACK, COLOR_CYAN);
    } else {
        ST7735_FillRect(40, 62, 80, 12, COLOR_DARK_GRAY);
        ST7735_DrawString(50, 64, "Resume", COLOR_WHITE, COLOR_DARK_GRAY);
    }
    
    if (g_pause_selection == 1) {
        ST7735_FillRect(40, 76, 80, 12, COLOR_RED);
        ST7735_DrawString(45, 78, "> Exit", COLOR_WHITE, COLOR_RED);
    } else {
        ST7735_FillRect(40, 76, 80, 12, COLOR_DARK_GRAY);
        ST7735_DrawString(50, 78, "Exit", COLOR_GRAY, COLOR_DARK_GRAY);
    }
}

/*============================================================================
 * INTRO ANIMATION
 *============================================================================*/

static void DrawBMOFace(uint8_t expression, uint8_t blink) {
    ST7735_FillScreen(BMO_SCREEN_COLOR);
    
    if (blink) {
        ST7735_FillRect(30, 40, 35, 5, BMO_FACE_COLOR);
        ST7735_FillRect(95, 40, 35, 5, BMO_FACE_COLOR);
    } else {
        ST7735_FillRect(35, 25, 25, 25, BMO_FACE_COLOR);
        ST7735_FillRect(100, 25, 25, 25, BMO_FACE_COLOR);
        ST7735_FillRect(42, 30, 10, 10, COLOR_WHITE);
        ST7735_FillRect(107, 30, 10, 10, COLOR_WHITE);
    }
    
    switch (expression) {
        case 0:
            ST7735_FillRect(55, 70, 50, 5, BMO_FACE_COLOR);
            break;
        case 1:
            ST7735_FillRect(50, 75, 60, 5, BMO_FACE_COLOR);
            ST7735_FillRect(42, 70, 12, 5, BMO_FACE_COLOR);
            ST7735_FillRect(106, 70, 12, 5, BMO_FACE_COLOR);
            break;
        case 2:
            ST7735_FillRect(50, 65, 60, 25, BMO_FACE_COLOR);
            ST7735_FillRect(56, 72, 48, 14, BMO_SCREEN_COLOR);
            ST7735_FillRect(65, 78, 30, 6, COLOR_RED);
            break;
    }
}

void Menu_PlayIntroAnimation(void) {
    int16_t ball_x = 80, ball_y = 82;
    int16_t ball_dx = 2, ball_dy = 1;
    int16_t p1_y = 72, p2_y = 72;
    const uint8_t BALL_SZ = 5, PAD_W = 4, PAD_H = 18;
    const int16_t DEMO_TOP = 66, DEMO_BOTTOM = 102;
    uint32_t anim_start, last_blink = 0;
    uint8_t blink_state = 0;
    
    Joystick_ButtonPressed();
    
    /* BMO Animation */
    ST7735_FillScreen(BMO_SCREEN_COLOR);
    delay_ms(300);
    DrawBMOFace(0, 0);
    delay_ms(500);
    DrawBMOFace(0, 1);
    delay_ms(150);
    DrawBMOFace(0, 0);
    delay_ms(300);
    
    ST7735_DrawStringCentered(100, "Hi!", COLOR_BLACK, BMO_SCREEN_COLOR, 2);
    delay_ms(600);
    DrawBMOFace(1, 0);
    delay_ms(400);
    
    ST7735_FillRect(40, 95, 80, 25, BMO_SCREEN_COLOR);
    ST7735_DrawStringCentered(100, "Let's play", COLOR_BLACK, BMO_SCREEN_COLOR, 1);
    delay_ms(500);
    DrawBMOFace(2, 0);
    ST7735_DrawStringCentered(112, "PONG!", COLOR_YELLOW, BMO_SCREEN_COLOR, 2);
    delay_ms(1000);
    
    /* Transition flash */
    for (int i = 0; i < 3; i++) {
        ST7735_FillScreen(COLOR_WHITE);
        delay_ms(50);
        ST7735_FillScreen(BMO_SCREEN_COLOR);
        delay_ms(100);
    }
    
    /* Pong Demo */
    ST7735_FillScreen(COLOR_BLACK);
    ST7735_DrawRect(4, 4, 152, 120, COLOR_WHITE);
    
    const char* letters = "PONG";
    int16_t letter_x[] = {26, 54, 82, 110};
    uint16_t letter_colors[] = {COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA, COLOR_GREEN};
    for (int i = 0; i < 4; i++) {
        ST7735_DrawChar(letter_x[i], 15, letters[i], letter_colors[i], COLOR_BLACK, 3);
    }
    
    ST7735_DrawHLine(20, 42, 120, COLOR_WHITE);
    ST7735_DrawStringCentered(50, "ARCADE EDITION", COLOR_ORANGE, COLOR_BLACK, 1);
    ST7735_DrawHLine(15, 62, 130, COLOR_DARK_GRAY);
    ST7735_DrawHLine(15, 105, 130, COLOR_DARK_GRAY);
    
    ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_CYAN);
    ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_MAGENTA);
    ST7735_DrawStringCentered(112, "Press to Start", COLOR_WHITE, COLOR_BLACK, 1);
    
    anim_start = g_systick_ms;
    
    while (!Joystick_ButtonPressed()) {
        uint32_t now = g_systick_ms;
        
        ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, COLOR_BLACK);
        ball_x += ball_dx;
        ball_y += ball_dy;
        
        if (ball_y <= DEMO_TOP || ball_y >= DEMO_BOTTOM - BALL_SZ) ball_dy = -ball_dy;
        if (ball_x <= 12 && ball_y + BALL_SZ >= p1_y && ball_y <= p1_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 13;
        }
        if (ball_x >= 148 - BALL_SZ && ball_y + BALL_SZ >= p2_y && ball_y <= p2_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 147 - BALL_SZ;
        }
        if (ball_x < 5 || ball_x > 155) {
            ball_x = 80;
            ball_y = 82;
        }
        
        ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_BLACK);
        if (ball_y > p1_y + PAD_H/2) p1_y += 2;
        else if (ball_y < p1_y + PAD_H/2) p1_y -= 2;
        if (p1_y < DEMO_TOP) p1_y = DEMO_TOP;
        if (p1_y > DEMO_BOTTOM - PAD_H) p1_y = DEMO_BOTTOM - PAD_H;
        ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_CYAN);
        
        ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_BLACK);
        if (ball_y > p2_y + PAD_H/2) p2_y += 2;
        else if (ball_y < p2_y + PAD_H/2) p2_y -= 2;
        if (p2_y < DEMO_TOP) p2_y = DEMO_TOP;
        if (p2_y > DEMO_BOTTOM - PAD_H) p2_y = DEMO_BOTTOM - PAD_H;
        ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_MAGENTA);
        
        uint16_t ball_color = rainbow_colors[(now / 200) % NUM_RAINBOW_COLORS];
        ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, ball_color);
        
        if ((now - last_blink) >= 400) {
            last_blink = now;
            blink_state = (blink_state + 1) % 4;
            uint16_t blink_colors[] = {COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA};
            ST7735_DrawStringCentered(112, "Press to Start", blink_colors[blink_state], COLOR_BLACK, 1);
        }
        
        delay_ms(33);
        
        if ((now - anim_start) > 60000) break;
    }
    
    ST7735_FillScreen(COLOR_WHITE);
    delay_ms(50);
    ST7735_FillScreen(COLOR_BLACK);
    delay_ms(80);
}

/*============================================================================
 * NAVIGATION
 *============================================================================*/

void Menu_MoveUp(void) {
    if (g_menuState.selectedIndex > 0) {
        g_menuState.selectedIndex--;
    } else {
        g_menuState.selectedIndex = g_menuState.maxItems - 1;
    }
    g_needsRedraw = 1;
}

void Menu_MoveDown(void) {
    g_menuState.selectedIndex++;
    if (g_menuState.selectedIndex >= g_menuState.maxItems) {
        g_menuState.selectedIndex = 0;
    }
    g_needsRedraw = 1;
}

bool Menu_CanStartGame(void) {
    if (g_player2_input == INPUT_NONE && !IS_CPU_INPUT(g_player2_input)) {
        ST7735_FillRect(0, 100, 160, 28, COLOR_BLACK);
        ST7735_DrawString(10, 105, "ERR: P2 missing!", COLOR_RED, COLOR_BLACK);
        delay_ms(1000);
        g_needsRedraw = 1;
        return false;
    }
    return true;
}

void Menu_Select(void) {
    switch (g_currentScreen) {
        case SCREEN_INTRO:
            Menu_PlayIntroAnimation();
            g_currentScreen = SCREEN_BOOT_HELP;
            g_menuState.selectedIndex = 0;
            g_menuState.maxItems = 1;
            break;
            
        case SCREEN_BOOT_HELP:
            g_currentScreen = SCREEN_MAIN;
            g_menuState.selectedIndex = 0;
            g_menuState.maxItems = 3;
            break;
            
        case SCREEN_MAIN:
            if (g_menuState.selectedIndex == 0) {
                g_currentScreen = SCREEN_START;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else if (g_menuState.selectedIndex == 1) {
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else if (g_menuState.selectedIndex == 2) {
                g_currentScreen = SCREEN_BOOT_HELP;
            }
            break;
            
        case SCREEN_START:
            if (g_menuState.selectedIndex == 0) {
                /* Player vs Player */
                if (IS_CPU_INPUT(g_player2_input)) {
                    g_player2_input = INPUT_REMOTE;
                }
                if (g_player1_input == g_player2_input) {
                    g_player2_input = (g_player1_input == INPUT_JOYSTICK) ? 
                                      INPUT_REMOTE : INPUT_JOYSTICK;
                }
                if (Menu_CanStartGame()) {
                    Game_Start();
                    g_currentScreen = SCREEN_GAMEPLAY;
                }
            } else if (g_menuState.selectedIndex == 1) {
                /* Player vs CPU */
                g_currentScreen = SCREEN_DIFFICULTY;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 4;
            } else {
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            }
            break;
            
        case SCREEN_DIFFICULTY:
            if (g_menuState.selectedIndex == 3) {
                g_currentScreen = SCREEN_START;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else {
                g_currentDifficulty = (Difficulty_t)g_menuState.selectedIndex;
                g_player2_input = INPUT_CPU_EASY + g_menuState.selectedIndex;
                Game_Start();
                g_currentScreen = SCREEN_GAMEPLAY;
            }
            break;
            
        case SCREEN_SELECT_INPUT:
            if (g_menuState.selectedIndex == 0) {
                g_currentScreen = SCREEN_SELECT_P1;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 4;
            } else if (g_menuState.selectedIndex == 1) {
                g_currentScreen = SCREEN_SELECT_P2;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 4;
            } else {
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 1;
                g_menuState.maxItems = 3;
            }
            break;
            
        case SCREEN_SELECT_P1:
            if (g_menuState.selectedIndex == 3) {
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            } else {
                InputType_t inputs[] = {INPUT_JOYSTICK, INPUT_REMOTE, INPUT_NONE};
                InputType_t sel = inputs[g_menuState.selectedIndex];
                if (IsInputAvailable(sel, 1)) {
                    g_player1_input = sel;
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 0;
                    g_menuState.maxItems = 3;
                } else {
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawStringCentered(110, "USED BY P2!", COLOR_WHITE, COLOR_RED, 1);
                    delay_ms(800);
                }
            }
            break;
            
        case SCREEN_SELECT_P2:
            if (g_menuState.selectedIndex == 3) {
                g_currentScreen = SCREEN_SELECT_INPUT;
                g_menuState.selectedIndex = 1;
                g_menuState.maxItems = 3;
            } else {
                InputType_t inputs[] = {INPUT_JOYSTICK, INPUT_REMOTE, INPUT_NONE};
                InputType_t sel = inputs[g_menuState.selectedIndex];
                if (IsInputAvailable(sel, 2)) {
                    g_player2_input = sel;
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 1;
                    g_menuState.maxItems = 3;
                } else {
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawStringCentered(110, "USED BY P1!", COLOR_WHITE, COLOR_RED, 1);
                    delay_ms(800);
                }
            }
            break;
            
        case SCREEN_GAME_OVER:
            if (g_menuState.selectedIndex == 0) {
                Game_Start();
                g_currentScreen = SCREEN_GAMEPLAY;
            } else {
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 0;
                g_menuState.maxItems = 3;
            }
            break;
            
        default:
            break;
    }
    g_needsRedraw = 1;
}

void Menu_DrawCurrent(void) {
    switch (g_currentScreen) {
        case SCREEN_INTRO:
            Menu_PlayIntroAnimation();
            g_currentScreen = SCREEN_BOOT_HELP;
            g_menuState.selectedIndex = 0;
            g_menuState.maxItems = 1;
            DrawBootHelpScreen();
            break;
        case SCREEN_BOOT_HELP:
            g_menuState.maxItems = 1;
            DrawBootHelpScreen();
            break;
        case SCREEN_MAIN:
            g_menuState.maxItems = 3;
            DrawMainScreen();
            break;
        case SCREEN_START:
            g_menuState.maxItems = 3;
            DrawStartScreen();
            break;
        case SCREEN_SELECT_INPUT:
            g_menuState.maxItems = 3;
            DrawSelectInputScreen();
            break;
        case SCREEN_SELECT_P1:
            g_menuState.maxItems = 4;
            DrawSelectP1Screen();
            break;
        case SCREEN_SELECT_P2:
            g_menuState.maxItems = 4;
            DrawSelectP2Screen();
            break;
        case SCREEN_DIFFICULTY:
            g_menuState.maxItems = 4;
            DrawDifficultyScreen();
            break;
        case SCREEN_GAME_OVER:
            g_menuState.maxItems = 2;
            Menu_DrawGameOverScreen();
            break;
        case SCREEN_PAUSED:
            Menu_DrawPauseScreen();
            break;
        default:
            break;
    }
    g_needsRedraw = 0;
}
