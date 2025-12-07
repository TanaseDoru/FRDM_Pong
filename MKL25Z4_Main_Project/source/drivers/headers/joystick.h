#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

void Joystick_Init(void);
void Joystick_Process(void); // Se apeleaza in main loop
UI_Action_t Joystick_GetMenuAction(void); // Returneaza UP/DOWN/SELECT
int16_t Joystick_GetY_Percent(void); // Pentru control paleta (-100 la 100)

#endif
