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

/* --- CONFIGURĂRI PIN --- */
#define IR_PIN 12u
#define IR_GPIO GPIOA
#define IR_PORT PORTA

/* --- PARAMETRI TIMING NEC (în microsecunde) --- */
/* Timer-ul rulează la 1.5 MHz (48MHz / 32). 1 tick = 0.666 us */
/* Factor de conversie: Ticks = us * 1.5 */
#define TICKS_US_FACTOR      1.5f

#define NEC_HDR_MARK_MIN     12000 // ~9000us (acceptăm toleranță mare pt start)
#define NEC_HDR_SPACE_MIN    6000  // ~4500us
#define NEC_BIT_THRESHOLD    2500  // Prag între 0 (560us) și 1 (1690us). ~1666us = 2500 ticks

/* Filtru zgomot: ignorăm orice puls sub 100 ticks (~66us) */
#define GLITCH_THRESHOLD     100

/* Variabile Globale */
volatile uint32_t ir_code = 0;
volatile uint8_t ir_ready = 0;
volatile uint32_t pulse_count = 0;
volatile uint32_t pulse_buffer[100]; // Buffer puțin mai mare pentru siguranță
volatile uint8_t capture_complete = 0;

/* Prototypes */
void IR_Init(void);
void TPM0_Init(void);
void DecodeNEC(void);
void delay_ms(uint32_t ms);

/* ----------------------------------------------------------------------------
   INTERRUPT HANDLERS
   ---------------------------------------------------------------------------- */

void TPM0_IRQHandler(void) {
    if (TPM0->SC & TPM_SC_TOF_MASK) {
        TPM0->SC |= TPM_SC_TOF_MASK; // Clear flag

        // Timeout: Dacă timer-ul a dat overflow, înseamnă că transmisia s-a terminat
        if (pulse_count >= 66) { // Minimul necesar pentru un cadru valid
            capture_complete = 1;
            ir_ready = 1;
        } else {
            // Zgomot sau semnal incomplet -> reset
            pulse_count = 0;
            capture_complete = 0;
        }
    }
}
/* ----------------------------------------------------------------------------
   INTERRUPT HANDLER OPTIMIZAT
   ---------------------------------------------------------------------------- */
void PORTA_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(IR_PORT);

    if (isfr & (1U << IR_PIN)) {
        PORT_ClearPinsInterruptFlags(IR_PORT, 1U << IR_PIN);

        if (!capture_complete) {
            uint32_t current_val = TPM0->CNT;

            // Resetăm imediat timerul pentru a avea precizie maximă pentru următorul front
            TPM0->CNT = 0;

            // --- GLITCH FILTER ---
            // Ignorăm pulsuri extrem de scurte (< 100 ticks / 66us)
            // Dar le acceptăm pe celelalte.
            if (current_val > 100) {
                if (pulse_count < 100) {
                    pulse_buffer[pulse_count++] = current_val;
                }
            }
        }
    }
}

/* ----------------------------------------------------------------------------
   DECODING LOGIC CORECTATĂ (Index Shift Fix)
   ---------------------------------------------------------------------------- */
void DecodeNEC(void) {
    ir_code = 0;

    // Căutăm Header-ul real (aprox 13500 ticks) în primele poziții
    // De obicei este la indexul 1 (indexul 0 este "liniștea" dinainte)
    int start_index = -1;

    // Verificăm index 0 și 1
    if (pulse_buffer[0] > 12000 && pulse_buffer[0] < 15000) start_index = 0;
    else if (pulse_buffer[1] > 12000 && pulse_buffer[1] < 15000) start_index = 1;

    if (start_index == -1) {
        PRINTF("Header NEC invalid (Mark=%u, Space=%u)\r\n",
               pulse_buffer[0], pulse_buffer[1]);
        return;
    }

    // Luăm valorile corecte bazate pe indexul găsit
    uint32_t hdr_mark = pulse_buffer[start_index];
    uint32_t hdr_space = pulse_buffer[start_index + 1];

    PRINTF("Header OK! Mark=%u, Space=%u. Decodare...\r\n", hdr_mark, hdr_space);

    // Verificare Repeat Code (Space mic ~2.25ms = ~3375 ticks)
    if (hdr_space < 4000) {
        PRINTF(" -> Repeat Code (Tasta tinuta apasata)\r\n");
        return;
    }

    // Decodare Biți
    // Structura: [Mark] [Space] ...
    // Datele încep după Header Mark și Header Space.
    // Primul bit (Bit 0):
    //   Mark: buffer[start_index + 2]
    //   Space: buffer[start_index + 3]  <-- Aici e informația (0 sau 1)

    for (int i = 0; i < 32; i++) {
        // Calculăm indexul pentru durata de SPACE a bitului 'i'
        // Formula: start + header(2) + (i*2) + componenta_space(1)
        int idx = start_index + 3 + (i * 2);

        if (idx >= pulse_count) break;

        uint32_t space_duration = pulse_buffer[idx];

        // Pragul:
        // 0 Logic = 560us (~840 ticks)
        // 1 Logic = 1690us (~2535 ticks)
        // Prag ales: 1600 ticks (aprox 1000us)
        if (space_duration > 1600) {
            ir_code |= (1UL << i);
        }
    }
}

/* ----------------------------------------------------------------------------
   INITIALIZATION
   ---------------------------------------------------------------------------- */

void IR_Init(void) {
    CLOCK_EnableClock(kCLOCK_PortA);
    PORT_SetPinMux(IR_PORT, IR_PIN, kPORT_MuxAsGpio);

    // IMPORTANT: Pull-up activat, senzorul este Open-Drain
    IR_PORT->PCR[IR_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    gpio_pin_config_t gpioPinConfig = { kGPIO_DigitalInput, 0 };
    GPIO_PinInit(IR_GPIO, IR_PIN, &gpioPinConfig);

    PORT_SetPinInterruptConfig(IR_PORT, IR_PIN, kPORT_InterruptEitherEdge);

    NVIC_SetPriority(PORTA_IRQn, 2);
    EnableIRQ(PORTA_IRQn);
}

void TPM0_Init(void) {
    CLOCK_EnableClock(kCLOCK_Tpm0);

    // Sursa Clock: MCGFLLCLK (aprox 48MHz standard pe FRDM-KL25Z)
    CLOCK_SetTpmClock(1U);

    TPM0->SC = 0;
    TPM0->CNT = 0;
    TPM0->MOD = 0xFFFF; // Max count

    // Prescaler 32 -> 48MHz / 32 = 1.5 MHz (1 tick = 0.66 us)
    // Overflow la ~43ms (suficient pt a detecta pauza dintre cadre)
    TPM0->SC = TPM_SC_PS(5) | TPM_SC_TOIE_MASK | TPM_SC_CMOD(1);

    NVIC_SetPriority(TPM0_IRQn, 3);
    EnableIRQ(TPM0_IRQn);
}

void delay_ms(uint32_t ms) {
    // Estimare brută pentru delay
    for (volatile uint32_t i = 0; i < ms * 4000; i++);
}

/* ----------------------------------------------------------------------------
   MAIN
   ---------------------------------------------------------------------------- */

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    PRINTF("\r\n=== IR NEC Decoder - Optimizat ===\r\n");

    IR_Init();
    TPM0_Init();

    PRINTF("Sistem gata. Astept semnal...\r\n");

    while (1) {
        if (ir_ready) {
            // Dezactivăm întreruperile scurt timp pentru a procesa datele atomic (opțional dar recomandat)
            NVIC_DisableIRQ(PORTA_IRQn);

            DecodeNEC();

            // Dacă codul e valid (nu e 0)
            if (ir_code != 0) {
                // Inversăm ordinea pentru a citi mai ușor (unele telecomenzi trimit invers)
                // Dar afișăm raw hex cum ai cerut:
                PRINTF("Cod Decodat: 0x%08X\r\n", ir_code);

                // Analiză structură
                uint8_t cmd = (ir_code >> 8) & 0xFF;
                PRINTF(" -> Tasta (CMD): 0x%02X\r\n", cmd);

                // Exemplu mapare taste bazat pe lista ta
                switch(ir_code) {
                    case 0xF30CFF00: PRINTF(" -> Tasta: 1\r\n"); break;
                    case 0xE718FF00: PRINTF(" -> Tasta: 2\r\n"); break;
                    case 0xA15EFF00: PRINTF(" -> Tasta: 3\r\n"); break;
                    case 0xBA45FF00: PRINTF(" -> Tasta: POWER/CH-\r\n"); break;
                    default: PRINTF(" -> Tasta necunoscuta\r\n"); break;
                }
            }

            // Reset flags
            ir_ready = 0;
            capture_complete = 0;
            pulse_count = 0;

            PRINTF("-------------------------------\r\n");

            NVIC_EnableIRQ(PORTA_IRQn);
        }

        // Buclă idle, așteptăm interrupt
        __asm("wfi");
    }
}
