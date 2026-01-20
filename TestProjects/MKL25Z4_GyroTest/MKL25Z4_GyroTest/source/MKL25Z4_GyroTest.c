#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"
#include "fsl_uart.h"
#include "fsl_port.h"


#define ESP_UART UART1
#define ESP_UART_CLKSRC kCLOCK_BusClk
#define BAUD_RATE 115200

#define SCREEN_HEIGHT 160
#define BUFFER_SIZE 64


char rxBuffer[BUFFER_SIZE];
uint8_t rxIndex = 0;

// Variabile Joc
float currentAngle = 0.0;
float compassHeading = 0.0;
int paddleY = 0;


void Init_ESP_UART_Polling(void) {
    uart_config_t config;

    CLOCK_EnableClock(kCLOCK_PortE);
    CLOCK_EnableClock(kCLOCK_Uart1);

    PORT_SetPinMux(PORTE, 0U, kPORT_MuxAlt3); // PTE0 = TX
    PORT_SetPinMux(PORTE, 1U, kPORT_MuxAlt3); // PTE1 = RX

    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = BAUD_RATE;
    config.enableTx = true;
    config.enableRx = true;

    UART_Init(ESP_UART, &config, CLOCK_GetFreq(ESP_UART_CLKSRC));
    UART_DisableInterrupts(ESP_UART, kUART_RxDataRegFullInterruptEnable | kUART_RxOverrunInterruptEnable);
}


int Check_UART_Data(void) {
    uint32_t flags = UART_GetStatusFlags(ESP_UART);

    if (flags & (kUART_RxOverrunFlag | kUART_NoiseErrorFlag | kUART_FramingErrorFlag)) {
        UART_ClearStatusFlags(ESP_UART, kUART_RxOverrunFlag | kUART_NoiseErrorFlag | kUART_FramingErrorFlag);
        rxIndex = 0;
        return 0;
    }

    if (flags & kUART_RxDataRegFullFlag) {
        uint8_t data = UART_ReadByte(ESP_UART);
        if (data == '\n' || data == '\r') {
            if (rxIndex > 0) {
                rxBuffer[rxIndex] = 0;
                rxIndex = 0;
                return 1;
            }
        } else {
            if (rxIndex < BUFFER_SIZE - 1) {
                rxBuffer[rxIndex++] = data;
            }
        }
    }
    return 0;
}

//Stanga->Jos / Dreapta->Sus
int Map_Angle_To_Screen(float angle) {
    if (angle > 45.0f) angle = 45.0f;
    if (angle < -45.0f) angle = -45.0f;

    // Angle = -45 (Stanga) -> offset = 0   -> y = 160 (Jos)
    // Angle = +45 (Dreapta)-> offset = 90  -> y = 0   (Sus)
    float offset = (angle + 45.0f) * ((float)SCREEN_HEIGHT / 90.0f);
    int y = SCREEN_HEIGHT - (int)offset;

    if (y < 0) y = 0;
    if (y > SCREEN_HEIGHT) y = SCREEN_HEIGHT;
    return y;
}

//o directie incepe cu 45/2=22.5 (+/- 22.5)
//am impartit cercul de 360 grade in 8 (8 coordonate geografice) => 45 grade
const char* Get_Compass_Dir(float heading) {
    if (heading >= 337.5 || heading < 22.5)  return "NORD";
    if (heading >= 22.5  && heading < 67.5)  return "N-EST";
    if (heading >= 67.5  && heading < 112.5) return "EST";
    if (heading >= 112.5 && heading < 157.5) return "S-EST";
    if (heading >= 157.5 && heading < 202.5) return "SUD";
    if (heading >= 202.5 && heading < 247.5) return "S-VEST";
    if (heading >= 247.5 && heading < 292.5) return "VEST";
    if (heading >= 292.5 && heading < 337.5) return "N-VEST";
    return "???";
}


void Process_Data(char* input) {
    int ang = 0;
    int head = 0;

    char* p1 = strchr(input, ',');
    if (p1) {
        *p1 = 0;
        ang = atoi(input);

        // a doua virgula (care separa shot de busola)
        char* p2 = strchr(p1 + 1, ',');
        if (p2) {
            *p2 = 0;
            head = atoi(p2 + 1);
        }
    }

    currentAngle = (float)ang;
    compassHeading = (float)head;
    paddleY = Map_Angle_To_Screen(currentAngle);


    // textul de directie (SUS/JOS)
    char directionStr[20];
    if (ang > 3) {
        strcpy(directionStr, "SUS (Dreapta)");
    } else if (ang < -3) {
        strcpy(directionStr, "JOS (Stanga)");
    } else {
        strcpy(directionStr, "CENTRU");
    }

    // semnul (+ sau -)
    char semn = (ang >= 0) ? '+' : '-';

    // directia busolei
    const char* compassDir = Get_Compass_Dir(compassHeading);

    PRINTF("Inclinatie: %c%d [%s] | Y: %d | Busola: %d (%s)\r\n",
           semn,
           abs(ang),
           directionStr,    // Textul SUS/JOS
           paddleY,
           head,
           compassDir);
}


int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    PRINTF("FRDM-KL25Z Pong Controller Started...\r\n");

    Init_ESP_UART_Polling();

    while (1) {
        if (Check_UART_Data()) {
            Process_Data(rxBuffer);
        }
    }
}
