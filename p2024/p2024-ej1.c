// CLK = 70 MHz
// Configuracion inicial por 3 segundos a traves de Systick
    // Switch para configurar tiempo de cierre en P3.4 -> Uso P2.10 para usar EXTI
        // Contar cuantas veces se presiona el switch, y en funcion de eso, establecer tiempo de cierre X de la barrera
// Sensor para autos en P2.3 (no voy a usar EINT porque me parece que hay una solucion mejor)
    // Flanco de subida -> Auto llega -> Prendo validador ticket en P2.4 y chequeo cada 1s con Systick
        // Ticket invalido -> LED rojo en P1.5
        // Ticket valido -> Activo Motor en P0.15 y apago LED
            // Motor activado hasta que entre el auto
    // Flanco de bajada -> Auto se va -> Apago validador ticket, apago LED apagado y, si es que la barrera estaba levantada, bajarla Xs mas tarde con Systick de que el auto entró

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_systick.h"

#define SYSTICK_TIME_MS 100
#define SYSTICK_COUNTS_FOR_1SEC 10 // Interrupciones que llega a hacer el Systick para cuando pase 1 segundo

#define INPUT 0
#define OUTPUT 1

#define RISING_EDGE 0
#define FALLING_EDGE 1

#define CONFIG_MODE 0
#define VALIDATION_MODE 1
#define VALIDATION_RETRY_SEC 1
#define OPEN_MODE 2
#define WAITING_MODE 3

#define CONFIG_SWITCH_PIN ((uint32_t)(1 << 10)) // P2.10 (EINT0) para el switch de configuracion
#define TICKET_VALIDATOR_OUTPUT_PIN ((uint32_t)(1 << 5)) // P2.5 (GPIO IN) para la salida del sensor de tickets
#define TICKET_VALIDATOR_VCC_PIN ((uint32_t)(1 << 4)) // P2.4 (GPIO OUT) para la alimentacion del sensor de tickets
#define CAR_SENSOR_PIN ((uint32_t)(1 << 3)) // P2.3 (GPIO IN c/Int) para la salida del sensor del auto
#define INVALID_TICKET_LED_PIN ((uint32_t)(1 << 5)) // P1.5 (GPIO OUT) para la alimentacion del LED de ticket invalido
#define BARRIER_MOTOR_PIN ((uint32_t)(1 << 15)) // P0.15 (GPIO OUT) para la alimentacion del Motor de la barrera

uint16_t const SECOND = SYSTICK_COUNTS_FOR_1SEC;
uint32_t systick_counter = 3*SECOND; // En un principio, esperar 3 segundos para la configuracion
uint8_t gate_timer_config = 0;
uint8_t system_mode = CONFIG_MODE;

void config_pins(void);
void config_exti(void);
void config_systick(void);
void EINT0_IRQHandler(void);
void try_validation(void);
void SysTick_Handler(void);
void EINT3_IRQHandler(void);

int main(void) {
    SystemInit();
    config_pins();
    config_exti();
    config_systick();

    while (1) {
        __WFI();
    }

    return 0;
}

void config_pins(void) {
    PINSEL_CFG_Type pin;

    pin.Portnum = PINSEL_PORT_2;
    pin.Pinnum = PINSEL_PIN_10;
    pin.Funcnum = PINSEL_FUNC_1; // EXTI
    pin.Pinmode = PINSEL_PINMODE_PULLDOWN;
    pin.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&pin); // CONFIG_SWITCH_PIN (EINT0 - Flanco de subida)

    pin.Funcnum = PINSEL_FUNC_0; // GPIO
    pin.Pinnum = PINSEL_PIN_5;
    PINSEL_ConfigPin(&pin); // TICKET_VALIDATOR_OUTPUT_PIN (GPIO)
    
    pin.Pinnum = PINSEL_PIN_4;
    PINSEL_ConfigPin(&pin); // TICKET_VALIDATOR_VCC_PIN (GPIO)

    pin.Pinnum = PINSEL_PIN_3;
    PINSEL_ConfigPin(&pin); // CAR_SENSOR_PIN (GPIO c/Int de Subida y Bajada)

    pin.Portnum = PINSEL_PORT_1;
    pin.Pinnum = PINSEL_PIN_5;
    PINSEL_ConfigPin(&pin); // INVALID_TICKET_LED_PIN

    pin.Portnum = PINSEL_PORT_0;
    pin.Pinnum = PINSEL_PIN_15;
    PINSEL_ConfigPin(&pin); // BARRIER_MOTOR_PIN

    GPIO_SetDir(PINSEL_PORT_2, TICKET_VALIDATOR_OUTPUT_PIN | CAR_SENSOR_PIN, INPUT);
    GPIO_SetDir(PINSEL_PORT_2, TICKET_VALIDATOR_VCC_PIN, OUTPUT);
    GPIO_SetDir(PINSEL_PORT_1, INVALID_TICKET_LED_PIN, OUTPUT);
    GPIO_SetDir(PINSEL_PORT_0, BARRIER_MOTOR_PIN, OUTPUT);

    // Configuro interrupcion GPIO por subida y bajada p/activar y desactivar validador de ticket
    GPIO_IntCmd(PINSEL_PORT_2, CAR_SENSOR_PIN, FALLING_EDGE);
    GPIO_IntCmd(PINSEL_PORT_2, CAR_SENSOR_PIN, RISING_EDGE);

    NVIC_SetPriority(EINT3_IRQn,0);
    NVIC_DisableIRQ(EINT3_IRQn); // Deshabilito interrupcion GPIO hasta pasar tiempo de configuracion
}

void config_exti(void) {
    EXTI_InitTypeDef exti;
    exti.EXTI_Line = EXTI_EINT0; // CONFIG_SWITCH_PIN
    exti.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE; 
    exti.EXTI_polarity = EXTI_POLARITY_HIGH_ACTIVE_OR_RISING_EDGE; // Flanco de subida

    EXTI_Config(&exti);

    NVIC_SetPriority(EINT0_IRQn, 1);
    NVIC_EnableIRQ(EINT0_IRQn); // Activado para la configuracion inicial
    EXTI_Init();
}

void config_systick(void) {
    SYSTICK_InternalInit(SYSTICK_TIME_MS);
    SYSTICK_IntCmd(ENABLE);
    SYSTICK_Cmd(ENABLE);
}

// Interrupt handler for the configuration switch (P2.10).
// This function increments the gate timer configuration each time the switch is pressed.
void EINT0_IRQHandler(void) {
    gate_timer_config++; // Cada vez que se aprieta switch en P2.10, se cambia el modo
    if (gate_timer_config == 4) {
        gate_timer_config = 0;
    }
    EXTI_ClearEXTIFlag(EXTI_EINT0);
}

// This function checks the ticket validator status and updates the system mode accordingly.
// If the ticket is valid, it opens the barrier and sets the appropriate timer.
// If the ticket is invalid, it lights up the invalid ticket LED.
void try_validation(void) {
    if (GPIO_ReadValue(PINSEL_PORT_2) & TICKET_VALIDATOR_OUTPUT_PIN) {
        system_mode = OPEN_MODE;
        switch (gate_timer_config) {
            case 0:
                systick_counter = 2*SECOND;
                break;
            case 1:
                systick_counter = 4*SECOND;
                break;
            case 2:
                systick_counter = 6*SECOND;
                break;
            case 3:
                systick_counter = 8*SECOND;
                break;
        }
        // Configuro el contador pero no activo interrupcion systick hasta que entre el auto
        GPIO_SetValue(PINSEL_PORT_0, BARRIER_MOTOR_PIN); // Levanto la baterra
        GPIO_ClearValue(PINSEL_PORT_1, INVALID_TICKET_LED_PIN); // Apago el LED (por si estaba prendido)
    } else {
        GPIO_SetValue(PINSEL_PORT_1, INVALID_TICKET_LED_PIN); // Enciendo el LED
    }
}

// SysTick_Handler is called periodically by the SysTick timer interrupt.
// It manages the different system modes and transitions based on the systick_counter.
void SysTick_Handler(void) {
    systick_counter--;
    if (systick_counter == 0) {
        switch (system_mode) {
            case CONFIG_MODE:
                NVIC_DisableIRQ(EINT0_IRQn); // Termino tiempo de configuracion
                NVIC_EnableIRQ(EINT3_IRQn); // Interrupcion por llegada/retirada de auto
                SYSTICK_IntCmd(DISABLE); // Deshabilitar interrupciones Systick timer hasta que se necesite validar ticket
                system_mode = WAITING_MODE;
                break;
            case VALIDATION_MODE:
                try_validation();
                systick_counter = VALIDATION_RETRY_SEC*SECOND;
                break;
            case OPEN_MODE:
                GPIO_ClearValue(PINSEL_PORT_0, BARRIER_MOTOR_PIN); // Cierro la barrera
                SYSTICK_IntCmd(DISABLE); // Deshabilitar interrupciones Systick timer hasta que se necesite validar ticket
                break;
        }
    }
    SYSTICK_ClearCounterFlag();
}

// Interrupt handler for the car sensor (P2.3).
// This function handles the arrival and departure of cars, activating or deactivating the ticket validator and updating the system mode accordingly.
void EINT3_IRQHandler(void) {
    // Si llegó un auto
    if (GPIO_GetIntStatus(PINSEL_PORT_2, CAR_SENSOR_PIN, RISING_EDGE) == ENABLE) {
        GPIO_SetValue(PINSEL_PORT_2, TICKET_VALIDATOR_VCC_PIN); // Enciendo validador
        system_mode = VALIDATION_MODE;
        systick_counter = VALIDATION_RETRY_SEC*SECOND;
        SYSTICK_IntCmd(ENABLE); // Habilito interrupciones del timer para que chequee el ticket cada tanto
    }
    // Si se fue un auto
    if(GPIO_GetIntStatus(PINSEL_PORT_2, CAR_SENSOR_PIN, FALLING_EDGE) == ENABLE) {
        if (system_mode == OPEN_MODE) { // Que quede abierta el tiempo configurado contando a partir que el auto entra habiendo validado el ticket
            SYSTICK_IntCmd(ENABLE);
        }
        GPIO_ClearValue(PINSEL_PORT_2, TICKET_VALIDATOR_VCC_PIN); // Apago validador
        GPIO_ClearValue(PINSEL_PORT_1, INVALID_TICKET_LED_PIN); // Apago el LED (por si estaba prendido)
        system_mode = WAITING_MODE;
    }
    GPIO_ClearInt(PINSEL_PORT_2, CAR_SENSOR_PIN);
    EXTI_ClearEXTIFlag(EXTI_EINT3);
}
