#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* --- Input Types --- */
typedef enum {
    INPUT_NONE = 0,
    INPUT_JOYSTICK,
    INPUT_REMOTE,
    INPUT_GYROSCOPE
} InputType_t;

/* --- Screen States --- */
typedef enum {
    SCREEN_BOOT_HELP = 0,
    SCREEN_MAIN,
    SCREEN_START,
    SCREEN_SELECT_INPUT,
    SCREEN_SELECT_P1,
    SCREEN_SELECT_P2,
	SCREEN_DIFFICULTY,
    SCREEN_GAMEPLAY,
    SCREEN_GAME_OVER
} Screen_t;

/*  --- Difficulty Selections ---  */
typedef enum{
	DIFF_EASY = 0,
	DIFF_NORMAL,
	DIFF_HARD
} Difficulty_t;

/* --- Abstract Input Actions --- */
typedef enum {
    ACTION_NONE = 0,
    ACTION_UP,
    ACTION_DOWN,
    ACTION_SELECT,
    ACTION_BACK
} UI_Action_t;

/* --- Global Player Config --- */
extern InputType_t g_player1_input;
extern InputType_t g_player2_input;

#endif
