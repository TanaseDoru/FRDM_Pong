#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include "game_config.h"

/* Taste definite */
#define IR_CODE_CH_UP    0xB847FF00
#define IR_CODE_CH_DOWN  0xBA45FF00
#define IR_CODE_VOL_UP   0xEA15FF00
#define IR_CODE_VOL_DOWN 0xF807FF00
#define IR_CODE_PLAY     0xBC43FF00
#define IR_CODE_0        0xE916FF00


#define IR_CODE_UP      0xB847FF00  // CH+
#define IR_CODE_DOWN    0xBA45FF00  // CH-
#define IR_CODE_SELECT  0xBC43FF00  // Play/Pause
#define IR_CODE_BACK    0xE916FF00  // tasta 0


// Navigare Meniu & Player 1
// Player 2 Controls (Folosim aceleasi, sau tastele de volum)
// Propun sa folosim CH+/CH- pentru Player 2 in joc
#define IR_P2_UP        0xB847FF00  // CH+
#define IR_P2_DOWN      0xBA45FF00  // CH-


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
