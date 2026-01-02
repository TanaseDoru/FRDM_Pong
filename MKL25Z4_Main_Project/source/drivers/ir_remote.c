/*
 * ir_remote.c
 * Driver pentru telecomanda IR cu protocol NEC
 * Pin: PTA12, Timer: TPM1
 * Suport pentru hold (tine apasat) pentru miscare continua in joc
 */

#include "headers/ir_remote.h"
#include "MKL25Z4.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"

/*============================================================================
 * HARDWARE CONFIGURATION
 *============================================================================*/

#define IR_PIN          12U
#define IR_PORT         PORTA
#define IR_GPIO         GPIOA
#define IR_IRQ          PORTA_IRQn

/*============================================================================
 * NEC PROTOCOL TIMING (in timer ticks @ 1.5MHz)
 * 1 tick = 0.667 us
 *============================================================================*/

/* Header: 9ms mark + 4.5ms space = 13.5ms total */
#define NEC_HEADER_MIN      18000   /* ~12ms */
#define NEC_HEADER_MAX      22500   /* ~15ms */

/* Bit timing */
#define NEC_BIT_THRESHOLD   2400    /* ~1.6ms - sub = 0, peste = 1 */

/* Repeat code: 9ms mark + 2.25ms space = 11.25ms total */
#define NEC_REPEAT_MIN      15000   /* ~10ms */
#define NEC_REPEAT_MAX      18000   /* ~12ms */

/*============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/

/* Buffer circular pentru pulsuri */
#define PULSE_BUF_SIZE      128
static volatile uint32_t pulse_buffer[PULSE_BUF_SIZE];
static volatile uint8_t buf_head = 0;
static volatile uint8_t buf_tail = 0;

/* Cod IR decodat */
static volatile uint32_t ir_code = 0;
static volatile uint8_t ir_ready = 0;

/* Stare pentru hold detection */
static volatile uint32_t last_code = 0;           /* Ultimul cod valid (nu repeat) */
static volatile uint32_t last_ir_time = 0;        /* Timestamp ultimul IR primit */
static volatile bool holding = false;             /* Flag pentru hold activ */

/* Pentru navigare meniu - debounce */
static volatile bool menu_action_consumed = false;
static volatile uint32_t last_menu_action_time = 0;

/* Extern: timer global din main */
extern volatile uint32_t g_systick_ms;

/*============================================================================
 * INTERRUPT HANDLER - Captureaza pulsuri IR
 *============================================================================*/

void PORTA_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(IR_PORT);
    
    if (isfr & (1U << IR_PIN)) {
        PORT_ClearPinsInterruptFlags(IR_PORT, 1U << IR_PIN);
        
        /* Citeste valoarea timer-ului si reseteaza */
        uint32_t val = TPM1->CNT;
        TPM1->CNT = 0;
        
        /* Filtru glitch: ignoram pulsuri sub ~100us (150 ticks) */
        if (val > 150) {
            pulse_buffer[buf_head] = val;
            buf_head = (buf_head + 1) % PULSE_BUF_SIZE;
        }
    }
}

/*============================================================================
 * INITIALIZATION
 *============================================================================*/

void IR_Init(void) {
    /* Enable clocks */
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_Tpm1);
    
    /* Configure PTA12 as GPIO input with pull-up */
    PORT_SetPinMux(IR_PORT, IR_PIN, kPORT_MuxAsGpio);
    IR_PORT->PCR[IR_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;  /* Pull-up */
    
    gpio_pin_config_t in_config = {kGPIO_DigitalInput, 0};
    GPIO_PinInit(IR_GPIO, IR_PIN, &in_config);
    
    /* Configure interrupt on both edges */
    PORT_SetPinInterruptConfig(IR_PORT, IR_PIN, kPORT_InterruptEitherEdge);
    
    /* Set interrupt priority and enable */
    NVIC_SetPriority(IR_IRQ, 2);
    EnableIRQ(IR_IRQ);
    
    /* Configure TPM1 for timing measurement */
    /* Clock source: MCGFLLCLK/MCGPLLCLK/2 (48MHz / 32 = 1.5MHz) */
    CLOCK_SetTpmClock(1U);  /* MCGFLLCLK */
    
    TPM1->SC = 0;  /* Stop timer */
    TPM1->CNT = 0; /* Reset counter */
    TPM1->MOD = 0xFFFF;  /* Max modulo */
    TPM1->SC = TPM_SC_PS(5) | TPM_SC_CMOD(1);  /* Prescaler /32, enable */
    
    PRINTF("[IR] Initialized on PTA12 with TPM1\r\n");
}

/*============================================================================
 * NEC DECODING
 *============================================================================*/

void IR_Process(void) {
    /* Verificam daca avem date in buffer */
    if (buf_head == buf_tail) {
        /* Nu avem date noi - verificam timeout pentru hold */
        if (holding && (g_systick_ms - last_ir_time) > IR_HOLD_TIMEOUT_MS) {
            holding = false;
        }
        return;
    }
    
    /* Procesam datele din buffer */
    while (buf_tail != buf_head) {
        uint32_t val = pulse_buffer[buf_tail];
        
        /* Verificam daca e REPEAT code (pentru hold) */
        if (val >= NEC_REPEAT_MIN && val < NEC_REPEAT_MAX) {
            /* Este un repeat - pastram ultimul cod si marcam holding */
            if (last_code != 0) {
                ir_code = IR_CODE_REPEAT;
                ir_ready = 1;
                holding = true;
                last_ir_time = g_systick_ms;
            }
            buf_tail = (buf_tail + 1) % PULSE_BUF_SIZE;
            continue;
        }
        
        /* Verificam daca e header NEC */
        if (val >= NEC_HEADER_MIN && val <= NEC_HEADER_MAX) {
            /* Calculam cate elemente avem in buffer */
            int items_available;
            if (buf_head >= buf_tail) {
                items_available = buf_head - buf_tail;
            } else {
                items_available = PULSE_BUF_SIZE - buf_tail + buf_head;
            }
            
            /* Avem nevoie de cel putin 64 tranzitii pentru 32 biti */
            if (items_available >= 64) {
                uint32_t temp_code = 0;
                
                /* Decodam cei 32 de biti */
                /* Structura: Header, apoi pentru fiecare bit: Mark + Space */
                /* Ne intereseaza durata Space-ului */
                for (int i = 0; i < 32; i++) {
                    /* Offset: 1(Header) + i*2(Mark+Space pentru bitii anteriori) + 1(Mark curent) */
                    int offset = 2 + (i * 2);
                    int idx = (buf_tail + offset) % PULSE_BUF_SIZE;
                    
                    uint32_t duration = pulse_buffer[idx];
                    
                    /* Logic 0: ~560us mark + ~560us space = ~1120us total */
                    /* Logic 1: ~560us mark + ~1690us space = ~2250us total */
                    /* Threshold la ~1600 ticks (~1.07ms) */
                    if (duration > NEC_BIT_THRESHOLD) {
                        temp_code |= (1UL << i);
                    }
                }
                
                /* Verificam integritatea codului (byte 2 = ~byte 3, byte 0 = ~byte 1 sau address) */
                uint8_t cmd = (temp_code >> 16) & 0xFF;
                uint8_t cmd_inv = (temp_code >> 24) & 0xFF;
                
                if ((cmd ^ cmd_inv) == 0xFF) {
                    /* Cod valid! */
                    ir_code = temp_code;
                    ir_ready = 1;
                    last_code = temp_code;
                    last_ir_time = g_systick_ms;
                    holding = true;
                    
                    PRINTF("[IR] Code: 0x%08X\r\n", (unsigned int)temp_code);
                }
                
                /* Consumam pachetul din buffer */
                buf_tail = buf_head;
                return;
            }
        }
        
        /* Nu e header valid, avansam */
        buf_tail = (buf_tail + 1) % PULSE_BUF_SIZE;
    }
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

uint32_t IR_GetLastCode(void) {
    if (ir_ready) {
        ir_ready = 0;
        uint32_t code = ir_code;
        
        /* Daca e repeat, returnam ultimul cod real */
        if (code == IR_CODE_REPEAT) {
            return last_code;
        }
        return code;
    }
    return 0;
}

UI_Action_t IR_GetMenuAction(void) {
    uint32_t code = IR_GetLastCode();
    if (code == 0) return ACTION_NONE;
    
    /* Debounce pentru meniu - 200ms intre actiuni */
    uint32_t now = g_systick_ms;
    if ((now - last_menu_action_time) < 200) {
        /* Dar permitem repeat daca se tine apasat mult timp */
        if (!holding || (now - last_menu_action_time) < 400) {
            return ACTION_NONE;
        }
    }
    
    UI_Action_t action = ACTION_NONE;
    
    switch(code) {
        case IR_CODE_UP:
            action = ACTION_UP;
            break;
        case IR_CODE_DOWN:
            action = ACTION_DOWN;
            break;
        case IR_CODE_SELECT:
            action = ACTION_SELECT;
            break;
    }
    
    if (action != ACTION_NONE) {
        last_menu_action_time = now;
    }
    
    return action;
}

int8_t IR_GetGameDirection(void) {
    /* Pentru joc, verificam daca se tine apasat */
    
    /* Prima verificare: cod nou */
    uint32_t code = IR_GetLastCode();
    
    if (code != 0) {
        /* Cod nou primit */
        if (code == IR_CODE_UP) return -1;  /* Sus */
        if (code == IR_CODE_DOWN) return 1; /* Jos */
    }
    
    /* A doua verificare: holding (tine apasat) */
    if (holding && (g_systick_ms - last_ir_time) < IR_HOLD_TIMEOUT_MS) {
        if (last_code == IR_CODE_UP) return -1;
        if (last_code == IR_CODE_DOWN) return 1;
    }
    
    return 0;  /* Neutru */
}

bool IR_IsHolding(void) {
    return holding && (g_systick_ms - last_ir_time) < IR_HOLD_TIMEOUT_MS;
}

void IR_Reset(void) {
    ir_ready = 0;
    ir_code = 0;
    last_code = 0;
    holding = false;
    menu_action_consumed = false;
    
    /* Golim buffer-ul */
    buf_tail = buf_head;
}
