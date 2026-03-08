#ifndef __UART_H__
#define __UART_H__

#include "ti/devices/msp/peripherals/hw_uart.h"
#include <stdbool.h>
#include <stdint.h>

#define UART_BUFFER_SIZE 256
typedef struct
{
    uint8_t data[UART_BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint16_t count;
    uint16_t progress;
    bool dirty;
} UART_RingBuffer;

typedef struct
{
    UART_RingBuffer rx;
    UART_RingBuffer tx;
} UART_Channel;

extern volatile UART_Channel host;
extern volatile UART_Channel peer;

#define MAX_DRAIN_SIZE 4
extern void UART0_IRQHandler(void);
extern void UART1_IRQHandler(void);

#define UART_HOST UART_0_INST
#define UART_PEER UART_1_INST

void UART_init(void);
void UART_RecvUntil(UART_Regs *channel, uint8_t chr);
void UART_RecvBytes(UART_Regs *channel, uint8_t *out, uint32_t length,
                    bool eof);
void UART_SendBytes(UART_Regs *channel, uint8_t *out, uint32_t length,
                    bool eof);

#endif
