#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

#define INPUT 0
#define OUTPUT 1

#define EDGE_RISING 0
#define EDGE_FALLING 1

#define TRUE 1
#define FALSE 0

uint8_t numbers [10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint16_t priority = 0;
uint16_t number_counter = 0;
uint16_t counter_limit = 200;

#define DIG0_PIN  ((uint32_t)(1 << 0)) /* Digit 0 of input number in P0.0 */
#define DIG1_PIN  ((uint32_t)(1 << 1)) /* Digit 1 of input number in P0.1 */
#define DIG2_PIN  ((uint32_t)(1 << 2)) /* Digit 2 of input number in P0.2 */
#define DIG3_PIN  ((uint32_t)(1 << 3)) /* Digit 3 of input number in P0.3 */

void configure_ports(void) {
    PINSEL_CFG_Type pin_cfg;
    pin_cfg.Portnum = PINSEL_PORT_0;
    pin_cfg.Pinnum = PINSEL_PIN_0;
    pin_cfg.Funcnum = PINSEL_FUNC_0;
    pin_cfg.Pinmode = PINSEL_PINMODE_TRISTATE;
    pin_cfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&pin_cfg);

    pin_cfg.Pinnum = PINSEL_PIN_1;
    PINSEL_ConfigPin(&pin_cfg);

    pin_cfg.Pinnum = PINSEL_PIN_2;
    PINSEL_ConfigPin(&pin_cfg);

    pin_cfg.Pinnum = PINSEL_PIN_3;
    PINSEL_ConfigPin(&pin_cfg);

    GPIO_SetDir(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, INPUT);

    GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_RISING);
    GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_FALLING);
    NVIC_SetPriority(EINT3_IRQn, priority);
    NVIC_EnableIRQ(EINT3_IRQn);
}

void add_number(void) {
    uint8_t number = GPIO_ReadValue(PINSEL_PORT_0) & (DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN);
    for (int i = 9; i > -1; i--) {
        if (i = 0) {
            numbers[i] = number;
        }
        else {
            numbers[i] = numbers[i - 1];
        }
    }
}

void EINT3_IRQHandler(void) {
    add_number();
    number_counter++;
    if (number_counter = counter_limit) {
        number_counter = 0;
        if (priority < 15) {
            priority++;
            NVIC_SetPriority(EINT3_IRQn, priority);
        }
        else {
            GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_RISING);
            GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_FALLING);
            NVIC_DisableIRQ(EINT3_IRQn);
        }
    }
    GPIO_ClearInt(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN);
}

int main(void) {
    SystemInit();
    configure_ports();
    while (TRUE) {
        __WFI();
    }
    return 0;
}