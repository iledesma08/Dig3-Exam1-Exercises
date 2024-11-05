#include lpc17xx_exti.h
#include lpc17xx_gpio.h
#include lpc17xx_systick.h

#define SYSTICK_TIME_MS 1
#define RESET_COUNT_PIN ((uint32_t)(1<<10))
#define PAUSE_RESUME_COUNT_PIN ((uint32_t)(1<<11))
#define COUNT_RATE_PIN ((uint32_t)(1<<12))
#define A_SEGMENT_PIN ((uint32_t)(1<<0))
#define B_SEGMENT_PIN ((uint32_t)(1<<1))
#define C_SEGMENT_PIN ((uint32_t)(1<<2))
#define D_SEGMENT_PIN ((uint32_t)(1<<3))
#define E_SEGMENT_PIN ((uint32_t)(1<<4))
#define F_SEGMENT_PIN ((uint32_t)(1<<5))
#define G_SEGMENT_PIN ((uint32_t)(1<<6))

volatile uint16_t systick_interrupts = 0;
#define DIGIT_PER_SECOND 1000
#define DIGIT_PER_MS 1
volatile uint16_t count_rate = DIGIT_PER_SECOND;

FunctionalState Systick_state = ENABLE;

uint8_t digit_codes [10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

uint8_t digit = 0;

#define INPUT 1
#define OUTPUT 0

#define EDGE_RISING 0
#define EDGE_FALLING 1

#define TRUE 1
#define FALSE 0

void configure_ports(void) {
    PINSEL_CFG_Type PinCfg;
    PinCfg.Portnum = PINSEL_PORT_2;
    PinCfg.Pinnum = PINSEL_PIN_10;
    PinCfg.Funcnum = PINSEL_FUNC_1; // EINT
    PinCfg.Pinmode = PINSEL_PINMODE_PULLDOWN; // Stays LOW until the button is pressed
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_11;
    PinCfg.Pinmode = PINSEL_PINMODE_PULLUP; // Stays HIGH until the button is pressed
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_12;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Portnum = PINSEL_PORT_0;
    PinCfg.Pinnum = PINSEL_PIN_0;
    PinCfg.Funcnum = PINSEL_FUNC_0; // GPIO
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_1;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_2;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_3;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_4;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_5;
    PINSEL_ConfigPin(&PinCfg);

    PinCfg.Pinnum = PINSEL_PIN_6;
    PINSEL_ConfigPin(&PinCfg);
    
    GPIO_SetDir(2, RESET_COUNT_PIN | PAUSE_RESUME_COUNT_PIN | COUNT_RATE_PIN, INPUT);
    GPIO_SetDir(0, A_SEGMENT_PIN | B_SEGMENT_PIN | C_SEGMENT_PIN | D_SEGMENT_PIN | E_SEGMENT_PIN | F_SEGMENT_PIN | G_SEGMENT_PIN, OUTPUT);
}	

void configure_exti(void) {
    EXTI_InitTypeDef exti_cfg;
    exti_cfg.EXTI_Line = EXTI_EINT0;
    exti_cfg.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
    exti_cfg.EXTI_polarity = EXTI_POLARITY_HIGH_ACTIVE_OR_RISING_EDGE;
    EXTI_Config(&exti_cfg);

    exti_cfg.EXTI_Line = EXTI_EINT1;
    exti_cfg.EXTI_polarity = EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
    EXTI_Config(&exti_cfg);

    exti_cfg.EXTI_Line = EXTI_EINT2;
    EXTI_Config(&exti_cfg);
}

void next_digit(void) {
    digit++;
    if (digit == 10) {
        digit = 0;
    }
    GPIO_SetValue(0, digit_codes[digit]);
}

/* Se puede hacer esto o parar al systick, cambiar el tiempo y volverlo a activar*/
void toggle_count_rate(void) {
    if (count_rate == DIGIT_PER_SECOND) {
        count_rate = DIGIT_PER_MS;
    } else {
        count_rate = DIGIT_PER_SECOND;
    }
}

void toggle_systick(void) {
    if (Systick_state == ENABLE) {
        SYSTICK_Cmd(DISABLE);
        Systick_state = DISABLE;
    } else {
        SYSTICK_Cmd(ENABLE);
        Systick_state = ENABLE;
    }
}

void reset_digit(void) {
    digit = 0;
    GPIO_SetValue(0, digit_codes[digit]);
}

void EINT0_IRQHandler(void) {
    toggle_systick();
    systick_interrupts = 0;
    reset_digit();
    for (uint32_t i = 0; i < 10000; i++) {
        ; // A delay to avoid bouncing
    }
    while (GPIO_ReadValue(2) & RESET_COUNT_PIN) {
        ; // Wait until the button is released to keep counting
    }
    EXTI_ClearEXTIFlag(EXTI_EINT0);
    toggle_systick();
}

void EINT1_IRQHandler(void) {
    toggle_systick();
    for (uint32_t i = 0; i < 10000; i++) {
        ; // A delay to avoid bouncing
    }
    EXTI_ClearEXTIFlag(EXTI_EINT1);
    systick_interrupts = 0;
}

void EINT2_IRQHandler(void) {
    toggle_count_rate();
    for (uint32_t i = 0; i < 10000; i++) {
        ; // A delay to avoid bouncing
    }
    EXTI_ClearEXTIFlag(EXTI_EINT2);
    systick_interrupts = 0;
}

void SYSTICK_Handler(void) {
    systick_interrupts++;
    if (systick_interrupts == count_rate) {
        systick_interrupts = 0;
        next_digit();
    }
}

int main(void) {
    SystemInit();
    configure_ports();
    configure_exti();
    SYSTICK_InternalInit(SYSTICK_TIME_MS);
    SYSTICK_IntCmd(ENABLE);
    SYSTICK_Cmd(ENABLE);
    NVIC_SetPriority(EINT0_IRQn, 0);
    NVIC_SetPriority(EINT1_IRQn, 1);
    NVIC_SetPriority(EINT2_IRQn, 2);
    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(EINT1_IRQn);
    NVIC_EnableIRQ(EINT2_IRQn);
    reset_digit();
    while(TRUE) {
        __WFI();
    }
    return 0;
}