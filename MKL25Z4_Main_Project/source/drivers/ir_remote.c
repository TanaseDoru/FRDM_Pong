#include "headers/ir_remote.h"
#include "MKL25Z4.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

#define IR_PIN 12u
#define IR_PT_IRQ PORTA_IRQn

/* --- 1. DEFINIREA VARIABILELOR GLOBALE (CRITIC PENTRU LINKER) --- */
volatile uint32_t ir_code = 0;
volatile uint8_t ir_ready = 0;

/* Buffer Circular Simplificat */
#define PULSE_BUF_SIZE 128
static volatile uint32_t pulse_buffer[PULSE_BUF_SIZE];
static volatile uint8_t buf_head = 0;
static volatile uint8_t buf_tail = 0;

/* ISR Lightweight - doar captureaza timpul */
void PORTA_IRQHandler(void) {
    PORT_ClearPinsInterruptFlags(PORTA, 1U << IR_PIN);

    uint32_t val = TPM0->CNT;
    TPM0->CNT = 0; // Reset

    /* Filtru glitch simplu: ignoram pulsuri sub ~66us (100 ticks) */
    if (val > 100) {
        pulse_buffer[buf_head] = val;
        buf_head = (buf_head + 1) % PULSE_BUF_SIZE;
    }
}

void IR_Init(void) {
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_Tpm0);

    /* GPIO */
    PORT_SetPinMux(PORTA, IR_PIN, kPORT_MuxAsGpio);
    PORTA->PCR[IR_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    gpio_pin_config_t in_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(GPIOA, IR_PIN, &in_config);
    PORT_SetPinInterruptConfig(PORTA, IR_PIN, kPORT_InterruptEitherEdge);

    NVIC_SetPriority(IR_PT_IRQ, 2);
    EnableIRQ(IR_PT_IRQ);

    /* Timer TPM0 - 1.5 MHz */
    CLOCK_SetTpmClock(1U);
    TPM0->SC = TPM_SC_PS(5) | TPM_SC_CMOD(1); // Prescaler 32
    TPM0->MOD = 0xFFFF;
}

/* --- 2. LOGICA DE DECODARE (TREBUIE SA FIE AICI CA SA MEARGA) --- */
void IR_Process(void) {
    /* Verificam daca avem date in buffer */
    if (buf_head == buf_tail) return;

    /* Procesam tot ce e in buffer */
    while (buf_tail != buf_head) {
        uint32_t val = pulse_buffer[buf_tail];

        // Cautam Header Mark (aprox 13500 ticks +/- toleranta)
        // Header NEC este ~9ms Mark + 4.5ms Space.
        // Valoarea 'val' este distanta dintre fronturi.
        // Cautam un interval mare care semnaleaza inceputul.
        if (val > 12000 && val < 15000) {

            // Verificam daca avem destule date in buffer pentru tot pachetul
            // Avem nevoie de ~66 tranzitii dupa header
            int items_available = (buf_head >= buf_tail) ?
                                  (buf_head - buf_tail) :
                                  (PULSE_BUF_SIZE - buf_tail + buf_head);

            if (items_available > 60) {
                // Avem date! Decodam bitii.
                uint32_t temp_code = 0;

                // Structura NEC: HeaderMark, HeaderSpace, Bit0_Mark, Bit0_Space...
                // 'val' este HeaderMark. Urmatorul este HeaderSpace.
                // Bitii incep de la indexul: buf_tail + 1 (Space) + 1 (Bit0_Mark) + 1 (Bit0_Space)
                // Ne intereseaza durata de SPACE a fiecarui bit.

                for (int i = 0; i < 32; i++) {
                    // Calculam indexul unde se afla durata SPACE a bitului i
                    // Offset: 1(H_Space) + 1(B0_M) + 1(B0_S) = 3.
                    // Apoi fiecare bit are 2 tranzitii (+2 per bit).
                    int offset = 3 + (i * 2);
                    int idx = (buf_tail + offset) % PULSE_BUF_SIZE;

                    uint32_t duration = pulse_buffer[idx];

                    // Logic 0 ~ 560us (~840 ticks)
                    // Logic 1 ~ 1690us (~2500 ticks)
                    // Prag ales: 1600 ticks
                    if (duration > 1600) {
                        temp_code |= (1UL << i);
                    }
                }

                // Am decodat complet
                ir_code = temp_code;
                ir_ready = 1;

                // Avansam tail-ul ca sa "consumam" pachetul din buffer
                // Resetam tail la head (sau calculam exact), pentru simplitate golim tot
                buf_tail = buf_head;
                return;
            }
        }

        // Avansam in buffer daca nu a fost header
        buf_tail = (buf_tail + 1) % PULSE_BUF_SIZE;
    }
}

uint32_t IR_GetLastCode(void) {
    // Aici nu mai folosim extern, accesam direct variabilele globale din acest fisier
    if (ir_ready) {
        ir_ready = 0;
        return ir_code;
    }
    return 0;
}

UI_Action_t IR_GetMenuAction(void) {
    uint32_t code = IR_GetLastCode();
    if (code == 0) return ACTION_NONE;

    switch(code) {
        case IR_CODE_CH_UP:
        case IR_CODE_VOL_UP:
            return ACTION_UP;
        case IR_CODE_VOL_DOWN:
            return ACTION_DOWN;
        case IR_CODE_PLAY:
        case 0xF30CFF00: // Tasta 1 (Select)
            return ACTION_SELECT;
        case 0xBA45FF00: // CH- (Poate fi si Back in functie de context)
        case IR_CODE_0:
            return ACTION_BACK;
    }
    return ACTION_NONE;
}
