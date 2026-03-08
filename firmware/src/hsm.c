#include "dl_config.h"
#include "ti/driverlib/dl_gpio.h"
#include "uart.h"
#include <assert.h>
#include <stdbool.h>

static void init(void)
{
    __disable_irq();

    UART_init();
    SYSCFG_DL_init();

    __enable_irq();
}

static void BlinkLED(void)
{
    while (true)
    {
        DL_GPIO_togglePins(LEDS_PORT, LEDS_STATUS_LED_PIN);
        delay_cycles(32000000 * 2); // 2 seconds
    }
}

int main(void)
{
    init();

    UART_RecvUntil(UART_HOST, '%');
    uint8_t op[3] = {0};
    UART_RecvBytes(UART_HOST, op, 3, true);

    uint8_t magic = '%';
    UART_SendBytes(UART_HOST, &magic, 1, false);
    UART_SendBytes(UART_HOST, op, 3, true);

    BlinkLED();
    return 0; /* Unreachable */
}
