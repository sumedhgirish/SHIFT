
#include "parser.h"
#include "stubs.h"
#include "uart.h"

void ListParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }
}

void ReadParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }
}

void WriteParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
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
}

void ReceiveParser(StatusCode *status)
{
    uint8_t pin[6] = {0};
    UART_RecvBytes(UART_HOST, pin, 6, true);

    if (!checkpin(pin))
    {
        *status = PERMISSIONERROR;
        return;
    }
}

void ListenParser(StatusCode *status)
{
}
