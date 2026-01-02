/*
 * pong_game.c
 * Logica completa pentru jocul Pong
 * Include fizica, coliziuni, AI (CPU) si rendering optimizat
 */

#include "headers/pong_game.h"
#include "headers/st7735_simple.h"
#include "headers/joystick.h"
#include "headers/ir_remote.h"
#include "fsl_debug_console.h"
#include <stdio.h>
#include <stdlib.h>

/*============================================================================
 * EXTERNAL VARIABLES
 *============================================================================*/

extern InputType_t g_player1_input;
extern InputType_t g_player2_input;
extern volatile uint32_t g_systick_ms;
extern Difficulty_t g_currentDifficulty;

/*============================================================================
 * GAME STATE
 *============================================================================*/

static Ball_t ball;
static Paddle_t paddle1, paddle2;
static GameState_t game;

/*============================================================================
 * DELAY FUNCTION (pentru animatii)
 *============================================================================*/

static void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __asm volatile("nop");
    }
}

/*============================================================================
 * AI / CPU LOGIC
 *============================================================================*/

/* Predictie simpla - unde va ajunge bila */
static int16_t AI_PredictBallY(int16_t target_x) {
    int16_t sim_x = ball.x;
    int16_t sim_y = ball.y;
    int16_t sim_dx = ball.dx;
    int16_t sim_dy = ball.dy;
    
    int iterations = 0;
    while (iterations < 200) {
        sim_x += sim_dx;
        sim_y += sim_dy;
        
        /* Bounce pe pereti sus/jos */
        if (sim_y <= 2 || sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) {
            sim_dy = -sim_dy;
            if (sim_y <= 2) sim_y = 3;
            if (sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) {
                sim_y = FIELD_HEIGHT - BALL_SIZE - 3;
            }
        }
        
        /* A ajuns la pozitia dorita? */
        if ((sim_dx > 0 && sim_x >= target_x) || 
            (sim_dx < 0 && sim_x <= target_x)) {
            return sim_y;
        }
        
        iterations++;
    }
    
    return FIELD_HEIGHT / 2;  /* Fallback la centru */
}

/* Update AI pentru o paleta */
static void AI_UpdatePaddle(Paddle_t* paddle, bool is_right_side) {
    int16_t paddle_center = paddle->y + PADDLE_HEIGHT / 2;
    int16_t target_y;
    int16_t speed;
    int16_t reaction_zone;
    int16_t error_margin;
    int16_t mistake_chance;
    int16_t update_interval;
    
    /* Parametri bazati pe dificultate */
    switch (paddle->input) {
        case INPUT_CPU_EASY:
            speed = 1;
            reaction_zone = 35;
            error_margin = 35;
            mistake_chance = 35;
            update_interval = 20;
            break;
        case INPUT_CPU_MEDIUM:
            speed = 2;
            reaction_zone = 80;
            error_margin = 15;
            mistake_chance = 12;
            update_interval = 10;
            break;
        case INPUT_CPU_HARD:
            speed = 4;
            reaction_zone = 160;
            error_margin = 5;
            mistake_chance = 0;
            update_interval = 3;
            break;
        default:
            return;
    }
    
    /* Verifica daca bila vine spre aceasta paleta */
    bool ball_coming = (is_right_side && ball.dx > 0) || 
                       (!is_right_side && ball.dx < 0);
    
    if (ball_coming) {
        /* Calculeaza tinta doar periodic */
        if (game.frame_count % update_interval == 0) {
            int16_t target_x = is_right_side ? PADDLE_X_P2 : PADDLE_X_P1 + PADDLE_WIDTH;
            paddle->target_y = AI_PredictBallY(target_x);
            
            /* Adauga eroare pentru a face AI-ul mai uman */
            if (error_margin > 0) {
                paddle->target_y += (rand() % (error_margin * 2)) - error_margin;
            }
        }
        target_y = paddle->target_y;
    } else {
        /* Bila pleaca - revino la centru incet */
        target_y = FIELD_HEIGHT / 2;
        speed = 1;
    }
    
    /* Distanta pana la bila */
    int16_t ball_dist = is_right_side ? (PADDLE_X_P2 - ball.x) : (ball.x - PADDLE_X_P1);
    
    /* Miscare doar daca bila e in zona de reactie */
    if (ball_dist < reaction_zone || !ball_coming) {
        bool make_mistake = (mistake_chance > 0) && ((rand() % 100) < mistake_chance);
        
        if (make_mistake) {
            int mistake_type = rand() % 3;
            if (mistake_type == 0) {
                paddle->y -= speed;
            } else if (mistake_type == 1) {
                paddle->y += speed;
            }
        } else {
            if (paddle_center < target_y - 5) {
                paddle->y += speed;
            } else if (paddle_center > target_y + 5) {
                paddle->y -= speed;
            }
        }
    }
    
    /* Limiteaza pozitia */
    if (paddle->y < PADDLE_MIN_Y) paddle->y = PADDLE_MIN_Y;
    if (paddle->y > PADDLE_MAX_Y) paddle->y = PADDLE_MAX_Y;
}

/*============================================================================
 * DRAWING FUNCTIONS
 *============================================================================*/

void Game_DrawField(void) {
    ST7735_FillScreen(COLOR_BLACK);
    
    /* Linie centrala punctata */
    for (int16_t y = 4; y < FIELD_HEIGHT - 4; y += 8) {
        ST7735_FillRect(79, y, 2, 4, COLOR_DARK_GRAY);
    }
    
    /* Margini sus/jos */
    ST7735_DrawHLine(0, 0, FIELD_WIDTH, COLOR_WHITE);
    ST7735_DrawHLine(0, FIELD_HEIGHT - 1, FIELD_WIDTH, COLOR_WHITE);
}

void Game_DrawScore(void) {
    char buf[8];
    
    /* Sterge zona scorului */
    ST7735_FillRect(55, SCORE_Y, 50, 10, COLOR_BLACK);
    
    /* Scor P1 (stanga) */
    snprintf(buf, sizeof(buf), "%d", paddle1.score);
    ST7735_DrawStringScaled(68, SCORE_Y, buf, COLOR_CYAN, COLOR_BLACK, 1);
    
    /* Separator */
    ST7735_DrawString(77, SCORE_Y, "-", COLOR_WHITE, COLOR_BLACK);
    
    /* Scor P2 (dreapta) */
    snprintf(buf, sizeof(buf), "%d", paddle2.score);
    ST7735_DrawStringScaled(86, SCORE_Y, buf, COLOR_MAGENTA, COLOR_BLACK, 1);
}

/* Deseneaza o paleta (cu stergere optimizata) */
static void DrawPaddle(Paddle_t* paddle, int16_t x, uint16_t color) {
    if (paddle->y != paddle->prev_y) {
        if (paddle->y > paddle->prev_y) {
            int16_t clear_h = paddle->y - paddle->prev_y;
            if (clear_h > PADDLE_HEIGHT) clear_h = PADDLE_HEIGHT;
            ST7735_FillRect(x, paddle->prev_y, PADDLE_WIDTH, clear_h, COLOR_BLACK);
        } else {
            int16_t clear_h = paddle->prev_y - paddle->y;
            if (clear_h > PADDLE_HEIGHT) clear_h = PADDLE_HEIGHT;
            ST7735_FillRect(x, paddle->y + PADDLE_HEIGHT, PADDLE_WIDTH, clear_h, COLOR_BLACK);
        }
    }
    
    ST7735_FillRect(x, paddle->y, PADDLE_WIDTH, PADDLE_HEIGHT, color);
    paddle->prev_y = paddle->y;
}

/* Redeseneaza linia centrala */
static void RedrawCenterLine(void) {
    for (int16_t y = 4; y < FIELD_HEIGHT - 4; y += 8) {
        ST7735_FillRect(79, y, 2, 4, COLOR_DARK_GRAY);
    }
}

/* Deseneaza bila */
static void DrawBall(void) {
    if (ball.x != ball.prev_x || ball.y != ball.prev_y) {
        ST7735_FillRect(ball.prev_x, ball.prev_y, ball.size, ball.size, COLOR_BLACK);
    }
    
    ST7735_FillRect(ball.x, ball.y, ball.size, ball.size, COLOR_YELLOW);
    
    /* Redeseneaza linia centrala daca bila e in zona */
    if ((ball.x >= 70 && ball.x <= 90) || (ball.prev_x >= 70 && ball.prev_x <= 90)) {
        RedrawCenterLine();
    }
    
    /* Redeseneaza scorul daca bila e in zona de sus */
    if (ball.y < 15 || ball.prev_y < 15) {
        Game_DrawScore();
    }
    
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
}

/* Reset bila dupa gol */
static void ResetBall(void) {
    ST7735_FillRect(ball.prev_x, ball.prev_y, ball.size, ball.size, COLOR_BLACK);
    
    ball.x = BALL_START_X;
    ball.y = BALL_START_Y;
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
    
    int8_t dir = (ball.dx > 0) ? -1 : 1;
    ball.dx = dir * BALL_SPEED_X;
    ball.dy = (rand() % 2) ? BALL_SPEED_Y : -BALL_SPEED_Y;
    
    game.rally_frames = 0;
    game.speed_level = 0;
    
    delay_ms(500);
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

void Game_Init(void) {
    /* Reset bila */
    ball.x = BALL_START_X;
    ball.y = BALL_START_Y;
    ball.dx = BALL_SPEED_X;
    ball.dy = BALL_SPEED_Y;
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
    ball.size = BALL_SIZE;
    
    /* Directie aleatoare */
    if (rand() % 2) ball.dx = -ball.dx;
    if (rand() % 2) ball.dy = -ball.dy;
    
    /* Reset palete */
    paddle1.y = PADDLE_START_Y;
    paddle1.prev_y = paddle1.y;
    paddle1.score = 0;
    paddle1.input = g_player1_input;
    paddle1.target_y = FIELD_HEIGHT / 2;
    
    paddle2.y = PADDLE_START_Y;
    paddle2.prev_y = paddle2.y;
    paddle2.score = 0;
    paddle2.input = g_player2_input;
    paddle2.target_y = FIELD_HEIGHT / 2;
    
    /* Pentru Player vs CPU, seteaza dificultatea */
    if (IS_CPU_INPUT(paddle2.input)) {
        switch (g_currentDifficulty) {
            case DIFF_EASY:
                paddle2.input = INPUT_CPU_EASY;
                break;
            case DIFF_NORMAL:
                paddle2.input = INPUT_CPU_MEDIUM;
                break;
            case DIFF_HARD:
                paddle2.input = INPUT_CPU_HARD;
                break;
        }
    }
    
    /* Reset stare joc */
    game.is_running = 1;
    game.is_paused = 0;
    game.winner = 0;
    game.winning_score = SCORE_TO_WIN;
    game.frame_count = 0;
    game.rally_frames = 0;
    game.speed_level = 0;
    
    PRINTF("[GAME] Init - P1:%d P2:%d\r\n", paddle1.input, paddle2.input);
}

void Game_Start(void) {
    PRINTF("\r\n=== GAME STARTED ===\r\n");
    
    Game_Init();
    Game_DrawField();
    Game_DrawScore();
    
    /* Deseneaza paletele initiale */
    ST7735_FillRect(PADDLE_X_P1, paddle1.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_CYAN);
    ST7735_FillRect(PADDLE_X_P2, paddle2.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_MAGENTA);
    
    /* Countdown animat */
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_YELLOW);
    ST7735_DrawStringCentered(58, "READY", COLOR_YELLOW, COLOR_DARK_GRAY, 2);
    delay_ms(700);
    
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "3", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);
    
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "2", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);
    
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "1", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);
    
    ST7735_FillRect(40, 50, 80, 30, COLOR_GREEN);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "GO!", COLOR_WHITE, COLOR_GREEN, 2);
    delay_ms(500);
    
    /* Sterge mesajul */
    ST7735_FillRect(40, 50, 80, 30, COLOR_BLACK);
    RedrawCenterLine();
}

void Game_Update(void) {
    if (!game.is_running || game.is_paused) return;
    
    game.frame_count++;
    game.rally_frames++;
    
    /*----- ACCELERARE MINGE -----*/
    if (game.rally_frames > 0 &&
        game.rally_frames % SPEED_UP_INTERVAL == 0 &&
        game.speed_level < MAX_SPEED_LEVEL) {
        
        game.speed_level++;
        
        if (ball.dx > 0) ball.dx++;
        else ball.dx--;
        
        if (game.speed_level % 2 == 0) {
            if (ball.dy > 0) ball.dy++;
            else if (ball.dy < 0) ball.dy--;
        }
        
        ST7735_FillRect(45, 55, 70, 18, COLOR_ORANGE);
        ST7735_DrawRect(45, 55, 70, 18, COLOR_WHITE);
        ST7735_DrawStringCentered(59, "SPEED UP!", COLOR_WHITE, COLOR_ORANGE, 1);
        delay_ms(300);
        ST7735_FillRect(45, 55, 70, 18, COLOR_BLACK);
        RedrawCenterLine();
    }
    
    /*----- MISCARE PALETA 1 (Stanga) -----*/
    if (IS_CPU_INPUT(paddle1.input)) {
        AI_UpdatePaddle(&paddle1, false);
    } else if (paddle1.input == INPUT_JOYSTICK) {
        int8_t dir = Joystick_GetGameDirection();
        /* Inversam: joystick jos = paleta jos */
        paddle1.y += dir * (PADDLE_SPEED - 1);
    } else if (paddle1.input == INPUT_REMOTE) {
        int8_t dir = IR_GetGameDirection();
        paddle1.y += dir * PADDLE_SPEED;
    }
    
    /*----- MISCARE PALETA 2 (Dreapta) -----*/
    if (IS_CPU_INPUT(paddle2.input)) {
        AI_UpdatePaddle(&paddle2, true);
    } else if (paddle2.input == INPUT_JOYSTICK) {
        /* P2 poate folosi acelasi joystick daca P1 nu il foloseste */
        if (paddle1.input != INPUT_JOYSTICK) {
            int8_t dir = Joystick_GetGameDirection();
            paddle2.y += dir * (PADDLE_SPEED - 1);
        }
    } else if (paddle2.input == INPUT_REMOTE) {
        /* P2 poate folosi telecomanda daca P1 nu o foloseste */
        if (paddle1.input != INPUT_REMOTE) {
            int8_t dir = IR_GetGameDirection();
            paddle2.y += dir * PADDLE_SPEED;
        }
    }
    
    /* Limite palete */
    if (paddle1.y < PADDLE_MIN_Y) paddle1.y = PADDLE_MIN_Y;
    if (paddle1.y > PADDLE_MAX_Y) paddle1.y = PADDLE_MAX_Y;
    if (paddle2.y < PADDLE_MIN_Y) paddle2.y = PADDLE_MIN_Y;
    if (paddle2.y > PADDLE_MAX_Y) paddle2.y = PADDLE_MAX_Y;
    
    /*----- MISCARE BILA -----*/
    ball.x += ball.dx;
    ball.y += ball.dy;
    
    /* Coliziune sus/jos */
    if (ball.y <= 2) {
        ball.y = 3;
        ball.dy = -ball.dy;
    }
    if (ball.y >= FIELD_HEIGHT - BALL_SIZE - 2) {
        ball.y = FIELD_HEIGHT - BALL_SIZE - 3;
        ball.dy = -ball.dy;
    }
    
    /*----- COLIZIUNE CU PALETE -----*/
    
    /* Paleta 1 (stanga) */
    if (ball.dx < 0 && 
        ball.x <= PADDLE_X_P1 + PADDLE_WIDTH + 1 && 
        ball.x >= PADDLE_X_P1) {
        if (ball.y + ball.size >= paddle1.y && 
            ball.y <= paddle1.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P1 + PADDLE_WIDTH + 2;
            ball.dx = -ball.dx;
            
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle1.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }
    
    /* Paleta 2 (dreapta) */
    if (ball.dx > 0 && 
        ball.x + ball.size >= PADDLE_X_P2 - 1 && 
        ball.x <= PADDLE_X_P2 + PADDLE_WIDTH) {
        if (ball.y + ball.size >= paddle2.y && 
            ball.y <= paddle2.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P2 - ball.size - 2;
            ball.dx = -ball.dx;
            
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle2.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }
    
    /*----- GOL -----*/
    
    if (ball.x < -ball.size) {
        paddle2.score++;
        Game_DrawScore();
        if (paddle2.score >= game.winning_score) {
            game.winner = 2;
            game.is_running = 0;
        } else {
            ResetBall();
        }
    }
    
    if (ball.x > FIELD_WIDTH + ball.size) {
        paddle1.score++;
        Game_DrawScore();
        if (paddle1.score >= game.winning_score) {
            game.winner = 1;
            game.is_running = 0;
        } else {
            ResetBall();
        }
    }
    
    /*----- DESENARE -----*/
    DrawPaddle(&paddle1, PADDLE_X_P1, COLOR_CYAN);
    DrawPaddle(&paddle2, PADDLE_X_P2, COLOR_MAGENTA);
    DrawBall();
}

uint8_t Game_GetWinner(void) {
    return game.winner;
}

int16_t Game_GetScore(uint8_t player) {
    if (player == 1) return paddle1.score;
    if (player == 2) return paddle2.score;
    return 0;
}

void Game_SetPaused(bool paused) {
    game.is_paused = paused ? 1 : 0;
}

bool Game_IsPaused(void) {
    return game.is_paused != 0;
}

bool Game_IsRunning(void) {
    return game.is_running != 0;
}
