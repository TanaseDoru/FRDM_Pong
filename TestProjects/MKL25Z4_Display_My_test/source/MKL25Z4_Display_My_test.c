/*
 * MKL25Z4_Display_Test.c
 * Sistem de Meniu + Joc Pong Complet
 * ST7735 LCD + FRDM-KL25Z
 * Cu suport Joystick, IR Remote, si CPU Bot
 *
 * MODIFICAT: IR functioneaza in joc + Timere UI debug
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

typedef enum {
    INPUT_NONE = 0,
    INPUT_JOYSTICK,
    INPUT_REMOTE,
    INPUT_GYROSCOPE,
    INPUT_CPU_EASY,
    INPUT_CPU_MEDIUM,
    INPUT_CPU_HARD
} InputType_t;

typedef enum {
    SCREEN_INTRO = 0,
    SCREEN_MAIN,
    SCREEN_START,
    SCREEN_SELECT_INPUT,
    SCREEN_SELECT_P1,
    SCREEN_SELECT_P2,
    SCREEN_DIFFICULTY,
    SCREEN_GAMEPLAY,
    SCREEN_GAME_OVER,
    SCREEN_PAUSED
} Screen_t;

/*============================================================================
 * STRUCTURI PENTRU JOC
 *============================================================================*/

typedef struct {
    int16_t x, y;
    int16_t dx, dy;
    int16_t prev_x, prev_y;
    uint8_t size;
} Ball_t;

typedef struct {
    int16_t y;
    int16_t prev_y;
    int16_t score;
    InputType_t input;
    int16_t target_y;
} Paddle_t;

typedef struct {
    uint8_t is_running;
    uint8_t is_paused;
    uint8_t winner;
    uint8_t winning_score;
    uint16_t frame_count;
    uint16_t rally_frames;
    uint8_t speed_level;
} GameState_t;

typedef struct {
    uint8_t selectedIndex;
    uint8_t maxItems;
} MenuState_t;

/*============================================================================
 * STRUCTURA PENTRU TIMERE UI DEBUG
 *============================================================================*/

typedef struct {
    uint32_t last_ball_draw_time;
    uint32_t last_paddle_draw_time;
    uint32_t last_score_draw_time;
    uint32_t last_field_draw_time;
    uint32_t last_menu_draw_time;
    uint32_t last_game_update_time;
    uint32_t ball_draw_count;
    uint32_t paddle_draw_count;
    uint32_t frame_count;
    uint32_t last_fps_print;
} UITimers_t;

static UITimers_t g_ui_timers = {0};

/*============================================================================
 * VARIABILE GLOBALE
 *============================================================================*/

InputType_t g_player1_input = INPUT_JOYSTICK;
InputType_t g_player2_input = INPUT_CPU_MEDIUM;

Screen_t g_currentScreen = SCREEN_INTRO;
MenuState_t g_menuState = {0, 3};

volatile uint8_t g_needsRedraw = 1;

static Ball_t ball;
static Paddle_t paddle1, paddle2;
static GameState_t game;

static int8_t g_p1_move = 0;
static int8_t g_p2_move = 0;

/*============================================================================
 * CONSTANTE PENTRU JOC
 *============================================================================*/

#define FIELD_WIDTH      160
#define FIELD_HEIGHT     128
#define PADDLE_WIDTH     4
#define PADDLE_HEIGHT    22
#define PADDLE_X_P1      4
#define PADDLE_X_P2      152
#define BALL_SIZE        4
#define BALL_START_X     80
#define BALL_START_Y     64
#define PADDLE_START_Y   53
#define SCORE_TO_WIN     5
#define SCORE_Y          2

#define PADDLE_SPEED     4
#define BALL_SPEED_X     1
#define BALL_SPEED_Y     1

#define SPEED_UP_INTERVAL   500
#define MAX_SPEED_LEVEL     5

#define PADDLE_MIN_Y     2
#define PADDLE_MAX_Y     (FIELD_HEIGHT - PADDLE_HEIGHT - 2)

/*============================================================================
 * CONFIGURARE JOYSTICK (ADC)
 *============================================================================*/

#define JOYSTICK_VRX_CHANNEL 8U
#define JOYSTICK_VRY_CHANNEL 9U

#define JOYSTICK_SW_PIN      4U
#define JOYSTICK_SW_PORT     PORTD
#define JOYSTICK_SW_GPIO     GPIOD
#define JOYSTICK_SW_IRQ      PORTD_IRQn

static volatile bool g_joy_btn_pressed = false;
static int16_t g_joy_y_percent = 0;
static bool g_joy_action_consumed = false;

/*============================================================================
 * CONFIGURARE IR REMOTE
 *============================================================================*/

#define IR_PIN 12u
#define IR_GPIO GPIOA
#define IR_PORT PORTA
#define IR_IRQ  PORTA_IRQn

#define TICKS_US_FACTOR      1.5f
#define NEC_HDR_MARK_MIN     12000
#define NEC_HDR_SPACE_MIN    6000
#define NEC_BIT_THRESHOLD    2500
#define GLITCH_THRESHOLD     100

volatile uint32_t ir_code = 0;
volatile uint8_t ir_ready = 0;
volatile uint32_t pulse_count = 0;
volatile uint32_t pulse_buffer[100];
volatile uint8_t capture_complete = 0;
volatile uint32_t last_ir_code = 0;
volatile uint32_t last_ir_time = 0;

/* === NOU: Stare IR pentru gameplay === */
static int8_t g_ir_current_direction = 0;
static uint32_t g_ir_last_input_time = 0;
#define IR_INPUT_TIMEOUT_MS  150

#define IR_DEBOUNCE_MS 200
#define IR_GAME_DEBOUNCE_MS 80

#define IR_KEY_UP       0xE718FF00
#define IR_KEY_DOWN     0xAD52FF00
#define IR_KEY_LEFT     0xF708FF00
#define IR_KEY_RIGHT    0xA55AFF00
#define IR_KEY_OK       0xE31CFF00
#define IR_KEY_POWER    0xBA45FF00
#define IR_KEY_BACK     0xF807FF00

/*============================================================================
 * CONSTANTE PENTRU UI
 *============================================================================*/

#define MENU_START_Y      35
#define MENU_ITEM_HEIGHT  14
#define MENU_MARGIN_X     10
#define TITLE_Y           8

#define COLOR_BG          COLOR_BLACK
#define COLOR_TITLE       COLOR_CYAN
#define COLOR_NORMAL      COLOR_WHITE
#define COLOR_SELECTED    COLOR_YELLOW
#define COLOR_DISABLED    COLOR_GRAY
#define COLOR_HIGHLIGHT   COLOR_GREEN
#define COLOR_BACK        COLOR_ORANGE

/*============================================================================
 * SYSTICK TIMER
 *============================================================================*/

static volatile uint32_t g_systick_ms = 0;

void SysTick_Handler(void) {
    g_systick_ms++;
}

void Timer_Init(void) {
    SysTick_Config(SystemCoreClock / 1000U);
    PRINTF("Timer initialized (SysTick 1ms)\r\n");
}

uint32_t Timer_GetMs(void) {
    return g_systick_ms;
}

void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __WFI();
    }
}

bool Timer_Elapsed(uint32_t start_time, uint32_t duration_ms) {
    return ((g_systick_ms - start_time) >= duration_ms);
}

/*============================================================================
 * FUNCTII HELPER
 *============================================================================*/

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

bool IsCPUInput(InputType_t input) {
    return (input == INPUT_CPU_EASY || input == INPUT_CPU_MEDIUM || input == INPUT_CPU_HARD);
}

bool IsInputAvailable(InputType_t input, uint8_t forPlayer) {
    if (input == INPUT_NONE) return true;
    if (IsCPUInput(input)) return true;

    if (forPlayer == 1) {
        if (g_player2_input == input) return false;
    } else {
        if (g_player1_input == input) return false;
    }
    return true;
}

void UI_PrintStats(void) {
    uint32_t now = Timer_GetMs();
    if ((now - g_ui_timers.last_fps_print) >= 5000) {
        g_ui_timers.last_fps_print = now;

        if (g_currentScreen == SCREEN_GAMEPLAY) {
            float fps = (float)g_ui_timers.frame_count / 5.0f;
            PRINTF("[UI STATS] FPS: %.1f | Ball draws: %lu | Paddle draws: %lu\r\n",
                   fps, g_ui_timers.ball_draw_count, g_ui_timers.paddle_draw_count);
            PRINTF("[UI TIMERS] Ball: %lu ms | Paddle: %lu ms | Update: %lu ms\r\n",
                   g_ui_timers.last_ball_draw_time, g_ui_timers.last_paddle_draw_time,
                   g_ui_timers.last_game_update_time);
        }

        g_ui_timers.frame_count = 0;
        g_ui_timers.ball_draw_count = 0;
        g_ui_timers.paddle_draw_count = 0;
    }
}

/*============================================================================
 * FUNCTII JOYSTICK
 *============================================================================*/

void PORTD_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);
    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);
        g_joy_btn_pressed = true;
    }
}

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

void Joystick_Init(void) {
    adc16_config_t adcConfig;

    CLOCK_EnableClock(kCLOCK_Adc0);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortD);

    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.resolution = kADC16_ResolutionSE12Bit;
    ADC16_Init(ADC0, &adcConfig);
    ADC16_DoAutoCalibration(ADC0);

    PORT_SetPinMux(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_MuxAsGpio);
    JOYSTICK_SW_PORT->PCR[JOYSTICK_SW_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    gpio_pin_config_t sw_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(JOYSTICK_SW_GPIO, JOYSTICK_SW_PIN, &sw_config);

    PORT_SetPinInterruptConfig(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_InterruptFallingEdge);

    NVIC_SetPriority(JOYSTICK_SW_IRQ, 3);
    EnableIRQ(JOYSTICK_SW_IRQ);

    PRINTF("Joystick initialized (VRY=PTB1, SW=PTD4)\r\n");
}

void Joystick_Process(void) {
    uint16_t y_raw = Joystick_ReadADC(JOYSTICK_VRY_CHANNEL);
    g_joy_y_percent = ((int32_t)y_raw - 2048) * 100 / 2048;

    if (g_joy_y_percent > -20 && g_joy_y_percent < 20) {
        g_joy_action_consumed = false;
    }
}

typedef enum {
    JOY_ACTION_NONE = 0,
    JOY_ACTION_UP,
    JOY_ACTION_DOWN,
    JOY_ACTION_SELECT
} JoyAction_t;

JoyAction_t Joystick_GetMenuAction(void) {
    if (g_joy_btn_pressed) {
        g_joy_btn_pressed = false;
        return JOY_ACTION_SELECT;
    }

    if (!g_joy_action_consumed) {
        if (g_joy_y_percent > 50) {
            g_joy_action_consumed = true;
            return JOY_ACTION_DOWN;
        }
        if (g_joy_y_percent < -50) {
            g_joy_action_consumed = true;
            return JOY_ACTION_UP;
        }
    }
    return JOY_ACTION_NONE;
}

int16_t Joystick_GetY_Percent(void) {
    return g_joy_y_percent;
}

/*============================================================================
 * FUNCTII IR REMOTE
 *============================================================================*/

void TPM0_IRQHandler(void) {
    if (TPM0->SC & TPM_SC_TOF_MASK) {
        TPM0->SC |= TPM_SC_TOF_MASK;

        if (pulse_count >= 66) {
            capture_complete = 1;
            ir_ready = 1;
        } else if (pulse_count >= 2 && pulse_count <= 4) {
            capture_complete = 1;
            ir_ready = 1;
        } else {
            pulse_count = 0;
            capture_complete = 0;
        }
    }
}

void PORTA_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(IR_PORT);

    if (isfr & (1U << IR_PIN)) {
        PORT_ClearPinsInterruptFlags(IR_PORT, 1U << IR_PIN);

        if (!capture_complete) {
            uint32_t current_val = TPM0->CNT;
            TPM0->CNT = 0;

            if (current_val > GLITCH_THRESHOLD) {
                if (pulse_count < 100) {
                    pulse_buffer[pulse_count++] = current_val;
                }
            }
        }
    }
}

void IR_DecodeNEC(void) {
    ir_code = 0;

    if (pulse_count >= 2 && pulse_count <= 4) {
        if (pulse_buffer[0] > 12000 && pulse_buffer[0] < 15000) {
            if (pulse_count >= 2 && pulse_buffer[1] > 2000 && pulse_buffer[1] < 4500) {
                ir_code = last_ir_code;
                return;
            }
        }
    }

    int start_index = -1;
    if (pulse_buffer[0] > 12000 && pulse_buffer[0] < 15000) start_index = 0;
    else if (pulse_count > 1 && pulse_buffer[1] > 12000 && pulse_buffer[1] < 15000) start_index = 1;

    if (start_index == -1) return;

    uint32_t hdr_space = pulse_buffer[start_index + 1];

    if (hdr_space < 4000) {
        ir_code = last_ir_code;
        return;
    }

    for (int i = 0; i < 32; i++) {
        int idx = start_index + 3 + (i * 2);
        if (idx >= (int)pulse_count) break;

        uint32_t space_duration = pulse_buffer[idx];
        if (space_duration > 1600) {
            ir_code |= (1UL << i);
        }
    }

    if (ir_code != 0) {
        last_ir_code = ir_code;
    }
}

void IR_Init(void) {
    CLOCK_EnableClock(kCLOCK_PortA);
    PORT_SetPinMux(IR_PORT, IR_PIN, kPORT_MuxAsGpio);

    IR_PORT->PCR[IR_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    gpio_pin_config_t gpioPinConfig = { kGPIO_DigitalInput, 0 };
    GPIO_PinInit(IR_GPIO, IR_PIN, &gpioPinConfig);

    PORT_SetPinInterruptConfig(IR_PORT, IR_PIN, kPORT_InterruptEitherEdge);

    NVIC_SetPriority(IR_IRQ, 2);
    EnableIRQ(IR_IRQ);

    PRINTF("IR Remote initialized (Pin=PTA12)\r\n");
}

void TPM0_Init(void) {
    CLOCK_EnableClock(kCLOCK_Tpm0);
    CLOCK_SetTpmClock(1U);

    TPM0->SC = 0;
    TPM0->CNT = 0;
    TPM0->MOD = 0xFFFF;

    TPM0->SC = TPM_SC_PS(5) | TPM_SC_TOIE_MASK | TPM_SC_CMOD(1);

    NVIC_SetPriority(TPM0_IRQn, 3);
    EnableIRQ(TPM0_IRQn);
}

JoyAction_t IR_GetMenuAction(void) {
    if (!ir_ready) return JOY_ACTION_NONE;

    uint32_t now = Timer_GetMs();
    if ((now - last_ir_time) < IR_DEBOUNCE_MS) {
        ir_ready = 0;
        capture_complete = 0;
        pulse_count = 0;
        return JOY_ACTION_NONE;
    }

    IR_DecodeNEC();

    JoyAction_t action = JOY_ACTION_NONE;

    if (ir_code != 0) {
        last_ir_time = now;

        switch (ir_code) {
            case IR_KEY_UP:    action = JOY_ACTION_UP; break;
            case IR_KEY_DOWN:  action = JOY_ACTION_DOWN; break;
            case IR_KEY_OK:    action = JOY_ACTION_SELECT; break;
            default: break;
        }

        PRINTF("[IR MENU] Code: 0x%08X -> Action: %d\r\n", ir_code, action);
    }

    ir_ready = 0;
    capture_complete = 0;
    pulse_count = 0;

    return action;
}

/* === NOU: Proceseaza IR pentru gameplay === */
void IR_ProcessGameInput(void) {
    uint32_t now = Timer_GetMs();

    /* Timeout - opreste miscarea daca nu vine semnal */
    if ((now - g_ir_last_input_time) > IR_INPUT_TIMEOUT_MS) {
        g_ir_current_direction = 0;
    }

    if (!ir_ready) return;

    if ((now - last_ir_time) < IR_GAME_DEBOUNCE_MS) {
        ir_ready = 0;
        capture_complete = 0;
        pulse_count = 0;
        return;
    }

    IR_DecodeNEC();

    if (ir_code != 0) {
        last_ir_time = now;
        g_ir_last_input_time = now;

        switch (ir_code) {
            case IR_KEY_UP:
                g_ir_current_direction = -1;
                PRINTF("[IR GAME] UP\r\n");
                break;
            case IR_KEY_DOWN:
                g_ir_current_direction = 1;
                PRINTF("[IR GAME] DOWN\r\n");
                break;
            case IR_KEY_OK:
            case IR_KEY_POWER:
                break;
            default:
                g_ir_current_direction = 0;
                break;
        }
    }

    ir_ready = 0;
    capture_complete = 0;
    pulse_count = 0;
}

int8_t IR_GetCurrentDirection(void) {
    return g_ir_current_direction;
}

void IR_ResetGameState(void) {
    g_ir_current_direction = 0;
    g_ir_last_input_time = 0;
    ir_ready = 0;
    capture_complete = 0;
    pulse_count = 0;
}

bool IR_CheckPauseOnly(void) {
    if (!ir_ready) return false;

    uint32_t now = Timer_GetMs();
    if ((now - last_ir_time) < IR_DEBOUNCE_MS) return false;

    IR_DecodeNEC();

    if (ir_code == IR_KEY_OK || ir_code == IR_KEY_POWER) {
        last_ir_time = now;
        ir_ready = 0;
        capture_complete = 0;
        pulse_count = 0;
        PRINTF("[IR] Pause detected\r\n");
        return true;
    }

    return false;
}

bool IR_IsPausePressed(void) {
    if (!ir_ready) return false;

    uint32_t now = Timer_GetMs();
    if ((now - last_ir_time) < IR_DEBOUNCE_MS) {
        ir_ready = 0;
        capture_complete = 0;
        pulse_count = 0;
        return false;
    }

    IR_DecodeNEC();

    bool pause_pressed = false;

    if (ir_code == IR_KEY_OK || ir_code == IR_KEY_POWER) {
        pause_pressed = true;
        last_ir_time = now;
    }

    ir_ready = 0;
    capture_complete = 0;
    pulse_count = 0;

    return pause_pressed;
}

/*============================================================================
 * FUNCTII AI / BOT
 *============================================================================*/

int16_t AI_PredictBallY(int16_t target_x) {
    int16_t sim_x = ball.x;
    int16_t sim_y = ball.y;
    int16_t sim_dx = ball.dx;
    int16_t sim_dy = ball.dy;

    int iterations = 0;
    while (iterations < 200) {
        sim_x += sim_dx;
        sim_y += sim_dy;

        if (sim_y <= 2 || sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) {
            sim_dy = -sim_dy;
            if (sim_y <= 2) sim_y = 3;
            if (sim_y >= FIELD_HEIGHT - BALL_SIZE - 2) sim_y = FIELD_HEIGHT - BALL_SIZE - 3;
        }

        if ((sim_dx > 0 && sim_x >= target_x) || (sim_dx < 0 && sim_x <= target_x)) {
            return sim_y;
        }

        iterations++;
    }

    return FIELD_HEIGHT / 2;
}

void AI_UpdatePaddle(Paddle_t* paddle, bool is_right_side) {
    int16_t paddle_center = paddle->y + PADDLE_HEIGHT / 2;
    int16_t target_y;
    int16_t speed;
    int16_t reaction_zone;
    int16_t error_margin;
    int16_t mistake_chance;
    int16_t update_interval;

    switch (paddle->input) {
        case INPUT_CPU_EASY:
            speed = 1; reaction_zone = 35; error_margin = 35;
            mistake_chance = 35; update_interval = 20;
            break;
        case INPUT_CPU_MEDIUM:
            speed = 2; reaction_zone = 80; error_margin = 15;
            mistake_chance = 12; update_interval = 10;
            break;
        case INPUT_CPU_HARD:
            speed = 4; reaction_zone = 160; error_margin = 5;
            mistake_chance = 0; update_interval = 3;
            break;
        default:
            return;
    }

    bool ball_coming = (is_right_side && ball.dx > 0) || (!is_right_side && ball.dx < 0);

    if (ball_coming) {
        if (game.frame_count % update_interval == 0) {
            int16_t target_x = is_right_side ? PADDLE_X_P2 : PADDLE_X_P1 + PADDLE_WIDTH;
            paddle->target_y = AI_PredictBallY(target_x);

            if (error_margin > 0) {
                paddle->target_y += (rand() % (error_margin * 2)) - error_margin;
            }
        }
        target_y = paddle->target_y;
    } else {
        target_y = FIELD_HEIGHT / 2;
        speed = 1;
    }

    int16_t ball_dist = is_right_side ? (PADDLE_X_P2 - ball.x) : (ball.x - PADDLE_X_P1);

    if (ball_dist < reaction_zone || !ball_coming) {
        bool make_mistake = (mistake_chance > 0) && ((rand() % 100) < mistake_chance);

        if (make_mistake) {
            int mistake_type = rand() % 3;
            if (mistake_type == 0) paddle->y -= speed;
            else if (mistake_type == 1) paddle->y += speed;
        } else {
            if (paddle_center < target_y - 5) paddle->y += speed;
            else if (paddle_center > target_y + 5) paddle->y -= speed;
        }
    }

    if (paddle->y < PADDLE_MIN_Y) paddle->y = PADDLE_MIN_Y;
    if (paddle->y > PADDLE_MAX_Y) paddle->y = PADDLE_MAX_Y;
}

/*============================================================================
 * FUNCTII JOC PONG
 *============================================================================*/

void Game_Init(void) {
    ball.x = BALL_START_X;
    ball.y = BALL_START_Y;
    ball.dx = BALL_SPEED_X;
    ball.dy = BALL_SPEED_Y;
    ball.prev_x = ball.x;
    ball.prev_y = ball.y;
    ball.size = BALL_SIZE;

    if (rand() % 2) ball.dx = -ball.dx;
    if (rand() % 2) ball.dy = -ball.dy;

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

    game.is_running = 1;
    game.is_paused = 0;
    game.winner = 0;
    game.winning_score = SCORE_TO_WIN;
    game.frame_count = 0;
    game.rally_frames = 0;
    game.speed_level = 0;

    g_p1_move = 0;
    g_p2_move = 0;

    IR_ResetGameState();

    memset(&g_ui_timers, 0, sizeof(g_ui_timers));
    g_ui_timers.last_fps_print = Timer_GetMs();
}

void Game_DrawField(void) {
    uint32_t start = Timer_GetMs();

    ST7735_FillScreen(COLOR_BLACK);

    for (int16_t y = 4; y < FIELD_HEIGHT - 4; y += 8) {
        ST7735_FillRect(79, y, 2, 4, COLOR_DARK_GRAY);
    }

    ST7735_DrawHLine(0, 0, FIELD_WIDTH, COLOR_WHITE);
    ST7735_DrawHLine(0, FIELD_HEIGHT - 1, FIELD_WIDTH, COLOR_WHITE);

    g_ui_timers.last_field_draw_time = Timer_GetMs() - start;
}

void Game_DrawScore(void) {
    uint32_t start = Timer_GetMs();
    char buf[8];

    ST7735_FillRect(55, SCORE_Y, 50, 10, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "%d", paddle1.score);
    ST7735_DrawStringScaled(68, SCORE_Y, buf, COLOR_CYAN, COLOR_BLACK, 1);

    ST7735_DrawString(77, SCORE_Y, "-", COLOR_WHITE, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "%d", paddle2.score);
    ST7735_DrawStringScaled(86, SCORE_Y, buf, COLOR_MAGENTA, COLOR_BLACK, 1);

    g_ui_timers.last_score_draw_time = Timer_GetMs() - start;
}

void Game_DrawPaddle(Paddle_t* paddle, int16_t x, uint16_t color) {
    uint32_t start = Timer_GetMs();

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

    g_ui_timers.last_paddle_draw_time = Timer_GetMs() - start;
    g_ui_timers.paddle_draw_count++;
}

void Game_RedrawCenterLine(void) {
    for (int16_t y = 4; y < FIELD_HEIGHT - 4; y += 8) {
        ST7735_FillRect(79, y, 2, 4, COLOR_DARK_GRAY);
    }
}

void Game_DrawBall(void) {
    uint32_t start = Timer_GetMs();

    if (ball.x != ball.prev_x || ball.y != ball.prev_y) {
        ST7735_FillRect(ball.prev_x, ball.prev_y, ball.size, ball.size, COLOR_BLACK);
    }

    ST7735_FillRect(ball.x, ball.y, ball.size, ball.size, COLOR_YELLOW);

    if ((ball.x >= 70 && ball.x <= 90) || (ball.prev_x >= 70 && ball.prev_x <= 90)) {
        Game_RedrawCenterLine();
    }

    if (ball.y < 15 || ball.prev_y < 15) {
        Game_DrawScore();
    }

    ball.prev_x = ball.x;
    ball.prev_y = ball.y;

    g_ui_timers.last_ball_draw_time = Timer_GetMs() - start;
    g_ui_timers.ball_draw_count++;
}

void Game_ResetBall(void) {
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

void Game_Update(void) {
    uint32_t start = Timer_GetMs();

    if (!game.is_running || game.is_paused) return;

    game.frame_count++;
    game.rally_frames++;
    g_ui_timers.frame_count++;

    /* Accelerare minge */
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
        Game_RedrawCenterLine();

        PRINTF("[GAME] Speed level: %d\r\n", game.speed_level);
    }

    /* Paleta 1 (stanga) */
    if (IsCPUInput(paddle1.input)) {
        AI_UpdatePaddle(&paddle1, false);
    } else if (paddle1.input == INPUT_JOYSTICK) {
        int16_t joy_y = Joystick_GetY_Percent();
        if (joy_y > 15) paddle1.y += (joy_y / 25);
        else if (joy_y < -15) paddle1.y += (joy_y / 25);
    } else if (paddle1.input == INPUT_REMOTE) {
        int8_t ir_dir = IR_GetCurrentDirection();
        paddle1.y += ir_dir * PADDLE_SPEED;
    }

    /* Paleta 2 (dreapta) */
    if (IsCPUInput(paddle2.input)) {
        AI_UpdatePaddle(&paddle2, true);
    } else if (paddle2.input == INPUT_JOYSTICK) {
        int16_t joy_y = Joystick_GetY_Percent();
        if (joy_y > 15) paddle2.y += (joy_y / 25);
        else if (joy_y < -15) paddle2.y += (joy_y / 25);
    } else if (paddle2.input == INPUT_REMOTE) {
        int8_t ir_dir = IR_GetCurrentDirection();
        paddle2.y += ir_dir * PADDLE_SPEED;
    }

    /* Limiteaza pozitiile */
    if (paddle1.y < PADDLE_MIN_Y) paddle1.y = PADDLE_MIN_Y;
    if (paddle1.y > PADDLE_MAX_Y) paddle1.y = PADDLE_MAX_Y;
    if (paddle2.y < PADDLE_MIN_Y) paddle2.y = PADDLE_MIN_Y;
    if (paddle2.y > PADDLE_MAX_Y) paddle2.y = PADDLE_MAX_Y;

    /* Miscare bila */
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

    /* Coliziune Paleta 1 */
    if (ball.dx < 0 && ball.x <= PADDLE_X_P1 + PADDLE_WIDTH + 1 && ball.x >= PADDLE_X_P1) {
        if (ball.y + ball.size >= paddle1.y && ball.y <= paddle1.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P1 + PADDLE_WIDTH + 2;
            ball.dx = -ball.dx;
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle1.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }

    /* Coliziune Paleta 2 */
    if (ball.dx > 0 && ball.x + ball.size >= PADDLE_X_P2 - 1 && ball.x <= PADDLE_X_P2 + PADDLE_WIDTH) {
        if (ball.y + ball.size >= paddle2.y && ball.y <= paddle2.y + PADDLE_HEIGHT) {
            ball.x = PADDLE_X_P2 - ball.size - 2;
            ball.dx = -ball.dx;
            int16_t hit_pos = (ball.y + ball.size / 2) - (paddle2.y + PADDLE_HEIGHT / 2);
            ball.dy = hit_pos / 4;
            if (ball.dy == 0) ball.dy = (rand() % 2) ? 1 : -1;
        }
    }

    /* Gol stanga */
    if (ball.x < -ball.size) {
        paddle2.score++;
        Game_DrawScore();
        if (paddle2.score >= game.winning_score) {
            game.winner = 2;
            game.is_running = 0;
        } else {
            Game_ResetBall();
        }
    }

    /* Gol dreapta */
    if (ball.x > FIELD_WIDTH + ball.size) {
        paddle1.score++;
        Game_DrawScore();
        if (paddle1.score >= game.winning_score) {
            game.winner = 1;
            game.is_running = 0;
        } else {
            Game_ResetBall();
        }
    }

    /* Desenare */
    Game_DrawPaddle(&paddle1, PADDLE_X_P1, COLOR_CYAN);
    Game_DrawPaddle(&paddle2, PADDLE_X_P2, COLOR_MAGENTA);
    Game_DrawBall();

    g_ui_timers.last_game_update_time = Timer_GetMs() - start;
}

void Game_Start(void) {
    PRINTF("\r\n=== GAME STARTED ===\r\n");
    PRINTF("P1: %s | P2: %s\r\n", GetInputName(g_player1_input), GetInputName(g_player2_input));

    Game_Init();
    Game_DrawField();
    Game_DrawScore();

    ST7735_FillRect(PADDLE_X_P1, paddle1.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_CYAN);
    ST7735_FillRect(PADDLE_X_P2, paddle2.y, PADDLE_WIDTH, PADDLE_HEIGHT, COLOR_MAGENTA);

    /* Countdown */
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

    ST7735_FillRect(40, 50, 80, 30, COLOR_BLACK);
    Game_RedrawCenterLine();

    g_currentScreen = SCREEN_GAMEPLAY;
}

/*============================================================================
 * FUNCTII DE DESENARE MENIU
 *============================================================================*/

void DrawTitle(const char* title) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, 30, COLOR_BG);
    ST7735_DrawStringCentered(TITLE_Y, title, COLOR_TITLE, COLOR_BG, 2);
    ST7735_DrawHLine(0, 32, ST7735_WIDTH, COLOR_TITLE);
}

void DrawMenuItem(uint8_t index, const char* text, bool selected, bool enabled) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;

    if (!enabled) textColor = COLOR_DISABLED;
    else if (selected) textColor = COLOR_SELECTED;
    else textColor = COLOR_NORMAL;

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, text, textColor, bgColor);
}

void DrawInputMenuItem(uint8_t index, InputType_t input, bool selected, uint8_t forPlayer) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    bool available = IsInputAvailable(input, forPlayer);
    bool isCurrentSelection;

    if (forPlayer == 1) isCurrentSelection = (g_player1_input == input);
    else isCurrentSelection = (g_player2_input == input);

    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;
    uint16_t textColor;

    if (!available) textColor = COLOR_DISABLED;
    else if (isCurrentSelection) textColor = COLOR_HIGHLIGHT;
    else if (selected) textColor = COLOR_SELECTED;
    else textColor = COLOR_NORMAL;

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);

    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, GetInputName(input), textColor, bgColor);

    if (isCurrentSelection) {
        ST7735_DrawString(115, y + 3, "<-", COLOR_HIGHLIGHT, bgColor);
    } else if (!available) {
        if (forPlayer == 1) ST7735_DrawString(108, y + 3, "[P2]", COLOR_RED, bgColor);
        else ST7735_DrawString(108, y + 3, "[P1]", COLOR_CYAN, bgColor);
    }
}

void DrawBackOption(uint8_t index, bool selected) {
    int16_t y = MENU_START_Y + index * MENU_ITEM_HEIGHT;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BG;

    ST7735_FillRect(0, y, ST7735_WIDTH, MENU_ITEM_HEIGHT, bgColor);
    if (selected) ST7735_DrawString(MENU_MARGIN_X, y + 3, ">", COLOR_SELECTED, bgColor);
    ST7735_DrawString(MENU_MARGIN_X + 12, y + 3, "<< Back", COLOR_BACK, bgColor);
}

/*============================================================================
 * FUNCTII PENTRU FIECARE ECRAN
 *============================================================================*/

void DrawMainScreen(void) {
    uint32_t start = Timer_GetMs();

    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PONG GAME");

    DrawMenuItem(0, "Start", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Select Input", g_menuState.selectedIndex == 1, true);
    DrawMenuItem(2, "Mute Music", g_menuState.selectedIndex == 2, true);

    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY);

    char buf[32];
    snprintf(buf, sizeof(buf), "P1:%s", GetInputName(g_player1_input));
    ST7735_DrawString(5, 108, buf, COLOR_CYAN, COLOR_BG);

    snprintf(buf, sizeof(buf), "P2:%s", GetInputName(g_player2_input));
    ST7735_DrawString(85, 108, buf, COLOR_MAGENTA, COLOR_BG);

    g_ui_timers.last_menu_draw_time = Timer_GetMs() - start;
}

void DrawStartScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("START");

    DrawMenuItem(0, "Player vs Player", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Player vs CPU", g_menuState.selectedIndex == 1, true);
    DrawBackOption(2, g_menuState.selectedIndex == 2);
}

void DrawSelectInputScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("SELECT INPUT");

    DrawMenuItem(0, "Player 1", g_menuState.selectedIndex == 0, true);
    DrawMenuItem(1, "Player 2", g_menuState.selectedIndex == 1, true);
    DrawBackOption(2, g_menuState.selectedIndex == 2);

    ST7735_DrawHLine(0, 95, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawString(5, 100, "Current Setup:", COLOR_GRAY, COLOR_BG);

    char buf[24];
    snprintf(buf, sizeof(buf), "P1: %s", GetInputName(g_player1_input));
    ST7735_DrawString(5, 112, buf, COLOR_CYAN, COLOR_BG);

    snprintf(buf, sizeof(buf), "P2: %s", GetInputName(g_player2_input));
    ST7735_DrawString(85, 112, buf, COLOR_MAGENTA, COLOR_BG);
}

void DrawSelectP1Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PLAYER 1");

    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 1);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 1);
    DrawInputMenuItem(2, INPUT_CPU_EASY, g_menuState.selectedIndex == 2, 1);
    DrawInputMenuItem(3, INPUT_CPU_HARD, g_menuState.selectedIndex == 3, 1);
    DrawBackOption(4, g_menuState.selectedIndex == 4);
}

void DrawSelectP2Screen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("PLAYER 2");

    DrawInputMenuItem(0, INPUT_JOYSTICK, g_menuState.selectedIndex == 0, 2);
    DrawInputMenuItem(1, INPUT_REMOTE, g_menuState.selectedIndex == 1, 2);
    DrawInputMenuItem(2, INPUT_CPU_EASY, g_menuState.selectedIndex == 2, 2);
    DrawInputMenuItem(3, INPUT_CPU_HARD, g_menuState.selectedIndex == 3, 2);
    DrawBackOption(4, g_menuState.selectedIndex == 4);
}

void DrawDifficultyScreen(void) {
    ST7735_FillScreen(COLOR_BG);
    DrawTitle("DIFFICULTY");

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

    ST7735_DrawHLine(0, 100, ST7735_WIDTH, COLOR_GRAY);
    ST7735_DrawString(10, 108, "P1: Joystick", COLOR_CYAN, COLOR_BG);
    ST7735_DrawString(90, 108, "P2: CPU", COLOR_MAGENTA, COLOR_BG);
}

void DrawGameOverScreen(void) {
    ST7735_FillScreen(COLOR_BG);

    ST7735_DrawStringCentered(15, "GAME OVER", COLOR_RED, COLOR_BG, 2);

    char buf[32];
    uint16_t winner_color = (game.winner == 1) ? COLOR_CYAN : COLOR_MAGENTA;
    snprintf(buf, sizeof(buf), "Player %d Wins!", game.winner);
    ST7735_DrawStringCentered(45, buf, winner_color, COLOR_BG, 1);

    snprintf(buf, sizeof(buf), "%d - %d", paddle1.score, paddle2.score);
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

static uint8_t g_pause_selection = 0;

void DrawPausedScreen(void) {
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
 * ECRAN INTRO ANIMAT
 *============================================================================*/

static const uint16_t rainbow_colors[] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BLUE, COLOR_MAGENTA
};
#define NUM_RAINBOW_COLORS 7

#define BMO_SCREEN_COLOR  0x0400
#define BMO_FACE_COLOR    COLOR_BLACK

void DrawBMOFace(uint8_t expression, uint8_t blink) {
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
            ST7735_FillRect(36, 64, 10, 6, BMO_FACE_COLOR);
            ST7735_FillRect(114, 64, 10, 6, BMO_FACE_COLOR);
            break;
        case 2:
            ST7735_FillRect(50, 65, 60, 25, BMO_FACE_COLOR);
            ST7735_FillRect(56, 72, 48, 14, BMO_SCREEN_COLOR);
            ST7735_FillRect(65, 78, 30, 6, COLOR_RED);
            break;
        case 3:
            ST7735_FillRect(35, 25, 25, 25, BMO_FACE_COLOR);
            ST7735_FillRect(42, 30, 10, 10, COLOR_WHITE);
            ST7735_FillRect(95, 40, 35, 5, BMO_FACE_COLOR);
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
    uint32_t anim_start;

    /* Faza BMO */
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
    delay_ms(800);

    DrawBMOFace(2, 1);
    delay_ms(150);
    DrawBMOFace(2, 0);
    delay_ms(200);

    DrawBMOFace(3, 0);
    delay_ms(400);

    DrawBMOFace(1, 0);
    ST7735_DrawStringCentered(100, "Let's play", COLOR_BLACK, BMO_SCREEN_COLOR, 1);
    ST7735_DrawStringCentered(112, "PONG!", COLOR_YELLOW, BMO_SCREEN_COLOR, 2);
    delay_ms(1000);

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

    anim_start = Timer_GetMs();

    for (int16_t y = 0; y < 128; y += 2) {
        uint16_t scan_color = rainbow_colors[y % NUM_RAINBOW_COLORS];
        ST7735_DrawHLine(0, y, 160, scan_color);
        delay_ms(8);
    }
    delay_ms(100);

    ST7735_FillScreen(COLOR_CYAN);
    delay_ms(40);
    ST7735_FillScreen(COLOR_MAGENTA);
    delay_ms(40);
    ST7735_FillScreen(COLOR_YELLOW);
    delay_ms(40);
    ST7735_FillScreen(COLOR_WHITE);
    delay_ms(60);

    for (int16_t y = 0; y < 128; y++) {
        uint8_t intensity = 10 + (y / 8);
        uint16_t grad_color = ((intensity >> 3) << 11);
        ST7735_DrawHLine(0, y, 160, grad_color);
    }
    delay_ms(100);

    for (int i = 0; i < 4; i++) {
        ST7735_DrawRect(78 - i*20, 62 - i*16, 4 + i*40, 4 + i*32, rainbow_colors[i % NUM_RAINBOW_COLORS]);
        delay_ms(60);
    }
    delay_ms(100);

    ST7735_FillRect(6, 6, 148, 116, COLOR_BLACK);
    ST7735_DrawRect(4, 4, 152, 120, COLOR_WHITE);
    ST7735_DrawRect(6, 6, 148, 116, COLOR_GRAY);

    delay_ms(150);

    const char* letters = "PONG";
    int16_t letter_x[] = {26, 54, 82, 110};
    uint16_t letter_colors[] = {COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA, COLOR_GREEN};
    const uint8_t TITLE_SIZE = 3;

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

    for (int flash = 0; flash < 3; flash++) {
        for (int i = 0; i < 4; i++) {
            ST7735_DrawChar(letter_x[i], 15, letters[i],
                           rainbow_colors[(flash + i) % NUM_RAINBOW_COLORS], COLOR_BLACK, TITLE_SIZE);
        }
        delay_ms(80);
    }

    for (int i = 0; i < 4; i++) {
        ST7735_DrawChar(letter_x[i], 15, letters[i], letter_colors[i], COLOR_BLACK, TITLE_SIZE);
    }

    for (int16_t w = 0; w <= 70; w += 3) {
        ST7735_DrawHLine(80 - w, 42, w * 2, COLOR_WHITE);
        if (w > 10) {
            ST7735_DrawHLine(80 - w + 5, 43, (w - 5) * 2, COLOR_GRAY);
        }
        delay_ms(6);
    }

    delay_ms(100);

    const char* subtitle = "ARCADE EDITION";
    char typing_buf[20] = "";
    for (int i = 0; subtitle[i] != '\0'; i++) {
        typing_buf[i] = subtitle[i];
        typing_buf[i+1] = '\0';
        ST7735_FillRect(30, 48, 100, 12, COLOR_BLACK);
        ST7735_DrawStringCentered(50, typing_buf, COLOR_ORANGE, COLOR_BLACK, 1);
        delay_ms(40);
    }

    ST7735_DrawStringCentered(50, subtitle, COLOR_WHITE, COLOR_BLACK, 1);
    delay_ms(80);
    ST7735_DrawStringCentered(50, subtitle, COLOR_ORANGE, COLOR_BLACK, 1);

    delay_ms(200);

    ST7735_DrawHLine(15, 62, 130, COLOR_DARK_GRAY);
    ST7735_DrawHLine(15, 105, 130, COLOR_DARK_GRAY);

    ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_CYAN);
    ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_MAGENTA);

    ST7735_DrawStringCentered(112, "Press to Start", COLOR_WHITE, COLOR_BLACK, 1);
    ST7735_DrawStringCentered(120, ">> PUSH <<", COLOR_GRAY, COLOR_BLACK, 1);

    PRINTF("Intro animation started\r\n");

    g_joy_btn_pressed = false;
    uint8_t border_color_idx = 0;
    uint32_t last_border_update = 0;
    uint8_t title_glow = 0;
    uint32_t last_title_glow = 0;
    uint8_t blink_state = 0;
    uint32_t last_blink = 0;

    while (!g_joy_btn_pressed && !IR_IsPausePressed()) {
        uint32_t now = Timer_GetMs();

        ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, COLOR_BLACK);

        ball_x += ball_dx;
        ball_y += ball_dy;

        if (ball_y <= DEMO_TOP || ball_y >= DEMO_BOTTOM - BALL_SZ) {
            ball_dy = -ball_dy;
        }
        if (ball_x <= 12 && ball_y + BALL_SZ >= p1_y && ball_y <= p1_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 13;
            ST7735_FillRect(8, p1_y, PAD_W, PAD_H, COLOR_WHITE);
        }
        if (ball_x >= 148 - BALL_SZ && ball_y + BALL_SZ >= p2_y && ball_y <= p2_y + PAD_H) {
            ball_dx = -ball_dx;
            ball_x = 147 - BALL_SZ;
            ST7735_FillRect(148, p2_y, PAD_W, PAD_H, COLOR_WHITE);
        }

        if (ball_x < 5 || ball_x > 155) {
            ball_x = 80;
            ball_y = 82;
            ball_dx = (ball_dx > 0) ? -2 : 2;
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

        if ((now - last_border_update) >= 150) {
            last_border_update = now;
            border_color_idx = (border_color_idx + 1) % NUM_RAINBOW_COLORS;
            ST7735_DrawRect(4, 4, 152, 120, rainbow_colors[border_color_idx]);
        }

        if ((now - last_title_glow) >= 200) {
            last_title_glow = now;
            title_glow = (title_glow + 1) % NUM_RAINBOW_COLORS;

            for (int i = 0; i < 4; i++) {
                uint8_t color_idx = (title_glow + i * 2) % NUM_RAINBOW_COLORS;
                uint16_t letter_col;
                if ((title_glow / 2) % 2 == 0) {
                    letter_col = rainbow_colors[color_idx];
                } else {
                    uint16_t orig_colors[] = {COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA, COLOR_GREEN};
                    letter_col = orig_colors[i];
                }
                ST7735_DrawChar(letter_x[i], 15, letters[i], letter_col, COLOR_BLACK, TITLE_SIZE);
            }
        }

        if ((now - last_blink) >= 400) {
            last_blink = now;
            blink_state = (blink_state + 1) % 4;
            uint16_t blink_colors[] = {COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA};
            ST7735_DrawStringCentered(112, "Press to Start", blink_colors[blink_state], COLOR_BLACK, 1);
        }

        delay_ms(33);

        if ((now - anim_start) > 60000) {
            PRINTF("Intro timeout\r\n");
            break;
        }
    }

    PRINTF("Button pressed - going to menu\r\n");

    for (int i = 0; i < 5; i++) {
        ST7735_DrawRect(i * 15, i * 12, 160 - i * 30, 128 - i * 24, COLOR_WHITE);
        delay_ms(30);
    }

    ST7735_FillScreen(COLOR_CYAN);
    delay_ms(30);
    ST7735_FillScreen(COLOR_WHITE);
    delay_ms(50);
    ST7735_FillScreen(COLOR_BLACK);
    delay_ms(80);

    g_joy_btn_pressed = false;
}

void DrawCurrentScreen(void) {
    switch (g_currentScreen) {
        case SCREEN_INTRO:
            PlayIntroAnimation();
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
            g_menuState.maxItems = 5;
            DrawSelectP1Screen();
            break;
        case SCREEN_SELECT_P2:
            g_menuState.maxItems = 5;
            DrawSelectP2Screen();
            break;
        case SCREEN_DIFFICULTY:
            g_menuState.maxItems = 4;
            DrawDifficultyScreen();
            break;
        case SCREEN_GAMEPLAY:
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

void Menu_MoveUp(void) {
    if (g_menuState.selectedIndex > 0) g_menuState.selectedIndex--;
    else g_menuState.selectedIndex = g_menuState.maxItems - 1;
    g_needsRedraw = 1;
}

void Menu_MoveDown(void) {
    g_menuState.selectedIndex++;
    if (g_menuState.selectedIndex >= g_menuState.maxItems) g_menuState.selectedIndex = 0;
    g_needsRedraw = 1;
}

void ChangeScreen(Screen_t newScreen) {
    g_currentScreen = newScreen;
    g_menuState.selectedIndex = 0;
    g_needsRedraw = 1;
}

void Menu_Select(void) {
    switch (g_currentScreen) {
        case SCREEN_MAIN:
            switch (g_menuState.selectedIndex) {
                case 0: ChangeScreen(SCREEN_START); break;
                case 1: ChangeScreen(SCREEN_SELECT_INPUT); break;
                case 2: PRINTF("Mute Music toggled\r\n"); break;
            }
            break;

        case SCREEN_START:
            switch (g_menuState.selectedIndex) {
                case 0:
                    PRINTF("Starting PvP game!\r\n");
                    if (IsCPUInput(g_player2_input)) g_player2_input = INPUT_REMOTE;
                    if (g_player1_input == g_player2_input && !IsCPUInput(g_player1_input)) g_player2_input = INPUT_REMOTE;
                    Game_Start();
                    break;
                case 1: ChangeScreen(SCREEN_DIFFICULTY); break;
                case 2: ChangeScreen(SCREEN_MAIN); break;
            }
            break;

        case SCREEN_DIFFICULTY:
            switch (g_menuState.selectedIndex) {
                case 0: g_player2_input = INPUT_CPU_EASY; Game_Start(); break;
                case 1: g_player2_input = INPUT_CPU_MEDIUM; Game_Start(); break;
                case 2: g_player2_input = INPUT_CPU_HARD; Game_Start(); break;
                case 3: ChangeScreen(SCREEN_START); break;
            }
            break;

        case SCREEN_SELECT_INPUT:
            switch (g_menuState.selectedIndex) {
                case 0: ChangeScreen(SCREEN_SELECT_P1); break;
                case 1: ChangeScreen(SCREEN_SELECT_P2); break;
                case 2: ChangeScreen(SCREEN_MAIN); break;
            }
            break;

        case SCREEN_SELECT_P1:
            if (g_menuState.selectedIndex == 4) {
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                InputType_t inputMap[] = {INPUT_JOYSTICK, INPUT_REMOTE, INPUT_CPU_EASY, INPUT_CPU_HARD};
                InputType_t selectedInput = inputMap[g_menuState.selectedIndex];

                if (IsInputAvailable(selectedInput, 1)) {
                    g_player1_input = selectedInput;
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 1;
                    g_needsRedraw = 1;
                } else {
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawRect(20, 105, 120, 20, COLOR_WHITE);
                    ST7735_DrawStringCentered(110, "USED BY P2!", COLOR_WHITE, COLOR_RED, 1);
                    delay_ms(800);
                    g_needsRedraw = 1;
                }
            }
            break;

        case SCREEN_SELECT_P2:
            if (g_menuState.selectedIndex == 4) {
                ChangeScreen(SCREEN_SELECT_INPUT);
            } else {
                InputType_t inputMap[] = {INPUT_JOYSTICK, INPUT_REMOTE, INPUT_CPU_EASY, INPUT_CPU_HARD};
                InputType_t selectedInput = inputMap[g_menuState.selectedIndex];

                if (IsInputAvailable(selectedInput, 2)) {
                    g_player2_input = selectedInput;
                    g_currentScreen = SCREEN_SELECT_INPUT;
                    g_menuState.selectedIndex = 2;
                    g_needsRedraw = 1;
                } else {
                    ST7735_FillRect(20, 105, 120, 20, COLOR_RED);
                    ST7735_DrawRect(20, 105, 120, 20, COLOR_WHITE);
                    ST7735_DrawStringCentered(110, "USED BY P1!", COLOR_WHITE, COLOR_RED, 1);
                    delay_ms(800);
                    g_needsRedraw = 1;
                }
            }
            break;

        case SCREEN_GAME_OVER:
            if (g_menuState.selectedIndex == 0) Game_Start();
            else ChangeScreen(SCREEN_MAIN);
            break;

        default:
            break;
    }
}

/*============================================================================
 * INPUT PROCESSING
 *============================================================================*/

void ProcessMenuInput(void) {
    JoyAction_t joy_action = Joystick_GetMenuAction();
    JoyAction_t ir_action = IR_GetMenuAction();
    JoyAction_t action = (joy_action != JOY_ACTION_NONE) ? joy_action : ir_action;

    switch (action) {
        case JOY_ACTION_UP: Menu_MoveUp(); break;
        case JOY_ACTION_DOWN: Menu_MoveDown(); break;
        case JOY_ACTION_SELECT: Menu_Select(); break;
        default: break;
    }
}

void ProcessGameInput(void) {
    if (g_currentScreen == SCREEN_PAUSED) {
        Joystick_Process();
        int16_t joy_y = Joystick_GetY_Percent();

        static bool pause_nav_consumed = false;
        if (joy_y > 50 && !pause_nav_consumed) {
            g_pause_selection = 1;
            pause_nav_consumed = true;
            g_needsRedraw = 1;
        } else if (joy_y < -50 && !pause_nav_consumed) {
            g_pause_selection = 0;
            pause_nav_consumed = true;
            g_needsRedraw = 1;
        } else if (joy_y > -30 && joy_y < 30) {
            pause_nav_consumed = false;
        }

        bool select_pressed = false;
        if (g_joy_btn_pressed) {
            g_joy_btn_pressed = false;
            select_pressed = true;
        }
        if (IR_IsPausePressed()) {
            select_pressed = true;
        }

        if (select_pressed) {
            if (g_pause_selection == 0) {
                g_currentScreen = SCREEN_GAMEPLAY;
                game.is_paused = 0;
                Game_DrawField();
                Game_DrawScore();
                PRINTF("[GAME] Resumed\r\n");
            } else {
                game.is_running = 0;
                g_currentScreen = SCREEN_MAIN;
                g_menuState.selectedIndex = 0;
                g_needsRedraw = 1;
                PRINTF("[GAME] Exited to menu\r\n");
            }
        }

    } else if (g_currentScreen == SCREEN_GAMEPLAY) {
        /* Verifica pauza mai intai */
        bool pause_pressed = false;
        if (g_joy_btn_pressed) {
            g_joy_btn_pressed = false;
            pause_pressed = true;
        }
        if (IR_CheckPauseOnly()) {
            pause_pressed = true;
        }

        if (pause_pressed) {
            g_currentScreen = SCREEN_PAUSED;
            game.is_paused = 1;
            g_pause_selection = 0;
            g_needsRedraw = 1;
            PRINTF("[GAME] Paused\r\n");
            return;
        }

        /* Proceseaza IR pentru miscare paleta */
        IR_ProcessGameInput();
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

    Timer_Init();

    srand(g_systick_ms ^ 0xDEADBEEF);

    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("   PONG GAME - FRDM-KL25Z + ST7735     \r\n");
    PRINTF("   Joystick + IR Remote + AI Bot       \r\n");
    PRINTF("   === IR FIX + UI TIMERS ===          \r\n");
    PRINTF("========================================\r\n\r\n");

    PRINTF("=== PINOUT ===\r\n");
    PRINTF("DISPLAY: BL->3.3V VCC->3.3V GND->GND\r\n");
    PRINTF("         SCK->PTC5 SDA->PTC6 DC->PTC3\r\n");
    PRINTF("         RES->PTC0 CS->PTC4\r\n");
    PRINTF("JOYSTICK: VRY->PTB1 SW->PTD4\r\n");
    PRINTF("IR: OUT->PTA12\r\n");
    PRINTF("==============\r\n\r\n");

    PRINTF("Initializing display...\r\n");
    ST7735_Init();
    PRINTF("Display OK!\r\n");

    PRINTF("Initializing joystick...\r\n");
    Joystick_Init();
    PRINTF("Joystick OK!\r\n");

    PRINTF("Initializing IR Remote...\r\n");
    IR_Init();
    TPM0_Init();
    PRINTF("IR Remote OK!\r\n\r\n");

    PRINTF("=== CONTROLS ===\r\n");
    PRINTF("MENU: Joystick/IR UP/DOWN + Button/OK=Select\r\n");
    PRINTF("GAME: Joystick/IR UP/DOWN = Move paddle\r\n");
    PRINTF("      Button/OK = Pause\r\n");
    PRINTF("      IR: Hold UP/DOWN for continuous move\r\n");
    PRINTF("================\r\n\r\n");

    DrawCurrentScreen();

    PRINTF(">>> Ready! <<<\r\n\r\n");

    uint32_t last_game_update = 0;
    uint32_t last_menu_update = 0;
    const uint32_t GAME_FRAME_MS = 20;
    const uint32_t MENU_FRAME_MS = 50;

    while (1) {
        uint32_t now = Timer_GetMs();

        Joystick_Process();

        if (g_currentScreen == SCREEN_GAMEPLAY) {
            ProcessGameInput();

            if ((now - last_game_update) >= GAME_FRAME_MS) {
                last_game_update = now;

                Game_Update();

                if (!game.is_running && game.winner != 0) {
                    PRINTF("\r\n=== GAME OVER ===\r\n");
                    PRINTF("Winner: Player %d\r\n", game.winner);
                    PRINTF("Score: %d - %d\r\n\r\n", paddle1.score, paddle2.score);

                    g_currentScreen = SCREEN_GAME_OVER;
                    g_menuState.selectedIndex = 0;
                    g_needsRedraw = 1;
                }

                g_p1_move = 0;
                g_p2_move = 0;
            }

            UI_PrintStats();

        } else if (g_currentScreen == SCREEN_PAUSED) {
            ProcessGameInput();

            if (g_needsRedraw) {
                DrawCurrentScreen();
            }

        } else {
            if ((now - last_menu_update) >= MENU_FRAME_MS) {
                last_menu_update = now;
                ProcessMenuInput();
            }

            if (g_needsRedraw) {
                DrawCurrentScreen();
            }
        }

        __WFI();
    }

    return 0;
}
