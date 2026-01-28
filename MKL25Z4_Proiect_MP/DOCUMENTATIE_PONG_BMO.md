# Documentatie Tehnica - BMO PONG Game
## Sistem de Joc Pong cu Display ST7735 pe FRDM-KL25Z

---

## Cuprins
1. [Prezentare Generala](#1-prezentare-generala)
2. [Hardware Utilizat](#2-hardware-utilizat)
3. [Sistemul de Display ST7735](#3-sistemul-de-display-st7735)
4. [Sistemul de Timing (SysTick)](#4-sistemul-de-timing-systick)
5. [Sistemul de Input](#5-sistemul-de-input)
6. [Logica Jocului Pong](#6-logica-jocului-pong)
7. [Sistemul de AI/Bot](#7-sistemul-de-aibot)
8. [Sistemul de Meniuri](#8-sistemul-de-meniuri)
9. [Animatia Intro BMO](#9-animatia-intro-bmo)
10. [Diagrame si Flow-uri](#10-diagrame-si-flow-uri)

---

## 1. Prezentare Generala

Acest proiect implementeaza un joc **Pong clasic** in stil arcade pe microcontrollerul **NXP FRDM-KL25Z** (ARM Cortex-M0+), folosind un display **TFT ST7735** de 1.8" (160x128 pixeli).

### Caracteristici Principale:
- Display color RGB565 (65K culori)
- Animatie intro cu fata lui **BMO** (din Adventure Time)
- Sistem de meniuri navigabil
- 3 nivele de dificultate AI (Easy, Medium, Hard)
- Control prin joystick analogic
- Timing precis prin SysTick Timer
- Efecte vizuale arcade (rainbow, pulsare, flash-uri)

---

## 2. Hardware Utilizat

### 2.1 Microcontroller: FRDM-KL25Z
- **MCU**: MKL25Z128VLK4 (ARM Cortex-M0+)
- **Frecventa**: 48 MHz
- **RAM**: 16 KB
- **Flash**: 128 KB

### 2.2 Display: ST7735 TFT 1.8"
- **Rezolutie**: 160 x 128 pixeli
- **Culori**: RGB565 (16-bit, 65536 culori)
- **Interfata**: SPI

### 2.3 Conexiuni Hardware

```
DISPLAY ST7735          FRDM-KL25Z
-----------------------------------------
BL  (Backlight)    -->  3.3V
VCC                -->  3.3V
GND                -->  GND
SCK (Clock)        -->  PTC5 (SPI0_SCK)
SDA (MOSI)         -->  PTC6 (SPI0_MOSI)
DC  (Data/Command) -->  PTC3 (GPIO)
RES (Reset)        -->  PTC0 (GPIO)
CS  (Chip Select)  -->  PTC4 (GPIO)

JOYSTICK               FRDM-KL25Z
-----------------------------------------
VRY (Axa Y)        -->  PTB1 (ADC0_SE9)
SW  (Buton)        -->  PTD4 (GPIO + IRQ)
VCC                -->  3.3V
GND                -->  GND
```

---

## 3. Sistemul de Display ST7735

### 3.1 Initializare SPI

Display-ul comunica prin **SPI** la viteza de **12 MHz**:

```c
/* Configurare SPI */
#define SPI_BAUDRATE     12000000U  /* 12 MHz */

/* Pini folositi */
#define LCD_SCK_PIN      5   /* PTC5 */
#define LCD_MOSI_PIN     6   /* PTC6 */
#define LCD_CS_PIN       4   /* PTC4 */
#define LCD_DC_PIN       3   /* PTC3 */
#define LCD_RST_PIN      0   /* PTC0 */
```

### 3.2 Secventa de Initializare

1. **Reset Hardware**: Puls LOW pe pinul RST (10ms)
2. **Software Reset**: Comanda 0x01
3. **Sleep Out**: Comanda 0x11 (iese din sleep mode)
4. **Color Mode**: RGB565 (16-bit per pixel)
5. **Display On**: Comanda 0x29

### 3.3 Functii de Desenare Principale

```c
/* Umple intreg ecranul cu o culoare */
void ST7735_FillScreen(uint16_t color);

/* Deseneaza un dreptunghi plin */
void ST7735_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/* Deseneaza un caracter cu dimensiune scalabila */
void ST7735_DrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);

/* Deseneaza un string centrat pe ecran */
void ST7735_DrawStringCentered(int16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);

/* Deseneaza linii orizontale/verticale */
void ST7735_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ST7735_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
```

### 3.4 Format Culori RGB565

Culorile sunt codificate pe 16 biti:
- **Rosu**: 5 biti (bits 15-11)
- **Verde**: 6 biti (bits 10-5)
- **Albastru**: 5 biti (bits 4-0)

```c
/* Macro pentru conversie RGB la RGB565 */
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

/* Culori predefinite */
#define COLOR_BLACK      0x0000
#define COLOR_WHITE      0xFFFF
#define COLOR_RED        0xF800
#define COLOR_GREEN      0x07E0
#define COLOR_BLUE       0x001F
#define COLOR_YELLOW     0xFFE0
#define COLOR_CYAN       0x07FF
#define COLOR_MAGENTA    0xF81F
#define COLOR_ORANGE     0xFD20
```

---

## 4. Sistemul de Timing (SysTick)

### 4.1 De ce SysTick?

In loc de **busy-wait delays** (care blocheaza CPU-ul), folosim **SysTick Timer** pentru:
- Timing precis (1ms rezolutie)
- Economie de energie (CPU poate intra in sleep)
- Posibilitatea de a face mai multe task-uri "in paralel"

### 4.2 Configurare SysTick

```c
static volatile uint32_t g_systick_ms = 0;  /* Contor global milisecunde */

/* Handler-ul se apeleaza la fiecare 1ms */
void SysTick_Handler(void) {
    g_systick_ms++;
}

/* Initializare pentru 1ms interrupt */
void Timer_Init(void) {
    /* SystemCoreClock = 48MHz
     * Pentru 1ms: 48000000 / 1000 = 48000 ticks */
    SysTick_Config(SystemCoreClock / 1000U);
}
```

### 4.3 Functii de Timing

```c
/* Returneaza timpul curent in milisecunde */
uint32_t Timer_GetMs(void) {
    return g_systick_ms;
}

/* Delay BLOCANT (dar mai eficient decat busy-wait) */
void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms) {
        __WFI();  /* Wait For Interrupt - economiseste energie */
    }
}

/* Verifica daca a trecut un anumit timp (NON-BLOCANT) */
bool Timer_Elapsed(uint32_t start_time, uint32_t duration_ms) {
    return ((g_systick_ms - start_time) >= duration_ms);
}
```

### 4.4 Frame Rate Control

Jocul ruleaza la **~50 FPS** (20ms per frame):

```c
uint32_t last_game_update = 0;
const uint32_t GAME_FRAME_MS = 20;  /* ~50 FPS */

while (1) {
    uint32_t now = Timer_GetMs();

    if ((now - last_game_update) >= GAME_FRAME_MS) {
        last_game_update = now;
        Game_Update();  /* Actualizeaza logica jocului */
    }

    __WFI();  /* Sleep intre frame-uri */
}
```

---

## 5. Sistemul de Input

### 5.1 Joystick Analogic (ADC)

Joystick-ul are doua axe (X si Y) conectate la **ADC** (Analog-to-Digital Converter):

```c
#define JOYSTICK_VRY_CHANNEL 9U  /* PTB1 - Axa Y */

/* Citeste valoarea ADC (0-4095 pentru 12-bit) */
static uint16_t Joystick_ReadADC(uint32_t channel) {
    adc16_channel_config_t chConfig = {
        .channelNumber = channel,
        .enableInterruptOnConversionCompleted = false,
        .enableDifferentialConversion = false
    };
    ADC16_SetChannelConfig(ADC0, 0, &chConfig);
    while (!(ADC16_GetChannelStatusFlags(ADC0, 0) & kADC16_ChannelConversionDoneFlag));
    return ADC16_GetChannelConversionValue(ADC0, 0);
}
```

### 5.2 Conversie la Procent

Valoarea ADC (0-4095) este convertita la procent (-100 la +100):

```c
void Joystick_Process(void) {
    uint16_t y_raw = Joystick_ReadADC(JOYSTICK_VRY_CHANNEL);
    /* Centrul este ~2048 */
    g_joy_y_percent = ((int32_t)y_raw - 2048) * 100 / 2048;
}
```

### 5.3 Buton Joystick (Interrupt)

Butonul este conectat pe **PTD4** cu **interrupt pe falling edge**:

```c
void PORTD_IRQHandler(void) {
    uint32_t isfr = PORT_GetPinsInterruptFlags(JOYSTICK_SW_PORT);
    if (isfr & (1U << JOYSTICK_SW_PIN)) {
        PORT_ClearPinsInterruptFlags(JOYSTICK_SW_PORT, 1U << JOYSTICK_SW_PIN);
        g_joy_btn_pressed = true;  /* Flag setat, procesat in main loop */
    }
}
```

### 5.4 Tipuri de Input Suportate

```c
typedef enum {
    INPUT_NONE = 0,
    INPUT_KEYBOARD,      /* Serial/UART (pentru debug) */
    INPUT_JOYSTICK,      /* Joystick analogic */
    INPUT_REMOTE,        /* IR Remote (optional) */
    INPUT_CPU_EASY,      /* Bot - nivel usor */
    INPUT_CPU_MEDIUM,    /* Bot - nivel mediu */
    INPUT_CPU_HARD       /* Bot - nivel greu */
} InputType_t;
```

---

## 6. Logica Jocului Pong

### 6.1 Structuri de Date

```c
/* Bila */
typedef struct {
    int16_t x, y;           /* Pozitia curenta */
    int16_t dx, dy;         /* Viteza (directie + magnitudine) */
    int16_t prev_x, prev_y; /* Pozitia anterioara (pentru stergere) */
    uint8_t size;           /* Dimensiunea in pixeli */
} Ball_t;

/* Paleta */
typedef struct {
    int16_t y;              /* Pozitia Y (X este fix) */
    int16_t prev_y;         /* Pozitia anterioara */
    int16_t score;          /* Scorul jucatorului */
    InputType_t input;      /* Tipul de control */
    int16_t target_y;       /* Tinta pentru AI */
} Paddle_t;

/* Starea jocului */
typedef struct {
    uint8_t is_running;
    uint8_t is_paused;
    uint8_t winner;         /* 0=nimeni, 1=P1, 2=P2 */
    uint8_t winning_score;  /* Scorul pentru castig (default 5) */
    uint16_t frame_count;
    uint16_t rally_frames;  /* Frame-uri de la ultimul gol */
    uint8_t speed_level;    /* Nivel viteza (0-5) */
} GameState_t;
```

### 6.2 Constante de Joc

```c
#define FIELD_WIDTH      160
#define FIELD_HEIGHT     128
#define PADDLE_WIDTH     4
#define PADDLE_HEIGHT    22
#define PADDLE_X_P1      4      /* Pozitia X paleta stanga */
#define PADDLE_X_P2      152    /* Pozitia X paleta dreapta */
#define BALL_SIZE        4
#define BALL_START_X     80     /* Centrul ecranului */
#define BALL_START_Y     64

/* Viteze */
#define PADDLE_SPEED     4
#define BALL_SPEED_X     1      /* Viteza initiala mica */
#define BALL_SPEED_Y     1

/* Accelerare */
#define SPEED_UP_INTERVAL   500   /* ~10 secunde pana la accelerare */
#define MAX_SPEED_LEVEL     5
```

### 6.3 Bucla Principala de Joc (Game_Update)

```
1. CITESTE INPUT
   - Joystick/Keyboard/AI pentru fiecare paleta

2. MISCA PALETELE
   - Aplica viteza bazata pe input
   - Limiteaza pozitia la marginile ecranului

3. MISCA BILA
   - Sterge bila de la pozitia veche
   - Actualizeaza pozitia: x += dx, y += dy

4. DETECTEAZA COLIZIUNI
   - Pereti sus/jos: inverseaza dy
   - Paleta stanga: inverseaza dx, ajusteaza pozitia
   - Paleta dreapta: inverseaza dx, ajusteaza pozitia

5. VERIFICA GOL
   - Bila iese pe stanga: P2 marcheaza
   - Bila iese pe dreapta: P1 marcheaza
   - Reset bila la centru

6. ACCELERARE BILA
   - La fiecare SPEED_UP_INTERVAL frame-uri
   - Creste dx si dy (pana la MAX_SPEED_LEVEL)

7. DESENEAZA
   - Deseneaza paletele (doar daca s-au miscat)
   - Deseneaza bila la noua pozitie
   - Actualizeaza scorul daca s-a schimbat
```

### 6.4 Detectie Coliziuni

```c
/* Coliziune cu peretii sus/jos */
if (ball.y <= 0 || ball.y >= FIELD_HEIGHT - ball.size) {
    ball.dy = -ball.dy;  /* Inverseaza directia Y */
}

/* Coliziune cu paleta stanga (P1) */
if (ball.x <= PADDLE_X_P1 + PADDLE_WIDTH &&
    ball.y + ball.size >= paddle1.y &&
    ball.y <= paddle1.y + PADDLE_HEIGHT) {
    ball.dx = -ball.dx;  /* Inverseaza directia X */
    ball.x = PADDLE_X_P1 + PADDLE_WIDTH + 1;  /* Previne "lipirea" */
}
```

### 6.5 Sistem de Scor

```c
/* Bila iese pe stanga - P2 marcheaza */
if (ball.x < 0) {
    paddle2.score++;
    ResetBall();
}

/* Bila iese pe dreapta - P1 marcheaza */
if (ball.x > FIELD_WIDTH) {
    paddle1.score++;
    ResetBall();
}

/* Verificare castigator */
if (paddle1.score >= SCORE_TO_WIN) {
    game.winner = 1;
    game.is_running = 0;
}
```

---

## 7. Sistemul de AI/Bot

### 7.1 Filosofia AI-ului

AI-ul **simuleaza** unde va ajunge bila si incearca sa mute paleta acolo. Pentru a face jocul echilibrat, AI-ul are **imperfectiuni intentionate**:
- **Timp de reactie**: Nu reactioneaza instant
- **Eroare de predictie**: Nu nimereste perfect centrul
- **Greseli aleatorii**: Uneori face miscari gresite

### 7.2 Parametri per Dificultate

```c
switch (paddle->input) {
    case INPUT_CPU_EASY:
        speed = 1;              /* Viteza mica */
        reaction_zone = 35;     /* Reactioneaza doar cand bila e aproape */
        error_margin = 35;      /* Eroare mare de predictie */
        mistake_chance = 35;    /* 35% sansa de greseala */
        update_interval = 20;   /* Actualizeaza rar tinta */
        break;

    case INPUT_CPU_MEDIUM:
        speed = 2;
        reaction_zone = 80;
        error_margin = 15;
        mistake_chance = 12;
        update_interval = 10;
        break;

    case INPUT_CPU_HARD:
        speed = 4;              /* Viteza maxima */
        reaction_zone = 160;    /* Reactioneaza de oriunde */
        error_margin = 5;       /* Foarte precis */
        mistake_chance = 0;     /* Fara greseli */
        update_interval = 3;    /* Actualizeaza foarte des */
        break;
}
```

### 7.3 Algoritmul de Predictie

```c
void AI_Update(Paddle_t* paddle) {
    /* 1. Verifica daca bila vine spre aceasta paleta */
    bool ball_coming = (paddle == &paddle2) ? (ball.dx > 0) : (ball.dx < 0);

    if (!ball_coming) {
        /* Bila pleaca - revino la centru */
        paddle->target_y = FIELD_HEIGHT / 2 - PADDLE_HEIGHT / 2;
        return;
    }

    /* 2. Verifica zona de reactie */
    int16_t distance = abs(ball.x - paddle_x);
    if (distance > reaction_zone) return;

    /* 3. Predictie simpla - unde va fi bila cand ajunge la paleta */
    int16_t predicted_y = ball.y;
    int16_t sim_x = ball.x;
    int16_t sim_dy = ball.dy;

    while (sim_x > PADDLE_X_P1 && sim_x < PADDLE_X_P2) {
        sim_x += ball.dx;
        predicted_y += sim_dy;

        /* Simuleaza bounce pe pereti */
        if (predicted_y < 0 || predicted_y > FIELD_HEIGHT) {
            sim_dy = -sim_dy;
        }
    }

    /* 4. Adauga eroare de predictie */
    predicted_y += (rand() % error_margin) - (error_margin / 2);

    /* 5. Sansa de greseala aleatorie */
    if ((rand() % 100) < mistake_chance) {
        predicted_y += (rand() % 40) - 20;  /* Deviaza random */
    }

    paddle->target_y = predicted_y - PADDLE_HEIGHT / 2;
}
```

### 7.4 Miscare spre Tinta

```c
void AI_MovePaddle(Paddle_t* paddle) {
    int16_t diff = paddle->target_y - paddle->y;

    if (abs(diff) > 2) {
        if (diff > 0) {
            paddle->y += speed;  /* Misca in jos */
        } else {
            paddle->y -= speed;  /* Misca in sus */
        }
    }

    /* Limiteaza la marginile ecranului */
    if (paddle->y < PADDLE_MIN_Y) paddle->y = PADDLE_MIN_Y;
    if (paddle->y > PADDLE_MAX_Y) paddle->y = PADDLE_MAX_Y;
}
```

---

## 8. Sistemul de Meniuri

### 8.1 Ecranele Disponibile

```c
typedef enum {
    SCREEN_INTRO = 0,       /* Animatia BMO + PONG */
    SCREEN_MAIN,            /* Meniu principal */
    SCREEN_START,           /* Start Game submeniu */
    SCREEN_SELECT_INPUT,    /* Selectie tip input */
    SCREEN_SELECT_P1,       /* Input pentru Player 1 */
    SCREEN_SELECT_P2,       /* Input pentru Player 2 */
    SCREEN_DIFFICULTY,      /* Selectie dificultate CPU */
    SCREEN_GAMEPLAY,        /* Jocul propriu-zis */
    SCREEN_GAME_OVER,       /* Ecran final */
    SCREEN_PAUSED           /* Meniu pauza */
} Screen_t;
```

### 8.2 Navigare Meniu

```c
/* Starea meniului */
typedef struct {
    uint8_t selectedIndex;  /* Optiunea selectata curent */
    uint8_t maxItems;       /* Numarul total de optiuni */
} MenuState_t;

/* Navigare sus */
void Menu_MoveUp(void) {
    if (g_menuState.selectedIndex > 0) {
        g_menuState.selectedIndex--;
    } else {
        g_menuState.selectedIndex = g_menuState.maxItems - 1;  /* Wrap */
    }
    g_needsRedraw = 1;
}

/* Navigare jos */
void Menu_MoveDown(void) {
    if (g_menuState.selectedIndex < g_menuState.maxItems - 1) {
        g_menuState.selectedIndex++;
    } else {
        g_menuState.selectedIndex = 0;  /* Wrap */
    }
    g_needsRedraw = 1;
}
```

### 8.3 Flow-ul Meniurilor

```
SCREEN_INTRO (Animatie BMO)
    |
    v [Buton]
SCREEN_MAIN
    |-- Start Game --> SCREEN_START
    |                      |-- Player vs CPU --> SCREEN_DIFFICULTY --> SCREEN_GAMEPLAY
    |                      |-- Player vs Player --> SCREEN_GAMEPLAY
    |                      |-- Back --> SCREEN_MAIN
    |
    |-- Select Input --> SCREEN_SELECT_INPUT
    |                      |-- Player 1 Input --> SCREEN_SELECT_P1 --> auto-back la P2
    |                      |-- Player 2 Input --> SCREEN_SELECT_P2 --> auto-back
    |                      |-- Back --> SCREEN_MAIN
    |
    |-- About --> (info)

SCREEN_GAMEPLAY
    |
    v [Buton]
SCREEN_PAUSED
    |-- Resume --> SCREEN_GAMEPLAY
    |-- Exit --> SCREEN_MAIN

SCREEN_GAME_OVER
    |-- Play Again --> SCREEN_GAMEPLAY
    |-- Main Menu --> SCREEN_MAIN
```

### 8.4 Validare Input (Evita Duplicate)

```c
/* Verifica daca un input este disponibil pentru un jucator */
bool IsInputAvailable(InputType_t input, uint8_t forPlayer) {
    /* CPU inputs pot fi folosite de oricine */
    if (input >= INPUT_CPU_EASY) return true;

    /* Verifica daca celalalt jucator foloseste deja acest input */
    if (forPlayer == 1) {
        return (g_player2_input != input);
    } else {
        return (g_player1_input != input);
    }
}
```

---

## 9. Animatia Intro BMO

### 9.1 Fata lui BMO

BMO (din Adventure Time) are o fata simpla pe un ecran verde:

```c
#define BMO_SCREEN_COLOR  0x0400  /* Verde inchis */
#define BMO_FACE_COLOR    COLOR_BLACK

void DrawBMOFace(uint8_t expression, uint8_t blink) {
    /* Fundal verde */
    ST7735_FillScreen(BMO_SCREEN_COLOR);

    /* Ochi - patrate 25x25 cu highlight alb */
    if (!blink) {
        ST7735_FillRect(35, 25, 25, 25, BMO_FACE_COLOR);   /* Stanga */
        ST7735_FillRect(100, 25, 25, 25, BMO_FACE_COLOR);  /* Dreapta */
        ST7735_FillRect(42, 30, 10, 10, COLOR_WHITE);      /* Highlight */
        ST7735_FillRect(107, 30, 10, 10, COLOR_WHITE);
    } else {
        /* Ochi inchisi - linii */
        ST7735_FillRect(30, 40, 35, 5, BMO_FACE_COLOR);
        ST7735_FillRect(95, 40, 35, 5, BMO_FACE_COLOR);
    }

    /* Gura - depinde de expresie */
    /* 0=Neutral, 1=Happy, 2=Excited, 3=Wink */
}
```

### 9.2 Secventa Animatiei

```
1. Ecran verde BMO (300ms)
2. Fata neutra + clipire
3. Text "Hi!" (600ms)
4. BMO zambeste
5. Text "Let's play" (500ms)
6. BMO excited (gura deschisa)
7. Text "PONG!" mare galben (800ms)
8. BMO clipeste fericit
9. BMO face wink
10. Pauza 1 secunda
11. Flash tranzitie (alb)
12. --> Animatia PONG (scanlines rainbow, titlu animat, demo)
```

### 9.3 Efecte Rainbow in Demo

```c
/* Culori pentru efecte */
static const uint16_t rainbow_colors[] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW,
    COLOR_GREEN, COLOR_CYAN, COLOR_BLUE, COLOR_MAGENTA
};

/* In bucla demo: */
/* Bordura care isi schimba culoarea */
if ((now - last_border_update) >= 150) {
    border_color_idx = (border_color_idx + 1) % 7;
    ST7735_DrawRect(4, 4, 152, 120, rainbow_colors[border_color_idx]);
}

/* Titlu PONG cu litere care pulseaza */
for (int i = 0; i < 4; i++) {
    uint8_t color_idx = (title_glow + i * 2) % 7;
    ST7735_DrawChar(letter_x[i], 15, letters[i],
                   rainbow_colors[color_idx], COLOR_BLACK, 3);
}

/* Mingea cu culoare rainbow */
uint16_t ball_color = rainbow_colors[(now / 200) % 7];
ST7735_FillRect(ball_x, ball_y, BALL_SZ, BALL_SZ, ball_color);
```

---

## 10. Diagrame si Flow-uri

### 10.1 Diagrama Bloc Hardware

```
                    +------------------+
                    |   FRDM-KL25Z     |
                    |   (MKL25Z128)    |
                    +--------+---------+
                             |
         +-------------------+-------------------+
         |                   |                   |
    +----+----+        +-----+-----+       +-----+-----+
    |   SPI   |        |    ADC    |       |   GPIO    |
    | (12MHz) |        | (12-bit)  |       |  + IRQ    |
    +----+----+        +-----+-----+       +-----+-----+
         |                   |                   |
    +----+----+        +-----+-----+       +-----+-----+
    | ST7735  |        | Joystick  |       |  Buton    |
    | Display |        |   VRY     |       | Joystick  |
    | 160x128 |        |  (PTB1)   |       |  (PTD4)   |
    +---------+        +-----------+       +-----------+
```

### 10.2 Flow Principal Program

```
main()
    |
    +-> BOARD_Init...()
    +-> Timer_Init()         // SysTick 1ms
    +-> ST7735_Init()        // Display
    +-> Joystick_Init()      // ADC + GPIO
    |
    +-> DrawCurrentScreen()  // SCREEN_INTRO -> PlayIntroAnimation()
    |
    +-> while(1) {
            now = Timer_GetMs();
            Joystick_Process();

            if (SCREEN_GAMEPLAY) {
                ProcessJoystickGameInput();
                if (elapsed >= 20ms) {
                    Game_Update();
                }
            }
            else if (SCREEN_PAUSED) {
                ProcessJoystickGameInput();  // Resume/Exit
            }
            else {
                ProcessJoystickMenuInput();  // Navigare meniu
            }

            if (g_needsRedraw) {
                DrawCurrentScreen();
            }

            __WFI();  // Sleep
        }
```

### 10.3 Flow Game_Update()

```
Game_Update()
    |
    +-> Citeste input P1 (Joystick/AI)
    +-> Citeste input P2 (AI/Joystick)
    |
    +-> Misca paletele
    +-> Limiteaza pozitii
    |
    +-> Sterge bila veche
    +-> Misca bila (x += dx, y += dy)
    |
    +-> Coliziune pereti? -> dy = -dy
    +-> Coliziune P1? -> dx = -dx
    +-> Coliziune P2? -> dx = -dx
    |
    +-> Gol P1? -> P2.score++, Reset
    +-> Gol P2? -> P1.score++, Reset
    |
    +-> Castigator? -> SCREEN_GAME_OVER
    |
    +-> Speed up? -> dx++, dy++
    |
    +-> Deseneaza palete
    +-> Deseneaza bila
    +-> Deseneaza scor (daca s-a schimbat)
```

---

## Anexa: Cod Complet Util

### A.1 Initializare Completa

```c
int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    Timer_Init();      // SysTick
    ST7735_Init();     // Display
    Joystick_Init();   // Input

    srand(g_systick_ms ^ 0xDEADBEEF);  // Seed random

    DrawCurrentScreen();  // Porneste cu SCREEN_INTRO

    // Main loop...
}
```

### A.2 Paleta de Culori Completa

```c
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
```

---

**Document generat pentru proiectul BMO PONG**
**FRDM-KL25Z + ST7735 Display**
**Decembrie 2024**
