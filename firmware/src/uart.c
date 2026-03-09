
#include "uart.h"
#include "dl_config.h"
#include "stubs.h"
#include "ti/driverlib/dl_uart.h"
#include <assert.h>

volatile UART_Channel host = {0};
volatile UART_Channel peer = {0};

static inline void WaitForHostACK(void);
static inline void WaitForPeerACK(void);

static inline void UART_TransmitAll(UART_Regs *channel)
{
    volatile UART_RingBuffer *buffer =
        (channel == UART_HOST) ? &host.tx : &peer.tx;
    void (*WaitForAck)(void) =
        (channel == UART_HOST) ? WaitForHostACK : WaitForPeerACK;

    while (buffer->count > 0)
    {
        if (buffer->progress >= UART_BUFFER_SIZE)
        {
            WaitForAck();
            buffer->progress = 0;
        }
        DL_UART_transmitDataBlocking(channel, buffer->data[buffer->tail]);
        buffer->tail = (buffer->tail + 1) % UART_BUFFER_SIZE;
        buffer->count--;
        buffer->progress++;
    }
}

static inline void UART_PushByte(UART_Regs *source, uint8_t byte)
{
    volatile UART_RingBuffer *buffer =
        (source == UART_HOST) ? &host.tx : &peer.tx;

    while (buffer->count >= UART_BUFFER_SIZE)
    {
        UART_TransmitAll(source);
    }

    buffer->data[buffer->head] = byte;
    buffer->head = (buffer->head + 1) % UART_BUFFER_SIZE;
    buffer->count++;
}

static inline uint8_t UART_PopByte(UART_Regs *source)
{
    volatile UART_RingBuffer *buffer =
        (source == UART_HOST) ? &host.rx : &peer.rx;

    while (buffer->count == 0)
        __WFI();

    uint8_t byte = buffer->data[buffer->tail];
    buffer->tail = (buffer->tail + 1) % UART_BUFFER_SIZE;
    buffer->count--;

    return byte;
}

static inline void WaitForHostACK(void)
{
    static const uint8_t ackbytes[4] = {'%', 'A', '\x00', '\x00'};

    uint8_t idx = 0;
    uint8_t currentByte;
    while (idx < 4)
    {
        currentByte = UART_PopByte(UART_HOST);
        if (currentByte == ackbytes[idx])
            idx++;
        else if (currentByte == ackbytes[0])
            idx = 1;
        else
            idx = 0;
    }
}

static inline void WaitForPeerACK(void)
{
    static const uint8_t ackbytes[4] = {'^', 'A', '\x00', '\x00'};

    uint8_t idx = 0;
    uint8_t currentByte;
    while (idx < 4)
    {
        currentByte = UART_PopByte(UART_PEER);
        if (currentByte == ackbytes[idx])
            idx++;
        else if (currentByte == ackbytes[0])
            idx = 1;
        else
            idx = 0;
    }
}

static inline void SendHostACK(void)
{
    const uint8_t ackbytes[4] = {'%', 'A', '\x00', '\x00'};
    for (size_t i = 0; i < 4; i++)
        DL_UART_transmitDataBlocking(UART_HOST, ackbytes[i]);
}

static inline void SendPeerACK(void)
{
    const uint8_t ackbytes[4] = {'^', 'A', '\x00', '\x00'};
    for (size_t i = 0; i < 4; i++)
        DL_UART_transmitDataBlocking(UART_PEER, ackbytes[i]);
}

void UART_RecvUntil(UART_Regs *channel, uint8_t chr)
{
    while (UART_PopByte(channel) != chr)
        ;

    volatile UART_RingBuffer *buffer =
        (channel == UART_HOST) ? &host.rx : &peer.rx;

    buffer->progress = 0;
    buffer->dirty = false;
}

void UART_RecvBytes(UART_Regs *channel, uint8_t *out, uint32_t length, bool eof)
{
    volatile UART_Channel *source = (channel == UART_HOST) ? &host : &peer;
    void (*sendACK)(void) = (channel == UART_HOST) ? SendHostACK : SendPeerACK;

    uint32_t bytesRead = 0;
    while (bytesRead < length)
    {
        if (source->rx.progress >= UART_BUFFER_SIZE && source->rx.dirty)
        {
            source->rx.progress = 0;
            source->rx.dirty = false;
            sendACK();
        }

        if (out != NULL)
            out[bytesRead++] = UART_PopByte(channel);
        else
        {
            UART_PopByte(channel);
            bytesRead++;
        }

        source->rx.progress++;
    }

    if (eof)
    {
        source->rx.progress = 0;
        source->rx.dirty = false;
        sendACK();
    }
}

void UART_SendBytes(UART_Regs *channel, uint8_t *out, uint32_t length, bool eof)
{
    volatile UART_Channel *source = (channel == UART_HOST) ? &host : &peer;
    void (*waitACK)(void) =
        (channel == UART_HOST) ? WaitForHostACK : WaitForPeerACK;

    uint32_t bytesSent = 0;
    while (bytesSent < length)
    {
        UART_PushByte(channel, out[bytesSent++]);
    }

    UART_TransmitAll(channel);
    if (eof)
    {
        waitACK();
        source->tx.dirty = false;
        source->tx.progress = 0;
    }
}

static inline void DrainPushBytes(uint8_t tmp[MAX_DRAIN_SIZE],
                                  volatile UART_RingBuffer *buffer,
                                  uint32_t bytesRead)
{
    for (size_t i = 0; i < bytesRead; i++)
    {
        if (buffer->count < UART_BUFFER_SIZE)
        {
            buffer->data[buffer->head] = tmp[i];
            buffer->head = (buffer->head + 1) % UART_BUFFER_SIZE;
            buffer->count++;
            buffer->dirty = true;
        }
        else
        {
            break;
        }
    }
}

static inline void UART_ClearBuffer(volatile UART_RingBuffer *buffer)
{
    memclear((uint8_t *) &buffer->data, UART_BUFFER_SIZE, 0);
    buffer->count = 0;
    buffer->dirty = false;
    buffer->head = buffer->tail = 0;
    buffer->progress = 0;
}

static inline void UART_ClearChannel(volatile UART_Channel *channel)
{
    UART_ClearBuffer(&channel->rx);
    UART_ClearBuffer(&channel->tx);
}

void UART_init(void)
{
    UART_ClearChannel(&host);
    UART_ClearChannel(&peer);
}

static inline void IRQ_Handler(UART_Regs *source, volatile UART_Channel *buffer)
{
    uint8_t tmp[MAX_DRAIN_SIZE] = {0};
    switch (DL_UART_getPendingInterrupt(source))
    {
        case DL_UART_IIDX_RX:
        case DL_UART_IIDX_RX_TIMEOUT_ERROR:
            DrainPushBytes(tmp, &buffer->rx,
                           DL_UART_drainRXFIFO(source, tmp, MAX_DRAIN_SIZE));
            break;
        default:
            break;
    }
}

void UART0_IRQHandler(void)
{
    IRQ_Handler(UART_0_INST, &host);
}

void UART1_IRQHandler(void)
{
    IRQ_Handler(UART_1_INST, &peer);
}
