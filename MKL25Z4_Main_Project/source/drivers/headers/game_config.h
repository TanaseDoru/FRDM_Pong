/*
 * game_config.h
 * Configuratii globale pentru jocul Pong
 * Include tipuri, constante si variabile globale
 */

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * INPUT TYPES
 *============================================================================*/
typedef enum {
    INPUT_NONE = 0,
    INPUT_JOYSTICK,
    INPUT_REMOTE,
    INPUT_GYROSCOPE,    /* Neimplementat */
    INPUT_CPU_EASY,     /* Bot - nivel usor */
    INPUT_CPU_MEDIUM,   /* Bot - nivel mediu */
    INPUT_CPU_HARD      /* Bot - nivel greu */
} InputType_t;

/*============================================================================
 * SCREEN STATES
 *============================================================================*/
typedef enum {
    SCREEN_INTRO = 0,       /* Ecranul de intro animat (BMO) */
    SCREEN_BOOT_HELP,       /* Ecran cu controale */
    SCREEN_MAIN,            /* Meniu principal */
    SCREEN_START,           /* Selectie mod joc */
    SCREEN_SELECT_INPUT,    /* Selectie input pentru jucatori */
    SCREEN_SELECT_P1,       /* Selectie input P1 */
    SCREEN_SELECT_P2,       /* Selectie input P2 */
    SCREEN_DIFFICULTY,      /* Selectare dificultate CPU */
    SCREEN_GAMEPLAY,        /* Ecranul de joc */
    SCREEN_GAME_OVER,       /* Ecranul de sfarsit */
    SCREEN_PAUSED           /* Joc in pauza */
} Screen_t;

/*============================================================================
 * DIFFICULTY LEVELS
 *============================================================================*/
typedef enum {
    DIFF_EASY = 0,
    DIFF_NORMAL,
    DIFF_HARD
} Difficulty_t;

/*============================================================================
 * UI ACTIONS (Abstract input actions)
 *============================================================================*/
typedef enum {
    ACTION_NONE = 0,
    ACTION_UP,
    ACTION_DOWN,
    ACTION_SELECT,
    ACTION_BACK
} UI_Action_t;

/*============================================================================
 * MENU STATE
 *============================================================================*/
typedef struct {
    uint8_t selectedIndex;
    uint8_t maxItems;
} MenuState_t;

/*============================================================================
 * GAME CONSTANTS
 *============================================================================*/

/* Display */
#define FIELD_WIDTH      160
#define FIELD_HEIGHT     128

/* Paddle */
#define PADDLE_WIDTH     4
#define PADDLE_HEIGHT    22
#define PADDLE_X_P1      4       /* Pozitia X a paletei P1 */
#define PADDLE_X_P2      152     /* Pozitia X a paletei P2 */
#define PADDLE_START_Y   53      /* (128 - 22) / 2 */
#define PADDLE_SPEED     4
#define PADDLE_MIN_Y     2
#define PADDLE_MAX_Y     (FIELD_HEIGHT - PADDLE_HEIGHT - 2)

/* Ball */
#define BALL_SIZE        4
#define BALL_START_X     80
#define BALL_START_Y     64
#define BALL_SPEED_X     2
#define BALL_SPEED_Y     1

/* Game */
#define SCORE_TO_WIN     5
#define SCORE_Y          2       /* Pozitia Y a scorului */

/* Speed acceleration */
#define SPEED_UP_INTERVAL   500   /* Frames pana la urmatoarea accelerare (~10 sec) */
#define MAX_SPEED_LEVEL     5     /* Nivel maxim de viteza */

/*============================================================================
 * GAME STRUCTURES
 *============================================================================*/

typedef struct {
    int16_t x, y;           /* Pozitia bilei */
    int16_t dx, dy;         /* Viteza bilei */
    int16_t prev_x, prev_y; /* Pozitia anterioara (pentru stergere) */
    uint8_t size;
} Ball_t;

typedef struct {
    int16_t y;              /* Pozitia paletei */
    int16_t prev_y;         /* Pozitia anterioara */
    int16_t score;          /* Scorul */
    InputType_t input;      /* Tipul de input */
    int16_t target_y;       /* Pentru AI - tinta */
} Paddle_t;

typedef struct {
    uint8_t is_running;
    uint8_t is_paused;
    uint8_t winner;         /* 0=nimeni, 1=P1, 2=P2 */
    uint8_t winning_score;
    uint16_t frame_count;
    uint16_t rally_frames;  /* Frames de la ultimul gol - pentru accelerare */
    uint8_t speed_level;    /* Nivelul curent de viteza (0-5) */
} GameState_t;

/*============================================================================
 * GLOBAL VARIABLES (extern declarations)
 *============================================================================*/

extern InputType_t g_player1_input;
extern InputType_t g_player2_input;
extern Screen_t g_currentScreen;
extern MenuState_t g_menuState;
extern volatile uint8_t g_needsRedraw;
extern Difficulty_t g_currentDifficulty;

/* Timer global */
extern volatile uint32_t g_systick_ms;

/*============================================================================
 * UTILITY MACROS
 *============================================================================*/

/* Verifica daca input-ul este de tip CPU/Bot */
#define IS_CPU_INPUT(input) ((input) == INPUT_CPU_EASY || \
                              (input) == INPUT_CPU_MEDIUM || \
                              (input) == INPUT_CPU_HARD)

#endif /* GAME_CONFIG_H */
