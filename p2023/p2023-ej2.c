// Numero a guardar se cambia a traves de 4 switches son resistencias de Pull Up EXTERNAS
    // Cambiar switch tira interrupcion por GPIO
    // La interrupcion empieza con prioridad alta (0) y cada 200 numeros se va reduciendo (hasta 15)
        // Si llega a 15, se deshabilita la interrupcion
// Se almacenan los ultimos 10 numeros en un array, el mas antiguo es el 9 y el mas nuevo el 0

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

#define INPUT 0
#define OUTPUT 1

#define EDGE_RISING 0
#define EDGE_FALLING 1

#define TRUE 1
#define FALSE 0

#define SAVED_DIGITS 10
#define NUM_TO_REDUCE_PRIORITY 200

uint8_t numbers [SAVED_DIGITS] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint16_t priority = 0;
uint16_t number_counter = 0;
uint16_t counter_limit = NUM_TO_REDUCE_PRIORITY;

#define PORT0_BIT_DIG0 0
#define DIG0_PIN  ((uint32_t)(1 << 0)) /* Digit 0 of input number in P0.0 */
#define DIG1_PIN  ((uint32_t)(1 << 1)) /* Digit 1 of input number in P0.1 */
#define DIG2_PIN  ((uint32_t)(1 << 2)) /* Digit 2 of input number in P0.2 */
#define DIG3_PIN  ((uint32_t)(1 << 3)) /* Digit 3 of input number in P0.3 */

void configure_ports(void) {
    PINSEL_CFG_Type pin_cfg;
    pin_cfg.Portnum = PINSEL_PORT_0;
    pin_cfg.Funcnum = PINSEL_FUNC_0; // GPIO
    pin_cfg.Pinmode = PINSEL_PINMODE_TRISTATE; // Porque tiene resistencias de PU externas
    pin_cfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    
    for (uint32_t pin = PINSEL_PIN_0; pin <= PINSEL_PIN_3; pin++) {
        pin_cfg.Pinnum = pin;
        PINSEL_ConfigPin(&pin_cfg);
    }

    GPIO_SetDir(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, INPUT);

    // Activo interrupciones por ambos flancos
    GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_RISING);
    GPIO_IntCmd(PINSEL_PORT_0, DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN, EDGE_FALLING);
    NVIC_SetPriority(EINT3_IRQn, priority);
    NVIC_EnableIRQ(EINT3_IRQn);
}

// El nuevo numero va en 0 y el mas viejo en 9
void add_number(void) {
    // Con >> PORT0_BIT_DIG0 se corren los bits para que el numero quede en los 4 bits menos significativos
    uint8_t number = (GPIO_ReadValue(PINSEL_PORT_0) & (DIG0_PIN | DIG1_PIN | DIG2_PIN | DIG3_PIN)) >> PORT0_BIT_DIG0;
    for (int i = 9; i >= 0; i--) {
        if (i = 0) {
            numbers[i] = number;
        }
        else {
            numbers[i] = numbers[i - 1];
        }
    }
}

// Cada vez que cambio numero, lo agrego al array y aumento el contador
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