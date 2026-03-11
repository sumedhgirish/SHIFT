/**
 * @file status.h
 * @author Sumedh Girish
 * @brief Global Status Codes and Operations.
 *
 * Defines the canonical `StatusCode` enumeration used throughout the firmware
 * to track the current operation type and report fatal exceptions.
 */

#ifndef __STATUS_H__
#define __STATUS_H__

/**
 * @brief Represents the active operation or a terminal error state.
 */
typedef enum
{
    // --- Operations ---
    OPLIST,         /**< Execution of a LIST operation. */
    OPWRITE,        /**< Execution of a WRITE operation. */
    OPREAD,         /**< Execution of a READ operation. */
    OPSEND,         /**< Execution of a peer SEND. */
    OPRECEIVE,      /**< Execution of a peer RECEIVE. */
    OPINTERROGATE,  /**< Execution of a peer INTERROGATE. */
    OPREPLY,        /**< Execution of a peer REPLY. */
    OPLISTEN,       /**< Waiting for peer provisioning. */

    // --- Exceptions ---
    UNKNOWNOP,        /**< Unrecognized command opcode received. */
    INVALIDBODYSIZE,  /**< Payload size does not match expected bounds. */
    INVALIDSLOT,      /**< Requested slot ID is out of range. */
    PERMISSIONERROR,  /**< Access denied due to key mismatch. */
    KEYGENERROR,      /**< Failure during cryptographic key generation. */
    SLOTEMPTY,        /**< Target slot contains no data. */
    FLASHERASEERROR,  /**< Hard fault during flash sector erase. */
    FLASHWRITEERROR,  /**< Hard fault or misalignment during flash write. */
    DECRYPTIONERROR,  /**< Payload authentication/decryption failed. */
    ENCRYPTIONERROR,  /**< Payload encryption failed. */
    PEERERROR,        /**< Peer HSM responded with an error or timeout. */
} StatusCode;

#endif
