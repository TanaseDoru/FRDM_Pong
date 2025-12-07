#include "headers/joystick.h"
#include "fsl_adc16.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "board.h"
/* --- MODIFICARE HARDWARE: Folosim PTD4 pentru buton (suporta IRQ) --- */
#define JOYSTICK_VRX_CHANNEL 8U    // PTB0 (J10-8)
#define JOYSTICK_VRY_CHANNEL 9U    // PTB1 (J10-10)

// Noul pin pentru buton: PTD4 (J2 Pin 6)
#define JOYSTICK_SW_PIN 4u
#define JOYSTICK_SW_PORT PORTD
#define JOYSTICK_SW_GPIO GPIOD
#define JOYSTICK_SW_IRQ  PORTD_IRQn

static volatile bool btn_pressed_flag = false;
static int16_t y_percent = 0;
static bool action_consumed = false;

/* ISR pentru Portul D (Buton Joystick) */
void PORTD_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);
    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);
        btn_pressed_flag = true;
    }
}

void Joystick_Init(void) {
    adc16_config_t adcConfig;
    CLOCK_EnableClock(kCLOCK_Adc0);

    // Enable clock pentru Port B (Analog) si Port D (Buton)
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortD);

    ADC16_GetDefaultConfig(&adcConfig);
    ADC16_Init(ADC0, &adcConfig);
    ADC16_DoAutoCalibration(ADC0);

    /* GPIO Init pentru SW pe PTD4 */
    PORT_SetPinMux(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_MuxAsGpio);
    // Pull-up este necesar!
    JOYSTICK_SW_PORT->PCR[JOYSTICK_SW_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    gpio_pin_config_t sw_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(JOYSTICK_SW_GPIO, JOYSTICK_SW_PIN, &sw_config);

    /* Intrerupere pe Falling Edge (cand apesi butonul leaga la masa) */
    PORT_SetPinInterruptConfig(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_InterruptFallingEdge);

    NVIC_SetPriority(JOYSTICK_SW_IRQ, 3);
    EnableIRQ(JOYSTICK_SW_IRQ);
}

static uint16_t ReadADC(uint32_t ch) {
    adc16_channel_config_t chConfig = {ch, false, false};
    ADC16_SetChannelConfig(ADC0, 0, &chConfig);
    while (0U == (kADC16_ChannelConversionDoneFlag & ADC16_GetChannelStatusFlags(ADC0, 0)));
    return ADC16_GetChannelConversionValue(ADC0, 0);
}

void Joystick_Process(void) {
    uint16_t y_raw = ReadADC(JOYSTICK_VRY_CHANNEL);
    // Centrul este ~2048.
    // Inversam semnul daca merge invers (depinde cum tii joystickul)
    y_percent = ((int32_t)y_raw - 2048) * 100 / 2048;

    if (y_percent > -20 && y_percent < 20) {
        action_consumed = false;
    }
}

UI_Action_t Joystick_GetMenuAction(void) {
    if (btn_pressed_flag) {
        btn_pressed_flag = false;
        return ACTION_SELECT;
    }

    if (!action_consumed) {
        // Ajusteaza pragurile daca e prea sensibil
        if (y_percent > 60) { action_consumed = true; return ACTION_UP; }
        if (y_percent < -60) { action_consumed = true; return ACTION_DOWN; }
    }
    return ACTION_NONE;
}

int16_t Joystick_GetY_Percent(void) {
    return y_percent;
}
