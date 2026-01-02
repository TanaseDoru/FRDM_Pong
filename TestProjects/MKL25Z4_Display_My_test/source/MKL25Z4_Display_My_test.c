/*
 * MKL25Z4_Display_Test.c
 * Sistem de Meniu + Joc Pong Complet
 * ST7735 LCD + FRDM-KL25Z
 * Cu suport Bot/CPU si control de la tastatura
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "fsl_adc16.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "st7735_simple.h"

/*============================================================================
 * DEFINITII TIPURI INPUT SI ECRANE
 *============================================================================*/

/* Tipuri de input disponibile */
typedef enum {
    INPUT_NONE = 0,
    INPUT_JOYSTICK,
    INPUT_REMOTE,
	INPUT_GYROSCOPE,
    INPUT_CPU_EASY,      /* Bot - nivel usor */
    INPUT_CPU_MEDIUM,    /* Bot - nivel mediu */
    INPUT_CPU_HARD       /* Bot - nivel greu */
} InputType_t;

/* Ecranele disponibile */
typedef enum {
    SCREEN_INTRO = 0,    /* Ecranul de intro animat */
    SCREEN_MAIN,
    SCREEN_START,
    SCREEN_SELECT_INPUT,
    SCREEN_SELECT_P1,
    SCREEN_SELECT_P2,
    SCREEN_DIFFICULTY,   /* Selectare dificultate CPU */
    SCREEN_GAMEPLAY,     /* Ecranul de joc */
    SCREEN_GAME_OVER,    /* Ecranul de sfarsit */
    SCREEN_PAUSED        /* Joc in pauza */
} Screen_t;

/*============================================================================
 * STRUCTURI PENTRU JOC
 *============================================================================*/

typedef struct {
    int16_t x, y;        /* Pozitia bilei */
    int16_t dx, dy;      /* Viteza bilei */
    int16_t prev_x, prev_y; /* Pozitia anterioara (pentru stergere) */
    uint8_t size;
} Ball_t;

typedef struct {
    int16_t y;           /* Pozitia paletei */
    int16_t prev_y;      /* Pozitia anterioara */
    int16_t score;       /* Scorul */
    InputType_t input;   /* Tipul de input */
    int16_t target_y;    /* Pentru AI - tinta */
} Paddle_t;

typedef struct {
    uint8_t is_running;
    uint8_t is_paused;
    uint8_t winner;      /* 0=nimeni, 1=P1, 2=P2 */
    uint8_t winning_score;
    uint16_t frame_count;
    uint16_t rally_frames;    /* Frames de la ultimul gol - pentru accelerare */
    uint8_t speed_level;      /* Nivelul curent de viteza (0-5) */
} GameState_t;

/* Starea meniului pentru fiecare ecran */
typedef struct {
    uint8_t selectedIndex;
    uint8_t maxItems;
} MenuState_t;

/*============================================================================
 * VARIABILE GLOBALE
 *============================================================================*/

InputType_t g_player1_input = INPUT_JOYSTICK;   /* Default: joystick */
InputType_t g_player2_input = INPUT_CPU_MEDIUM; /* Default: CPU mediu */

Screen_t g_currentScreen = SCREEN_INTRO;
MenuState_t g_menuState = {0, 3};

/* Flag pentru refresh ecran */
volatile uint8_t g_needsRedraw = 1;

/* Variabile pentru joc */
static Ball_t ball;
static Paddle_t paddle1, paddle2;
static GameState_t game;

/* Input curent de la tastatura pentru joc */
static int8_t g_p1_move = 0;  /* -1=sus, 0=stop, 1=jos */
static int8_t g_p2_move = 0;

/*============================================================================
 * CONSTANTE PENTRU JOC
 *============================================================================*/

#define FIELD_WIDTH      160
#define FIELD_HEIGHT     128
#define PADDLE_WIDTH     4
#define PADDLE_HEIGHT    22
#define PADDLE_X_P1      4       /* Pozitia X a paletei P1 */
#define PADDLE_X_P2      152     /* Pozitia X a paletei P2 */
#define BALL_SIZE        4
#define BALL_START_X     80
#define BALL_START_Y     64
#define PADDLE_START_Y   53      /* (128 - 22) / 2 */
#define SCORE_TO_WIN     5
#define SCORE_Y          2       /* Pozitia Y a scorului */

/* Viteze */
#define PADDLE_SPEED     4
#define BALL_SPEED_X     1       /* Viteza initiala mai mica */
#define BALL_SPEED_Y     1

/* Accelerare minge in timp */
#define SPEED_UP_INTERVAL   500   /* Frames pana la urmatoarea accelerare (~10 sec) */
#define MAX_SPEED_LEVEL     5     /* Nivel maxim de viteza */

/* Zone de coliziune */
#define PADDLE_MIN_Y     2
#define PADDLE_MAX_Y     (FIELD_HEIGHT - PADDLE_HEIGHT - 2)

/*============================================================================
 * CONFIGURARE JOYSTICK (ADC)
 *============================================================================*/

#define JOYSTICK_VRX_CHANNEL 8U    /* PTB0 - X axis (optional) */
#define JOYSTICK_VRY_CHANNEL 9U    /* PTB1 - Y axis */

/* Buton joystick pe PTD4 */
#define JOYSTICK_SW_PIN      4U
#define JOYSTICK_SW_PORT     PORTD
#define JOYSTICK_SW_GPIO     GPIOD
#define JOYSTICK_SW_IRQ      PORTD_IRQn

/* Variabile joystick */
static volatile bool g_joy_btn_pressed = false;
static int16_t g_joy_y_percent = 0;
static bool g_joy_action_consumed = false;

/*============================================================================
 * CONSTANTE PENTRU UI
 *============================================================================*/

#define MENU_START_Y      35   /* Pozitia de start a meniului */
#define MENU_ITEM_HEIGHT  14   /* Inaltimea fiecarui item (pentru 6 itemi) */
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
 * SYSTICK TIMER - pentru timing precis
 *============================================================================*/

static volatile uint32_t g_systick_ms = 0;  /* Contor milisecunde */

/* Handler SysTick - se apeleaza la fiecare 1ms */
void SysTick_Handler(void) {
    g_systick_ms++;
}

/* Initializeaza SysTick pentru 1ms interrupt */
void Timer_Init(void) {
    /* SystemCoreClock = 48MHz
     * Pentru 1ms: 48000000 / 1000 = 48000 ticks */
    SysTick_Config(SystemCoreClock / 1000U);
    PRINTF("Timer initialized (SysTick 1ms)\r\n");
}

/* Returneaza timpul curent in milisecunde */
uint32_t Timer_GetMs(void) {
    return g_systick_ms;
}

/* Delay BLOCANT folosind timer (mai precis decat busy-wait) */
void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __WFI();  /* Wait For Interrupt - economiseste energie */
    }
}

/* Delay NON-BLOCANT - verifica daca a trecut timpul */
/* Returneaza true daca timpul a expirat */
bool Timer_Elapsed(uint32_t start_time, uint32_t duration_ms) {
    return ((g_systick_ms - start_time) >= duration_ms);
}

/*============================================================================
 * FUNCTII HELPER
 *============================================================================*/

/*============================================================================
 * FUNCTII JOYSTICK
 *============================================================================*/

/* ISR pentru butonul joystick (Port D) */
void PORTD_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);
    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);
        g_joy_btn_pressed = true;
    }
}

/* Citeste valoarea ADC de pe un canal */
static uint16_t Joystick_ReadADC(uint32_t channel) {
    adc16_channel_config_t chConfig = {
        .channelNumber = channel,
        .enableInterruptOnConversionCompleted = false,
        .enableDifferentialConversion = false
    };
    ADC16_SetChannelConfig(ADC0, 0, &chConfig);
    while (0U == (kADC16_ChannelConversionDoneFlag & ADC16_GetChannelStatusFlags(ADC0, 0)));
    return ADC16_GetChannelConversionValue(ADC0, 0);
}

/* Initializeaza joystick-ul (ADC + GPIO pentru buton) */
void Joystick_Init(void) {
    adc16_config_t adcConfig;

    /* Enable clocks */
    CLOCK_EnableClock(kCLOCK_Adc0);
    CLOCK_EnableClock(kCLOCK_PortB);  /* Pentru pinii analogici */
    CLOCK_EnableClock(kCLOCK_PortD);  /* Pentru buton */

    /* Configureaza ADC */
    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.resolution = kADC16_ResolutionSE12Bit;
    ADC16_Init(ADC0, &adcConfig);
    ADC16_DoAutoCalibration(ADC0);

    /* Configureaza butonul pe PTD4 */
    PORT_SetPinMux(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_MuxAsGpio);
    /* Pull-up intern */
    JOYSTICK_SW_PORT->PCR[JOYSTICK_SW_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    gpio_pin_config_t sw_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(JOYSTICK_SW_GPIO, JOYSTICK_SW_PIN, &sw_config);

    /* Intrerupere pe falling edge (buton apasat = GND) */
    PORT_SetPinInterruptConfig(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_InterruptFallingEdge);

    NVIC_SetPriority(JOYSTICK_SW_IRQ, 3);
    EnableIRQ(JOYSTICK_SW_IRQ);

    PRINTF("Joystick initialized (VRY=PTB1, SW=PTD4)\r\n");
}

/* Proceseaza joystick-ul - citeste ADC si actualizeaza procentul */
void Joystick_Process(void) {
    uint16_t y_raw = Joystick_ReadADC(JOYSTICK_VRY_CHANNEL);

    /* Centrul este ~2048 (la 12 biti). Convertim la procent (-100 la +100) */
    g_joy_y_percent = ((int32_t)y_raw - 2048) * 100 / 2048;

    /* Dead zone - reset consumed flag cand joystick-ul e in centru */
    if (g_joy_y_percent > -20 && g_joy_y_percent < 20) {
        g_joy_action_consumed = false;
    }
}

/* Returneaza actiunea pentru meniu (UP/DOWN/SELECT) */
typedef enum {
    JOY_ACTION_NONE = 0,
    JOY_ACTION_UP,
    JOY_ACTION_DOWN,
    JOY_ACTION_SELECT
} JoyAction_t;

JoyAction_t Joystick_GetMenuAction(void) {
    /* Buton apasat = SELECT */
    if (g_joy_btn_pressed) {
        g_joy_btn_pressed = false;
        return JOY_ACTION_SELECT;
    }

    /* Miscare joystick */
    if (!g_joy_action_consumed) {
        if (g_joy_y_percent > 50) {
            g_joy_action_consumed = true;
            return JOY_ACTION_DOWN;  /* Joystick in jos = meniu jos */
        }
        if (g_joy_y_percent < -50) {
            g_joy_action_consumed = true;
            return JOY_ACTION_UP;    /* Joystick in sus = meniu sus */
        }
    }
    return JOY_ACTION_NONE;
}

/* Returneaza procentul Y pentru control paleta (-100 la +100) */
int16_t Joystick_GetY_Percent(void) {
    return g_joy_y_percent;
}

/* Obtine numele input-ului */
const char* GetInputName(InputType_t input) {
    switch (input) {
        case INPUT_JOYSTICK:  return "Joystick";
        case INPUT_REMOTE:    return "Remote";
        case INPUT_CPU_EASY:  return "CPU Easy";
        case INPUT_CPU_MEDIUM: return "CPU Med";
        case INPUT_CPU_HARD:  return "CPU Hard";
        default:              return "None";
    }
}

/* Verifica daca input-ul este de tip CPU/Bot */
bool IsCPUInput(InputType_t input) {
    return (input == INPUT_CPU_EASY || input == INPUT_CPU_MEDIUM || input == INPUT_CPU_HARD);
}

/* Verifica daca un input e disponibil (nu e luat de celalalt jucator) */
bool IsInputAvailable(InputType_t input, uint8_t forPlayer) {
    /* INPUT_NONE e mereu disponibil */
    if (input == INPUT_NONE) return true;

    /* CPU-urile sunt mereu disponibile (poti avea CPU vs CPU) */
    if (IsCPUInput(input)) return true;

    /* Pentru input-uri fizice (Keyboard, Joystick, Remote): */
    /* Verifica daca celalalt jucator NU are deja acest input */
    if (forPlayer == 1) {
        /* P1 vrea sa selecteze - verifica daca P2 nu are deja */
        if (g_player2_input == input) {
            PRINTF("[CHECK] P1 blocked: %s already used by P2\r\n", GetInputName(input));
            return false;
        }
    } else {
        /* P2 vrea sa selecteze - verifica daca P1 nu are deja */
        if (g_player1_input == input) {
            PRINTF("[CHECK] P2 blocked: %s already used by P1\r\n", GetInputName(input));
            return false;
        }
    }

    return true;
}

/*============================================================================
 * FUNCTII AI / BOT
 *============================================================================*/

/* Calculeaza unde va ajunge bila - predictie simpla */
int16_t AI_PredictBallY(int16_t target_x) {
    int16_t sim_x = ball.x;
    int16_t sim_y = ball.y;
    int16_t sim_dx = ball.dx;
    int16_t sim_dy = ball.dy;

    /* Simuleaza miscarea bilei pana ajunge la target_x */
    int iterations = 0;
    while (iterations < 200) {
        sim_x += sim_dx;
        sim_y += sim_dy;

        /* Bounce pe pereti sus/jos */
        if (sim_y <= 2 || sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) {
            sim_dy = -sim_dy;
            if (sim_y <= 2) sim_y = 3;
            if (sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) sim_y = FIELD_HEIGHT - BALL_SIZE - 3;
        }

        /* A ajuns la pozitia dorita? */
        if ((sim_dx > 0 && sim_x >= target_x) || (sim_dx < 0 && sim_x <= target_x)) {
            return sim_y;
        }

        iterations++;
    }

    return FIELD_HEIGHT / 2; /* Fallback la centru */
}

/* Update AI pentru o paleta */
void AI_UpdatePaddle(Paddle_t* paddle, bool is_right_side) {
    int16_t paddle_center = paddle->y + PADDLE_HEIGHT / 2;
    int16_t target_y;
    int16_t speed;
    int16_t reaction_zone;
    int16_t error_margin;
    int16_t mistake_chance;    /* Sansa sa faca o greseala (din 100) */
    int16_t update_interval;   /* Cat de des recalculeaza tinta */

    /* Parametri bazati pe dificultate */
    switch (paddle->input) {
        case INPUT_CPU_EASY:
            speed = 1;             /* Foarte lent */
            reaction_zone = 35;    /* Reactioneaza foarte tarziu */
            error_margin = 35;     /* Eroare foarte mare */
            mistake_chance = 35;   /* 35% sansa sa mearga gresit */
            update_interval = 20;  /* Recalculeaza foarte rar */
            break;
        case INPUT_CPU_MEDIUM:
            speed = 2;
            reaction_zone = 80;
            error_margin = 15;
            mistake_chance = 12;   /* 12% greseli */
            update_interval = 10;
            break;
        case INPUT_CPU_HARD:
            speed = 4;
            reaction_zone = 160;   /* Reactioneaza mereu */
            error_margin = 5;      /* Foarte precis */
            mistake_chance = 0;    /* Fara greseli */
            update_interval = 3;
            break;
        default:
            return;
    }

    /* Verifica daca bila vine spre aceasta paleta */
    bool ball_coming = (is_right_side && ball.dx > 0) || (!is_right_side && ball.dx < 0);

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
        /* Verifica daca face o greseala random */
        bool make_mistake = (mistake_chance > 0) && ((rand() % 100) < mistake_chance);

        if (make_mistake) {
            /* Greseala: se misca in directia gresita sau sta pe loc */
            int mistake_type = rand() % 3;
            if (mistake_type == 0) {
                /* Se misca in sus cand ar trebui in jos */
                paddle->y -= speed;
            } else if (mistake_type == 1) {
                /* Se misca in jos cand ar trebui in sus */
                paddle->y += speed;
            }
            /* mistake_type == 2: sta pe loc */
        } else {
            /* Miscare normala spre tinta */
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
 * FUNCTII JOC PONG
 *============================================================================*/

/* Initializeaza jocul */
void Game_Init(void) {
    /* Reset bila */
    ball.x = BALL_START_X;
    ball.y = BALL_START_Y;
    ball.dx = BALL_SPEED_X;
    ball.dy = BALL_SPEED_Y;
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
    ball.size = BALL_SIZE;

    /* Directie aleatoare la start */
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

    /* Reset stare joc */
    game.is_running = 1;
    game.is_paused = 0;
    game.winner = 0;
    game.winning_score = SCORE_TO_WIN;
    game.frame_count = 0;
    game.rally_frames = 0;
    game.speed_level = 0;

    /* Reset input */
    g_p1_move = 0;
    g_p2_move = 0;
}

/* Deseneaza terenul de joc initial */
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

/* Deseneaza scorul */
void Game_DrawScore(void) {
    char buf[8];

    /* Sterge zona scorului - centrata pe ecran */
    ST7735_FillRect(55, SCORE_Y, 50, 10, COLOR_BLACK);

    /* Centrul ecranului = 80 */
    /* Scor P1 (stanga de centru) */
    snprintf(buf, sizeof(buf), "%d", paddle1.score);
    ST7735_DrawStringScaled(68, SCORE_Y, buf, COLOR_CYAN, COLOR_BLACK, 1);

    /* Separator - fix la centru (80 - 3 = 77) */
    ST7735_DrawString(77, SCORE_Y, "-", COLOR_WHITE, COLOR_BLACK);

    /* Scor P2 (dreapta de centru) */
    snprintf(buf, sizeof(buf), "%d", paddle2.score);
    ST7735_DrawStringScaled(86, SCORE_Y, buf, COLOR_MAGENTA, COLOR_BLACK, 1);
}

/* Deseneaza o paleta (cu stergere optimizata) */
void Game_DrawPaddle(Paddle_t* paddle, int16_t x, uint16_t color) {
    /* Sterge doar zona care s-a schimbat */
    if (paddle->y != paddle->prev_y) {
        if (paddle->y > paddle->prev_y) {
            /* S-a mutat in jos - sterge partea de sus */
            int16_t clear_h = paddle->y - paddle->prev_y;
            if (clear_h > PADDLE_HEIGHT) clear_h = PADDLE_HEIGHT;
            ST7735_FillRect(x, paddle->prev_y, PADDLE_WIDTH, clear_h, COLOR_BLACK);
        } else {
            /* S-a mutat in sus - sterge partea de jos */
            int16_t clear_h = paddle->prev_y - paddle->y;
            if (clear_h > PADDLE_HEIGHT) clear_h = PADDLE_HEIGHT;
            ST7735_FillRect(x, paddle->y + PADDLE_HEIGHT, PADDLE_WIDTH, clear_h, COLOR_BLACK);
        }
    }

    /* Deseneaza paleta */
    ST7735_FillRect(x, paddle->y, PADDLE_WIDTH, PADDLE_HEIGHT, color);

    paddle->prev_y = paddle->y;
}

/* Redeseneaza linia centrala punctata complet */
void Game_RedrawCenterLine(void) {
    /* Linia centrala e la x=79, latime 2, segmente de 4px la fiecare 8px */
    for (int16_t y = 4; y < FIELD_HEIGHT - 4; y += 8) {
        ST7735_FillRect(79, y, 2, 4, COLOR_DARK_GRAY);
    }
}

/* Deseneaza bila (cu stergere optimizata) */
void Game_DrawBall(void) {
    /* Sterge bila veche */
    if (ball.x != ball.prev_x || ball.y != ball.prev_y) {
        ST7735_FillRect(ball.prev_x, ball.prev_y, ball.size, ball.size, COLOR_BLACK);
    }

    /* Deseneaza bila noua */
    ST7735_FillRect(ball.x, ball.y, ball.size, ball.size, COLOR_YELLOW);

    /* Redeseneaza linia centrala daca bila e in zona ei sau tocmai a trecut */
    if ((ball.x >= 70 && ball.x <= 90) || (ball.prev_x >= 70 && ball.prev_x <= 90)) {
        Game_RedrawCenterLine();
    }

    /* Redeseneaza scorul daca bila e in zona de sus */
    if (ball.y < 15 || ball.prev_y < 15) {
        Game_DrawScore();
    }

    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
}

/* Reset bila dupa gol */
void Game_ResetBall(void) {
    /* Sterge bila veche */
    ST7735_FillRect(ball.prev_x, ball.prev_y, ball.size, ball.size, COLOR_BLACK);

    ball.x = BALL_START_X;
    ball.y = BALL_START_Y;
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;

    /* Reset viteza la valoarea initiala */
    int8_t dir = (ball.dx > 0) ? -1 : 1;  /* Schimba directia spre cine a primit gol */
    ball.dx = dir * BALL_SPEED_X;
    ball.dy = (rand() % 2) ? BALL_SPEED_Y : -BALL_SPEED_Y;

    /* Reset rally si speed level */
    game.rally_frames = 0;
    game.speed_level = 0;

    delay_ms(500); /* Pauza scurta dupa gol */
}

/* Update logica jocului */
void Game_Update(void) {
    if (!game.is_running || game.is_paused) return;

    game.frame_count++;
    game.rally_frames++;

    /*----- 0. ACCELERARE MINGE IN TIMP -----*/
    /* La fiecare SPEED_UP_INTERVAL frames, creste viteza */
    if (game.rally_frames > 0 &&
        game.rally_frames % SPEED_UP_INTERVAL == 0 &&
        game.speed_level < MAX_SPEED_LEVEL) {

        game.speed_level++;

        /* Creste viteza bilei */
        if (ball.dx > 0) ball.dx++;
        else ball.dx--;

        /* Creste si viteza pe Y uneori */
        if (game.speed_level % 2 == 0) {
            if (ball.dy > 0) ball.dy++;
            else if (ball.dy < 0) ball.dy--;
        }

        /* Afiseaza indicator SPEED UP! */
        ST7735_FillRect(45, 55, 70, 18, COLOR_ORANGE);
        ST7735_DrawRect(45, 55, 70, 18, COLOR_WHITE);
        ST7735_DrawStringCentered(59, "SPEED UP!", COLOR_WHITE, COLOR_ORANGE, 1);
        delay_ms(300);
        ST7735_FillRect(45, 55, 70, 18, COLOR_BLACK);
        Game_RedrawCenterLine();

        PRINTF("[GAME] Speed level: %d\r\n", game.speed_level);
    }

    /*----- 1. MISCARE PALETE -----*/

    /* Paleta 1 (stanga) */
    if (IsCPUInput(paddle1.input)) {
        AI_UpdatePaddle(&paddle1, false);
    } else if (paddle1.input == INPUT_JOYSTICK) {
        /* Control cu joystick - citim procentul Y */
        int16_t joy_y = Joystick_GetY_Percent();
        /* Mapare: procent (-100 la +100) -> viteza paleta */
        /* Dead zone de +-15% pentru a evita drift */
        if (joy_y > 15) {
            paddle1.y += (joy_y / 25);  /* Miscare in jos */
        } else if (joy_y < -15) {
            paddle1.y += (joy_y / 25);  /* Miscare in sus */
        }
    }

    /* Paleta 2 (dreapta) */
    if (IsCPUInput(paddle2.input)) {
        AI_UpdatePaddle(&paddle2, true);
    } else if (paddle2.input == INPUT_JOYSTICK) {
        /* Al doilea joystick - daca e conectat pe alt canal ADC */
        /* Pentru moment, folosim acelasi joystick (pentru testare) */
        int16_t joy_y = Joystick_GetY_Percent();
        if (joy_y > 15) {
            paddle2.y += (joy_y / 25);
        } else if (joy_y < -15) {
            paddle2.y += (joy_y / 25);
        }
    }

    /* Limiteaza pozitiile paletelor */
    if (paddle1.y < PADDLE_MIN_Y) paddle1.y = PADDLE_MIN_Y;
    if (paddle1.y > PADDLE_MAX_Y) paddle1.y = PADDLE_MAX_Y;
    if (paddle2.y < PADDLE_MIN_Y) paddle2.y = PADDLE_MIN_Y;
    if (paddle2.y > PADDLE_MAX_Y) paddle2.y = PADDLE_MAX_Y;

    /*----- 2. MISCARE BILA -----*/
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

    /*----- 3. COLIZIUNE CU PALETE -----*/

    /* Paleta 1 (stanga) */
    if (ball.dx < 0 && ball.x <= PADDLE_X_P1 + PADDLE_WIDTH + 1 && ball.x >= PADDLE_X_P1) {
        if (ball.y + ball.size >= paddle1.y && ball.y <= paddle1.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P1 + PADDLE_WIDTH + 2;
            ball.dx = -ball.dx;

            /* Modifica unghiul bazat pe unde loveste paleta */
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle1.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }

    /* Paleta 2 (dreapta) */
    if (ball.dx > 0 && ball.x + ball.size >= PADDLE_X_P2 - 1 && ball.x <= PADDLE_X_P2 + PADDLE_WIDTH) {
        if (ball.y + ball.size >= paddle2.y && ball.y <= paddle2.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P2 - ball.size - 2;
            ball.dx = -ball.dx;

            /* Modifica unghiul */
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle2.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }

    /*----- 4. GOL -----*/

    /* Bila a iesit pe STANGA - P1 (tu) nu a aparat, P2 (CPU) marcheaza */
    if (ball.x < -ball.size) {
        paddle2.score++;  /* P2/CPU primeste punct */
        Game_DrawScore();
        if (paddle2.score >= game.winning_score) {
            game.winner = 2;
            game.is_running = 0;
        } else {
            Game_ResetBall();
        }
    }

    /* Bila a iesit pe DREAPTA - P2 (CPU) nu a aparat, P1 (tu) marcheaza */
    if (ball.x > FIELD_WIDTH + ball.size) {
        paddle1.score++;  /* P1/Tu primesti punct */
        Game_DrawScore();
        if (paddle1.score >= game.winning_score) {
            game.winner = 1;
            game.is_running = 0;
        } else {
            Game_ResetBall();
        }
    }

    /*----- 5. DESENARE -----*/
    Game_DrawPaddle(&paddle1, PADDLE_X_P1, COLOR_CYAN);
    Game_DrawPaddle(&paddle2, PADDLE_X_P2, COLOR_MAGENTA);
    Game_DrawBall();
}

/* Porneste jocul */
void Game_Start(void) {
    PRINTF("\r\n=== GAME STARTED ===\r\n");
    PRINTF("P1: %s | P2: %s\r\n", GetInputName(g_player1_input), GetInputName(g_player2_input));
    PRINTF("Controls: W/S=P1, I/K=P2, P=Pause, ESC=Quit\r\n\r\n");

    Game_Init();
    Game_DrawField();
    Game_DrawScore();

    /* Deseneaza paletele initiale */
    ST7735_FillRect(PADDLE_X_P1, paddle1.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_CYAN);
    ST7735_FillRect(PADDLE_X_P2, paddle2.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_MAGENTA);

    /* === ARCADE STYLE: Countdown === */
    /* READY */
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_YELLOW);
    ST7735_DrawStringCentered(58, "READY", COLOR_YELLOW, COLOR_DARK_GRAY, 2);
    delay_ms(700);

    /* 3 */
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "3", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);

    /* 2 */
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "2", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);

    /* 1 */
    ST7735_FillRect(40, 50, 80, 30, COLOR_DARK_GRAY);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "1", COLOR_WHITE, COLOR_DARK_GRAY, 2);
    delay_ms(400);

    /* GO! */
    ST7735_FillRect(40, 50, 80, 30, COLOR_GREEN);
    ST7735_DrawRect(40, 50, 80, 30, COLOR_WHITE);
    ST7735_DrawStringCentered(58, "GO!", COLOR_WHITE, COLOR_GREEN, 2);
    delay_ms(500);

    /* Sterge mesajul si redeseneaza linia centrala */
    ST7735_FillRect(40, 50, 80, 30, COLOR_BLACK);
    Game_RedrawCenterLine();

    g_currentScreen = SCREEN_GAMEPLAY;
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
        ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    }

    /* Deseneaza textul */
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, text, textColor, bgColor);
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
        ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    }

    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, GetInputName(input), textColor, bgColor);

    /* Afiseaza status */
    if (isCurrentSelection) {
        /* Selectia curenta - bifa verde */
        ST7735_DrawString(115, y + 3, "<-", COLOR_HIGHLIGHT, bgColor);
    } else if (!available) {
        /* Input folosit de celalalt jucator */
        if (forPlayer == 1) {
            ST7735_DrawString(108, y + 3, "[P2]", COLOR_RED, bgColor);
        } else {
            ST7735_DrawString(108, y + 3, "[P1]", COLOR_CYAN, bgColor);
        }
    }
}

/* Deseneaza optiunea Back */
void DrawBackOption(uint8_t index, bool selected) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    if (selected) {
        ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    }

    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, "<< Back", COLOR_BACK, bgColor);
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

    DrawInputMenuItem(1, INPUT_JOYSTICK, g_menuState.selectedIndex == 1, 1);
    DrawInputMenuItem(2, INPUT_REMOTE, g_menuState.selectedIndex == 2, 1);
    DrawInputMenuItem(3, INPUT_CPU_EASY, g_menuState.selectedIndex == 3, 1);
    DrawInputMenuItem(4, INPUT_CPU_HARD, g_menuState.selectedIndex == 4, 1);
    DrawBackOption(5, g_menuState.selectedIndex == 5);
}

/* ECRAN SELECT PLAYER 2 INPUT */
void DrawSelectP2Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PLAYER 2");

    DrawInputMenuItem(1, INPUT_JOYSTICK, g_menuState.selectedIndex == 1, 2);
    DrawInputMenuItem(2, INPUT_REMOTE, g_menuState.selectedIndex == 2, 2);
    DrawInputMenuItem(3, INPUT_CPU_EASY, g_menuState.selectedIndex == 3, 2);
    DrawInputMenuItem(4, INPUT_CPU_HARD, g_menuState.selectedIndex == 4, 2);
    DrawBackOption(5, g_menuState.selectedIndex == 5);
}

/* ECRAN SELECTARE DIFICULTATE CPU */
void DrawDifficultyScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("DIFFICULTY");

    /* Descrieri pentru fiecare nivel */
    DrawMenuItem(0, "Easy", g_menuState.selectedIndex == 0, true);
    if (g_menuState.selectedIndex == 0) {
        ST7735_DrawString(90, MENU_START_Y + 3, "(Slow)", COLOR_GRAY, COLOR_DARK_GRAY);
    }

    DrawMenuItem(1, "Medium", g_menuState.selectedIndex == 1, true);
    if (g_menuState.selectedIndex == 1) {
        ST7735_DrawString(90, MENU_START_Y + MENU_ITEM_HEIGHT + 3, "(Normal)", COLOR_GRAY, COLOR_DARK_GRAY);
    }

    DrawMenuItem(2, "Hard", g_menuState.selectedIndex == 2, true);
    if (g_menuState.selectedIndex == 2) {
        ST7735_DrawString(90, MENU_START_Y + 2*MENU_ITEM_HEIGHT + 3, "(Fast)", COLOR_GRAY, COLOR_DARK_GRAY);
    }

    DrawBackOption(3, g_menuState.selectedIndex == 3);

    /* Info jos */
    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawString(10, 108, "P1: Joystick", COLOR_CYAN, COLOR_BG);
    ST7735_DrawString(90, 108, "P2: CPU", COLOR_MAGENTA, COLOR_BG);
}

/* ECRAN GAME OVER */
void DrawGameOverScreen(void) {
    ST7735_FillScreen(COLOR_BG);

    /* Titlu mare */
    ST7735_DrawStringCentered(15, "GAME OVER", COLOR_RED, COLOR_BG, 2);

    /* Castigator */
    char buf[32];
    uint16_t winner_color = (game.winner == 1) ? COLOR_CYAN : COLOR_MAGENTA;
    snprintf(buf, sizeof(buf), "Player %d Wins!", game.winner);
    ST7735_DrawStringCentered(45, buf, winner_color, COLOR_BG, 1);

    /* Scor final */
    snprintf(buf, sizeof(buf), "%d - %d", paddle1.score, paddle2.score);
    ST7735_DrawStringCentered(60, buf, COLOR_WHITE, COLOR_BG, 2);

    /* Optiuni - desenam manual la pozitii fixe */
    ST7735_DrawHLine(0, 82, ST7735_WIDTH, COLOR_GRAY);

    /* Play Again */
    uint16_t bg0 = (g_menuState.selectedIndex == 0) ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t col0 = (g_menuState.selectedIndex == 0) ? COLOR_SELECTED : COLOR_NORMAL;
    ST7735_FillRect(0, 88, ST7735_WIDTH, 16, bg0);
    if (g_menuState.selectedIndex == 0) ST7735_DrawString(MENU_MARGIN_X, 92, ">", COLOR_SELECTED, bg0);
    ST7735_DrawString(MENU_MARGIN_X + 12, 92, "Play Again", col0, bg0);

    /* Main Menu */
    uint16_t bg1 = (g_menuState.selectedIndex == 1) ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t col1 = (g_menuState.selectedIndex == 1) ? COLOR_SELECTED : COLOR_NORMAL;
    ST7735_FillRect(0, 106, ST7735_WIDTH, 16, bg1);
    if (g_menuState.selectedIndex == 1) ST7735_DrawString(MENU_MARGIN_X, 110, ">", COLOR_SELECTED, bg1);
    ST7735_DrawString(MENU_MARGIN_X + 12, 110, "Main Menu", col1, bg1);
}

/* ECRAN PAUZA - cu meniu Resume/Exit */
static uint8_t g_pause_selection = 0;  /* 0 = Resume, 1 = Exit */

void DrawPausedScreen(void) {
    /* Suprapune centrul ecranului - mai mare pentru meniu */
    ST7735_FillRect(25, 35, 110, 60, COLOR_DARK_GRAY);
    ST7735_DrawRect(25, 35, 110, 60, COLOR_WHITE);
    ST7735_DrawRect(27, 37, 106, 56, COLOR_GRAY);

    /* Titlu */
    ST7735_DrawStringCentered(42, "PAUSED", COLOR_YELLOW, COLOR_DARK_GRAY, 2);

    /* Optiuni meniu */
    /* Resume */
    if (g_pause_selection == 0) {
        ST7735_FillRect(40, 62, 80, 12, COLOR_CYAN);
        ST7735_DrawString(45, 64, "> Resume", COLOR_BLACK, COLOR_CYAN);
    } else {
        ST7735_FillRect(40, 62, 80, 12, COLOR_DARK_GRAY);
        ST7735_DrawString(50, 64, "Resume", COLOR_WHITE, COLOR_DARK_GRAY);
    }

    /* Exit */
    if (g_pause_selection == 1) {
        ST7735_FillRect(40, 76, 80, 12, COLOR_RED);
        ST7735_DrawString(45, 78, "> Exit", COLOR_WHITE, COLOR_RED);
    } else {
        ST7735_FillRect(40, 76, 80, 12, COLOR_DARK_GRAY);
        ST7735_DrawString(50, 78, "Exit", COLOR_GRAY, COLOR_DARK_GRAY);
    }
}

/*============================================================================
 * ECRAN INTRO ANIMAT - Stil Arcade DELUXE cu BMO Face
 *============================================================================*/

/* Culori pentru efecte rainbow */
static const uint16_t rainbow_colors[] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BLUE, COLOR_MAGENTA
};
#define NUM_RAINBOW_COLORS 7

/* Culori BMO */
#define BMO_SCREEN_COLOR  0x0400  /* Verde inchis pentru "ecran" BMO */
#define BMO_FACE_COLOR    COLOR_BLACK

/* Deseneaza fata lui BMO - MARE */
void DrawBMOFace(uint8_t expression, uint8_t blink) {
    /* Fundal verde tip BMO */
    ST7735_FillScreen(BMO_SCREEN_COLOR);

    /* Ochi MARI - doua patrate negre */
    /* Pozitii centrate pe ecran 160x128 */

    if (blink) {
        /* Ochi inchisi - linii orizontale groase */
        ST7735_FillRect(30, 40, 35, 5, BMO_FACE_COLOR);
        ST7735_FillRect(95, 40, 35, 5, BMO_FACE_COLOR);
    } else {
        /* Ochi deschisi - patrate mari */
        ST7735_FillRect(35, 25, 25, 25, BMO_FACE_COLOR);
        ST7735_FillRect(100, 25, 25, 25, BMO_FACE_COLOR);

        /* Stralucire in ochi (highlight) - mai mare */
        ST7735_FillRect(42, 30, 10, 10, COLOR_WHITE);
        ST7735_FillRect(107, 30, 10, 10, COLOR_WHITE);
    }

    /* Gura MARE - depinde de expresie */
    switch (expression) {
        case 0: /* Neutral - linie simpla groasa */
            ST7735_FillRect(55, 70, 50, 5, BMO_FACE_COLOR);
            break;
        case 1: /* Happy - zambet mare */
            ST7735_FillRect(50, 75, 60, 5, BMO_FACE_COLOR);
            ST7735_FillRect(42, 70, 12, 5, BMO_FACE_COLOR);
            ST7735_FillRect(106, 70, 12, 5, BMO_FACE_COLOR);
            ST7735_FillRect(36, 64, 10, 6, BMO_FACE_COLOR);
            ST7735_FillRect(114, 64, 10, 6, BMO_FACE_COLOR);
            break;
        case 2: /* Excited - gura deschisa mare */
            ST7735_FillRect(50, 65, 60, 25, BMO_FACE_COLOR);
            ST7735_FillRect(56, 72, 48, 14, BMO_SCREEN_COLOR); /* Interior gura */
            /* Limba mare */
            ST7735_FillRect(65, 78, 30, 6, COLOR_RED);
            break;
        case 3: /* Wink + smile */
            /* Redeseneaza ochiul stang (normal) */
            ST7735_FillRect(35, 25, 25, 25, BMO_FACE_COLOR);
            ST7735_FillRect(42, 30, 10, 10, COLOR_WHITE);
            /* Ochiul drept inchis */
            ST7735_FillRect(95, 40, 35, 5, BMO_FACE_COLOR);
            /* Zambet mare */
            ST7735_FillRect(50, 75, 60, 5, BMO_FACE_COLOR);
            ST7735_FillRect(42, 70, 12, 5, BMO_FACE_COLOR);
            ST7735_FillRect(106, 70, 12, 5, BMO_FACE_COLOR);
            break;
    }
}

void PlayIntroAnimation(void) {
    int16_t ball_x = 80, ball_y = 82;
    int16_t ball_dx = 2, ball_dy = 1;
    int16_t p1_y = 72, p2_y = 72;
    const uint8_t BALL_SZ = 5;
    const uint8_t PAD_W = 4, PAD_H = 18;
    const int16_t DEMO_TOP = 66;
    const int16_t DEMO_BOTTOM = 102;
    uint8_t color_phase = 0;
    uint32_t last_color_change = 0;
    uint32_t last_blink = 0;
    uint32_t anim_start;
    uint8_t blink_state = 0;

    /* ============================================================
     * FAZA BMO: Fata lui BMO care zice "Let's play PONG!"
     * ============================================================ */

    /* Ecran verde BMO */
    ST7735_FillScreen(BMO_SCREEN_COLOR);
    delay_ms(300);

    /* Deseneaza fata BMO - neutral */
    DrawBMOFace(0, 0);
    delay_ms(500);

    /* BMO clipeste */
    DrawBMOFace(0, 1);
    delay_ms(150);
    DrawBMOFace(0, 0);
    delay_ms(300);

    /* Text "Hi!" apare */
    ST7735_DrawStringCentered(100, "Hi!", COLOR_BLACK, BMO_SCREEN_COLOR, 2);
    delay_ms(600);

    /* BMO zambeste */
    DrawBMOFace(1, 0);
    delay_ms(400);

    /* Sterge "Hi!" si scrie "Let's play" */
    ST7735_FillRect(40, 95, 80, 25, BMO_SCREEN_COLOR);
    ST7735_DrawStringCentered(100, "Let's play", COLOR_BLACK, BMO_SCREEN_COLOR, 1);
    delay_ms(500);

    /* BMO excited - gura deschisa */
    DrawBMOFace(2, 0);

    /* Text mare "PONG!" */
    ST7735_DrawStringCentered(112, "PONG!", COLOR_YELLOW, BMO_SCREEN_COLOR, 2);
    delay_ms(800);

    /* BMO clipeste fericit */
    DrawBMOFace(2, 1);
    delay_ms(150);
    DrawBMOFace(2, 0);
    delay_ms(200);

    /* Wink */
    DrawBMOFace(3, 0);
    delay_ms(400);

    /* BMO ramane pe ecran putin mai mult */
    DrawBMOFace(1, 0);  /* Zambet */
    ST7735_DrawStringCentered(100, "Let's play", COLOR_BLACK, BMO_SCREEN_COLOR, 1);
    ST7735_DrawStringCentered(112, "PONG!", COLOR_YELLOW, BMO_SCREEN_COLOR, 2);
    delay_ms(1000);  /* +1 secunda */

    /* Flash tranzitie de la BMO la joc */
    for (int i = 0; i < 3; i++) {
        ST7735_FillScreen(COLOR_WHITE);
        delay_ms(50);
        ST7735_FillScreen(BMO_SCREEN_COLOR);
        DrawBMOFace(1, 0);
        ST7735_DrawStringCentered(100, "Let's play", COLOR_BLACK, BMO_SCREEN_COLOR, 1);
        ST7735_DrawStringCentered(112, "PONG!", COLOR_YELLOW, BMO_SCREEN_COLOR, 2);
        delay_ms(100);
    }

    delay_ms(200);

    /* ============================================================
     * FAZA PONG: Animatia jocului
     * ============================================================ */

    anim_start = Timer_GetMs();

    /* === FAZA 0: Efect de pornire - scanlines colorate === */
    for (int16_t y = 0; y < 128; y += 2) {
        uint16_t scan_color = rainbow_colors[y % NUM_RAINBOW_COLORS];
        ST7735_DrawHLine(0, y, 160, scan_color);
        delay_ms(8);
    }
    delay_ms(100);

    /* Flash rapid prin culori */
    ST7735_FillScreen(COLOR_CYAN);
    delay_ms(40);
    ST7735_FillScreen(COLOR_MAGENTA);
    delay_ms(40);
    ST7735_FillScreen(COLOR_YELLOW);
    delay_ms(40);
    ST7735_FillScreen(COLOR_WHITE);
    delay_ms(60);

    /* === FAZA 1: Background gradient animat === */
    /* Fundal inchis cu gradient subtil */
    for (int16_t y = 0; y < 128; y++) {
        uint8_t intensity = 10 + (y / 8);  /* Gradient subtil */
        uint16_t grad_color = ((intensity >> 3) << 11);  /* Nuanta rosiatica */
        ST7735_DrawHLine(0, y, 160, grad_color);
    }
    delay_ms(100);

    /* Bordura animata - creste din centru */
    for (int i = 0; i < 4; i++) {
        ST7735_DrawRect(78 - i*20, 62 - i*16, 4 + i*40, 4 + i*32, rainbow_colors[i % NUM_RAINBOW_COLORS]);
        delay_ms(60);
    }
    delay_ms(100);

    /* Curata centrul pentru titlu */
    ST7735_FillRect(6, 6, 148, 116, COLOR_BLACK);
    ST7735_DrawRect(4, 4, 152, 120, COLOR_WHITE);
    ST7735_DrawRect(6, 6, 148, 116, COLOR_GRAY);

    delay_ms(150);

    /* === FAZA 2: Titlu "PONG" cu efecte === */
    const char* letters = "PONG";
    int16_t letter_x[] = {26, 54, 82, 110};
    uint16_t letter_colors[] = {COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA, COLOR_GREEN};
    const uint8_t TITLE_SIZE = 3;

    /* Efect de "zoom in" - literele cresc */
    for (uint8_t size = 1; size <= TITLE_SIZE; size++) {
        for (int i = 0; i < 4; i++) {
            int16_t offset = (TITLE_SIZE - size) * 4;
            ST7735_DrawChar(letter_x[i] + offset, 15 + offset, letters[i],
                           letter_colors[i], COLOR_BLACK, size);
        }
        delay_ms(80);
        if (size < TITLE_SIZE) {
            ST7735_FillRect(20, 10, 120, 40, COLOR_BLACK);
        }
    }

    /* Flash rainbow pe fiecare litera */
    for (int flash = 0; flash < 3; flash++) {
        for (int i = 0; i < 4; i++) {
            ST7735_DrawChar(letter_x[i], 15, letters[i],
                           rainbow_colors[(flash + i) % NUM_RAINBOW_COLORS], COLOR_BLACK, TITLE_SIZE);
        }
        delay_ms(80);
    }
    /* Revino la culorile originale */
    for (int i = 0; i < 4; i++) {
        ST7735_DrawChar(letter_x[i], 15, letters[i], letter_colors[i], COLOR_BLACK, TITLE_SIZE);
    }

    /* Sublinie cu efect de stralucire */
    for (int16_t w = 0; w <= 70; w += 3) {
        ST7735_DrawHLine(80 - w, 42, w * 2, COLOR_WHITE);
        /* Efect de glow */
        if (w > 10) {
            ST7735_DrawHLine(80 - w + 5, 43, (w - 5) * 2, COLOR_GRAY);
        }
        delay_ms(6);
    }

    delay_ms(100);

    /* Subtitlu cu efect de typing */
    const char* subtitle = "ARCADE EDITION";
    char typing_buf[20] = "";
    for (int i = 0; subtitle[i] != '\0'; i++) {
        typing_buf[i] = subtitle[i];
        typing_buf[i+1] = '\0';
        ST7735_FillRect(30, 48, 100, 12, COLOR_BLACK);
        ST7735_DrawStringCentered(50, typing_buf, COLOR_ORANGE, COLOR_BLACK, 1);
        delay_ms(40);
    }

    /* Flash subtitlu */
    ST7735_DrawStringCentered(50, subtitle, COLOR_WHITE, COLOR_BLACK, 1);
    delay_ms(80);
    ST7735_DrawStringCentered(50, subtitle, COLOR_ORANGE, COLOR_BLACK, 1);

    delay_ms(200);

    /* === FAZA 3: Deseneaza zona de demo === */
    /* Separator decorativ */
    ST7735_DrawHLine(15, 62, 130, COLOR_DARK_GRAY);
    ST7735_DrawHLine(15, 105, 130, COLOR_DARK_GRAY);

    /* Palete initiale */
    ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_CYAN);
    ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_MAGENTA);

    /* "Press to Start" */
    ST7735_DrawStringCentered(112, "Press to Start", COLOR_WHITE, COLOR_BLACK, 1);

    /* Indicator jos */
    ST7735_DrawStringCentered(120, ">> PUSH <<", COLOR_GRAY, COLOR_BLACK, 1);

    PRINTF("Intro animation started - press joystick button to continue\r\n");

    /* === FAZA 4: Loop animatie demo === */
    g_joy_btn_pressed = false;
    uint8_t border_color_idx = 0;
    uint32_t last_border_update = 0;
    uint8_t title_glow = 0;
    uint32_t last_title_glow = 0;

    while (!g_joy_btn_pressed) {
        uint32_t now = Timer_GetMs();

        /* Sterge mingea veche */
        ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, COLOR_BLACK);

        /* Misca mingea */
        ball_x += ball_dx;
        ball_y += ball_dy;

        /* Coliziuni */
        if (ball_y <= DEMO_TOP || ball_y >= DEMO_BOTTOM - BALL_SZ) {
            ball_dy = -ball_dy;
        }
        if (ball_x <= 12 && ball_y + BALL_SZ >= p1_y && ball_y <= p1_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 13;
            /* Flash la coliziune */
            ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_WHITE);
        }
        if (ball_x >= 148 - BALL_SZ && ball_y + BALL_SZ >= p2_y && ball_y <= p2_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 147 - BALL_SZ;
            ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_WHITE);
        }

        /* Reset la iesire */
        if (ball_x < 5 || ball_x > 155) {
            ball_x = 80;
            ball_y = 82;
            ball_dx = (ball_dx > 0) ? -2 : 2;
        }

        /* AI palete */
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

        /* Mingea cu culoare care se schimba */
        uint16_t ball_color = rainbow_colors[(now / 200) % NUM_RAINBOW_COLORS];
        ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, ball_color);

        /* Bordura rainbow animata - la fiecare 150ms */
        if ((now - last_border_update) >= 150) {
            last_border_update = now;
            border_color_idx = (border_color_idx + 1) % NUM_RAINBOW_COLORS;
            ST7735_DrawRect(4, 4, 152, 120, rainbow_colors[border_color_idx]);
        }

        /* Titlu pulsating - TOATE literele isi schimba culoarea */
        if ((now - last_title_glow) >= 200) {
            last_title_glow = now;
            title_glow = (title_glow + 1) % NUM_RAINBOW_COLORS;

            /* Fiecare litera are un offset diferit in rainbow */
            for (int i = 0; i < 4; i++) {
                uint8_t color_idx = (title_glow + i * 2) % NUM_RAINBOW_COLORS;
                /* Alterneaza intre culoarea rainbow si gri pentru efect de pulsare */
                uint16_t letter_col;
                if ((title_glow / 2) % 2 == 0) {
                    letter_col = rainbow_colors[color_idx];
                } else {
                    /* La fiecare a doua faza, revino la culorile originale */
                    uint16_t orig_colors[] = {COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA, COLOR_GREEN};
                    letter_col = orig_colors[i];
                }
                ST7735_DrawChar(letter_x[i], 15, letters[i], letter_col, COLOR_BLACK, TITLE_SIZE);
            }
        }

        /* Blink "Press to Start" cu rainbow */
        if ((now - last_blink) >= 400) {
            last_blink = now;
            blink_state = (blink_state + 1) % 4;
            uint16_t blink_colors[] = {COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA};
            ST7735_DrawStringCentered(112, "Press to Start", blink_colors[blink_state], COLOR_BLACK, 1);
        }

        delay_ms(33);

        /* Timeout */
        if ((now - anim_start) > 60000) {
            PRINTF("Intro timeout - going to menu\r\n");
            break;
        }
    }

    /* === Tranzitie finala spectaculoasa === */
    PRINTF("Button pressed - transitioning to menu\r\n");

    /* Efect de "zoom out" - ecranul se micsoreaza */
    for (int i = 0; i < 5; i++) {
        ST7735_DrawRect(i * 15, i * 12, 160 - i * 30, 128 - i * 24, COLOR_WHITE);
        delay_ms(30);
    }

    /* Flash final prin culori */
    ST7735_FillScreen(COLOR_CYAN);
    delay_ms(30);
    ST7735_FillScreen(COLOR_WHITE);
    delay_ms(50);
    ST7735_FillScreen(COLOR_BLACK);
    delay_ms(80);

    g_joy_btn_pressed = false;
}

/* Functie principala de desenare bazata pe ecranul curent */
void DrawCurrentScreen(void) {
    switch (g_currentScreen) {
        case SCREEN_INTRO:
            /* Ruleaza animatia intro */
            PlayIntroAnimation();
            /* Trece la meniul principal */
            g_currentScreen = SCREEN_MAIN;
            g_menuState.selectedIndex = 0;
            g_needsRedraw = 1;
            DrawMainScreen();
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
            g_menuState.maxItems = 6;  /* Keyboard, Joystick, Remote, CPU Easy, CPU Hard, Back */
            DrawSelectP1Screen();
            break;
        case SCREEN_SELECT_P2:
            g_menuState.maxItems = 6;
            DrawSelectP2Screen();
            break;
        case SCREEN_DIFFICULTY:
            g_menuState.maxItems = 4;  /* Easy, Medium, Hard, Back */
            DrawDifficultyScreen();
            break;
        case SCREEN_GAMEPLAY:
            /* Nu redesenam in timpul jocului - doar update */
            break;
        case SCREEN_GAME_OVER:
            g_menuState.maxItems = 2;
            DrawGameOverScreen();
            break;
        case SCREEN_PAUSED:
            DrawPausedScreen();
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
                    /* Foloseste input-urile deja selectate */
                    /* Daca P2 e CPU, pune-l pe Remote pentru PvP */
                    if (IsCPUInput(g_player2_input)) {
                        g_player2_input = INPUT_REMOTE;
                    }
                    /* Daca ambii au acelasi input fizic, pune P2 pe Remote*/
                    if (g_player1_input == g_player2_input && !IsCPUInput(g_player1_input)) {
                        g_player2_input = INPUT_REMOTE;
                        if (g_player1_input == INPUT_KEYBOARD) {
                            g_player2_input = INPUT_JOYSTICK;
                        }
                    }
                    Game_Start();
                    break;
                case 1: /* Player vs CPU */
                    /* Mergem la ecranul de selectare dificultate */
                    /* P1 ramane cum e setat (default Joystick) */
                    ChangeScreen(SCREEN_DIFFICULTY);
                    break;
                case 2: /* Back */
                    ChangeScreen(SCREEN_MAIN);
                    break;
            }
            break;

        case SCREEN_DIFFICULTY:
            switch (g_menuState.selectedIndex) {
                case 0: /* Easy */
                    PRINTF("Starting vs CPU Easy!\r\n");
                    g_player2_input = INPUT_CPU_EASY;
                    Game_Start();
                    break;
                case 1: /* Medium */
                    PRINTF("Starting vs CPU Medium!\r\n");
                    g_player2_input = INPUT_CPU_MEDIUM;
                    Game_Start();
                    break;
                case 2: /* Hard */
                    PRINTF("Starting vs CPU Hard!\r\n");
                    g_player2_input = INPUT_CPU_HARD;
                    Game_Start();
                    break;
                case 3: /* Back */
                    ChangeScreen(SCREEN_START);
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
            if (g_menuState.selectedIndex == 5) {
                /* Back */
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                /* Mapare index la InputType_t */
                InputType_t inputMap[] = {INPUT_KEYBOARD, INPUT_JOYSTICK, INPUT_REMOTE,
                                          INPUT_CPU_EASY, INPUT_CPU_HARD};
                InputType_t selectedInput = inputMap[g_menuState.selectedIndex];

                PRINTF("[P1 SELECT] Trying: %s (P1 has: %s, P2 has: %s)\r\n",
                       GetInputName(selectedInput),
                       GetInputName(g_player1_input),
                       GetInputName(g_player2_input));

                if (IsInputAvailable(selectedInput, 1)) {
                    g_player1_input = selectedInput;
                    PRINTF("[P1 SELECT] OK! P1 now has: %s\r\n", GetInputName(g_player1_input));
                    /* Dupa P1, mergi automat la selectia P2 */
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 1;  /* Player 2 */
                    g_needsRedraw = 1;
                } else {
                    /* Input indisponibil - afiseaza mesaj arcade style */
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawRect(20, 105, 120, 20, COLOR_WHITE);
                    ST7735_DrawStringCentered(110, "USED BY P2!", COLOR_WHITE, COLOR_RED, 1);
                    PRINTF("[P1 SELECT] BLOCKED! %s used by P2\r\n", GetInputName(selectedInput));
                    delay_ms(800);
                    g_needsRedraw = 1;
                }
            }
            break;

        case SCREEN_SELECT_P2:
            if (g_menuState.selectedIndex == 5) {
                /* Back */
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                InputType_t inputMap[] = {INPUT_KEYBOARD, INPUT_JOYSTICK, INPUT_REMOTE,
                                          INPUT_CPU_EASY, INPUT_CPU_HARD};
                InputType_t selectedInput = inputMap[g_menuState.selectedIndex];

                PRINTF("[P2 SELECT] Trying: %s (P1 has: %s, P2 has: %s)\r\n",
                       GetInputName(selectedInput),
                       GetInputName(g_player1_input),
                       GetInputName(g_player2_input));

                if (IsInputAvailable(selectedInput, 2)) {
                    g_player2_input = selectedInput;
                    PRINTF("[P2 SELECT] OK! P2 now has: %s\r\n", GetInputName(g_player2_input));
                    /* Dupa P2, mergi automat la Back */
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 2;  /* Back */
                    g_needsRedraw = 1;
                } else {
                    /* Input indisponibil - afiseaza mesaj arcade style */
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawRect(20, 105, 120, 20, COLOR_WHITE);
                    ST7735_DrawStringCentered(110, "USED BY P1!", COLOR_WHITE, COLOR_RED, 1);
                    PRINTF("[P2 SELECT] BLOCKED! %s used by P1\r\n", GetInputName(selectedInput));
                    delay_ms(800);
                    g_needsRedraw = 1;
                }
            }
            break;

        case SCREEN_GAME_OVER:
            if (g_menuState.selectedIndex == 0) {
                /* Play Again */
                Game_Start();
            } else {
                /* Main Menu */
                ChangeScreen(SCREEN_MAIN);
            }
            break;

        case SCREEN_GAMEPLAY:
        case SCREEN_PAUSED:
            /* Nu facem nimic - controlat in alta parte */
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

/* Proceseaza input de la tastatura - pentru MENIU */
void ProcessMenuKeyboardInput(char key) {
    switch (key) {
        case 'w':
        case 'W':
        case '8': /* Sus */
            Menu_MoveUp();
            break;

        case 's':
        case 'S':
        case '2': /* Jos */
            Menu_MoveDown();
            break;

        case ' ':     /* Space */
        case '\r':    /* Enter */
        case '\n':
        case '5':     /* Select */
        case 'e':
        case 'E':
            Menu_Select();
            break;

        case 'b':
        case 'B':
        case '0': /* Back */
            g_menuState.selectedIndex = g_menuState.maxItems - 1;
            g_needsRedraw = 1;
            Menu_Select();
            break;

        case '1':
            g_menuState.selectedIndex = 0;
            g_needsRedraw = 1;
            Menu_Select();
            break;

        case '3':
            if (g_menuState.maxItems > 1) {
                g_menuState.selectedIndex = 1;
                g_needsRedraw = 1;
                Menu_Select();
            }
            break;

        case 'r':
        case 'R':
            ChangeScreen(SCREEN_MAIN);
            break;

        case 'h':
        case 'H':
        case '?':
            PRINTF("\r\n=== CONTROLS ===\r\n");
            PRINTF("MENU: W/S=Navigate, Enter=Select, B=Back\r\n");
            PRINTF("GAME: W/S=P1, I/K=P2, P=Pause, Q=Quit\r\n");
            PRINTF("================\r\n\r\n");
            break;
    }
}

/* Proceseaza input de la tastatura - pentru JOC */
void ProcessGameKeyboardInput(char key) {
    switch (key) {
        /* Player 1 controls: W/S */
        case 'w':
        case 'W':
            if (paddle1.input == INPUT_KEYBOARD) {
                g_p1_move = -1; /* Sus */
            }
            break;
        case 's':
        case 'S':
            if (paddle1.input == INPUT_KEYBOARD) {
                g_p1_move = 1; /* Jos */
            }
            break;

        /* Player 2 controls: I/K (sau sageti pe numpad 8/2) */
        case 'i':
        case 'I':
        case '8':
            if (paddle2.input == INPUT_KEYBOARD) {
                g_p2_move = -1;
            }
            break;
        case 'k':
        case 'K':
        case '2':
            if (paddle2.input == INPUT_KEYBOARD) {
                g_p2_move = 1;
            }
            break;

        /* Pauza */
        case 'p':
        case 'P':
            if (g_currentScreen == SCREEN_PAUSED) {
                /* Resume */
                g_currentScreen = SCREEN_GAMEPLAY;
                game.is_paused = 0;
                /* Redeseneaza terenul */
                Game_DrawField();
                Game_DrawScore();
                PRINTF("[GAME] Resumed\r\n");
            } else {
                /* Pause */
                g_currentScreen = SCREEN_PAUSED;
                game.is_paused = 1;
                g_needsRedraw = 1;
                PRINTF("[GAME] Paused\r\n");
            }
            break;

        /* Quit - inapoi la meniu */
        case 'q':
        case 'Q':
        case 27: /* ESC */
            game.is_running = 0;
            ChangeScreen(SCREEN_MAIN);
            PRINTF("[GAME] Quit to menu\r\n");
            break;
    }
}

/* Proceseaza input de la tastatura */
void ProcessKeyboardInput(void) {
    char key = ReadKeyboard();

    if (key == 0) return;

    /* Diferentiem intre joc si meniu */
    if (g_currentScreen == SCREEN_GAMEPLAY || g_currentScreen == SCREEN_PAUSED) {
        ProcessGameKeyboardInput(key);
    } else {
        ProcessMenuKeyboardInput(key);
    }
}

/* Proceseaza input de la joystick pentru meniu */
void ProcessJoystickMenuInput(void) {
    JoyAction_t action = Joystick_GetMenuAction();

    switch (action) {
        case JOY_ACTION_UP:
            Menu_MoveUp();
            break;
        case JOY_ACTION_DOWN:
            Menu_MoveDown();
            break;
        case JOY_ACTION_SELECT:
            Menu_Select();
            break;
        default:
            break;
    }
}

/* Proceseaza input de la joystick in timpul jocului */
void ProcessJoystickGameInput(void) {
    if (g_currentScreen == SCREEN_PAUSED) {
        /* In meniul de pauza - navigare cu joystick */
        Joystick_Process();
        int16_t joy_y = Joystick_GetY_Percent();

        /* Navigare sus/jos in meniu pauza */
        static bool pause_nav_consumed = false;
        if (joy_y > 50 && !pause_nav_consumed) {
            /* Jos */
            g_pause_selection = 1;
            pause_nav_consumed = true;
            g_needsRedraw = 1;
        } else if (joy_y < -50 && !pause_nav_consumed) {
            /* Sus */
            g_pause_selection = 0;
            pause_nav_consumed = true;
            g_needsRedraw = 1;
        } else if (joy_y > -30 && joy_y < 30) {
            pause_nav_consumed = false;
        }

        /* Butonul = selecteaza optiunea */
        if (g_joy_btn_pressed) {
            g_joy_btn_pressed = false;

            if (g_pause_selection == 0) {
                /* Resume */
                g_currentScreen = SCREEN_GAMEPLAY;
                game.is_paused = 0;
                Game_DrawField();
                Game_DrawScore();
                PRINTF("[GAME] Resumed\r\n");
            } else {
                /* Exit - inapoi la meniu principal */
                game.is_running = 0;
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 0;
                g_needsRedraw = 1;
                PRINTF("[GAME] Exited to menu\r\n");
            }
        }
    } else if (g_currentScreen == SCREEN_GAMEPLAY) {
        /* In joc - butonul = pauza */
        if (g_joy_btn_pressed) {
            g_joy_btn_pressed = false;
            g_currentScreen = SCREEN_PAUSED;
            game.is_paused = 1;
            g_pause_selection = 0;  /* Reset selectie la Resume */
            g_needsRedraw = 1;
            PRINTF("[GAME] Paused\r\n");
        }
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

    /* Initializeaza timer-ul SysTick */
    Timer_Init();

    /* Seed pentru random */
    srand(g_systick_ms ^ 0xDEADBEEF);

    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("     PONG GAME - FRDM-KL25Z + ST7735   \r\n");
    PRINTF("     Cu Joystick si AI Bot             \r\n");
    PRINTF("========================================\r\n\r\n");

    PRINTF("=== PINOUT ===\r\n");
    PRINTF("DISPLAY (ST7735):\r\n");
    PRINTF("  BL  -> 3.3V    VCC -> 3.3V\r\n");
    PRINTF("  GND -> GND     SCK -> PTC5\r\n");
    PRINTF("  SDA -> PTC6    DC  -> PTC3\r\n");
    PRINTF("  RES -> PTC0    CS  -> PTC4\r\n");
    PRINTF("\r\n");
    PRINTF("JOYSTICK:\r\n");
    PRINTF("  VRY -> PTB1 (ADC0_SE9)\r\n");
    PRINTF("  SW  -> PTD4\r\n");
    PRINTF("  VCC -> 3.3V   GND -> GND\r\n");
    PRINTF("==============\r\n\r\n");

    /* Initializare display */
    PRINTF("Initializing display...\r\n");
    ST7735_Init();
    PRINTF("Display OK!\r\n");

    /* Initializare joystick */
    PRINTF("Initializing joystick...\r\n");
    Joystick_Init();
    PRINTF("Joystick OK!\r\n\r\n");

    PRINTF("=== CONTROLS ===\r\n");
    PRINTF("MENIU: Joystick Sus/Jos + Buton=Select\r\n");
    PRINTF("JOC:   Joystick = Miscare paleta\r\n");
    PRINTF("       Buton = Pauza\r\n");
    PRINTF("================\r\n\r\n");

    /* Deseneaza ecranul initial */
    DrawCurrentScreen();

    PRINTF(">>> Ready! Use joystick to navigate <<<\r\n\r\n");

    /* Variabile pentru timing */
    uint32_t last_game_update = 0;
    uint32_t last_menu_update = 0;
    const uint32_t GAME_FRAME_MS = 20;   /* ~50 FPS pentru joc */
    const uint32_t MENU_FRAME_MS = 50;   /* ~20 FPS pentru meniu */

    while (1) {
        uint32_t now = Timer_GetMs();

        /* Proceseaza joystick-ul (citeste ADC) - mereu */
        Joystick_Process();

        /* Citeste input de la tastatura (optional, pentru debug) - mereu */
        ProcessKeyboardInput();

        /* In modul joc - actualizeaza la frame rate fix */
        if (g_currentScreen == SCREEN_GAMEPLAY) {
            /* Proceseaza input joystick pentru pauza */
            ProcessJoystickGameInput();

            /* Update joc doar la interval fix (non-blocant) */
            if ((now - last_game_update) >= GAME_FRAME_MS) {
                last_game_update = now;

                /* Actualizeaza jocul */
                Game_Update();

                /* Verifica daca jocul s-a terminat */
                if (!game.is_running && game.winner != 0) {
                    PRINTF("\r\n=== GAME OVER ===\r\n");
                    PRINTF("Winner: Player %d\r\n", game.winner);
                    PRINTF("Score: %d - %d\r\n\r\n", paddle1.score, paddle2.score);

                    g_currentScreen = SCREEN_GAME_OVER;
                    g_menuState.selectedIndex = 0;
                    g_needsRedraw = 1;
                }

                /* Reset input tastatura dupa fiecare frame */
                g_p1_move = 0;
                g_p2_move = 0;
            }

        } else if (g_currentScreen == SCREEN_PAUSED) {
            /* In pauza - doar verificam butonul pentru resume */
            ProcessJoystickGameInput();

            if (g_needsRedraw) {
                DrawCurrentScreen();
            }

        } else {
            /* In meniu - navigare cu joystick */
            if ((now - last_menu_update) >= MENU_FRAME_MS) {
                last_menu_update = now;
                ProcessJoystickMenuInput();
            }

            /* Redeseneaza meniul daca e nevoie */
            if (g_needsRedraw) {
                DrawCurrentScreen();
            }
        }

        /* Economiseste energie cand nu e nimic de facut */
        __WFI();
    }

    return 0;
}
