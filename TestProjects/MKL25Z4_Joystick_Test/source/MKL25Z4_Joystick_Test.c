#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "fsl_adc16.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

/* Definirea pinilor pentru joystick */
#define JOYSTICK_VRX_CHANNEL 8U    // PTB0 -> ADC0_SE8  (Axa X)
#define JOYSTICK_VRY_CHANNEL 9U    // PTB1 -> ADC0_SE9  (Axa Y)
#define JOYSTICK_SW_PIN 2u         // PTB2 (Buton SW)
#define JOYSTICK_SW_GPIO GPIOB
#define JOYSTICK_SW_PORT PORTB

/* Variabile pentru butoanele joystick-ului */
volatile uint8_t button_pressed = 0;

/* Structura pentru valorile joystick-ului */
typedef struct {
    uint16_t x_raw;
    uint16_t y_raw;
    int16_t x_percent;
    int16_t y_percent;
    uint8_t button;
} joystick_data_t;

joystick_data_t joystick;

/* Functii */
void ADC_Init(void);
void Joystick_Button_Init(void);
uint16_t ADC_Read(uint32_t channel);
void Process_Joystick(void);
void Display_Joystick_Position(void);

/* Handler pentru butonul joystick-ului */
void PORTB_PORTC_PORTD_PORTE_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);

    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);

        // Citeste starea pinului direct din registru
        if ((GPIOB->PDIR & (1U << JOYSTICK_SW_PIN)) == 0) {
            button_pressed = 1;
        }
    }
}

void ADC_Init(void) {
    adc16_config_t adc16ConfigStruct;

    /* Enable clock pentru ADC0 */
    CLOCK_EnableClock(kCLOCK_Adc0);

    /* Configurare ADC */
    ADC16_GetDefaultConfig(&adc16ConfigStruct);
    adc16ConfigStruct.resolution = kADC16_ResolutionSE12Bit;
    adc16ConfigStruct.clockDivider = kADC16_ClockDivider4;
    adc16ConfigStruct.longSampleMode = kADC16_LongSampleCycle24;
    adc16ConfigStruct.referenceVoltageSource = kADC16_ReferenceVoltageSourceVref;

    ADC16_Init(ADC0, &adc16ConfigStruct);
    ADC16_DoAutoCalibration(ADC0);

    PRINTF("ADC initializat si calibrat!\r\n");
}

void Joystick_Button_Init(void) {
    /* Enable clock pentru PORTB */
    CLOCK_EnableClock(kCLOCK_PortB);

    /* Configurare pin buton SW */
    PORT_SetPinMux(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_MuxAsGpio);

    /* Enable pull-up pentru buton */
    JOYSTICK_SW_PORT->PCR[JOYSTICK_SW_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    /* Seteaza pin ca input */
    gpio_pin_config_t gpioPinConfig = {
        kGPIO_DigitalInput,
        0,
    };
    GPIO_PinInit(JOYSTICK_SW_GPIO, JOYSTICK_SW_PIN, &gpioPinConfig);

    /* Configureaza intrerupere pe falling edge (cand se apasa) */
    PORT_SetPinInterruptConfig(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN, kPORT_InterruptFallingEdge);

    /* Enable intreruperi pentru PORTB/C/D/E (IRQ combinat pe KL25Z) */
    NVIC_SetPriority(PORTD_IRQn, 2);
    EnableIRQ(PORTD_IRQn);

    PRINTF("Buton joystick initializat!\r\n");
}

uint16_t ADC_Read(uint32_t channel) {
    adc16_channel_config_t adc16ChannelConfigStruct;

    adc16ChannelConfigStruct.channelNumber = channel;
    adc16ChannelConfigStruct.enableInterruptOnConversionCompleted = false;
    adc16ChannelConfigStruct.enableDifferentialConversion = false;

    ADC16_SetChannelConfig(ADC0, 0, &adc16ChannelConfigStruct);

    /* Asteapta finalizarea conversiei */
    while (0U == (kADC16_ChannelConversionDoneFlag &
                  ADC16_GetChannelStatusFlags(ADC0, 0))) {
    }

    return ADC16_GetChannelConversionValue(ADC0, 0);
}

void Process_Joystick(void) {
    /* Citeste valorile raw de la ADC */
    joystick.x_raw = ADC_Read(JOYSTICK_VRX_CHANNEL);
    joystick.y_raw = ADC_Read(JOYSTICK_VRY_CHANNEL);

    /* Citeste starea butonului direct din registru */
    joystick.button = ((GPIOB->PDIR & (1U << JOYSTICK_SW_PIN)) == 0) ? 1 : 0;

    /* Converteste la procente (-100% la +100%)
     * Presupunem ca centrul este la ~2048 (jumatate din 4096 pentru 12-bit ADC)
     * Si ca valorile merg de la 0 la 4095
     */
    joystick.x_percent = ((int32_t)joystick.x_raw - 2048) * 100 / 2048;
    joystick.y_percent = ((int32_t)joystick.y_raw - 2048) * 100 / 2048;

    /* Limiteaza valorile la [-100, 100] */
    if (joystick.x_percent > 100) joystick.x_percent = 100;
    if (joystick.x_percent < -100) joystick.x_percent = -100;
    if (joystick.y_percent > 100) joystick.y_percent = 100;
    if (joystick.y_percent < -100) joystick.y_percent = -100;
}

void Display_Joystick_Position(void) {
    /* Afiseaza o reprezentare vizuala a pozitiei */
    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("     JOYSTICK 2-AXIS TEST              \r\n");
    PRINTF("========================================\r\n");

    /* Valorile RAW */
    PRINTF(" X RAW: %4u (0x%03X)                  \r\n",
           joystick.x_raw, joystick.x_raw);
    PRINTF(" Y RAW: %4u (0x%03X)                  \r\n",
           joystick.y_raw, joystick.y_raw);
    PRINTF("========================================\r\n");

    /* Valorile in procente */
    PRINTF(" X: %4d%%  ", joystick.x_percent);

    /* Bara de progress pentru X */
    PRINTF("[");
    int x_bar_pos = (joystick.x_percent + 100) * 20 / 200;
    for (int i = 0; i < 20; i++) {
        if (i == 10) PRINTF("|");
        else if (i == x_bar_pos) PRINTF("X");
        else PRINTF("-");
    }
    PRINTF("]\r\n");

    PRINTF(" Y: %4d%%  ", joystick.y_percent);

    /* Bara de progress pentru Y */
    PRINTF("[");
    int y_bar_pos = (joystick.y_percent + 100) * 20 / 200;
    for (int i = 0; i < 20; i++) {
        if (i == 10) PRINTF("|");
        else if (i == y_bar_pos) PRINTF("Y");
        else PRINTF("-");
    }
    PRINTF("]\r\n");

    PRINTF("========================================\r\n");

    /* Directie */
    PRINTF(" Directie: ");
    if (joystick.x_percent < -20 && joystick.y_percent > 20) {
        PRINTF("STANGA-SUS            ");
    } else if (joystick.x_percent > 20 && joystick.y_percent > 20) {
        PRINTF("DREAPTA-SUS           ");
    } else if (joystick.x_percent < -20 && joystick.y_percent < -20) {
        PRINTF("STANGA-JOS            ");
    } else if (joystick.x_percent > 20 && joystick.y_percent < -20) {
        PRINTF("DREAPTA-JOS           ");
    } else if (joystick.x_percent < -20) {
        PRINTF("STANGA                ");
    } else if (joystick.x_percent > 20) {
        PRINTF("DREAPTA               ");
    } else if (joystick.y_percent > 20) {
        PRINTF("SUS                   ");
    } else if (joystick.y_percent < -20) {
        PRINTF("JOS                   ");
    } else {
        PRINTF("CENTRU                ");
    }
    PRINTF("\r\n");

    /* Starea butonului */
    PRINTF(" Buton: %s                         \r\n",
           joystick.button ? "[APASAT]  " : "[ELIBERAT]");

    PRINTF("========================================\r\n");
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 6000; i++) {
        __asm volatile("nop");
    }
}

int main(void) {
    /* Init board hardware */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("  FRDM-KL25Z Joystick 2-Axis Test     \r\n");
    PRINTF("========================================\r\n");
    PRINTF("\r\n");

    PRINTF("Initializare ADC pentru joystick...\r\n");
    ADC_Init();

    PRINTF("Initializare buton joystick...\r\n");
    Joystick_Button_Init();

    PRINTF("\r\nPinout:\r\n");
    PRINTF("  VRX (Axa X) -> PTB0 (ADC0_SE8)\r\n");
    PRINTF("  VRY (Axa Y) -> PTB1 (ADC0_SE9)\r\n");
    PRINTF("  SW  (Buton) -> PTB2\r\n");
    PRINTF("  +5V         -> 5V/3.3V\r\n");
    PRINTF("  GND         -> GND\r\n");
    PRINTF("\r\nGata! Misca joystick-ul...\r\n");

    delay_ms(1000);

    while (1) {
        /* Citeste si proceseaza datele de la joystick */
        Process_Joystick();

        /* Afiseaza pozitia */
        Display_Joystick_Position();

        /* Verifica daca s-a apasat butonul */
        if (button_pressed) {
            button_pressed = 0;
            PRINTF("\r\n>>> BUTON APASAT! <<<\r\n");
        }

        /* Delay pentru actualizare ecran */
        delay_ms(200);
    }

    return 0;
}
