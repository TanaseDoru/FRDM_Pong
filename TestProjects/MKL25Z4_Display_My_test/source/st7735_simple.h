#ifndef ST7735_SIMPLE_H
#define ST7735_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

/* ST7735 Commands */
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A
#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

/* Display size */
#define ST7735_WIDTH  128
#define ST7735_HEIGHT 160

/* RGB565 Colors */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20
#define COLOR_GRAY        0x8410
#define COLOR_DARK_GRAY   0x4208

/* Font size */
#define FONT_WIDTH   6
#define FONT_HEIGHT  8

/* Basic Functions */
void ST7735_Init(void);
void ST7735_FillScreen(uint16_t color);
void ST7735_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ST7735_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ST7735_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ST7735_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ST7735_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

/* Text Functions */
void ST7735_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void ST7735_DrawString(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg);
void ST7735_DrawStringScaled(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);
void ST7735_DrawStringCentered(int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);

/* UI Helper Functions */
void ST7735_DrawMenuBox(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t borderColor, uint16_t fillColor);

#endif
