#include "st7735_simple.h"
#include "fsl_spi.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include <string.h>

/* PINOUT HARDWARE SPI:
 * SCK  -> PTC5 (SPI0_SCK)
 * SDA  -> PTC7 (SPI0_MOSI)
 * CS   -> PTC4
 * DC   -> PTC3
 * RES  -> PTC0
 */

/* Pin macros - acces direct la registre pentru viteza maxima */
#define CS_LOW()     (GPIOC->PCOR = (1U << 4))
#define CS_HIGH()    (GPIOC->PSOR = (1U << 4))
#define DC_LOW()     (GPIOC->PCOR = (1U << 3))
#define DC_HIGH()    (GPIOC->PSOR = (1U << 3))
#define RST_LOW()    (GPIOC->PCOR = (1U << 0))
#define RST_HIGH()   (GPIOC->PSOR = (1U << 0))

/* 5x7 Font */
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00,
    0x14, 0x7F, 0x14, 0x7F, 0x14, 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62,
    0x36, 0x49, 0x55, 0x22, 0x50, 0x00, 0x05, 0x03, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x41, 0x00,
    0x00, 0x41, 0x22, 0x1C, 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x08, 0x08, 0x3E, 0x08, 0x08,
    0x00, 0x50, 0x30, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x60, 0x60, 0x00, 0x00,
    0x20, 0x10, 0x08, 0x04, 0x02, 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00,
    0x42, 0x61, 0x51, 0x49, 0x46, 0x21, 0x41, 0x45, 0x4B, 0x31, 0x18, 0x14, 0x12, 0x7F, 0x10,
    0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x01, 0x71, 0x09, 0x05, 0x03,
    0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x36, 0x36, 0x00, 0x00,
    0x00, 0x56, 0x36, 0x00, 0x00, 0x08, 0x14, 0x22, 0x41, 0x00, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x00, 0x41, 0x22, 0x14, 0x08, 0x02, 0x01, 0x51, 0x09, 0x06, 0x32, 0x49, 0x79, 0x41, 0x3E,
    0x7E, 0x11, 0x11, 0x11, 0x7E, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E, 0x41, 0x41, 0x41, 0x22,
    0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09, 0x09, 0x09, 0x01,
    0x3E, 0x41, 0x49, 0x49, 0x7A, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00,
    0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40, 0x40,
    0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
    0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46,
    0x46, 0x49, 0x49, 0x49, 0x31, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F,
    0x1F, 0x20, 0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14, 0x63,
    0x07, 0x08, 0x70, 0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x00,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41, 0x7F, 0x00, 0x04, 0x02, 0x01, 0x02, 0x04,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 0x02, 0x04, 0x00, 0x20, 0x54, 0x54, 0x54, 0x78,
    0x7F, 0x48, 0x44, 0x44, 0x38, 0x38, 0x44, 0x44, 0x44, 0x20, 0x38, 0x44, 0x44, 0x48, 0x7F,
    0x38, 0x54, 0x54, 0x54, 0x18, 0x08, 0x7E, 0x09, 0x01, 0x02, 0x0C, 0x52, 0x52, 0x52, 0x3E,
    0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D, 0x40, 0x00, 0x20, 0x40, 0x44, 0x3D, 0x00,
    0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78,
    0x7C, 0x08, 0x04, 0x04, 0x78, 0x38, 0x44, 0x44, 0x44, 0x38, 0x7C, 0x14, 0x14, 0x14, 0x08,
    0x08, 0x14, 0x14, 0x18, 0x7C, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48, 0x54, 0x54, 0x54, 0x20,
    0x04, 0x3F, 0x44, 0x40, 0x20, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x1C, 0x20, 0x40, 0x20, 0x1C,
    0x3C, 0x40, 0x30, 0x40, 0x3C, 0x44, 0x28, 0x10, 0x28, 0x44, 0x0C, 0x50, 0x50, 0x50, 0x3C,
    0x44, 0x64, 0x54, 0x4C, 0x44
};

static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 6000; i++) __asm volatile("nop");
}

/* SPI Write optimizat - inline pentru viteza */
static inline void SPI_WriteByteFast(uint8_t data) {
    /* 1. Așteaptă până când bufferul de TRANSMISIE e gol (SPTEF) */
    while (!(SPI0->S & kSPI_TxBufferEmptyFlag));

    /* 2. Scrie datele */
    SPI0->D = data;

    /* 3. Așteaptă până când datele au fost trimise și s-a primit răspuns (SPRF) */
    while (!(SPI0->S & kSPI_RxBufferFullFlag));

    /* 4. Citește datele primite (dummy) pentru a curăța flag-ul */
    (void)SPI0->D;
}

/* SPI Write pentru mai multi bytes - fara overhead */
static inline void SPI_WriteDataFast(uint8_t *data, uint32_t len) {
    while (len--) {
        SPI0->D = *data++;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
    }
}

static void WriteCommand(uint8_t cmd) {
    DC_LOW();
    CS_LOW();
    SPI_WriteByteFast(cmd);
    CS_HIGH();
}

static void WriteData(uint8_t data) {
    DC_HIGH();
    CS_LOW();
    SPI_WriteByteFast(data);
    CS_HIGH();
}

/* SetWindow optimizat - mai putine CS toggles */
static void SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    DC_LOW();
    CS_LOW();
    SPI_WriteByteFast(ST7735_CASET);
    DC_HIGH();
    SPI_WriteByteFast(0x00);
    SPI_WriteByteFast(x0);
    SPI_WriteByteFast(0x00);
    SPI_WriteByteFast(x1);

    DC_LOW();
    SPI_WriteByteFast(ST7735_RASET);
    DC_HIGH();
    SPI_WriteByteFast(0x00);
    SPI_WriteByteFast(y0);
    SPI_WriteByteFast(0x00);
    SPI_WriteByteFast(y1);

    DC_LOW();
    SPI_WriteByteFast(ST7735_RAMWR);
    CS_HIGH();
}

void ST7735_Init(void) {
    spi_master_config_t spiConfig;
    gpio_pin_config_t gpioConfig = {kGPIO_DigitalOutput, 1};

    /* Enable clocks */
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_Spi0);

    /* Configure SPI pins (hardware) */
    PORT_SetPinMux(PORTC, 5U, kPORT_MuxAlt2);  /* SCK */
//    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt2);  /* MOSI */
    PORT_SetPinMux(PORTC, 6U, kPORT_MuxAlt2);  /* MOSI Corect (PTC6) */
    /* Configure GPIO pins */
    PORT_SetPinMux(PORTC, 4U, kPORT_MuxAsGpio); /* CS */
    PORT_SetPinMux(PORTC, 3U, kPORT_MuxAsGpio); /* DC */
    PORT_SetPinMux(PORTC, 0U, kPORT_MuxAsGpio); /* RST */

    GPIO_PinInit(GPIOC, 4U, &gpioConfig);
    GPIO_PinInit(GPIOC, 3U, &gpioConfig);
    GPIO_PinInit(GPIOC, 0U, &gpioConfig);

    /* Configure SPI0 - viteza rapida pentru afisare fluida */
    SPI_MasterGetDefaultConfig(&spiConfig);
    spiConfig.baudRate_Bps = 4000000U;  /* 4 MHz - rapid! */
    spiConfig.polarity = kSPI_ClockPolarityActiveHigh;  /* CPOL=0 */
    spiConfig.phase = kSPI_ClockPhaseFirstEdge;         /* CPHA=0 */
    spiConfig.direction = kSPI_MsbFirst;

    SPI_MasterInit(SPI0, &spiConfig, CLOCK_GetFreq(kCLOCK_BusClk));
    SPI_Enable(SPI0, true);

    /* Reset display - LONGER delays for slow displays */
    RST_HIGH(); delay_ms(20);
    RST_LOW(); delay_ms(100);
    RST_HIGH(); delay_ms(200);

    /* Init sequence with LONGER delays */
    WriteCommand(ST7735_SWRESET); delay_ms(200);
    WriteCommand(ST7735_SLPOUT); delay_ms(200);

    WriteCommand(ST7735_FRMCTR1);
    WriteData(0x01); WriteData(0x2C); WriteData(0x2D);
    delay_ms(10);

    WriteCommand(ST7735_FRMCTR2);
    WriteData(0x01); WriteData(0x2C); WriteData(0x2D);
    delay_ms(10);

    WriteCommand(ST7735_FRMCTR3);
    WriteData(0x01); WriteData(0x2C); WriteData(0x2D);
    WriteData(0x01); WriteData(0x2C); WriteData(0x2D);
    delay_ms(10);

    WriteCommand(ST7735_INVCTR);
    WriteData(0x07);
    delay_ms(10);

    WriteCommand(ST7735_PWCTR1);
    WriteData(0xA2); WriteData(0x02); WriteData(0x84);
    delay_ms(10);

    WriteCommand(ST7735_PWCTR2);
    WriteData(0xC5);
    delay_ms(10);

    WriteCommand(ST7735_PWCTR3);
    WriteData(0x0A); WriteData(0x00);
    delay_ms(10);

    WriteCommand(ST7735_PWCTR4);
    WriteData(0x8A); WriteData(0x2A);
    delay_ms(10);

    WriteCommand(ST7735_PWCTR5);
    WriteData(0x8A); WriteData(0xEE);
    delay_ms(10);

    WriteCommand(ST7735_VMCTR1);
    WriteData(0x0E);
    delay_ms(10);

    WriteCommand(ST7735_INVOFF);
    delay_ms(10);

    WriteCommand(ST7735_MADCTL);
//    WriteData(0xC8);
    WriteData(0xA8);
    delay_ms(10);

    WriteCommand(ST7735_COLMOD);
    WriteData(0x05);
    delay_ms(10);

    WriteCommand(ST7735_CASET);
    WriteData(0x00); WriteData(0x00);
    WriteData(0x00); WriteData(0x7F);
    delay_ms(10);

    WriteCommand(ST7735_RASET);
    WriteData(0x00); WriteData(0x00);
    WriteData(0x00); WriteData(0x9F);
    delay_ms(10);

    WriteCommand(ST7735_GMCTRP1);
    WriteData(0x02); WriteData(0x1c); WriteData(0x07); WriteData(0x12);
    WriteData(0x37); WriteData(0x32); WriteData(0x29); WriteData(0x2d);
    WriteData(0x29); WriteData(0x28); WriteData(0x2B); WriteData(0x37);
    WriteData(0x00); WriteData(0x01); WriteData(0x03); WriteData(0x10);
    delay_ms(10);

    WriteCommand(ST7735_GMCTRN1);
    WriteData(0x03); WriteData(0x1d); WriteData(0x07); WriteData(0x06);
    WriteData(0x2E); WriteData(0x2C); WriteData(0x29); WriteData(0x2D);
    WriteData(0x2E); WriteData(0x2E); WriteData(0x37); WriteData(0x3F);
    WriteData(0x00); WriteData(0x00); WriteData(0x02); WriteData(0x10);
    delay_ms(10);

    WriteCommand(ST7735_NORON);
    delay_ms(100);

    WriteCommand(ST7735_DISPON);
    delay_ms(200);
}

void ST7735_FillScreen(uint16_t color) {
    ST7735_FillRect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_DrawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= ST7735_WIDTH || y < 0 || y >= ST7735_HEIGHT) return;
    SetWindow(x, y, x, y);
    DC_HIGH();
    CS_LOW();
    SPI_WriteByteFast(color >> 8);
    SPI_WriteByteFast(color & 0xFF);
    CS_HIGH();
}

void ST7735_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    SetWindow(x, y, x + w - 1, y + h - 1);
    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint32_t pixels = (uint32_t)w * h;

    DC_HIGH();
    CS_LOW();

    /* Trimite pixeli rapid fara overhead */
    while (pixels--) {
        SPI0->D = hi;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
        SPI0->D = lo;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
    }

    CS_HIGH();
}

/* Desenare linie orizontala - optimizata */
void ST7735_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= ST7735_HEIGHT || x >= ST7735_WIDTH) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
    if (w <= 0) return;

    SetWindow(x, y, x + w - 1, y);
    uint8_t hi = color >> 8, lo = color & 0xFF;

    DC_HIGH();
    CS_LOW();
    while (w--) {
        SPI0->D = hi;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
        SPI0->D = lo;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
    }
    CS_HIGH();
}

/* Desenare linie verticala - optimizata */
void ST7735_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
    if (h <= 0) return;

    SetWindow(x, y, x, y + h - 1);
    uint8_t hi = color >> 8, lo = color & 0xFF;

    DC_HIGH();
    CS_LOW();
    while (h--) {
        SPI0->D = hi;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
        SPI0->D = lo;
        while (!(SPI0->S & SPI_S_SPRF_MASK));
        (void)SPI0->D;
    }
    CS_HIGH();
}

/* Desenare dreptunghi (doar contur) */
void ST7735_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    ST7735_DrawHLine(x, y, w, color);
    ST7735_DrawHLine(x, y + h - 1, w, color);
    ST7735_DrawVLine(x, y, h, color);
    ST7735_DrawVLine(x + w - 1, y, h, color);
}

/* Desenare caracter cu scaling */
void ST7735_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 122) return;

    const uint8_t* glyph = &font5x7[(c - 32) * 5];

    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (int8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                if (size == 1) {
                    ST7735_DrawPixel(x + i, y + j, color);
                } else {
                    ST7735_FillRect(x + i * size, y + j * size, size, size, color);
                }
            } else if (bg != color) {
                if (size == 1) {
                    ST7735_DrawPixel(x + i, y + j, bg);
                } else {
                    ST7735_FillRect(x + i * size, y + j * size, size, size, bg);
                }
            }
        }
    }
}

/* Desenare string simpla (marime 1) */
void ST7735_DrawString(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg) {
    while (*str) {
        ST7735_DrawChar(x, y, *str, color, bg, 1);
        x += FONT_WIDTH;
        str++;
    }
}

/* Desenare string cu scaling */
void ST7735_DrawStringScaled(int16_t x, int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    while (*str) {
        ST7735_DrawChar(x, y, *str, color, bg, size);
        x += FONT_WIDTH * size;
        str++;
    }
}

/* Desenare string centrat pe ecran */
void ST7735_DrawStringCentered(int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t len = strlen(str);
    int16_t x = (ST7735_WIDTH - len * FONT_WIDTH * size) / 2;
    ST7735_DrawStringScaled(x, y, str, color, bg, size);
}

/* Desenare box pentru meniu cu border */
void ST7735_DrawMenuBox(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t borderColor, uint16_t fillColor) {
    ST7735_FillRect(x + 1, y + 1, w - 2, h - 2, fillColor);
    ST7735_DrawRect(x, y, w, h, borderColor);
}
