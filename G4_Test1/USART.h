#ifndef USART_H_
#define USART_H_

#include "RTE_Components.h"
#include CMSIS_device_header

extern char rx_data[64];
extern uint8_t n;
extern volatile uint8_t cmd_ready;
extern volatile char uart_data;
extern uint8_t i;

void USART_config(uint32_t baudrate);
void USART_send(char c);
void USART_putString(char * string);

#endif /* USART_H_ */