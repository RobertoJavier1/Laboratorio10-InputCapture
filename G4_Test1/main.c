#include "RTE_Components.h"
#include CMSIS_device_header
#include "USART.h"

void pwm_config(void);
void ic_config(void);
void print_number(uint32_t num);

volatile uint32_t period = 0;
volatile uint32_t width = 0;
uint32_t last_rising_capture = 0; //ultimo valor de captura de flanco de subida

int main(void){

	USART_config(115200);
	pwm_config();
	ic_config();

	USART_putString("\n\r  Lab 10 - Input Capture y UART\n\r");
	USART_putString("  0: Periodo  1: Frecuencia  2: Ancho de pulso  3: Duty Cycle\n\r>> ");

	while(1){
		__WFI(); //dormir hasta la siguiente interrupcion

		if(cmd_ready){
			cmd_ready = 0;

			uint32_t p = period; //copia local para que no cambie a mitad del calculo
			uint32_t w = width;

			if(uart_data == '\r' || uart_data == '\n'){
				//ignorar Enter
			}else if(p == 0){
				USART_putString("\n\rNo hay senal en PA0\n\r>> ");
			}else{
				switch(uart_data){
					case '0':
						USART_putString("\n\rPeriodo = ");
						print_number(p);
						USART_putString(" µs\n\r>> ");
						break;
					case '1':
						USART_putString("\n\rFrecuencia = ");
						print_number(1000000 / p); //f = 1/T, T en us -> 1000000/T en Hz
						USART_putString(" Hz\n\r>> ");
						break;
					case '2':
						USART_putString("\n\rAncho de pulso = ");
						print_number(w);
						USART_putString(" µs\n\r>> ");
						break;
					case '3':
						USART_putString("\n\rDuty Cycle = ");
						print_number((w * 100) / p); //duty = ancho/periodo * 100
						USART_putString("%\n\r>> ");
						break;
					default:
						USART_putString("\n\rComando no valido, use 0-3\n\r>> ");
						break;
				}
			}
		}
	}
}

void pwm_config(void){
	RCC -> AHB2ENR |= RCC_AHB2ENR_GPIOAEN; //habilitar reloj GPIOA
	RCC -> APB2ENR |= RCC_APB2ENR_TIM1EN; //habilitar reloj TIM1

	GPIOA -> MODER &= ~(3<<16); //limpiar bits 17:16 (PA8)
	GPIOA -> MODER |= (2<<16); //PA8 en modo funcion alterna

	GPIOA -> AFR[1] &= ~(0xF<<0); //limpiar bits 3:0 de AFRH
	GPIOA -> AFR[1] |= (6<<0); //PA8 en AF6 (TIM1_CH1)

	TIM1 -> PSC = 15; //prescaler = 15, 16MHz / (15+1) = 1MHz -> 1 cuenta cada 1us
	TIM1 -> ARR = 199; // 200us = 5kHz
	TIM1 -> CCR1 = 100; //100us en alto 50% duty cycle

	TIM1 -> CCMR1 &= ~((7<<4)|(1<<16)); //limpiar bits OC1M (bits 6:4 y bit 16)
	TIM1 -> CCMR1 |= (6<<4)|(1<<3); //PWM modo 1 salida en 1 mientras CNT<CCR1, OC1PE=1 habilitar preload

	TIM1 -> CCER |= (1<<0); //habilitar salida CH1

	TIM1 -> BDTR |= (1<<15); //habilitar MOE (Main Output Enable)

	TIM1 -> EGR |= (1<<0); //generar evento de actualizacion para cargar PSC, ARR y CCR1 en los registros sombra y reiniciar CNT

	TIM1 -> CR1 |= (1<<7)|(1<<0); //habilitar ARPE y  arranca el contador
}

void ic_config(void){
	RCC -> AHB2ENR |= RCC_AHB2ENR_GPIOAEN; // habilitar reloj GPIOA
	RCC -> APB1ENR1 |= RCC_APB1ENR1_TIM2EN; // habilitar reloj TIM2

	GPIOA -> MODER &= ~(3<<0); // limpiar bits 1:0 (PA0)
	GPIOA -> MODER |= (2<<0); // PA0 en modo funcion alterna

	GPIOA -> AFR[0] &= ~(0xF<<0); // limpiar bits 3:0 de AFRL
	GPIOA -> AFR[0] |= (1<<0); // PA0 en AF1 (TIM2_CH1)

	TIM2 -> PSC = 15; // prescaler = 15, 16MHz / (15+1) = 1MHz -> 1 cuenta cada 1us
	TIM2 -> ARR = 0xFFFFFFFF; // cuenta libre hasta el maximo valor de 32 bits

	TIM2 -> CCMR1 &= ~((3<<0)|(3<<8)); // limpiar bits CC1S (bits 1:0) y CC2S (bits 9:8)
	TIM2 -> CCMR1 |= (1<<0); // CC1S=01 IC1 conectado a TI1 (PA0)
	TIM2 -> CCMR1 |= (2<<8); // CC2S=10 IC2 conectado a TI1 (PA0)

	TIM2 -> CCER &= ~((1<<1)|(1<<3)); //CC1P=0, CC1NP=0 CH1 captura flanco de subida
	TIM2 -> CCER |= (1<<5); //CC2P=1, CC2NP=0 CH2 captura flanco de bajada
	TIM2 -> CCER |= (1<<0)|(1<<4); // CC1E y CC2E habilitar captura en CH1 y CH2

	TIM2 -> DIER |= (1<<1)|(1<<2); // CC1IE y CC2IE habilitar interrupciones de captura en CH1 y CH2

	TIM2 -> EGR = (1<<0); // generar evento de actualizacion para cargar PSC
	TIM2 -> CR1 |= (1<<0); // CEN arrancar el contador

	NVIC_SetPriority(TIM2_IRQn, 0); // prioridad 0 (mas alta) para no perder flancos
	NVIC_EnableIRQ(TIM2_IRQn); // habilitar interrupcion TIM2 en el NVIC

}

void TIM2_IRQHandler(void){
	//CC1IF (bit 1): llego flanco de subida
	if(TIM2 -> SR & (1<<1)){
		uint32_t rising_flank = TIM2 -> CCR1; // leer CCR1 limpia CC1IF
		period = rising_flank - last_rising_capture; // subida - subida = periodo
		last_rising_capture = rising_flank; // guardar la ultima captura de flanco de subida
	}

	//CC2IF (bit 2): llego flanco de bajada
	if(TIM2 -> SR & (1<<2)){
		uint32_t falling_flank = TIM2 -> CCR2; // leer CCR2 limpia CC2IF
		width = falling_flank - last_rising_capture; // bajada - subida = ancho de pulso
	}
}

void print_number(uint32_t num){
        char buffer[11]; //uint32 tiene maximo 10 digitos + '\0'
        int index = 10;
        buffer[index] = '\0'; //fin de la cadena

        //llenar el buffer de derecha a izquierda
        do{
            buffer[--index] = '0' + (num % 10); //ultimo digito a ASCII
            num /= 10; //quitar el ultimo digito
        }while(num > 0);

        USART_putString(&buffer[index]); //imprimir desde el primer digito
}
