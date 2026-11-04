#include "USART.h"

char rx_data[64]; //buffer de linea de recepcion
uint8_t n;
uint8_t i; //indice de escritura en el buffer 
volatile char uart_data;   //ultimo caracter recibido por la UART
volatile uint8_t cmd_ready; //1 llego caracter nuevo

void USART_config(uint32_t baudrate){
	//relojes: GPIOA (AHB2) y USART1 (APB2)
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

	//PA9 y PA10 en modo funcion alterna (MODER = 10)
	GPIOA->MODER &= ~((3 << 18) | (3 << 20));
	GPIOA->MODER |=  ((2 << 18) | (2 << 20));

	//AF7 = USART1 en PA9 (bits 7:4) y PA10 (bits 11:8) de AFRH
	GPIOA->AFR[1] &= ~((0xF << 4) | (0xF << 8));
	GPIOA->AFR[1] |=  ((7 << 4) | (7 << 8));

	//pull-up en PA10 (RX) para que la linea quede en reposo en 1
	GPIOA->PUPDR &= ~(3 << 20);
	GPIOA->PUPDR |=  (1 << 20);

	//8N1 es el valor por defecto (M=00, PCE=0, STOP=00)
	USART1->CR1 = 0;
	USART1->BRR = (SystemCoreClock + baudrate/2) / baudrate; //oversampling x16, redondeado

	//TE (bit 3), RE (bit 2), RXNEIE (bit 5)
	USART1->CR1 |= (1 << 3) | (1 << 2) | (1 << 5);
	//UE (bit 0): habilitar el USART
	USART1->CR1 |= (1 << 0);

	//habilitar interrupcion del USART1 en el NVIC
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);
}

void USART_send(char c){
	while(!(USART1->ISR & (1 << 7))); //esperar TXE (bit 7): TDR libre
	USART1->TDR = (uint8_t)c;
}

void USART_putString(char * string){
	while(*string){
		USART_send(*string++);
	}
}

void USART1_IRQHandler(void){
	//limpiar overrun ORE (bit 3) si ocurrio, si no la IRQ se queda pegada
	if(USART1->ISR & (1 << 3)){
		USART1->ICR = (1 << 3); //ORECF
	}

	//RXNE (bit 5): llego un dato
	if(USART1->ISR & (1 << 5)){
		uart_data = (char)USART1->RDR; //leer RDR limpia RXNE

		//guardar tambien en el buffer de linea
		if(uart_data == '\r' || uart_data == '\n'){
			rx_data[i] = '\0';
			i = 0;
		} else if(i < sizeof(rx_data) - 1){
			rx_data[i++] = uart_data;
		}

		cmd_ready = 1; //avisar a main()
	}
}
