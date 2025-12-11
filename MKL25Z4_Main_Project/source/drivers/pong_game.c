#include "headers/pong_game.h"
#include "headers/st7735_simple.h"
#include "headers/joystick.h"
#include "headers/ir_remote.h"

typedef struct {
    int16_t x, y;
    int16_t dx, dy;
    uint8_t size;
} Ball_t;

typedef struct {
    int16_t y;
    int16_t score;
    InputType_t input;
} Paddle_t;

static Ball_t ball;
static Paddle_t p1, p2;

void Game_Init(void) {
    ST7735_FillScreen(COLOR_BLACK);

    // Reset pozitii
    ball.x = 80; ball.y = 64;
    ball.dx = 2; ball.dy = 1;
    ball.size = 4;

    p1.y = 50; p1.score = 0; p1.input = g_player1_input;
    p2.y = 50; p2.score = 0; p2.input = g_player2_input;

    // Desenare pereti/scor initial
    ST7735_DrawVLine(0, 0, 128, COLOR_WHITE);
    ST7735_DrawVLine(159, 0, 128, COLOR_WHITE);
}

void Game_Update(void) {
    /* 1. Miscare Paleta P1 */
    if (p1.input == INPUT_JOYSTICK) {
        int16_t joy_y = Joystick_GetY_Percent();
        // Mapare procent la viteza (-100 la 100 -> -3 la 3 pixeli)
        p1.y += (joy_y / 30);
    } else if (p1.input == INPUT_REMOTE) {
        // Logica IR pentru miscare continua e mai grea (nu ai hold),
        // folosim un "impuls" de miscare cand primim codul
        uint32_t ir = IR_GetLastCode();
        if (ir == IR_CODE_UP) p1.y -= 5;
        if (ir == IR_CODE_DOWN) p1.y += 5;
    }

    /* 2. Miscare Paleta P2 */
    if (p2.input == INPUT_REMOTE) {
        uint32_t ir = IR_GetLastCode();
        if (ir == IR_CODE_UP) p2.y -= 5;
        if (ir == IR_CODE_DOWN) p2.y += 5;
    }
    /* Joystick pentru P2 nu e implementat hardware (avem un singur ADC port conectat fizic)
       dar logica permite */

    /* Limite palete */
    if (p1.y < 2) p1.y = 2;
    if (p1.y > 100) p1.y = 100;
    if (p2.y < 2) p2.y = 2;
    if (p2.y > 100) p2.y = 100;

    /* 3. Miscare Bila */
    // Sterge bila veche (solutie rapida, sterge rect mic)
    ST7735_FillRect(ball.x, ball.y, ball.size, ball.size, COLOR_BLACK);

    ball.x += ball.dx;
    ball.y += ball.dy;

    /* Coliziuni Pereti Sus/Jos */
    if (ball.y <= 0 || ball.y >= 124) ball.dy = -ball.dy;

    /* Coliziuni Palete (Simplificat) */
    // P1 (Stanga, X ~ 5)
    if (ball.x <= 6 && ball.y >= p1.y && ball.y <= p1.y + 20) {
        ball.dx = -ball.dx;
        ball.x = 7;
    }
    // P2 (Dreapta, X ~ 150)
    if (ball.x >= 150 && ball.y >= p2.y && ball.y <= p2.y + 20) {
        ball.dx = -ball.dx;
        ball.x = 149;
    }

    /* Iesire din teren (Gol) */
    if (ball.x < 0 || ball.x > 160) {
        // Reset
        ball.x = 80; ball.y = 64;
        // Inversam directia
        ball.dx = -ball.dx;
    }

    /* 4. Desenare */
    // Palete
    ST7735_FillRect(2, 0, 4, 128, COLOR_BLACK); // Sterge urma P1
    ST7735_FillRect(2, p1.y, 4, 20, COLOR_CYAN); // Deseneaza P1

    ST7735_FillRect(154, 0, 4, 128, COLOR_BLACK); // Sterge urma P2
    ST7735_FillRect(154, p2.y, 4, 20, COLOR_MAGENTA); // Deseneaza P2

    // Bila
    ST7735_FillRect(ball.x, ball.y, ball.size, ball.size, COLOR_YELLOW);
}
