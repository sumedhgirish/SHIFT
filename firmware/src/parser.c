#include "parser.h"
#include "filesystem.h"
#include "randombytes.h"
#include "secrets.h"
#include "stubs.h"
#include "uart.h"
#include <assert.h>
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

void InterrogateParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
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

void ListenParser(StatusCode *status)
{
}
