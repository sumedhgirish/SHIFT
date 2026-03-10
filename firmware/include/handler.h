#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "status.h"
#include <stdint.h>

void ListHandler(StatusCode *status);
void ReadHandler(uint8_t slot, StatusCode *status);
void WriteHandler(uint8_t slot, StatusCode *status);
void InterrogateHandler(StatusCode *status);
void ReplyHandler(StatusCode *status);
void SendHandler(uint8_t slot, StatusCode *status);
void ReceiveHandler(uint8_t slot, StatusCode *status);

#endif
