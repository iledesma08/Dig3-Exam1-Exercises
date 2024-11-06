// Sensor puerta en P2.10 (EINT0) por flanco de subida
    // Systick cuenta 30 segundos
    // Introducir codigo de desactivacion en DIP Switch de 4 entradas del P2.0 al P2.3 (1010B = 10D = 0xAA)
        // Apretar boton P2.11 (EINT1) para confirmar contraseña (2 intentos)
    // 30 segundos pasan o 2 intentos fallidos -> Buzzer en P1.11

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_systick.h"

#define RISING_EDGE 0

#define INPUT 0
#define OUTPUT 1

#define SYSTICK_TIME_MS 100
#define SYSTICK_COUNTS_FOR_1SEC 10
#define ALARM_TIME_S 30
#define INCORRECT_PW_TRIES 2

#define PASSWORD 0xAA

#define DOOR_PIN ((uint32_t)(1 << 10)) // P2.10 para el sensor de la puerta
#define PORT2_BIT_DIG0 0
#define PW_DIG0 ((uint32_t)(1 << 0)) // P2.0 primer digito contraseña
#define PW_DIG1 ((uint32_t)(1 << 1)) // P2.1 segundo digito contraseña
#define PW_DIG2 ((uint32_t)(1 << 2)) // P2.2 tercer digito contraseña
#define PW_DIG3 ((uint32_t)(1 << 3)) // P2.3 cuarto digito contraseña
#define PW_BTN ((uint32_t)(1 << 4)) // P2.4 boton para comprobar contraseña
#define BUZZER_PIN ((uint32_t)(1 << 11)) // P1.11 buzzer

uint8_t const SECOND = SYSTICK_COUNTS_FOR_1SEC; // Cuantas interrupciones del Systick hacen 1 segundo
uint16_t systick_counter = ALARM_TIME_S*SECOND; // Cuantas interrupciones del Systick hacen el tiempo para disparar alarma
uint8_t tries = INCORRECT_PW_TRIES; // Cantidad de intentos aceptados

void config_pins(void);
void config_exti(void);
void config_systick(void);
void EINT0_IRQHandler(void);
void SYSTICK_IRQHandler(void);
void EINT3_IRQHandler(void);

int main(void) {
    SystemInit();
    config_pins();
    config_exti();
    config_systick();

    while (1) {
        __WFI();
    }
}

void config_pins(void) {
    PINSEL_CFG_Type pin;

    pin.Portnum = PINSEL_PORT_2;
    pin.Pinnum = PINSEL_PIN_10;
    pin.Funcnum = PINSEL_FUNC_1; // EXTI
    pin.Pinmode = PINSEL_PINMODE_PULLDOWN;
    pin.OpenDrain = PINSEL_PINMODE_NORMAL;

    PINSEL_ConfigPin(&pin); // P2.10 EINT0 (DOOR_PIN)

    pin.Funcnum = PINSEL_FUNC_0; // GPIO
    
    for (uint32_t i = PINSEL_PIN_0; i<=PINSEL_PIN_4; i++) {
        pin.Pinnum = i;
        PINSEL_ConfigPin(&pin); // P2.0-P2.3 GPIO (PW_DIGX) y P2.4 GPIO (PW_BTN)
    }

    pin.Portnum = PINSEL_PORT_1;
    pin.Pinmode = PINSEL_PINMODE_PULLUP; // Usar transistor PNP con la base conectada a la LPC (Logica negativa)

    PINSEL_ConfigPin(&pin); // P1.11 GPIO (BUZZER_PIN)

    GPIO_SetDir(PINSEL_PORT_2, PW_DIG0 | PW_DIG1 | PW_DIG2 | PW_DIG3 | PW_BTN, INPUT);
    GPIO_SetDir(PINSEL_PORT_1, BUZZER_PIN, OUTPUT);

    GPIO_IntCmd(PINSEL_PORT_2, PW_BTN, RISING_EDGE); // Habilito el boton para ingresar contraseña
    NVIC_DisableIRQ(EINT3_IRQn); // Lo mantengo desactivado hasta que se abra la puerta
    NVIC_SetPriority(EINT3_IRQn, 0);
}

void config_exti(void) {
    EXTI_InitTypeDef exti;
    exti.EXTI_Line = EXTI_EINT0;
    exti.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
    exti.EXTI_polarity = EXTI_POLARITY_HIGH_ACTIVE_OR_RISING_EDGE; // Se interrumpe cuando pasa de 0 a 1 (cuando se abre la puerta)

    EXTI_Config(&exti);

    NVIC_SetPriority(EINT0_IRQn, 2);
    NVIC_EnableIRQ(EINT0_IRQn);
    EXTI_Init();
}

void config_systick(void) {
    SYSTICK_InternalInit(SYSTICK_TIME_MS);
    SYSTICK_Cmd(ENABLE);
    SYSTICK_IntCmd(DISABLE); // Deshabilitado hasta que se abra la puerta
}

void EINT0_IRQHandler(void) {
    tries = INCORRECT_PW_TRIES;
    systick_counter = ALARM_TIME_S*SECOND;
    NVIC_DisableIRQ(EINT0_IRQn); // Deshabilitar trigger del timer hasta que se ingrese contraseña correcta
    NVIC_EnableIRQ(EINT3_IRQn); // Handler de interrupciones GPIO activado (para ingresar contraseña)
    SYSTICK_IntCmd(ENABLE);
    EXTI_ClearEXTIFlag(EXTI_EINT0);
}

void SYSTICK_IRQHandler(void) {
    systick_counter--;
    if (systick_counter == 0) {
        GPIO_ClearValue(PINSEL_PORT_1, BUZZER_PIN); // Disparar alarma (0=activada)
        SYSTICK_IntCmd(DISABLE); // Dejar de contar
    }
    SYSTICK_ClearCounterFlag();
}

void EINT3_IRQHandler(void) {
    // Se hace este if por si se vuelve a apretar el boton de contraseña habiendo desactivado la alarma
    tries--;
    // Con >> PORT2_BIT_DIG0 me aseguro de que los bits de los digitos esten en las posiciones correctas
    if (((GPIO_ReadValue(PINSEL_PORT_2) & (PW_DIG0 | PW_DIG1 | PW_DIG2 | PW_DIG3)) >> PORT2_BIT_DIG0) == PASSWORD) {
        GPIO_SetValue(PINSEL_PORT_1, BUZZER_PIN); // Logica negativa (1=desactivada)
        NVIC_EnableIRQ(EINT0_IRQn); // Solamente se va a volver a activar el contador cuando se vuelva a abrir la puerta
        NVIC_DisableIRQ(EINT3_IRQn); // Desactivo este handler hasta que se vuelva a abrir la puerta
        SYSTICK_IntCmd(DISABLE);
    } else if (tries == 0) {
        GPIO_ClearValue(PINSEL_PORT_1, BUZZER_PIN); // Disparar alarma (0=activada)
        SYSTICK_IntCmd(DISABLE);
        tries = 1; // Para evitar overflow y permitir que la alarma se pueda desactivar
    }   
    GPIO_ClearInt(PINSEL_PORT_2, PW_BTN);
    EXTI_ClearEXTIFlag(EXTI_EINT3);
}