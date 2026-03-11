/**
 * @file hsm.c
 * @author Sumedh Girish
 * @brief Main entry point and core dispatch loop for the SHIFT HSM.
 *
 * This file contains the primary `main` loop that constantly listens for
 * Host or Peer commands over UART. It handles opcode parsing, basic size
 * validation, and dispatches to the appropriate parser, before finally
 * formulating and transmitting a serialized response frame.
 */

#include "dl_config.h"
#include "filesystem.h"
#include "flash.h"
#include "parser.h"
#include "randombytes.h"
#include "status.h"
#include "stubs.h"
#include "ti/driverlib/dl_gpio.h"
#include "uECC.h"
#include "uart.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define HSM_PIN_SIZE 6

static int ecc_rng(uint8_t *dest, unsigned size)
{
    randombytes(dest, size);
    return 1;
}

static void init(void)
{
    __disable_irq();

    UART_init();
    SYSCFG_DL_init();
    memclear((uint8_t *) &stage, sizeof(FS_Stage), 0);
    uECC_set_rng(&ecc_rng);

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

/**
 * @brief Parses an incoming raw byte into a recognized operational Status.
 *
 * @param opCode The raw byte character from UART.
 * @param status Pointer to the current system state.
 */
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

static inline void ValidateBodySize(uint16_t bodyLen, StatusCode *status)
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

/**
 * @brief Top-level request dispatcher based on resolved Command State.
 *
 * Routes execution to specific parsers which will handle subsequent UART
 * payloads and trigger the actual Handler routines.
 *
 * @param status Current validated system state.
 * @param bodyLen Expected length of the incoming payload body.
 */
static inline void HandleRequest(StatusCode *status, uint16_t bodyLen)
{
    switch (*status)
    {
        case OPLIST:
            ListParser(status);
            break;
        case OPREAD:
            ReadParser(status);
            break;
        case OPWRITE:
            WriteParser(status);
            break;
        case OPINTERROGATE:
            InterrogateParser(status);
            break;
        case OPRECEIVE:
            ReceiveParser(status);
            break;
        case OPLISTEN:
            ListenParser(status);
            break;
        default:
            if (bodyLen > 0)
                UART_RecvBytes(UART_HOST, NULL, bodyLen, true);
            break;
    }
}

#define OP_ERROR 'E'
static const uint8_t host_magic_byte = '%';

static inline void SendError(const char *message)
{
    uint8_t responseCode = OP_ERROR;
    uint16_t responseLen = (uint16_t) strlen(message);
    UART_SendBytes(UART_HOST, (uint8_t *) &host_magic_byte, 1, false);
    UART_SendBytes(UART_HOST, (uint8_t *) &responseCode, 1, false);
    UART_SendBytes(UART_HOST, (uint8_t *) &responseLen, 2, true);
    UART_SendBytes(UART_HOST, (uint8_t *) message, responseLen, true);
}

static inline void SendHeader(uint8_t opCode, uint16_t bodyLen)
{
    UART_SendBytes(UART_HOST, (uint8_t *) &host_magic_byte, 1, false);
    UART_SendBytes(UART_HOST, (uint8_t *) &opCode, 1, false);
    UART_SendBytes(UART_HOST, (uint8_t *) &bodyLen, 2, true);
}

/**
 * @brief Constructs and transmits the final output frame to the Host.
 *
 * Serializes data from the volatile `stage` into the UART TX buffer. It handles
 * successful payload transmissions as well as sending human-readable error messages.
 *
 * @param status Final state of the request (success or specific error code).
 */
static inline void SendResponse(StatusCode *status)
{
    switch (*status)
    {
        case OPLIST:
            SendHeader(OP_LIST,
                       (uint16_t) (stage.as.fat.numEntries * sizeof(FS_Slot) +
                                   sizeof(uint32_t)));
            UART_SendBytes(UART_HOST, (uint8_t *) &stage.as.fat.numEntries,
                           sizeof(uint32_t), false);
            UART_SendBytes(UART_HOST, (uint8_t *) stage.as.fat.slots,
                           (stage.as.fat.numEntries * sizeof(FS_Slot)), true);
            break;
        case OPINTERROGATE:
            SendHeader(OP_INTERROGATE,
                       (uint16_t) (stage.as.fat.numEntries * sizeof(FS_Slot) +
                                   sizeof(uint32_t)));
            UART_SendBytes(UART_HOST, (uint8_t *) &stage.as.fat.numEntries,
                           sizeof(uint32_t), false);
            UART_SendBytes(UART_HOST, (uint8_t *) stage.as.fat.slots,
                           (stage.as.fat.numEntries * sizeof(FS_Slot)), true);
            break;
        case OPREAD:
            SendHeader(OP_READ, stage.as.file.entry.filesize + 32);
            UART_SendBytes(UART_HOST,
                           (uint8_t *) stage.as.file.metadata.filename, 32,
                           false);
            UART_SendBytes(UART_HOST, (uint8_t *) stage.as.file.content,
                           stage.as.file.entry.filesize, true);
            break;
        case OPWRITE:
            SendHeader(OP_WRITE, 0);
            break;
        case OPREPLY:
        case OPSEND:
        case OPLISTEN:
            SendHeader(OP_LISTEN, 0);
            break;
        case OPRECEIVE:
            SendHeader(OP_RECEIVE, 0);
            break;
        case FLASHWRITEERROR:
            SendError("ERROR: Damn, I couldn't commit that to memory.");
            break;
        case FLASHERASEERROR:
            SendError("ERROR: Damn, I can't forget that now that I know it.");
            break;
        case INVALIDSLOT:
            SendError("ERROR: That slot ain't flying.");
            break;
        case SLOTEMPTY:
            SendError(
                "ERROR: I can't read an empty slot. What did you expect?");
            break;
        case KEYGENERROR:
            SendError("ERROR: Misplaced my keys! Oops.");
            break;
        case DECRYPTIONERROR:
            SendError(
                "ERROR: Decryption failed. Maybe the data was corrupted?");
            break;
        case ENCRYPTIONERROR:
            SendError("ERROR: Encryption failed.");
            break;
        case PERMISSIONERROR:
            SendError(
                "ERROR: You dont have permission to do that. Sucks to be you.");
            break;
        case INVALIDBODYSIZE:
            SendError("ERROR: That message was too fat for comfort.");
            break;
        case PEERERROR:
            SendError("ERROR: Something went wrong with the peer. Maybe they "
                      "sent something weird?");
            break;
        case UNKNOWNOP:
            SendError("ERROR: You somehow managed send something that I "
                      "cant parse. Congrats!");
            break;
        default:
            SendError("ERROR: Request not sexy enough, hogli bidu");
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
        memclear((uint8_t *) &stage, sizeof(FS_Stage), 0);
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

        if (SystemStatus.timeout == 0)
        {
            delay_cycles((32000000 * 9) / 2);
            FLASH_Erase((uint32_t) &SystemStatus, &status);
        }

        SendResponse(&status);
    }

    BlinkLED();
    return 0; /* Unreachable */
}
