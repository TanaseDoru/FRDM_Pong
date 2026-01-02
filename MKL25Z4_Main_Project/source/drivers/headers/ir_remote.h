/*
 * ir_remote.h
 * Driver pentru telecomanda IR cu protocol NEC
 * Suport pentru hold (tine apasat) si navigare meniu/joc
 */

#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/*============================================================================
 * IR CODE DEFINITIONS (NEC Protocol)
 *============================================================================*/

/* Coduri raw de la telecomanda */
#define IR_CODE_CH_MINUS    0xBA45FF00  /* CH- = Sus */
#define IR_CODE_CH          0xB946FF00  /* CH  = Jos */
#define IR_CODE_PREV        0xBB44FF00  /* Prev = Select */

/* NEC Repeat code */
#define IR_CODE_REPEAT      0xFFFFFFFF

/* Mapping semantic */
#define IR_CODE_UP          IR_CODE_CH_MINUS
#define IR_CODE_DOWN        IR_CODE_CH
#define IR_CODE_SELECT      IR_CODE_PREV

/*============================================================================
 * TIMING CONSTANTS
 *============================================================================*/

/* Timeout pentru hold detection (ms) */
#define IR_HOLD_TIMEOUT_MS      150
/* Interval pentru repeat in joc (ms) */
#define IR_GAME_REPEAT_MS       50

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

/**
 * Initializeaza modulul IR:
 * - Configureaza pinul PTA12 ca input cu pull-up
 * - Configureaza TPM1 pentru masurare durata pulsuri
 * - Activeaza intreruperea pe ambele fronturi
 */
void IR_Init(void);

/**
 * Proceseaza datele IR primite (decodare NEC)
 * Trebuie apelat periodic in main loop
 */
void IR_Process(void);

/**
 * Returneaza ultimul cod IR receptionat si il sterge
 * @return Codul IR sau 0 daca nu exista
 */
uint32_t IR_GetLastCode(void);

/**
 * Returneaza actiunea pentru meniu (UP/DOWN/SELECT)
 * Cu debounce integrat pentru navigare fluida
 */
UI_Action_t IR_GetMenuAction(void);

/**
 * Returneaza directia pentru control paleta in joc
 * @return -1=sus, 0=neutru, +1=jos
 * Suporta hold (tine apasat)
 */
int8_t IR_GetGameDirection(void);

/**
 * Verifica daca se tine apasat pe o tasta
 * @return true daca exista un hold activ
 */
bool IR_IsHolding(void);

/**
 * Reseteaza starea IR (apelat la schimbarea ecranului)
 */
void IR_Reset(void);

#endif /* IR_REMOTE_H */
