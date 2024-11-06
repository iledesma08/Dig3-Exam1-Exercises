// CLK de 16 MHz -> Max Systick time = ((2^24 - 1)/16e6)*1000 = 1048 ms
// Pulsador en EINT0 (P2.10) para reinicia cuenta en 0 y se mantiene en ese valor hasta que se suelte
// Pulsador en EINT1 (P2.11) para pausar/reanudar cuenta cada vez que se presiona
// Pulsador en EINT2 (P2.12) para cambiar la velocidad de la cuenta (1s o 1ms)

#include "LPC17xx.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_systick.h"

#define SYSTICK_TIME_MS 1
#define DIGIT_PER_SECOND 1000
#define DIGIT_PER_MS 1

#define DIFFERENT_DIGITS 10

#define INPUT 1
#define OUTPUT 0

#define RESET_COUNT_PIN ((uint32_t)(1<<10)) // P2.10 (EINT0) para resetear la cuenta
#define PAUSE_RESUME_COUNT_PIN ((uint32_t)(1<<11)) // P2.11 (EINT1) para pausar/reanudar la cuenta
#define COUNT_RATE_PIN ((uint32_t)(1<<12)) // P2.12 (EINT2) para cambiar la velocidad de la cuenta
#define A_SEGMENT_PIN ((uint32_t)(1<<0)) // P0.0 para el segmento A del display
#define B_SEGMENT_PIN ((uint32_t)(1<<1)) // P0.1 para el segmento B del display
#define C_SEGMENT_PIN ((uint32_t)(1<<2)) // P0.2 para el segmento C del display
#define D_SEGMENT_PIN ((uint32_t)(1<<3)) // P0.3 para el segmento D del display
#define E_SEGMENT_PIN ((uint32_t)(1<<4)) // P0.4 para el segmento E del display
#define F_SEGMENT_PIN ((uint32_t)(1<<5)) // P0.5 para el segmento F del display
#define G_SEGMENT_PIN ((uint32_t)(1<<6)) // P0.6 para el segmento G del display

volatile uint16_t systick_interrupts = 0;
volatile uint16_t count_rate = DIGIT_PER_SECOND;

FunctionalState Systick_state = ENABLE;

uint8_t digit = 0;
uint8_t digit_codes[DIFFERENT_DIGITS] = {
    0x3F, // 0 -> 0b00111111
    0x06, // 1 -> 0b00000110
    0x5B, // 2 -> 0b01011011
    0x4F, // 3 -> 0b01001111
    0x66, // 4 -> 0b01100110
    0x6D, // 5 -> 0b01101101
    0x7D, // 6 -> 0b01111101
    0x07, // 7 -> 0b00000111
    0x7F, // 8 -> 0b01111111
    0x6F  // 9 -> 0b01101111
};

void configure_ports(void) {
    PINSEL_CFG_Type PinCfg;
    PinCfg.Portnum = PINSEL_PORT_2;
    PinCfg.Funcnum = PINSEL_FUNC_1; // EXTI
    PinCfg.Pinmode = PINSEL_PINMODE_PULLDOWN; // Stays LOW until the button is pressed (por como es el diagrama en el parcial)
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PinCfg.Pinnum = PINSEL_PIN_10;
    PINSEL_ConfigPin(&PinCfg); // EINT0 (RESET_COUNT_PIN)

    PinCfg.Pinmode = PINSEL_PINMODE_PULLDOWN; // Stays HIGH until the button is pressed (por como es el diagrama en el parcial)
    for (uint32_t pin = PINSEL_PIN_11; pin <= PINSEL_PIN_12; pin++) {
        PinCfg.Pinnum = pin;
        PINSEL_ConfigPin(&PinCfg); // EINT1 (PAUSE_RESUME_COUNT_PIN) and EINT2 (COUNT_RATE_PIN)
    }

    // Como hay resistencia de Pull Up, se supone transistor PNP con la base conectada a la LPC (Logica negativa)
    PinCfg.Portnum = PINSEL_PORT_0;
    PinCfg.Funcnum = PINSEL_FUNC_0; // GPIO
    
    for (uint32_t pin = PINSEL_PIN_0; pin <= PINSEL_PIN_6; pin++) {
        PinCfg.Pinnum = pin;
        PINSEL_ConfigPin(&PinCfg); // GPIO segmentos display
    }
    
    GPIO_SetDir(PINSEL_PORT_2, RESET_COUNT_PIN | PAUSE_RESUME_COUNT_PIN | COUNT_RATE_PIN, INPUT);
    GPIO_SetDir(PINSEL_PORT_0, A_SEGMENT_PIN | B_SEGMENT_PIN | C_SEGMENT_PIN | D_SEGMENT_PIN | E_SEGMENT_PIN | F_SEGMENT_PIN | G_SEGMENT_PIN, OUTPUT);
}	

void configure_exti(void) {
    EXTI_InitTypeDef exti_cfg;
    exti_cfg.EXTI_polarity = EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
    
    exti_cfg.EXTI_Line = EXTI_EINT0;
    exti_cfg.EXTI_Mode = EXTI_MODE_LEVEL_SENSITIVE; // Mientras este presionado, entra siempre a la interrupcion
    EXTI_Config(&exti_cfg);

    exti_cfg.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
    
    for (uint32_t exti = EXTI_EINT1; exti <= EXTI_EINT2; exti++) {
        exti_cfg.EXTI_Line = exti;
        EXTI_Config(&exti_cfg);
    }
    EXTI_Config(&exti_cfg);

    NVIC_SetPriority(EINT0_IRQn, 0);
    NVIC_SetPriority(EINT1_IRQn, 1);
    NVIC_SetPriority(EINT2_IRQn, 2);
    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(EINT1_IRQn);
    NVIC_EnableIRQ(EINT2_IRQn);
}

config_systick(void) {
    SYSTICK_InternalInit(SYSTICK_TIME_MS);
    SYSTICK_IntCmd(Systick_state);
    SYSTICK_Cmd(Systick_state);
}

void next_digit(void) {
    digit++;
    if (digit == 10) {
        digit = 0;
    }
    GPIO_SetValue(PINSEL_PORT_0, digit_codes[digit]);
}

void toggle_systick(void) {
    Systick_state = !Systick_state;
    SYSTICK_IntCmd(Systick_state);
}

void reset_digit(void) {
    digit = 0;
    GPIO_SetValue(PINSEL_PORT_0, digit_codes[digit]);
}

void EINT0_IRQHandler(void) {
    toggle_systick();
    systick_interrupts = 0;
    reset_digit();
    for (uint32_t i = 0; i < 10000; i++) {
        __NOP(); // A delay to avoid bouncing
    }
    EXTI_ClearEXTIFlag(EXTI_EINT0);
    toggle_systick();
}

void SYSTICK_Handler(void) {
    systick_interrupts++;
    if (systick_interrupts == count_rate) {
        systick_interrupts = 0;
        next_digit();
    }
}

void EINT1_IRQHandler(void) {
    toggle_systick();
    systick_interrupts = 0;
    for (uint32_t i = 0; i < 10000; i++) {
        __NOP(); // A delay to avoid bouncing
    }
    EXTI_ClearEXTIFlag(EXTI_EINT1);
}

/* Se puede hacer esto o parar al systick, cambiar el tiempo y volverlo a activar*/
void toggle_count_rate(void) {
    if (count_rate == DIGIT_PER_SECOND) {
        count_rate = DIGIT_PER_MS;
    } else {
        count_rate = DIGIT_PER_SECOND;
    }
}

void EINT2_IRQHandler(void) {
    toggle_count_rate();
    systick_interrupts = 0;
    for (uint32_t i = 0; i < 10000; i++) {
        __NOP(); // A delay to avoid bouncing
    }
    EXTI_ClearEXTIFlag(EXTI_EINT2);
}

int main(void) {
    SystemInit();
    configure_ports();
    configure_exti();
    config_systick();
    reset_digit();
    while(1) {
        __WFI();
    }
    return 0;
}