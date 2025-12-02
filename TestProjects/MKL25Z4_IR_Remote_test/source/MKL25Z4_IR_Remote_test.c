#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "fsl_uart.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

/* Definirea pinului pentru senzorul IR (PTA12) */
#define IR_PIN 12u
#define IR_GPIO GPIOA
#define IR_PORT PORTA

/* Variabile pentru decodarea IR */
volatile uint32_t ir_code = 0;
volatile uint8_t ir_ready = 0;
volatile uint32_t pulse_count = 0;
volatile uint32_t pulse_buffer[68];
volatile uint32_t last_edge_time = 0;
volatile uint8_t capture_complete = 0;

/* Functii IR */
void IR_Init(void);
void TPM0_Init(void);
void DecodeIR(void);

/* Handler pentru intreruperi TPM0 (timeout) */
void TPM0_IRQHandler(void) {
    if (TPM0->SC & TPM_SC_TOF_MASK) {
        TPM0->SC |= TPM_SC_TOF_MASK; // Clear overflow flag

        // Timeout - daca am capturat date, marcheaza ca gata
        if (pulse_count > 10) {
            capture_complete = 1;
            ir_ready = 1;
        } else {
            pulse_count = 0;
        }
    }
}

/* Handler pentru intreruperi pe portul A (senzor IR) */
void PORTA_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(IR_PORT);

    if (isfr & (1U << IR_PIN)) {
        PORT_ClearPinsInterruptFlags(IR_PORT, 1U << IR_PIN);

        if (!capture_complete) {
            uint32_t current_time = TPM0->CNT;

            if (pulse_count < 68) {
                pulse_buffer[pulse_count++] = current_time;
            }

            TPM0->CNT = 0; // Reset counter pentru urmatorul puls
        }
    }
}

void DecodeIR(void) {
    ir_code = 0;

    PRINTF("\r\n--- Analiza pulsuri (primele 40) ---\r\n");
    for (int i = 0; i < 40 && i < pulse_count; i++) {
        PRINTF("%2d: %5u  ", i, (unsigned int)pulse_buffer[i]);
        if ((i + 1) % 4 == 0) PRINTF("\r\n");
    }
    PRINTF("Total pulsuri captate: %u\r\n", (unsigned int)pulse_count);

    // Verifică dacă avem suficiente pulsuri
    if (pulse_count < 67) {
        PRINTF("Prea putine pulsuri: %u (minim 67)\r\n", (unsigned int)pulse_count);
        return;
    }

    // Găsește min/max pentru pulsurile de spațiu (impare, după preambul)
    uint32_t min_space = 0xFFFFFFFF;
    uint32_t max_space = 0;

    for (int i = 3; i < 67; i += 2) {
        if (pulse_buffer[i] < min_space) min_space = pulse_buffer[i];
        if (pulse_buffer[i] > max_space) max_space = pulse_buffer[i];
    }

    // Pragul este la jumătatea drumului între min și max
    uint32_t threshold = (min_space + max_space) / 2;

    PRINTF("Min: %u, Max: %u, Prag: %u\r\n",
           (unsigned int)min_space, (unsigned int)max_space, (unsigned int)threshold);

    // Decodifică cei 32 de biți
    for (int i = 0; i < 32; i++) {
        uint32_t space_duration = pulse_buffer[3 + i * 2 + 1];

        if (space_duration > threshold) {
            ir_code |= (1UL << i);
        }
    }
}

void IR_Init(void) {
    /* Enable clock pentru PORTA */
    CLOCK_EnableClock(kCLOCK_PortA);

    /* Configurare pin IR */
    PORT_SetPinMux(IR_PORT, IR_PIN, kPORT_MuxAsGpio);

    /* Enable pull-up */
    IR_PORT->PCR[IR_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    /* Seteaza pin ca input */
    gpio_pin_config_t gpioPinConfig = {
        kGPIO_DigitalInput,
        0,
    };
    GPIO_PinInit(IR_GPIO, IR_PIN, &gpioPinConfig);

    /* Configureaza intrerupere pe ambele fronturi */
    PORT_SetPinInterruptConfig(IR_PORT, IR_PIN, kPORT_InterruptEitherEdge);

    /* Enable intreruperi pentru PORTA */
    NVIC_SetPriority(PORTA_IRQn, 2);
    EnableIRQ(PORTA_IRQn);
}

void TPM0_Init(void) {
    /* Enable clock pentru TPM0 */
    CLOCK_EnableClock(kCLOCK_Tpm0);

    /* Selecteaza sursa de clock pentru TPM */
    CLOCK_SetTpmClock(1U); // MCGFLLCLK

    /* Configurare TPM0 */
    TPM0->SC = 0; // Disable timer
    TPM0->CNT = 0; // Reset counter
    TPM0->MOD = 0xFFFF; // Valoare maxima

    /* Prescaler = 32, overflow interrupt enable */
    TPM0->SC = TPM_SC_PS(5) | TPM_SC_TOIE_MASK | TPM_SC_CMOD(1);

    /* Enable intreruperi TPM0 */
    NVIC_SetPriority(TPM0_IRQn, 3);
    EnableIRQ(TPM0_IRQn);
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

    PRINTF("\r\n=== IR Remote Decoder - Debug Mode ===\r\n");
    PRINTF("Initializare senzor IR pe PTA12...\r\n");

    /* Initializare IR si Timer */
    IR_Init();
    TPM0_Init();

    PRINTF("Gata! Apasa UN buton pe telecomanda IR...\r\n\r\n");

    uint32_t last_code = 0;
    uint32_t cod_fix = 0xBA45FF00; // Schimba cu codul tau

    while (1) {
        if (ir_ready) {
            ir_ready = 0;
            capture_complete = 0;

            // Decodifica pulsurile
            DecodeIR();

            PRINTF("\r\nCod IR decodificat: 0x%08X\r\n", (unsigned int)ir_code);

            // Afiseaza si fiecare byte separat
            PRINTF("  Bytes: %02X %02X %02X %02X\r\n",
                   (unsigned int)((ir_code >> 24) & 0xFF),
                   (unsigned int)((ir_code >> 16) & 0xFF),
                   (unsigned int)((ir_code >> 8) & 0xFF),
                   (unsigned int)(ir_code & 0xFF));

            // Comparatie cu valoare fixa
            if (ir_code == cod_fix) {
                PRINTF("  -> MATCH! Cod recunoscut!\r\n");
            }

            // Protocol NEC: verificare
            uint8_t addr = (ir_code >> 24) & 0xFF;
            uint8_t addr_inv = (ir_code >> 16) & 0xFF;
            uint8_t cmd = (ir_code >> 8) & 0xFF;
            uint8_t cmd_inv = ir_code & 0xFF;

            PRINTF("  Address: 0x%02X (inv: 0x%02X) %s\r\n",
                   addr, addr_inv, (addr == (uint8_t)~addr_inv) ? "[OK]" : "[ERR]");
            PRINTF("  Command: 0x%02X (inv: 0x%02X) %s\r\n",
                   cmd, cmd_inv, (cmd == (uint8_t)~cmd_inv) ? "[OK]" : "[ERR]");

            last_code = ir_code;

            // Reset pentru următoarea captură
            pulse_count = 0;

            PRINTF("\r\nGata pentru urmatorul buton...\r\n\r\n");
        }

        delay_ms(10);
    }

    return 0;
}
