/*
 * fsl_pit.h
 * PIT (Periodic Interrupt Timer) driver for KL25Z
 * Simplified version for game timing
 */

#ifndef FSL_PIT_H
#define FSL_PIT_H

#include "MKL25Z4.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * DEFINITIONS
 *============================================================================*/

/* PIT Channels */
typedef enum {
    kPIT_Chnl_0 = 0U,
    kPIT_Chnl_1 = 1U
} pit_chnl_t;

/* PIT Status Flags */
enum {
    kPIT_TimerFlag = PIT_TFLG_TIF_MASK
};

/* PIT Interrupt Enable */
enum {
    kPIT_TimerInterruptEnable = PIT_TCTRL_TIE_MASK
};

/* PIT Configuration */
typedef struct {
    bool enableRunInDebug;  /* Continue in debug mode */
} pit_config_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * Get default PIT configuration
 */
static inline void PIT_GetDefaultConfig(pit_config_t *config) {
    config->enableRunInDebug = false;
}

/**
 * Initialize PIT module
 */
static inline void PIT_Init(PIT_Type *base, const pit_config_t *config) {
    /* Enable PIT clock */
    SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
    
    /* Enable module, set freeze in debug */
    base->MCR = PIT_MCR_FRZ(!config->enableRunInDebug);
}

/**
 * Deinitialize PIT module
 */
static inline void PIT_Deinit(PIT_Type *base) {
    base->MCR |= PIT_MCR_MDIS_MASK;
    SIM->SCGC6 &= ~SIM_SCGC6_PIT_MASK;
}

/**
 * Set timer period in clock cycles
 * Period = (count + 1) * clock_period
 */
static inline void PIT_SetTimerPeriod(PIT_Type *base, pit_chnl_t channel, uint32_t count) {
    base->CHANNEL[channel].LDVAL = count;
}

/**
 * Get current timer value
 */
static inline uint32_t PIT_GetCurrentTimerCount(PIT_Type *base, pit_chnl_t channel) {
    return base->CHANNEL[channel].CVAL;
}

/**
 * Start timer
 */
static inline void PIT_StartTimer(PIT_Type *base, pit_chnl_t channel) {
    base->CHANNEL[channel].TCTRL |= PIT_TCTRL_TEN_MASK;
}

/**
 * Stop timer
 */
static inline void PIT_StopTimer(PIT_Type *base, pit_chnl_t channel) {
    base->CHANNEL[channel].TCTRL &= ~PIT_TCTRL_TEN_MASK;
}

/**
 * Enable timer interrupts
 */
static inline void PIT_EnableInterrupts(PIT_Type *base, pit_chnl_t channel, uint32_t mask) {
    base->CHANNEL[channel].TCTRL |= mask;
}

/**
 * Disable timer interrupts
 */
static inline void PIT_DisableInterrupts(PIT_Type *base, pit_chnl_t channel, uint32_t mask) {
    base->CHANNEL[channel].TCTRL &= ~mask;
}

/**
 * Get status flags
 */
static inline uint32_t PIT_GetStatusFlags(PIT_Type *base, pit_chnl_t channel) {
    return base->CHANNEL[channel].TFLG;
}

/**
 * Clear status flags
 */
static inline void PIT_ClearStatusFlags(PIT_Type *base, pit_chnl_t channel, uint32_t mask) {
    base->CHANNEL[channel].TFLG = mask;
}

#endif /* FSL_PIT_H */
