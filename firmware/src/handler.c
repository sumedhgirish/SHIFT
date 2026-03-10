#include "handler.h"
#include "crypto_aead.h"
#include "filesystem.h"
#include "secrets.h"
#include "status.h"
#include "stubs.h"
#include <string.h>

void ListHandler(StatusCode *status)
{
    if (*status != OPLIST)
        return;

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

void ReadHandler(uint8_t slot, StatusCode *status)
{
    if (*status != OPREAD)
        return;

    LoadFile(slot, status);
    if (*status != OPREAD)
        return;

    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectReadKey(stage.as.file.metadata.groupid, derivedKey);

    int dstat;
    if (*status == OPREAD)
    {
        dstat = ascon_aead_decrypt(
            (uint8_t *) &stage.as.file.content,
            (uint8_t *) &stage.as.file.metadata.tag,
            (uint8_t *) &stage.as.file.content, MAX_FILE_SIZE,
            (uint8_t *) stage.as.file.metadata.filename,
            32 + sizeof(FS_FileEntry), (uint8_t *) stage.as.file.metadata.nonce,
            derivedKey);
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
    }
    else
    {
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
        return;
    }

    if (dstat != 0)
    {
        *status = DECRYPTIONERROR;
        return;
    }
}

void WriteHandler(uint8_t slot, StatusCode *status)
{
    if (*status != OPWRITE)
        return;

    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectWriteKey(stage.as.file.metadata.groupid, derivedKey);

    int estat;
    if (*status == OPWRITE)
    {
        estat = ascon_aead_encrypt(
            (uint8_t *) &stage.as.file.metadata.tag,
            (uint8_t *) &stage.as.file.content,
            (uint8_t *) &stage.as.file.content, MAX_FILE_SIZE,
            (uint8_t *) stage.as.file.metadata.filename,
            32 + sizeof(FS_FileEntry), (uint8_t *) stage.as.file.metadata.nonce,
            derivedKey);
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
    }
    else
    {
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
        return;
    }

    if (estat != 0)
    {
        *status = ENCRYPTIONERROR;
        return;
    }

    StoreFile(slot, status);
}

static inline bool isReceivable(uint16_t groupid)
{
    for (uint8_t i = 0; i < N_RECV_GROUPS; ++i)
    {
        if (recv_groups[i] == groupid)
            return true;
    }
    return false;
}

void InterrogateHandler(StatusCode *status)
{
    if (*status != OPINTERROGATE)
        return;

    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectInterrogateKey(derivedKey);

    int dstat;
    if (*status == OPINTERROGATE)
    {
        dstat = ascon_aead_decrypt(
            (uint8_t *) &stage.as.fat, (uint8_t *) &stage.preamble.tag,
            (uint8_t *) &stage.as.fat, sizeof(FS_FAT),
            (uint8_t *) &stage.preamble.nonce, NONCE_SIZE + ID_SIZE,
            (uint8_t *) stage.preamble.nonce, derivedKey);
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
    }
    else
    {
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
        return;
    }

    if (dstat != 0)
    {
        *status = DECRYPTIONERROR;
        return;
    }

    uint32_t recvFiles = 0;
    for (uint8_t i = 0; i < stage.as.fat.numEntries; ++i)
    {
        if (isReceivable(stage.as.fat.slots[i].groupid))
        {
            stage.as.fat.slots[recvFiles] = stage.as.fat.slots[i];
            recvFiles++;
        }
    }
    stage.as.fat.numEntries = recvFiles;
}

void ReplyHandler(StatusCode *status)
{
    if (*status != OPREPLY)
        return;

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

    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectReplyKey(derivedKey);

    int estat;
    if (*status == OPREPLY)
    {
        estat = ascon_aead_encrypt(
            (uint8_t *) &stage.preamble.tag, (uint8_t *) &stage.as.fat,
            (uint8_t *) &stage.as.fat, sizeof(FS_FAT),
            (uint8_t *) &stage.preamble.nonce, NONCE_SIZE + ID_SIZE,
            (uint8_t *) stage.preamble.nonce, derivedKey);
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
    }
    else
    {
        memclear(derivedKey, ASCON_KEY_SIZE, 0);
        return;
    }

    if (estat != 0)
    {
        *status = ENCRYPTIONERROR;
        return;
    }
}
