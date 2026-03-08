#ifndef __PARSER_H__
#define __PARSER_H__

#include "status.h"

void ListParser(StatusCode *status);
void ReadParser(StatusCode *status);
void WriteParser(StatusCode *status);
void InterrogateParser(StatusCode *status);
void ReceiveParser(StatusCode *status);
void ListenParser(StatusCode *status);

#endif
