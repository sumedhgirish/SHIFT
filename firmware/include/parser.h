/**
 * @file parser.h
 * @author Sumedh Girish
 * @brief Command Parsing and Validation layer.
 *
 * Implements the protocol decoding logic. Reads raw frames from the UART
 * host interface, validates their syntax and payload sizes, and forwards
 * them to the appropriately handlers.
 */

#ifndef __PARSER_H__
#define __PARSER_H__

#include "status.h"

/** @brief Parses and validates a LIST command frame. */
void ListParser(StatusCode *status);

/** @brief Parses and validates a READ command frame. */
void ReadParser(StatusCode *status);

/** @brief Parses and validates a WRITE command frame. */
void WriteParser(StatusCode *status);

/** @brief Parses and validates an INTERROGATE command frame. */
void InterrogateParser(StatusCode *status);

/** @brief Parses and validates a RECEIVE command frame. */
void ReceiveParser(StatusCode *status);

/** @brief Parses and validates a LISTEN command frame. */
void ListenParser(StatusCode *status);

#endif
