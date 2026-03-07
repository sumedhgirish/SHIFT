#include "dl_config.h"
#include "ti/driverlib/dl_gpio.h"
#include <stdbool.h>

static void init(void)
{
    SYSCFG_DL_init();
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

    BlinkLED();
    return 0; /* Unreachable */
}
