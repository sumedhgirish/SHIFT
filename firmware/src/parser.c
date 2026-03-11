/**
 * @file parser.c
 * @author Sumedh Girish
 * @brief Request stream parsing and initial validation layer.
 *
 * The parser layer reads incoming UART payloads, extracts parameters like
 * user PINs, slot indices, and payload sizes, and performs the first layer
 * of authorization (e.g., matching the User PIN) before handing off cleanly
 * parsed structures to the core Handlers.
 */

#include "parser.h"
#include "filesystem.h"
#include "handler.h"
#include "randombytes.h"
#include "secrets.h"
#include "status.h"
#include "stubs.h"
#include "uart.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief Parses an inbound Host LIST command and verifies the user PIN.
 */
void ListParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }

    ListHandler(status);
}

/**
 * @brief Parses an inbound Host READ command, fetching target slot and PIN.
 */
void ReadParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    uint8_t readSlot;

    UART_RecvBytes(UART_HOST, pin, 6, false);
    UART_RecvBytes(UART_HOST, &readSlot, 1, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }

    if (readSlot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }

    ReadHandler(readSlot, status);
}

/**
 * @brief Parses an inbound Host WRITE command and manages volatile staging.
 *
 * Consumes the PIN, target slot, group assignment, filename, and payload. If
 * verification fails natively, the UART is drained to maintain synchronization.
 */
void WriteParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    uint8_t writeSlot;
    uint16_t groupid;
    uint8_t filename[32] = {0};
    uint16_t filesize = 0;

    UART_RecvBytes(UART_HOST, pin, 6, false);
    UART_RecvBytes(UART_HOST, &writeSlot, 1, false);
    UART_RecvBytes(UART_HOST, (uint8_t *) &groupid, 2, false);
    UART_RecvBytes(UART_HOST, filename, 32, false);
    UART_RecvBytes(UART_HOST, (uint8_t *) stage.as.file.entry.uuid, 16, false);
    UART_RecvBytes(UART_HOST, (uint8_t *) &filesize, 2, false);

    if (!checkpin(pin))
    {
        memclear((uint8_t *) &stage, sizeof(stage), 0);
        UART_RecvBytes(UART_HOST, NULL, filesize, true);
        *status = PERMISSIONERROR;
        return;
    }

    if (writeSlot >= NUM_SLOTS)
    {
        memclear((uint8_t *) &stage, sizeof(stage), 0);
        UART_RecvBytes(UART_HOST, NULL, filesize, true);
        *status = INVALIDSLOT;
        return;
    }

    if (filesize > MAX_FILE_SIZE)
    {
        memclear((uint8_t *) &stage, sizeof(stage), 0);
        UART_RecvBytes(UART_HOST, NULL, filesize, true);
        *status = INVALIDBODYSIZE;
        return;
    }

    UART_RecvBytes(UART_HOST, (uint8_t *) stage.as.file.content, filesize,
                   true);

    strncpy((char *) stage.as.file.metadata.filename, (const char *) filename,
            32);
    stage.as.file.metadata.groupid = groupid;
    stage.as.file.entry.filesize = filesize;
    randombytes((uint8_t *) stage.as.file.metadata.fileid, ID_SIZE);

    WriteHandler(writeSlot, status);
}

const uint8_t peer_magic_byte = '^';
#define OP_INTERROGATE 'I'
#define OP_RECEIVE 'R'
#define OP_ERROR 'E'

static inline void ResolveError(StatusCode *status)
{
    uint16_t errorCode;
    UART_RecvBytes(UART_PEER, (uint8_t *) &errorCode, 2, true);
    switch (errorCode)
    {
        case PERMISSIONERROR:
            *status = PERMISSIONERROR;
            break;
        case INVALIDSLOT:
            *status = INVALIDSLOT;
            break;
        case INVALIDBODYSIZE:
            *status = INVALIDBODYSIZE;
            break;
        default:
            *status = PEERERROR;
            break;
    }
}

void InterrogateParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }

    uint8_t opCode = OP_INTERROGATE;
    UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
    UART_SendBytes(UART_PEER, &opCode, 1, false);

    uint16_t bodyLen = 0;
    UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);

    *status = UNKNOWNOP;
    while (*status == UNKNOWNOP)
    {
        UART_RecvUntil(UART_PEER, peer_magic_byte);
        UART_RecvBytes(UART_PEER, &opCode, 1, false);
        switch (opCode)
        {
            case OP_INTERROGATE:
                *status = OPINTERROGATE;
                break;
            case OP_ERROR:
                ResolveError(status);
                return;
            default:
                *status = UNKNOWNOP;
                break;
        }
    }

    uint16_t bodySize;
    UART_RecvBytes(UART_PEER, (uint8_t *) &bodySize, 2, true);

    *status = (bodySize <= sizeof(FS_FAT)) ? OPINTERROGATE : INVALIDBODYSIZE;

    UART_RecvBytes(UART_PEER, (uint8_t *) &stage.preamble.nonce, NONCE_SIZE,
                   false);
    UART_RecvBytes(UART_PEER, (uint8_t *) &stage.preamble.mesgid, ID_SIZE,
                   false);
    UART_RecvBytes(UART_PEER, (uint8_t *) &stage.preamble.tag, ASCON_TAG_SIZE,
                   true);

    switch (*status)
    {
        case OPINTERROGATE:
            UART_RecvBytes(UART_PEER, (uint8_t *) &stage.as.fat, bodySize,
                           true);
            break;
        default:
            UART_RecvBytes(UART_PEER, NULL, bodySize, true);
            break;
    }

    InterrogateHandler(status);
}

/**
 * @brief Parses an inbound Peer RECEIVE command, intercepting secure file transfers.
 *
 * Reads metadata and the encrypted file chunk from the peer link, syncing it
 * into the `stage` buffer before requesting handler decryption and storage.
 */
void ReceiveParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    uint8_t readSlot;
    uint8_t writeSlot;

    UART_RecvBytes(UART_HOST, pin, 6, false);
    UART_RecvBytes(UART_HOST, &readSlot, 1, false);
    UART_RecvBytes(UART_HOST, &writeSlot, 1, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }

    if (readSlot >= NUM_SLOTS || writeSlot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }

    uint8_t opCode = OP_RECEIVE;
    UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
    UART_SendBytes(UART_PEER, &opCode, 1, false);

    uint16_t bodyLen = 1;
    UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);

    UART_SendBytes(UART_PEER, &readSlot, 1, true);

    *status = UNKNOWNOP;
    while (*status == UNKNOWNOP)
    {
        UART_RecvUntil(UART_PEER, peer_magic_byte);
        UART_RecvBytes(UART_PEER, &opCode, 1, false);
        switch (opCode)
        {
            case OP_RECEIVE:
                *status = OPRECEIVE;
                break;
            case OP_ERROR:
                ResolveError(status);
                return;
            default:
                *status = UNKNOWNOP;
                break;
        }
    }

    uint16_t bodySize;
    UART_RecvBytes(UART_PEER, (uint8_t *) &bodySize, 2, true);

    *status = (bodySize == sizeof(FS_File)) ? OPRECEIVE : INVALIDBODYSIZE;

    UART_RecvBytes(UART_PEER, (uint8_t *) &stage.preamble, sizeof(FS_Preamble),
                   true);

    switch (*status)
    {
        case OPRECEIVE:
            UART_RecvBytes(UART_PEER, (uint8_t *) &stage.as.file,
                           sizeof(FS_File), true);
            ReceiveHandler(writeSlot, status);
            break;
        default:
            UART_RecvBytes(UART_PEER, NULL, bodySize, true);
            break;
    }
}

static void ReplyParser(StatusCode *status)
{
    ReplyHandler(status);

    uint8_t opCode;
    uint16_t bodyLen;

    if (*status != OPREPLY)
    {
        opCode = OP_ERROR;
        bodyLen = (uint16_t) (*status);
        UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
        UART_SendBytes(UART_PEER, &opCode, 1, false);
        UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);
        return;
    }

    opCode = OP_INTERROGATE;
    UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
    UART_SendBytes(UART_PEER, &opCode, 1, false);

    bodyLen = sizeof(FS_FAT);
    UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.nonce, NONCE_SIZE,
                   false);
    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.mesgid, ID_SIZE,
                   false);
    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.tag, ASCON_TAG_SIZE,
                   true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.as.fat, bodyLen, true);
}

static void SendParser(StatusCode *status)
{
    uint8_t readSlot;
    UART_RecvBytes(UART_PEER, &readSlot, 1, true);

    if (readSlot >= NUM_SLOTS)
    {
        *status = INVALIDSLOT;
        return;
    }

    SendHandler(readSlot, status);

    uint8_t opCode;
    uint16_t bodyLen;

    if (*status != OPSEND)
    {
        opCode = OP_ERROR;
        bodyLen = (uint16_t) (*status);
        UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
        UART_SendBytes(UART_PEER, &opCode, 1, false);
        UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);
        return;
    }

    opCode = OP_RECEIVE;
    UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
    UART_SendBytes(UART_PEER, &opCode, 1, false);

    bodyLen = sizeof(FS_File);
    UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble, sizeof(FS_Preamble),
                   true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.as.file, sizeof(FS_File),
                   true);
}

void ListenParser(StatusCode *status)
{
    uint8_t opCode;

    *status = UNKNOWNOP;
    while (*status == UNKNOWNOP)
    {
        UART_RecvUntil(UART_PEER, peer_magic_byte);
        UART_RecvBytes(UART_PEER, &opCode, 1, false);
        switch (opCode)
        {
            case OP_INTERROGATE:
                *status = OPREPLY;
                break;
            case OP_RECEIVE:
                *status = OPSEND;
                break;
            default:
                *status = UNKNOWNOP;
                break;
        }
    }

    uint16_t bodySize;
    UART_RecvBytes(UART_PEER, (uint8_t *) &bodySize, 2, true);

    switch (*status)
    {
        case OPREPLY:
            *status = (bodySize == 0) ? OPREPLY : INVALIDBODYSIZE;
            break;
        case OPSEND:
            *status = (bodySize == 1) ? OPSEND : INVALIDBODYSIZE;
            break;
        default:
            *status = UNKNOWNOP;
    }

    switch (*status)
    {
        case OPREPLY:
            ReplyParser(status);
            break;
        case OPSEND:
            SendParser(status);
            break;
        default:
            return;
    }
}
