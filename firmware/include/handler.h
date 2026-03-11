/**
 * @file handler.h
 * @author Sumedh Girish
 * @brief Command Handlers for Host and Peer interactions.
 *
 * Contains the business logic for all implemented protocol commands.
 * These functions parse decrypted command structures and invoke the
 * necessary filesystem or crypto routines.
 */

#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "status.h"
#include <stdint.h>

/** @brief Handles the LIST command. */
void ListHandler(StatusCode *status);

/** @brief Handles the READ command for a specific slot. */
void ReadHandler(uint8_t slot, StatusCode *status);

/** @brief Handles the WRITE command for a specific slot. */
void WriteHandler(uint8_t slot, StatusCode *status);

/** @brief Handles the INTERROGATE command. */
void InterrogateHandler(StatusCode *status);

/** @brief Handles the REPLY command from a peer. */
void ReplyHandler(StatusCode *status);

/** @brief Begins the SEND command logic (HSM to HSM transfer). */
void SendHandler(uint8_t slot, StatusCode *status);

/** @brief Begins the RECEIVE command logic (HSM to HSM transfer). */
void ReceiveHandler(uint8_t slot, StatusCode *status);

#endif
