#include "parser.h"
#include "filesystem.h"
#include "randombytes.h"
#include "secrets.h"
#include "status.h"
#include "stubs.h"
#include "uart.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

void ListParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }

    uint32_t numEntries = 0;
    for (uint8_t i = 0; i < NUM_SLOTS; i++)
    {
        if (Metadata_Table[i].status.magic == 0xdeadf00d &&
            securecmp((uint8_t *) Metadata_Table[i].status.id,
                      (uint8_t *) FAT_Table.entry[i].uuid, 16) == true)
        {
            stage.as.fat.slots[numEntries].groupid =
                Metadata_Table[i].data.groupid;
            stage.as.fat.slots[numEntries].slot = i;
            memcpy((uint8_t *) stage.as.fat.slots[numEntries].filename,
                   (uint8_t *) Metadata_Table[i].data.filename, 32);

            numEntries++;
        }
    }

    stage.as.fat.numEntries = numEntries;
}

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

    LoadFile(readSlot, status);
}

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

    strncpy((char *) stage.as.file.metadata.filename, (const char *) filename,
            32);
    stage.as.file.metadata.groupid = groupid;
    stage.as.file.entry.filesize = filesize;
    randombytes((uint8_t *) stage.as.file.metadata.fileid, ID_SIZE);

    UART_RecvBytes(UART_HOST, (uint8_t *) stage.as.file.content, filesize,
                   true);

    StoreFile(writeSlot, status);
}

const uint8_t peer_magic_byte = '^';
#define OP_INTERROGATE 'I'
#define OP_RECEIVE 'R'

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
}

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
}

static void ReplyParser(StatusCode *status)
{
    uint32_t numEntries = 0;
    for (uint8_t i = 0; i < NUM_SLOTS; i++)
    {
        if (Metadata_Table[i].status.magic == 0xdeadf00d &&
            securecmp((uint8_t *) Metadata_Table[i].status.id,
                      (uint8_t *) FAT_Table.entry[i].uuid, 16) == true)
        {
            stage.as.fat.slots[numEntries].groupid =
                Metadata_Table[i].data.groupid;
            stage.as.fat.slots[numEntries].slot = i;
            memcpy((uint8_t *) stage.as.fat.slots[numEntries].filename,
                   (uint8_t *) Metadata_Table[i].data.filename, 32);

            numEntries++;
        }
    }

    stage.as.fat.numEntries = numEntries;

    uint8_t opCode = OP_INTERROGATE;
    UART_SendBytes(UART_PEER, (uint8_t *) &peer_magic_byte, 1, false);
    UART_SendBytes(UART_PEER, &opCode, 1, false);

    uint16_t bodyLen =
        (uint16_t) numEntries * sizeof(FS_Slot) + sizeof(uint32_t);
    UART_SendBytes(UART_PEER, (uint8_t *) &bodyLen, 2, true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.nonce, NONCE_SIZE,
                   false);
    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.mesgid, ID_SIZE,
                   false);
    UART_SendBytes(UART_PEER, (uint8_t *) &stage.preamble.tag, ASCON_TAG_SIZE,
                   true);

    UART_SendBytes(UART_PEER, (uint8_t *) &stage.as.fat, bodyLen, true);
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
        case OPINTERROGATE:
            break;
        default:
            return;
    }
}
