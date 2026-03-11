/**
 * @file handler.c
 * @author Sumedh Girish
 * @brief Implementation of Host and Peer Command Handlers.
 *
 * Implements the execution flows for all system operations. Handlers are
 * responsible for orchestrating key derivation, AEAD encryption/decryption,
 * and calling the appropriate filesystem routines.
 */

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

/**
 * @brief Execution flow for a Host READ command.
 *
 * 1. Loads the target slot into the volatile `stage`.
 * 2. Derives the unique read key for the file's owner group.
 * 3. In-place decrypts the file payload using ASCON-128a.
 */
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

/**
 * @brief Execution flow for a Host WRITE command.
 *
 * 1. Derives the unique write key for the targeted group ID.
 * 2. In-place encrypts the staged plaintext using ASCON-128a.
 * 3. Triggers a commit to persistent flash storage.
 */
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

/**
 * @brief Execution flow for a Peer INTERROGATE command.
 *
 * Decrypts a filesystem allocation table (FAT) sent by a peer to determine
 * which files the peer holds that this HSM is authorized to RECEIVE. Filters
 * the FAT array in-place.
 */
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

/**
 * @brief Execution flow for generating a Peer REPLY.
 *
 * Constructs a FAT table containing all valid files currently held by this
 * HSM. Encrypts the table with the secure Interrogate key before transmission.
 */
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

/**
 * @brief Execution flow for a Peer SEND operation.
 *
 * Prepares a file sequence for inter-HSM transfer. Loads the specific slot,
 * re-encrypts the metadata (including the file tag and internal keys) using
 * the inter-HSM SEND key for secure wire transport.
 */
void SendHandler(uint8_t slot, StatusCode *status)
{
    if (*status != OPSEND)
        return;

    LoadFile(slot, status);
    if (*status != OPSEND)
        return;

    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectSendKey(stage.as.file.metadata.groupid, derivedKey);

    int estat;
    if (*status == OPSEND)
    {
        estat = ascon_aead_encrypt(
            (uint8_t *) &stage.preamble.tag,
            (uint8_t *) &stage.as.file.metadata.key,
            (uint8_t *) &stage.as.file.metadata.key,
            sizeof(FS_FileEntry) + sizeof(FS_Metadata) - sizeof(uint16_t),
            (uint8_t *) &stage.preamble.nonce,
            NONCE_SIZE + ID_SIZE + sizeof(uint16_t),
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

/**
 * @brief Execution flow for a Peer RECEIVE operation.
 *
 * Decrypts the metadata of an incoming file transfer using the inter-HSM
 * RECEIVE key. On success, commits the newly received file to flash.
 */
void ReceiveHandler(uint8_t slot, StatusCode *status)
{
    if (*status != OPRECEIVE)
        return;
    uint8_t derivedKey[ASCON_KEY_SIZE] = {0};
    *status = SelectRecvKey(stage.as.file.metadata.groupid, derivedKey);
    int dstat;
    if (*status == OPRECEIVE)
    {
        dstat = ascon_aead_decrypt(
            (uint8_t *) &stage.as.file.metadata.key,
            (uint8_t *) &stage.preamble.tag,
            (uint8_t *) &stage.as.file.metadata.key,
            sizeof(FS_FileEntry) + sizeof(FS_Metadata) - sizeof(uint16_t),
            (uint8_t *) &stage.preamble.nonce,
            NONCE_SIZE + ID_SIZE + sizeof(uint16_t),
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
    StoreFile(slot, status);
}
