#include "dl_config.h"
#include "filesystem.h"
#include "status.h"
#include "ti/driverlib/dl_gpio.h"
#include "uart.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define HSM_PIN_SIZE 6

static void init(void)
{
    __disable_irq();

    UART_init();
    SYSCFG_DL_init();

    __enable_irq();
}

static void BlinkLED(void)
{
    while (true)
    {
        DL_GPIO_togglePins(LEDS_PORT, LEDS_STATUS_LED_PIN);
        delay_cycles(32000000 * 2); // 2 seconds
    }
}

#define OP_LIST 'L'
#define OP_READ 'R'
#define OP_WRITE 'W'
#define OP_INTERROGATE 'I'
#define OP_RECEIVE 'C'
#define OP_LISTEN 'N'

static inline void ParseOpcode(uint8_t opCode, StatusCode *status)
{
    switch (opCode)
    {
        case OP_LIST:
            *status = OPLIST;
            break;
        case OP_READ:
            *status = OPREAD;
            break;
        case OP_WRITE:
            *status = OPWRITE;
            break;
        case OP_INTERROGATE:
            *status = OPINTERROGATE;
            break;
        case OP_RECEIVE:
            *status = OPRECEIVE;
            break;
        case OP_LISTEN:
            *status = OPLISTEN;
            break;
        default:
            *status = UNKNOWNOP;
            break;
    }
}

#define LIST_METADATA_SIZE (HSM_PIN_SIZE)
#define READ_METADATA_SIZE (HSM_PIN_SIZE + sizeof(uint8_t))
#define WRITE_METADATA_SIZE                                                    \
    (HSM_PIN_SIZE + sizeof(uint8_t) + sizeof(uint16_t) + 32 + 16 +             \
     sizeof(uint16_t))
#define INTERROGATE_METADATA_SIZE (HSM_PIN_SIZE)
#define RECEIVE_METADATA_SIZE (HSM_PIN_SIZE + sizeof(uint8_t) + sizeof(uint8_t))
#define LISTEN_METADATA_SIZE 0

inline void ValidateBodySize(uint16_t bodyLen, StatusCode *status)
{
    bool validWriteSize = false;
    switch (*status)
    {
        case OPLIST:
            *status =
                (bodyLen == LIST_METADATA_SIZE) ? (*status) : INVALIDBODYSIZE;
            break;
        case OPREAD:
            *status =
                (bodyLen == READ_METADATA_SIZE) ? (*status) : INVALIDBODYSIZE;
            break;
        case OPWRITE:
            validWriteSize = (bodyLen >= WRITE_METADATA_SIZE) &&
                             ((bodyLen - WRITE_METADATA_SIZE) <= MAX_FILE_SIZE);
            *status = (validWriteSize) ? (*status) : INVALIDBODYSIZE;
            break;
        case OPINTERROGATE:
            *status = (bodyLen == INTERROGATE_METADATA_SIZE) ? (*status)
                                                             : INVALIDBODYSIZE;
            break;
        case OPRECEIVE:
            *status = (bodyLen == RECEIVE_METADATA_SIZE) ? (*status)
                                                         : INVALIDBODYSIZE;
            break;
        case OPLISTEN:
            *status =
                (bodyLen == LISTEN_METADATA_SIZE) ? (*status) : INVALIDBODYSIZE;
            break;
        default:
            break;
    }
}

inline void HandleRequest(StatusCode *status, uint16_t bodyLen)
{
    switch (*status)
    {
        default:
            if (bodyLen > 0)
                UART_RecvBytes(UART_HOST, NULL, bodyLen, true);
            break;
    }
}

#define OP_DEBUG 'D'
#define OP_ERROR 'E'
static const uint8_t host_magic_byte = '%';

inline void SendResponse(StatusCode *status)
{
    uint8_t responseCode;
    uint16_t responseLen = 0;
    UART_SendBytes(UART_HOST, (uint8_t *) &host_magic_byte, 1, false);
    switch (*status)
    {
        default:
            responseCode = OP_ERROR;
            UART_SendBytes(UART_HOST, (uint8_t *) &responseCode, 1, false);
            const char errormsg[] = "TODO: This is a default error message";
            responseLen = (uint16_t) strlen(errormsg);
            UART_SendBytes(UART_HOST, (uint8_t *) &responseLen, 2, true);
            UART_SendBytes(UART_HOST, (uint8_t *) &errormsg, responseLen, true);
            break;
    }
}

int main(void)
{
    init();

    StatusCode status;
    uint8_t opCode;

    while (true)
    {
        status = UNKNOWNOP;
        while (status == UNKNOWNOP)
        {
            UART_RecvUntil(UART_HOST, host_magic_byte);
            UART_RecvBytes(UART_HOST, &opCode, 1, false);
            ParseOpcode(opCode, &status);
        }

        uint16_t bodyLen = 0;
        UART_RecvBytes(UART_HOST, (uint8_t *) &bodyLen, 2, true);

        ValidateBodySize(bodyLen, &status);

        HandleRequest(&status, bodyLen);

        SendResponse(&status);
    }

    BlinkLED();
    return 0; /* Unreachable */
}
