#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include "game_config.h"

/* Taste definite */
#define IR_CODE_CH_MINUS 0xBA45FF00
#define IR_CODE_CH 0xB946FF00
#define IR_CODE_PREV 0xBB44FF00

#define IR_CODE_UP IR_CODE_CH_MINUS
#define IR_CODE_DOWN IR_CODE_CH
#define IR_CODE_ACTION IR_CODE_PREV


void IR_Init(void);
void IR_Process(void); // Chemat in loop
uint32_t IR_GetLastCode(void); // Returneaza codul si il sterge
UI_Action_t IR_GetMenuAction(void);

#endif

#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include "game_config.h"

/* --- MAPARE CODURI TASTE (Conform listei tale) --- */

void IR_Init(void);
void IR_Process(void);
uint32_t IR_GetLastCode(void);
UI_Action_t IR_GetMenuAction(void);

#endif
