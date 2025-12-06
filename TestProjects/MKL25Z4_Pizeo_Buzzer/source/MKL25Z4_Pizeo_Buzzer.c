/*
 * FRDM-KL25Z - Jingle Bells FULL VERSION (Loop)
 * Pin: PTB0 (J10 Pin 8)
 */

#include <stdio.h>
#include <string.h>
#include "MKL25Z4.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"

/* ==========================================
 * DATA MUZICALA (STROFA + REFREN)
 * ========================================== */

/* * LEGENDA NOTE NOI:
 * c,d,e,f,g = Note normale (Do4 - Sol4)
 * a, b      = La, Si
 * L         = Sol de jos (Low G - G3)
 * h         = Do de sus (High C - C5)
 * i         = Re de sus (High D - D5)
 */

char notes[] =
    /* STROFA (Verse) */
    /* Dashing through the snow */
    "gedcL"  "gedcL"
    /* O'er the fields we go, Laughing all the way */
    "afedb"  "ggfde"
    /* Bells on bobtail ring, Making spirits bright */
    "gedcL"  "gedcL"
    /* What fun it is to ride and sing a sleighing song tonight */
    "afedhhhh" "agfdc"

    /* REFREN (Chorus) */
    /* Jingle bells, jingle bells, jingle all the way */
    "eee" "eee" "egcde"
    /* Oh what fun it is to ride in a one horse open sleigh */
    "fffff" "eeeee" "ddedg"
    /* Jingle bells, jingle bells, jingle all the way */
    "eee" "eee" "egcde"
    /* Oh what fun it is to ride in a one horse open sleigh */
    "fffff" "eeeee" "ggfdc";

int duration[] = {
    /* STROFA */
    1,1,1,1,4,  1,1,1,1,4,      // Dashing... snow / In a... sleigh
    1,1,1,1,4,  1,1,1,1,4,      // O'er... go / Laughing... way
    1,1,1,1,4,  1,1,1,1,4,      // Bells... ring / Making... bright
    1,1,1,1,1,1,1,1,  1,1,1,1,4,// What fun... tonight

    /* REFREN */
    1,1,2, 1,1,2, 1,1,1,1,4,    // Jingle... way
    1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,2, // Oh what fun... sleigh (part 1)
    1,1,2, 1,1,2, 1,1,1,1,4,    // Jingle... way
    1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,4  // Oh what fun... sleigh (part 2)
};

int tempo = 100; // Viteza melodiei (mai mic = mai rapid)

/* ==========================================
 * SISTEM HARDWARE
 * ========================================== */

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 4000; i++) __asm volatile("nop");
}

void Buzzer_Init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    SIM->SCGC6 |= SIM_SCGC6_TPM1_MASK;
    PORTB->PCR[0] = PORT_PCR_MUX(3); // PWM mode
    SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1);

    TPM1->SC = TPM_SC_PS(7); // Prescaler 128
    TPM1->CONTROLS[0].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK;
    TPM1->MOD = 0xFFFF;
    TPM1->CONTROLS[0].CnV = 0;
    TPM1->SC |= TPM_SC_CMOD(1);
}

void Tone_Generate(uint16_t freq) {
    if (freq == 0) {
        TPM1->CONTROLS[0].CnV = 0;
    } else {
        uint32_t mod = 375000 / freq;
        TPM1->MOD = mod;
        TPM1->CONTROLS[0].CnV = mod / 2;
    }
}

/* ==========================================
 * LOGICA DE INTERPRETARE NOTE
 * ========================================== */

void Play_Char_Note(char note, int duration_ms) {
    /* Tabela extinsa de note */
    char names[] = { 'L', 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'h', 'i' };
    int  freqs[] = { 196, 261, 293, 329, 349, 392, 440, 494, 523, 587 };
    /* G3   C4   D4   E4   F4   G4   A4   B4   C5   D5  */

    int freq = 0;

    /* Cautam nota */
    for (int i = 0; i < sizeof(names); i++) {
        if (note == names[i]) {
            freq = freqs[i];
            break;
        }
    }

    if (freq > 0) Tone_Generate(freq);

    delay_ms(duration_ms);

    /* Mic pauza intre note pentru efect staccato */
    Tone_Generate(0);
    delay_ms(20);
}

/* ==========================================
 * MAIN
 * ========================================== */
int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();

    Buzzer_Init();

    while (1) {
        int len = strlen(notes);

        for (int i = 0; i < len; i++) {
            if (notes[i] == ' ') {
                delay_ms(duration[i] * tempo);
            } else {
                Play_Char_Note(notes[i], duration[i] * tempo);
            }

            /* Pauza intre note proportionala cu tempoul */
            delay_ms(tempo / 2);
        }

        /* AM SCOS delay-ul mare de aici.
         * Acum va sari imediat inapoi la inceputul melodiei. */
    }
}
