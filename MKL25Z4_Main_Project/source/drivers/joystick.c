/*
 * joystick.c
 * Driver pentru joystick analog cu buton
 * VRY pe PTB1 (ADC0_SE9), SW pe PTD4
 */

#include "headers/joystick.h"
#include "MKL25Z4.h"
#include "fsl_adc16.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* Dead zone pentru meniu (procent) */
#define MENU_DEADZONE       20
/* Threshold pentru actiune meniu */
#define MENU_THRESHOLD      60

/* Dead zone pentru joc (mai mica pentru control precis) */
#define GAME_DEADZONE       15

/*============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/

static volatile bool btn_pressed_flag = false;
static int16_t y_percent = 0;
static bool menu_action_consumed = false;

/* Extern: timer global din main */
extern volatile uint32_t g_systick_ms;

/*============================================================================
 * INTERRUPT HANDLER - Buton joystick
 *============================================================================*/

void PORTD_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);
    
    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);
        btn_pressed_flag = true;
    }
}

/*============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

static uint16_t ReadADC(uint32_t channel) {
    adc16_channel_config_t chConfig = {
        .channelNumber = channel,
        .enableInterruptOnConversionCompleted = false,
        .enableDifferentialConversion = false
    };
    
    ADC16_SetChannelConfig(ADC0, 0, &chConfig);
    
    /* Asteapta conversie completa */
    while (0U == (kADC16_ChannelConversionDoneFlag & 
                  ADC16_GetChannelStatusFlags(ADC0, 0)));
    
    return ADC16_GetChannelConversionValue(ADC0, 0);
}

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

void Joystick_Init(void) {
    adc16_config_t adcConfig;
    
    /* Enable clocks */
    CLOCK_EnableClock(kCLOCK_Adc0);
    CLOCK_EnableClock(kCLOCK_PortB);  /* Pentru pinii analogici */
    CLOCK_EnableClock(kCLOCK_PortD);  /* Pentru buton */
    
    /* Configure ADC */
    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.resolution = kADC16_ResolutionSE12Bit;
    ADC16_Init(ADC0, &adcConfig);
    ADC16_DoAutoCalibration(ADC0);
    
    /* Configure button pin PTD4 */
    PORT_SetPinMux(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_MuxAsGpio);
    /* Enable pull-up */
    JOYSTICK_SW_PORT->PCR[JOYSTICK_SW_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    
    gpio_pin_config_t sw_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(JOYSTICK_SW_GPIO, JOYSTICK_SW_PIN, &sw_config);
    
    /* Configure interrupt on falling edge (button press = GND) */
    PORT_SetPinInterruptConfig(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, 
                               kPORT_InterruptFallingEdge);
    
    NVIC_SetPriority(JOYSTICK_SW_IRQ, 3);
    EnableIRQ(JOYSTICK_SW_IRQ);
    
    PRINTF("[Joystick] Initialized (VRY=PTB1, SW=PTD4)\r\n");
}

void Joystick_Process(void) {
    uint16_t y_raw = ReadADC(JOYSTICK_VRY_CHANNEL);
    
    /* Centrul este ~2048 (la 12 biti). Convertim la procent (-100 la +100) */
    /* Nota: valorile pot fi inversate in functie de orientarea joystick-ului */
    y_percent = ((int32_t)y_raw - 2048) * 100 / 2048;
    
    /* Reset consumed flag cand joystick-ul e in dead zone */
    if (y_percent > -MENU_DEADZONE && y_percent < MENU_DEADZONE) {
        menu_action_consumed = false;
    }
}

UI_Action_t Joystick_GetMenuAction(void) {
    /* Verifica buton */
    if (btn_pressed_flag) {
        btn_pressed_flag = false;
        return ACTION_SELECT;
    }
    
    /* Verifica miscare joystick (cu debounce) */
    if (!menu_action_consumed) {
        if (y_percent > MENU_THRESHOLD) {
            menu_action_consumed = true;
            return ACTION_DOWN;  /* Joystick in jos = meniu jos */
        }
        if (y_percent < -MENU_THRESHOLD) {
            menu_action_consumed = true;
            return ACTION_UP;    /* Joystick in sus = meniu sus */
        }
    }
    
    return ACTION_NONE;
}

int16_t Joystick_GetY_Percent(void) {
    return y_percent;
}

int8_t Joystick_GetGameDirection(void) {
    /* Pentru joc, folosim dead zone mai mica */
    if (y_percent > GAME_DEADZONE) {
        return 1;   /* Jos */
    }
    if (y_percent < -GAME_DEADZONE) {
        return -1;  /* Sus */
    }
    return 0;  /* Neutru */
}

bool Joystick_ButtonPressed(void) {
    if (btn_pressed_flag) {
        btn_pressed_flag = false;
        return true;
    }
    return false;
}

void Joystick_Reset(void) {
    btn_pressed_flag = false;
    menu_action_consumed = false;
}
