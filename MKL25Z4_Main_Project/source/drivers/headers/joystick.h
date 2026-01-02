/*
 * joystick.h
 * Driver pentru joystick analog cu buton
 * ADC pentru axe, GPIO cu intrerupere pentru buton
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/*============================================================================
 * HARDWARE CONFIGURATION
 *============================================================================*/

/* ADC Channels */
#define JOYSTICK_VRX_CHANNEL    8U    /* PTB0 - X axis (optional) */
#define JOYSTICK_VRY_CHANNEL    9U    /* PTB1 - Y axis */

/* Button Pin */
#define JOYSTICK_SW_PIN         4U
#define JOYSTICK_SW_PORT        PORTD
#define JOYSTICK_SW_GPIO        GPIOD
#define JOYSTICK_SW_IRQ         PORTD_IRQn

/*============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

/**
 * Initializeaza joystick-ul:
 * - Configureaza ADC pentru citire axe
 * - Configureaza GPIO pentru buton cu intrerupere
 */
void Joystick_Init(void);

/**
 * Proceseaza joystick-ul (citeste ADC)
 * Trebuie apelat periodic in main loop
 */
void Joystick_Process(void);

/**
 * Returneaza actiunea pentru meniu (UP/DOWN/SELECT)
 * Cu debounce integrat pentru navigare fluida
 */
UI_Action_t Joystick_GetMenuAction(void);

/**
 * Returneaza procentul Y pentru control paleta
 * @return Valoare intre -100 (sus) si +100 (jos)
 */
int16_t Joystick_GetY_Percent(void);

/**
 * Returneaza directia pentru control paleta in joc
 * @return -1=sus, 0=neutru, +1=jos (bazat pe deadzone)
 */
int8_t Joystick_GetGameDirection(void);

/**
 * Verifica daca butonul a fost apasat (cu clear automat)
 * @return true daca butonul a fost apasat
 */
bool Joystick_ButtonPressed(void);

/**
 * Reseteaza starea joystick-ului (debounce, flags)
 */
void Joystick_Reset(void);

#endif /* JOYSTICK_H */
