/*
 * MKL25Z4_Display_Test.c
 * Sistem de Meniu pentru Pong Game
 * ST7735 LCD + FRDM-KL25Z
 */

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "st7735_simple.h"

/*============================================================================
 * DEFINITII TIPURI INPUT SI ECRANE
 *============================================================================*/

/* Tipuri de input disponibile */
typedef enum {
    INPUT_NONE = 0,
    INPUT_JOYSTICK,
    INPUT_REMOTE,
    INPUT_GYROSCOPE
} InputType_t;

/* Ecranele disponibile */
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_START,
    SCREEN_SELECT_INPUT,
    SCREEN_SELECT_P1,
    SCREEN_SELECT_P2
} Screen_t;

/* Starea meniului pentru fiecare ecran */
typedef struct {
    uint8_t selectedIndex;
    uint8_t maxItems;
} MenuState_t;

/*============================================================================
 * VARIABILE GLOBALE - Input selectat pentru fiecare jucator
 *============================================================================*/

InputType_t g_player1_input = INPUT_NONE;
InputType_t g_player2_input = INPUT_NONE;

Screen_t g_currentScreen = SCREEN_MAIN;
MenuState_t g_menuState = {0, 3};

/* Flag pentru refresh ecran */
volatile uint8_t g_needsRedraw = 1;

/*============================================================================
 * CONSTANTE PENTRU UI
 *============================================================================*/

#define MENU_START_Y      35   /* Urcăm meniul puțin mai sus (era 40) */
#define MENU_ITEM_HEIGHT  18   /* Micșorăm puțin înălțimea rândurilor (era 20) */
#define MENU_MARGIN_X     10
#define TITLE_Y           8    /* Titlul puțin mai sus */

/* Culori pentru UI */
#define COLOR_BG          COLOR_BLACK
#define COLOR_TITLE       COLOR_CYAN
#define COLOR_NORMAL      COLOR_WHITE
#define COLOR_SELECTED    COLOR_YELLOW
#define COLOR_DISABLED    COLOR_GRAY
#define COLOR_HIGHLIGHT   COLOR_GREEN
#define COLOR_BACK        COLOR_ORANGE

/*============================================================================
 * FUNCTII HELPER
 *============================================================================*/

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 6000; i++) __asm volatile("nop");
}

/* Obtine numele input-ului */
const char* GetInputName(InputType_t input) {
    switch (input) {
        case INPUT_JOYSTICK:  return "Joystick";
        case INPUT_REMOTE:    return "Remote";
        case INPUT_GYROSCOPE: return "Gyroscope";
        default:              return "None";
    }
}

/* Verifica daca un input e disponibil (nu e luat de celalalt jucator) */
bool IsInputAvailable(InputType_t input, uint8_t forPlayer) {
    if (input == INPUT_NONE) return true;
    if (forPlayer == 1) {
        return (g_player2_input != input);
    } else {
        return (g_player1_input != input);
    }
}

/*============================================================================
 * FUNCTII DE DESENARE MENIU
 *============================================================================*/

/* Deseneaza titlul ecranului */
void DrawTitle(const char* title) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, 30, COLOR_BG);
    ST7735_DrawStringCentered(TITLE_Y, title, COLOR_TITLE, COLOR_BG, 2);
    ST7735_DrawHLine(0, 32, ST7735_WIDTH, COLOR_TITLE);
}

/* Deseneaza un item de meniu */
void DrawMenuItem(uint8_t index, const char* text, bool selected, bool enabled) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;

    if (!enabled) {
        textColor = COLOR_DISABLED;
    } else if (selected) {
        textColor = COLOR_SELECTED;
    } else {
        textColor = COLOR_NORMAL;
    }

    /* Sterge zona item-ului */
    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    /* Deseneaza indicator selectie */
    if (selected) {
        ST7735_DrawString(MENU_MARGIN_X, y + 6, ">", COLOR_SELECTED, bgColor);
    }

    /* Deseneaza textul */
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 6, text, textColor, bgColor);
}

/* Deseneaza item pentru selectie input cu status */
void DrawInputMenuItem(uint8_t index, InputType_t input, bool selected, uint8_t forPlayer) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    bool available = IsInputAvailable(input, forPlayer);
    bool isCurrentSelection;

    if (forPlayer == 1) {
        isCurrentSelection = (g_player1_input == input);
    } else {
        isCurrentSelection = (g_player2_input == input);
    }

    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;

    if (!available) {
        textColor = COLOR_DISABLED;
    } else if (isCurrentSelection) {
        textColor = COLOR_HIGHLIGHT;
    } else if (selected) {
        textColor = COLOR_SELECTED;
    } else {
        textColor = COLOR_NORMAL;
    }

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    if (selected) {
        ST7735_DrawString(MENU_MARGIN_X, y + 6, ">", COLOR_SELECTED, bgColor);
    }

    ST7735_DrawString(MENU_MARGIN_X + 12, y + 6, GetInputName(input), textColor, bgColor);

    /* Afiseaza [X] daca e deja selectat */
    if (isCurrentSelection) {
        ST7735_DrawString(100, y + 6, "[X]", COLOR_HIGHLIGHT, bgColor);
    } else if (!available) {
        ST7735_DrawString(100, y + 6, "[P?]",
            (forPlayer == 1) ? COLOR_RED : COLOR_BLUE, bgColor);
    }
}

/* Deseneaza optiunea Back */
void DrawBackOption(uint8_t index, bool selected) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    if (selected) {
        ST7735_DrawString(MENU_MARGIN_X, y + 6, ">", COLOR_SELECTED, bgColor);
    }

    ST7735_DrawString(MENU_MARGIN_X + 12, y + 6, "<< Back", COLOR_BACK, bgColor);
}

/*============================================================================
 * FUNCTII PENTRU FIECARE ECRAN
 *============================================================================*/

/* ECRAN PRINCIPAL (Main Menu) */
void DrawMainScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PONG GAME");

    DrawMenuItem(0, "Start", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Select Input", g_menuState.selectedIndex == 1, true);
    DrawMenuItem(2, "Mute Music", g_menuState.selectedIndex == 2, true);

    /* Afiseaza status input curent jos - AJUSTAT PENTRU INALTIME 128 */
    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY); // Linia la Y=100

    char buf[32];
    snprintf(buf, sizeof(buf), "P1:%s", GetInputName(g_player1_input));
    ST7735_DrawString(5, 108, buf, COLOR_CYAN, COLOR_BG); // Y=108

    snprintf(buf, sizeof(buf), "P2:%s", GetInputName(g_player2_input));
    ST7735_DrawString(85, 108, buf, COLOR_MAGENTA, COLOR_BG); // Y=108 (Punem P2 in dreapta lui P1)
}

/* ECRAN START (Player vs Player / Player vs Computer) */
void DrawStartScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("START");

    DrawMenuItem(0, "Player vs Player", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Player vs CPU", g_menuState.selectedIndex == 1, true);
    DrawBackOption(2, g_menuState.selectedIndex == 2);
}

/* ECRAN SELECT INPUT (Player 1 / Player 2) */
void DrawSelectInputScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("SELECT INPUT");

    char buf[24];
    snprintf(buf, sizeof(buf), "Player 1 [%s]",
        g_player1_input == INPUT_NONE ? "-" : GetInputName(g_player1_input));
    DrawMenuItem(0, "Player 1", g_menuState.selectedIndex == 0, true);

    snprintf(buf, sizeof(buf), "Player 2 [%s]",
        g_player2_input == INPUT_NONE ? "-" : GetInputName(g_player2_input));
    DrawMenuItem(1, "Player 2", g_menuState.selectedIndex == 1, true);

    DrawBackOption(2, g_menuState.selectedIndex == 2);

    /* Status curent - AJUSTAT */
    ST7735_DrawHLine(0, 95, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawString(5, 100, "Current Setup:", COLOR_GRAY, COLOR_BG);

    snprintf(buf, sizeof(buf), "P1: %s", GetInputName(g_player1_input));
    ST7735_DrawString(5, 112, buf, COLOR_CYAN, COLOR_BG);

    snprintf(buf, sizeof(buf), "P2: %s", GetInputName(g_player2_input));
    ST7735_DrawString(85, 112, buf, COLOR_MAGENTA, COLOR_BG);
}

/* ECRAN SELECT PLAYER 1 INPUT */
void DrawSelectP1Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PLAYER 1");

    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 1);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 1);
    DrawInputMenuItem(2, INPUT_GYROSCOPE, g_menuState.selectedIndex == 2, 1);
    DrawBackOption(3, g_menuState.selectedIndex == 3);
}

/* ECRAN SELECT PLAYER 2 INPUT */
void DrawSelectP2Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PLAYER 2");

    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 2);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 2);
    DrawInputMenuItem(2, INPUT_GYROSCOPE, g_menuState.selectedIndex == 2, 2);
    DrawBackOption(3, g_menuState.selectedIndex == 3);
}

/* Functie principala de desenare bazata pe ecranul curent */
void DrawCurrentScreen(void) {
    switch (g_currentScreen) {
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
    }
    g_needsRedraw = 0;
}

/*============================================================================
 * NAVIGARE MENIU
 *============================================================================*/

/* Muta selectia in sus */
void Menu_MoveUp(void) {
    if (g_menuState.selectedIndex > 0) {
        g_menuState.selectedIndex--;
    } else {
        g_menuState.selectedIndex = g_menuState.maxItems - 1;
    }
    g_needsRedraw = 1;
}

/* Muta selectia in jos */
void Menu_MoveDown(void) {
    g_menuState.selectedIndex++;
    if (g_menuState.selectedIndex >= g_menuState.maxItems) {
        g_menuState.selectedIndex = 0;
    }
    g_needsRedraw = 1;
}

/* Schimba ecranul */
void ChangeScreen(Screen_t newScreen) {
    g_currentScreen = newScreen;
    g_menuState.selectedIndex = 0;
    g_needsRedraw = 1;
}

/* Proceseaza selectia curenta */
void Menu_Select(void) {
    switch (g_currentScreen) {
        case SCREEN_MAIN:
            switch (g_menuState.selectedIndex) {
                case 0: /* Start */
                    ChangeScreen(SCREEN_START);
                    break;
                case 1: /* Select Input */
                    ChangeScreen(SCREEN_SELECT_INPUT);
                    break;
                case 2: /* Mute Music */
                    PRINTF("Mute Music toggled\r\n");
                    break;
            }
            break;

        case SCREEN_START:
            switch (g_menuState.selectedIndex) {
                case 0: /* Player vs Player */
                    PRINTF("Starting PvP game!\r\n");
                    /* Aici pornesti jocul */
                    break;
                case 1: /* Player vs CPU */
                    PRINTF("Starting vs CPU game!\r\n");
                    break;
                case 2: /* Back */
                    ChangeScreen(SCREEN_MAIN);
                    break;
            }
            break;

        case SCREEN_SELECT_INPUT:
            switch (g_menuState.selectedIndex) {
                case 0: /* Player 1 */
                    ChangeScreen(SCREEN_SELECT_P1);
                    break;
                case 1: /* Player 2 */
                    ChangeScreen(SCREEN_SELECT_P2);
                    break;
                case 2: /* Back */
                    ChangeScreen(SCREEN_MAIN);
                    break;
            }
            break;

        case SCREEN_SELECT_P1:
            if (g_menuState.selectedIndex == 3) {
                /* Back */
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                InputType_t selectedInput = (InputType_t)(g_menuState.selectedIndex + 1);
                if (IsInputAvailable(selectedInput, 1)) {
                    g_player1_input = selectedInput;
                    PRINTF("Player 1 input: %s\r\n", GetInputName(g_player1_input));
                    g_needsRedraw = 1;
                }
            }
            break;

        case SCREEN_SELECT_P2:
            if (g_menuState.selectedIndex == 3) {
                /* Back */
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                InputType_t selectedInput = (InputType_t)(g_menuState.selectedIndex + 1);
                if (IsInputAvailable(selectedInput, 2)) {
                    g_player2_input = selectedInput;
                    PRINTF("Player 2 input: %s\r\n", GetInputName(g_player2_input));
                    g_needsRedraw = 1;
                }
            }
            break;
    }
}

/*============================================================================
 * INPUT DE LA TASTATURA (Serial/UART)
 *============================================================================*/

/* Citeste un caracter de la tastatura (non-blocking) */
char ReadKeyboard(void) {
    /* Verifica daca exista date in buffer UART */
    if (UART0->S1 & UART0_S1_RDRF_MASK) {
        return UART0->D;
    }
    return 0; /* Niciun caracter */
}

/* Proceseaza input de la tastatura */
void ProcessKeyboardInput(void) {
    char key = ReadKeyboard();

    if (key == 0) return; /* Niciun input */

    switch (key) {
        case 'w':
        case 'W':
        case '8': /* Sus */
            PRINTF("[KEY] UP\r\n");
            Menu_MoveUp();
            break;

        case 's':
        case 'S':
        case '2': /* Jos */
            PRINTF("[KEY] DOWN\r\n");
            Menu_MoveDown();
            break;

        case ' ':     /* Space */
        case '\r':    /* Enter */
        case '\n':
        case '5':     /* Select */
        case 'e':
        case 'E':
            PRINTF("[KEY] SELECT\r\n");
            Menu_Select();
            break;

        case 'b':
        case 'B':
        case '0': /* Back - selecteaza ultima optiune (Back) */
            PRINTF("[KEY] BACK\r\n");
            g_menuState.selectedIndex = g_menuState.maxItems - 1;
            g_needsRedraw = 1;
            Menu_Select();
            break;

        case '1': /* Selecteaza direct optiunea 1 */
            PRINTF("[KEY] Option 1\r\n");
            g_menuState.selectedIndex = 0;
            g_needsRedraw = 1;
            Menu_Select();
            break;

        case '3': /* Selecteaza direct optiunea 2 */
            PRINTF("[KEY] Option 2\r\n");
            if (g_menuState.maxItems > 1) {
                g_menuState.selectedIndex = 1;
                g_needsRedraw = 1;
                Menu_Select();
            }
            break;

        case '4': /* Selecteaza direct optiunea 3 */
            PRINTF("[KEY] Option 3\r\n");
            if (g_menuState.maxItems > 2) {
                g_menuState.selectedIndex = 2;
                g_needsRedraw = 1;
                Menu_Select();
            }
            break;

        case 'r':
        case 'R': /* Reset la Main Menu */
            PRINTF("[KEY] RESET to Main\r\n");
            g_player1_input = INPUT_NONE;
            g_player2_input = INPUT_NONE;
            ChangeScreen(SCREEN_MAIN);
            break;

        case 'h':
        case 'H':
        case '?': /* Help */
            PRINTF("\r\n");
            PRINTF("=== KEYBOARD CONTROLS ===\r\n");
            PRINTF("  W/8     - Move UP\r\n");
            PRINTF("  S/2     - Move DOWN\r\n");
            PRINTF("  E/5/Enter/Space - SELECT\r\n");
            PRINTF("  B/0     - BACK\r\n");
            PRINTF("  1,3,4   - Direct select option\r\n");
            PRINTF("  R       - Reset to Main Menu\r\n");
            PRINTF("  H/?     - Show this help\r\n");
            PRINTF("=========================\r\n\r\n");
            break;

        default:
            PRINTF("[KEY] Unknown: '%c' (0x%02X)\r\n", key, key);
            break;
    }
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    PRINTF("\r\n");
    PRINTF("╔════════════════════════════════════════╗\r\n");
    PRINTF("║     PONG GAME - Menu System Test       ║\r\n");
    PRINTF("║         FRDM-KL25Z + ST7735            ║\r\n");
    PRINTF("╚════════════════════════════════════════╝\r\n\r\n");

    PRINTF("=== DISPLAY PINOUT ===\r\n");
    PRINTF("  BL  (Backlight) -> 3.3V\r\n");
    PRINTF("  VCC             -> 3.3V\r\n");
    PRINTF("  GND             -> GND\r\n");
    PRINTF("  SCK             -> PTC5\r\n");
    PRINTF("  SDA (MOSI)      -> PTC7\r\n");
    PRINTF("  DC              -> PTC3\r\n");
    PRINTF("  RES (Reset)     -> PTC0\r\n");
    PRINTF("  CS              -> PTC4\r\n");
    PRINTF("======================\r\n\r\n");

    PRINTF("Initializing display...\r\n");
    ST7735_Init();
    PRINTF("Display initialized OK!\r\n\r\n");

    PRINTF("=== KEYBOARD CONTROLS ===\r\n");
    PRINTF("  W/8     - Move UP\r\n");
    PRINTF("  S/2     - Move DOWN\r\n");
    PRINTF("  E/5/Enter/Space - SELECT\r\n");
    PRINTF("  B/0     - BACK\r\n");
    PRINTF("  1,3,4   - Direct select option 1,2,3\r\n");
    PRINTF("  R       - Reset to Main Menu\r\n");
    PRINTF("  H/?     - Show help\r\n");
    PRINTF("=========================\r\n\r\n");

    /* Deseneaza ecranul initial */
    DrawCurrentScreen();

    PRINTF(">>> Menu system ready! Use keyboard to navigate <<<\r\n\r\n");

    while (1) {
        /* Citeste input de la tastatura */
        ProcessKeyboardInput();

        /* Redeseneaza daca e nevoie */
        if (g_needsRedraw) {
            DrawCurrentScreen();
        }

        delay_ms(50);
    }

    return 0;
}
