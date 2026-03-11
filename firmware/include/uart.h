/**
 * @file uart.h
 * @author Sumedh Girish
 * @brief UART Ring-Buffer Hardware Abstraction.
 *
 * Provides a non-blocking, interrupt-driven UART driver using dual
 * ring buffers (RX/TX) for host and peer communications.
 */

#ifndef __UART_H__
#define __UART_H__

#include "dl_config.h"
#include "ti/devices/msp/peripherals/hw_uart.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief Dimension of the RX/TX circular ring buffers. */
#define UART_BUFFER_SIZE 256

/**
 * @brief Circular ring buffer control structure.
 */
typedef struct
{
    uint8_t data[UART_BUFFER_SIZE]; /**< The physical byte array buffer. */
    uint8_t head;                   /**< Insertion index. */
    uint8_t tail;                   /**< Removal index. */
    uint16_t count;                 /**< Number of bytes currently enqueued. */
    uint16_t progress;              /**< Bytes processed during chunked ops. */
    bool dirty;                     /**< Flag indicating unread new data. */
} UART_RingBuffer;

/**
 * @brief A complete bidirectional UART channel state.
 */
typedef struct
{
    UART_RingBuffer rx;
    UART_RingBuffer tx;
} UART_Channel;

extern volatile UART_Channel host;
extern volatile UART_Channel peer;

/** @brief Maximum bytes to drain per ISR execution. */
#define MAX_DRAIN_SIZE 4

extern void UART0_IRQHandler(void);
extern void UART1_IRQHandler(void);

#define UART_HOST UART_0_INST
#define UART_PEER UART_1_INST

/**
 * @brief Initializes the software buffers for all UART channels.
 */
void UART_init(void);

/**
 * @brief Blocks until a specific character is received.
 *
 * @param channel The UART controller register base (e.g. UART_HOST).
 * @param chr The character to block for.
 */
void UART_RecvUntil(UART_Regs *channel, uint8_t chr);

/**
 * @brief Synchronously receives a fixed number of bytes.
 *
 * @param channel The UART controller register base.
 * @param out Destination buffer for the received bytes.
 * @param length Exact number of bytes to wait for.
 * @param eof If true, expects an EOF marker after the payload.
 */
void UART_RecvBytes(UART_Regs *channel, uint8_t *out, uint32_t length,
                    bool eof);

/**
 * @brief Synchronously transmits a fixed number of bytes.
 *
 * @param channel The UART controller register base.
 * @param out Source buffer to transmit.
 * @param length Exact number of bytes to send.
 * @param eof If true, appends an EOF marker after transmission.
 */
void UART_SendBytes(UART_Regs *channel, uint8_t *out, uint32_t length,
                    bool eof);

#endif
